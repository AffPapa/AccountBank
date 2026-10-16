# Disposable storage corruption check (macOS)

Use only a newly created, **never logged in** AccountBank work directory under
`AccountBank/.tools/storage-fixture/`. Never point these steps at a test package
that contains a real session, another Telegram app, or a personal data folder.
The agent shell cannot start AppKit `.app` bundles, so this is a manual GUI
check from the user's Terminal.app session.

1. Create an empty `AccountBank/.tools/storage-fixture/baseline` directory. Launch
   the current test app executable with `-workdir` set to that absolute path and
   `-noupdate`. Do **not** log in. Quit normally, then launch and quit this
   *same* disposable directory once more. A normal blank account may have an
   encrypted authorization file and no `map`; that restart must succeed.
2. Confirm the baseline contains an account key (`tdata/key_<data-name>s`, or
   legacy `0/1`). For the blank-slot case, confirm a top-level encrypted
   authorization file `<account-hash>s` and no
   `tdata/<same-account-hash>/maps` (or `map0/1`). The app's work directory is
   the given `-workdir`; `tdata` is its child. If the blank auth file is absent,
   this case is **not run**. Do not invent a stand-in. For a separate map case,
   change one harmless local setting until a `map` file exists; copy that
   fixture separately and never merge it into the blank baseline.
3. For each test, copy its entire baseline to a new sibling directory. In one
   blank-slot copy, flip one byte in its authorization file; in another, delete
   only its key. In the map fixture copies, flip one byte in exactly one key,
   map, or authorization file; in a separate map copy, delete only the
   authorization file. Record SHA-256 of every remaining file **in
   that copy** before launch. Do not reuse or modify a baseline.
4. Launch each copy once with its own `-workdir`. The missing-key case still
   contains encrypted account files, so it must show a storage error and quit:
   without the key, the app cannot prove that the authorization belongs to a
   blank account. A truly empty work directory should create a new blank
   profile. Corrupt key, map, or authorization must show a storage error and
   quit, with no new blank login replacing the original account.
5. After each run, compare SHA-256 of every file that existed before launch
   with the hashes recorded in step 3. A mismatch is a failure. Also inspect
   whether a new `key_*s` or account directory appeared.

Additional startup cases for a **disposable** fixture:

| Case | Expected result |
| --- | --- |
| Fresh app, QR screen opened but never scanned, normal quit and relaunch | Opens the blank login again; existing encrypted auth may be present without a map. |
| One valid synthetic session followed by a blank last slot | Retains the valid session and skips the unfinished blank slot. |
| Several blank slots and no valid session | Keeps one blank login, without creating duplicates. |
| Existing mapless auth with a fully parsed nonzero user ID | Recover that authorized slot without a `map`; never start a blank replacement. Requires a synthetic fixture or an isolated test login interrupted before the delayed map write. |
| Existing mapless auth with unreadable ciphertext | Storage error; no new blank login. |
| Existing map, missing authorization file | Storage error; authorization is not recreated, and map/key hashes stay unchanged. |
| Existing map, valid encrypted envelope but malformed inner authorization | Storage error; no slot is silently dropped. Requires a synthetic fixture writer. |

Only the first case can be produced by simply launching the app without a
Telegram login. The others require a synthetic encrypted fixture generator;
do not substitute copied real sessions. They are specified checks, not tests
claimed as run.

The byte flip can be done with a small script against a copied fixture file:

```python
from pathlib import Path
import sys

path = Path(sys.argv[1]).resolve()
if "storage-fixture" not in path.parts:
    raise SystemExit("refusing a path outside storage-fixture")
data = bytearray(path.read_bytes())
if len(data) < 32:
    raise SystemExit("file too short")
data[-17] ^= 0x01
path.write_bytes(data)
```

This test proves only that the known bytes are preserved for these synthetic
cases. It does not prove recovery of every historical Telegram file format,
or simultaneous 30-account startup. A mapless nonzero-user authorization
fixture requires a dedicated synthetic encrypted writer; do not make one by
copying a real session. The current startup path can still write
per-account migration files before another later account fails. Preserve a
full copy of any real account folder before attempting repair.
