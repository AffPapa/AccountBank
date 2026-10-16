# AccountBank macOS resource benchmark

The sampler reads macOS `libproc` counters for **one PID**. It never opens a
profile directory or reads Telegram messages, session files or API credentials.
Its CSV contains the numeric PID, a manually declared account count and phase,
CPU percent (100% means one full core), memory in MiB, thread count, and disk
read/write traffic during each interval. Disk traffic is **not** disk space used
by the profile. `physical_footprint_mib` is the primary RAM comparison metric;
`resident_mib` is included for reference.

Build on the Mac inside the PaSEO workspace:

```sh
cd /Users/dumay/Desktop/PaSEO/AccountBank
clang -O2 -Wall -Wextra -Werror tools/accountbank-benchmark.c \
  -o /Users/dumay/Desktop/PaSEO/.tools/accountbank-benchmark
```

For each milestone (1, 5, 10, 20, 30 *actually logged-in* accounts), launch
the same AccountBank build with the same test workdir. Obtain its PID from
Activity Monitor. Wait two minutes after login/sync, then sample an untouched
idle window and a repeatable active task separately:

```sh
/Users/dumay/Desktop/PaSEO/.tools/accountbank-benchmark \
  --pid PID --accounts 5 --phase idle --seconds 60 --interval 5 \
  > /Users/dumay/Desktop/PaSEO/.tools/accountbank-5-idle.csv
```

Replace `PID` with the actual process number; never paste a shell command line
containing session or API values. Run a second sample with `--phase active`
while manually scrolling the same type of chat. Use the same 60-second window
at every milestone. Repeat after sleep/wake and after one hour of idle time to
catch reconnection and memory growth. Record separately whether all declared
accounts still receive updates, since the sampler cannot verify logins or
message delivery. Do not copy profiles from Telegram or another application.

The tool labels account counts; it does not inspect or verify them. Running it
against a one-account process with `--accounts 30` would produce invalid
evidence. A source build, CSV, or synthetic 30-account fixture alone cannot
prove real 30-account performance.
