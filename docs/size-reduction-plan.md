# AccountBank size reduction plan

## Baseline (v11, Apple Silicon Release)

- Test ZIP: 94,741,193 bytes.
- Unpacked app payload: 278,594,411 bytes.
- Stripped arm64 executable: 262,442,784 bytes (94.2% of payload).
- Resources: about 16.15 MB, including `Telegram.rcc` at 13.32 MB.
- These are disk sizes. RAM, CPU, startup time and 20–30-account behavior have not been measured.

## v12 result (Release package, 2026-10-16)

- ZIP: 93,691,280 bytes, down 1,049,913 bytes (1.11%) from v11.
- Stripped arm64 executable: 259,095,968 bytes, down 3,346,816 bytes from v11.
- Release build and its second, no-op build passed. The ZIP hash and ad-hoc signature passed verification; `otool -L` shows no Swift or Translation framework dependency.
- Export UI/manager and sticker-set authoring/overlay sources are excluded from the build. Existing export storage keys remain readable and preserved.
- Ordinary sticker messages now show localized text, including the sticker's emoji description when available. This is a presentation change only. Custom emoji in text, isolated animated emoji, dice, reactions, previews and per-account sticker preloads still use their existing renderers; no RAM saving is claimed.
- The upgraded-profile GUI, login after restart and multi-account behavior remain unverified. Keep v11 as rollback until the user confirms v12 works with the copied profile. Never run both versions at once: their separate workdirs currently contain the same authorization data.

The v12 Release package used `strip -x`. The much larger unstripped build output is not a further release-size opportunity. Compare only packages made with the same signing, stripping and compression process.

## Packaging experiment after v12 (2026-10-16)

- An isolated copy of the v12 app was fully stripped, ad-hoc signed, compressed with `ditto --zlibCompressionLevel 9`, extracted, and signature-verified. This did not use a live account profile.
- The resulting ZIP was 93,161,932 bytes, down 529,348 bytes from the v12 ZIP. Its executable was 257,746,128 bytes, down 1,349,840 bytes. This changes distribution size only; it is not a RAM or runtime improvement.
- `tools/package-test-release.command` now uses those measured options. The next real package still needs its own ZIP checksum and signature verification.
- The separate `-Os` arm64 Release build in `out-size-os` completed, and a second build reported no work. It uses the same generated API configuration as `out-release` (checked by byte comparison without printing the file).

## v13 size experiment (2026-10-16)

- Test ZIP: 85,662,838 bytes, down 8,028,442 bytes from v12. Against the v12 code packaged with the new strip/compression settings (93,161,932 bytes), the `-Os` build saves 7,499,094 bytes.
- Stripped executable: 227,870,736 bytes, down 31,225,232 bytes from v12. Against the v12 code with full strip (257,746,128 bytes), `-Os` saves 29,875,392 bytes.
- The v13 archive passed SHA-256 and ad-hoc signature verification after extraction, and the executable is arm64. The ordinary build's generated API configuration header is byte-identical. No live session data was opened for this experiment.
- Source behavior, startup/scroll performance and 20–30-account resource use have not been verified in the GUI. Keep v12 as rollback. The v13 package is an isolated test build; `out-release` retains its prior `-O3` configuration.
- Reproduce the build from the AccountBank repository root with the local Release Qt installation and local API configuration already present:

  ```sh
  qt_prefix="$(cd ../Libraries/local/Qt-6.11.2-Release-arm64 && pwd -P)"
  QT=6.11.2 cmake -S . -B out-size-os -G 'Ninja Multi-Config' \
    "-DACCOUNTBANK_QT_PREFIX=$qt_prefix" \
    "-DCMAKE_PREFIX_PATH=$qt_prefix" \
    -DDESKTOP_APP_DISABLE_AUTOUPDATE=ON \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    '-DCMAKE_CXX_FLAGS_RELEASE=-Os -DNDEBUG' \
    '-DCMAKE_C_FLAGS_RELEASE=-Os -DNDEBUG' \
    '-DCMAKE_OBJCXX_FLAGS_RELEASE=-Os -DNDEBUG' \
    '-DCMAKE_OBJC_FLAGS_RELEASE=-Os -DNDEBUG'
  cmake --build out-size-os --config Release --target Telegram -j 8
  ACCOUNTBANK_BUILD_TREE=out-size-os tools/package-test-release.command v13
  ```

## v22 `-Oz` experiment (2026-10-16)

- Same source and generated API header as the v21 `-Os` build; the four Release language flags changed from `-Os` to `-Oz`. Swift translation and auto-update remain disabled, target remains arm64.
- v21 `-Os`: ZIP 85,670,410 bytes; stripped executable 227,887,280 bytes.
- v22 `-Oz`: ZIP 77,232,145 bytes; stripped executable 179,651,360 bytes. Measured saving: 8,438,265 bytes (9.85%) in ZIP and 48,235,920 bytes (21.17%) in the executable. Unstripped build artifacts were larger with `-Oz`, so comparing them would have given the wrong conclusion.
- The first experimental build failed because the new profile localization key was added after that tree had generated its language header. Regeneration and a resumed build passed, followed by a no-op rebuild. The packaged ZIP passed checksum, ad-hoc signature verification after extraction and arm64 architecture check.
- This is **disk size**, not measured RAM or speed. v21 stays as the `-Os` performance rollback. Make v22 the preferred size test only after login/restart, scrolling and 1/5/10/20/30-account benchmarks.
- Reproduce from the same configure command as v13 above, replacing the four `-Os` flags with `-Oz` and using `out-size-oz`. Do not print values from the local API header or CMake cache.

## Product boundary

Keep authorization, encrypted storage, account switching, MTProto updates, text, photos, captions, files, notifications and protected-content rules. The user has confirmed one real-account login on v11; login after restart and the 20–30-account target remain unverified. The user chose **text placeholders** for incoming stickers and animated custom emoji. Keep ordinary Unicode emoji as text characters. Preserve message IDs, reaction IDs and original entities in the data model; only replace presentation. Do not inspect or copy live account data for code-size experiments.

## Work slices

| Slice | Candidate | Why | Gate |
| --- | --- | --- | --- |
| 1 | Remove sticker/emoji creation and GIF-to-sticker authoring from the sticker-set screen; exclude `sticker_creator_box.cpp` from CMake | Narrow incoming dependency; viewing incoming stickers and custom emoji remains | Compile/link, sticker-set viewing, incoming static/animated/video sticker and custom-emoji rendering |
| 2 | Disable local Swift translation (`DESKTOP_APP_DISABLE_SWIFT6`) | Existing supported switch, removes a macOS 27 Swift object from a macOS 12 link | Build/link, ordinary message text, language settings and translations unaffected except local on-device translation |
| 3 | Remove chat-history export UI and `td_export` target, preserving export-settings storage schema and the separate rich-message HTML export | Large, mostly isolated feature with no incoming-message handling | Compile/link; synthetic storage round-trip; account logout/quit; auto-download slider values; chat/photo/file QA |
| 4 | Replace sticker messages and animated custom emoji with text-only presentation, including captions, replies, previews and reactions | User-approved display simplification enables later decoder removal | UTF-16 entity-offset tests; selection/copy/accessibility; cold/warm cache; no frame generator or payload download for placeholders |
| 5 | Remove sticker/player sources only after every caller is replaced | Actually reduces linked code rather than just hiding controls | Reference scan, build/link and incoming-content GUI regression |
| 6 | Investigate call, wallet, stories, Instant View and editor cuts independently | Larger size potential, but each has incoming update or shared rendering dependencies | Dependency map, explicit fallback, then one feature per build and GUI regression |

Linker symbol spans suggest possible savings from calls/WebRTC (~13.6 MB), wallet (~11.8 MB), Instant View (~4.6 MB), stories (~3.6 MB) and export (~2.4 MB). These spans overlap and are **not** measured savings. The packaged executable after each cut is the measure.

Do not remove protocol parsing, incoming sticker/custom-emoji/reaction **identities** or shared media decoders just because the corresponding picker or player button is hidden. A text placeholder must be visible when a sticker is the entire message; returning `nullptr` would leave an empty bubble. Custom-emoji replacement must adjust UTF-16 offsets of later entities and preserve links, bold and spoilers. Lottie and FFmpeg have other consumers, so deleting their sticker callers alone does not permit deleting those libraries. View-once and protected files must retain safe viewing behavior. Global `-fno-rtti` is a separate experiment: live `dynamic_cast` and `typeid` users make it unsafe as a quick flag change. `-Os` is also only an experiment until startup and scrolling are benchmarked.

## Acceptance

1. Record source state and baseline package size before each cut. Build arm64 Release, then require a second no-op build and `git diff --check`.
2. Package with the existing script; verify ZIP checksum, ad-hoc signature, architecture and a separate data directory. Never delete the previous working package until the new one has passed login/restart checks.
3. Manually verify login survives restart; send/receive text and photos with captions; unread and notification routing; ordinary and protected files; incoming custom emoji and reactions; account switching, logout and quit.
4. Benchmark startup, idle RSS/CPU/threads/disk and sleep recovery at 1, 5, 10, 20 and 30 authorized accounts before claiming runtime or memory savings.
5. Keep user Data outside code-size experiments. Twelve pre-v11 test packages with empty Data were removed after the user confirmed AccountBank was closed; v11 Data was preserved. Future packages should use a stable workspace-local Data path. Migrate only after the app is closed, retain v11 as rollback until v12 login/restart is accepted, then remove v11.
