// AccountBank process benchmark for macOS. Reads kernel counters only.
// Build: clang -O2 -Wall -Wextra tools/accountbank-benchmark.c -o accountbank-benchmark
// Example: ./accountbank-benchmark --pid 12345 --accounts 5 --phase idle --seconds 60

#include <errno.h>
#include <inttypes.h>
#include <libproc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc_info.h>
#include <sys/resource.h>
#include <time.h>

typedef struct {
  struct rusage_info_v2 usage;
  struct proc_taskinfo task;
  struct timespec time;
} Sample;

static void usage(const char *program) {
  fprintf(stderr,
      "Usage: %s --pid PID --accounts {1|5|10|20|30} "
      "--phase {idle|active} [--seconds 60] [--interval 5]\n"
      "Accounts and phase are labels supplied by the tester, not verified by this tool.\n"
      "Output contains kernel counters only; redirect CSV into the PaSEO workspace.\n",
      program);
}

static bool parse_int(const char *text, int min, int max, int *out) {
  char *end = NULL;
  errno = 0;
  long value = strtol(text, &end, 10);
  if (errno || end == text || *end != '\0' || value < min || value > max) {
    return false;
  }
  *out = (int)value;
  return true;
}

static bool take_sample(int pid, Sample *sample) {
  if (clock_gettime(CLOCK_MONOTONIC, &sample->time) != 0) {
    return false;
  }
  memset(&sample->usage, 0, sizeof(sample->usage));
  if (proc_pid_rusage(pid, RUSAGE_INFO_V2,
          (rusage_info_t *)&sample->usage) != 0) {
    return false;
  }
  memset(&sample->task, 0, sizeof(sample->task));
  return proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &sample->task,
      sizeof(sample->task)) == sizeof(sample->task);
}

static uint64_t difference(uint64_t after, uint64_t before) {
  return after >= before ? after - before : 0;
}

static double elapsed_seconds(const struct timespec *after,
    const struct timespec *before) {
  return (double)(after->tv_sec - before->tv_sec)
      + (double)(after->tv_nsec - before->tv_nsec) / 1000000000.0;
}

int main(int argc, char **argv) {
  int pid = 0, accounts = 0, seconds = 60, interval = 5;
  const char *phase = NULL;
  for (int i = 1; i < argc; i++) {
    if (i + 1 >= argc) {
      usage(argv[0]);
      return 2;
    }
    const char *name = argv[i];
    const char *value = argv[++i];
    if (!strcmp(name, "--pid")) {
      if (!parse_int(value, 1, INT32_MAX, &pid)) return 2;
    } else if (!strcmp(name, "--accounts")) {
      if (!parse_int(value, 1, 30, &accounts)
          || (accounts != 1 && accounts != 5 && accounts != 10
              && accounts != 20 && accounts != 30)) return 2;
    } else if (!strcmp(name, "--seconds")) {
      if (!parse_int(value, 1, 3600, &seconds)) return 2;
    } else if (!strcmp(name, "--interval")) {
      if (!parse_int(value, 1, 60, &interval)) return 2;
    } else if (!strcmp(name, "--phase")) {
      if (strcmp(value, "idle") && strcmp(value, "active")) return 2;
      phase = value;
    } else {
      usage(argv[0]);
      return 2;
    }
  }
  if (!pid || !accounts || !phase || interval > seconds
      || seconds % interval != 0) {
    usage(argv[0]);
    return 2;
  }

  Sample previous, current;
  if (!take_sample(pid, &previous)) {
    fprintf(stderr, "Cannot read process counters for PID %d: %s\n",
        pid, strerror(errno));
    return 1;
  }
  const struct timespec started = previous.time;
  const uint64_t process_started = previous.usage.ri_proc_start_abstime;
  puts("pid,accounts_declared,phase,elapsed_s,cpu_pct,physical_footprint_mib,"
       "resident_mib,threads,disk_read_delta_mib,disk_write_delta_mib");
  fflush(stdout);

  for (int elapsed = interval; elapsed <= seconds; elapsed += interval) {
    struct timespec delay = {.tv_sec = interval, .tv_nsec = 0};
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
    if (!take_sample(pid, &current)) {
      fprintf(stderr, "Process %d exited or counters became unavailable.\n", pid);
      return 1;
    }
    if (current.usage.ri_proc_start_abstime != process_started) {
      fprintf(stderr, "PID %d now belongs to a different process.\n", pid);
      return 1;
    }
    const double wall = elapsed_seconds(&current.time, &previous.time);
    const uint64_t cpu = difference(current.usage.ri_user_time,
        previous.usage.ri_user_time)
        + difference(current.usage.ri_system_time,
            previous.usage.ri_system_time);
    const uint64_t read = difference(current.usage.ri_diskio_bytesread,
        previous.usage.ri_diskio_bytesread);
    const uint64_t written = difference(current.usage.ri_diskio_byteswritten,
        previous.usage.ri_diskio_byteswritten);
    const double mib = 1024.0 * 1024.0;
    printf("%d,%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%.2f,%.2f\n",
        pid, accounts, phase, elapsed_seconds(&current.time, &started),
        wall > 0 ? (double)cpu / (wall * 10000000.0) : 0,
        (double)current.usage.ri_phys_footprint / mib,
        (double)current.task.pti_resident_size / mib,
        current.task.pti_threadnum,
        (double)read / mib,
        (double)written / mib);
    fflush(stdout);
    previous = current;
  }
  return 0;
}
