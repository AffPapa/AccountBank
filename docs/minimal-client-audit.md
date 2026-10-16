# AccountBank minimal client audit

This is a macOS Apple Silicon fork of Telegram Desktop for keeping many authorized accounts online while reading and sending text and photos. The user confirmed that the previous test build opened and logged into one real account. That does not establish 20–30 account reliability or performance.

## What stays

| Component | Reason |
| --- | --- |
MTProto session, update handling, read state, notifications, sleep recovery | Each account must continue receiving messages. |
Incoming message parsing, captions, entities, reaction data and custom emoji assets | Hiding a picker must not corrupt message display. |
Photo handling and manual document downloads | Core chat use and access to incoming files. |
Audio decoding support in the core | Notification sounds and protected or expiring content may depend on it. |
Sponsored-message handling in channels | Preserve the upstream protocol and reporting behavior. |
Account security, active sessions, proxy and cache controls | Essential for a 30-account bank. |

## Changes in the current slice

- The main menu and Settings home expose accounts, profile, Saved Messages, theme, notifications, privacy, network/cache, language, scale and the send key. Settings search omits removed sections. The support-mode menu no longer exposes chat-order and template controls.
- Ordinary and secondary chat composers hide the emoji/sticker/GIF picker and custom-emoji suggestions. Voice and round-video recording controls were already hidden.
- Documents, voice, music, video and GIF files do not download or autoplay passively. Ordinary audio/video clicks save manually and offer macOS Open With after a file exists. Photos keep normal handling. Sticker files still load because they also carry incoming custom emoji and reaction art; the chat renders ordinary animated stickers as a frozen frame while dice and slot messages retain the frames needed to show their results. Premium sticker effects are removed from this chat renderer.
- Chat call buttons and optional promotional actions are removed from visible menus. Incoming call handling remains in the core; the Notifications setting to accept calls on this device remains available.
- New accounts use a 128 MiB cache limit for each of the two cache databases, instead of the upstream 1 GiB default. Existing stored limits remain as the user set them.
- The upstream updater is blocked by a CMake guard. The macOS restart path now launches the same executable after the old process exits, instead of calling the absent Updater; the graphical restart still needs testing.
- A failed read of an existing account key stops startup instead of replacing it with an empty profile. A mapless account is accepted when its encrypted authorization parses completely, whether its user ID is zero (unfinished login) or nonzero (interrupted map write after login). Unfinished blank slots are skipped when another account is available; an all-blank list retains one login screen. A map with a saved user and a blank authorization is rejected rather than silently dropping that account. The account index list and optional active index are parsed completely before any account is added; a stored active index outside the list falls back to a retained account. A duplicate or incomplete session list stops startup without rewriting the domain key. Per-account preparation can still migrate its own files before such a failure, so byte-for-byte preservation of every account file is not claimed.
- The macOS app icon is original AccountBank artwork. API credentials can be supplied through an ignored local CMake file and are injected from a generated build header, avoiding compiler command-line disclosure.
- `Настроить API.command` collects those credentials in hidden macOS dialogs, validates their shape and creates the ignored local file with mode `0600`. The user ran it, and the local file's presence, permissions and generated build-header match were checked without printing values.
- The main menu no longer starts a seasonal snow animation, and its version link goes to the AccountBank repository. The macOS Edit menu keeps ordinary clipboard actions while omitting the optional formatting submenu.
- The login title, window titles, notification previews, crash dialog and visible English fallback strings identify AccountBank as an unofficial client. Telegram service names in protocol and legal text remain accurate.
- App-specific English fallback strings for startup, passcode unlock, language restart and cache now name AccountBank. Telegram service and official mobile-app references remain unchanged. Cloud language packs may override local fallback strings.
- The Settings home no longer refreshes Premium and cloud themes on every visit, and the profile photo control no longer allows choosing a video avatar.
- A macOS `libproc` sampler in `tools/accountbank-benchmark.c` can record aggregate process memory, CPU, thread count and disk I/O for a tester-provided PID. It does not inspect profiles or prove the tester-provided account count.
- Source inspection confirms the account list sits in a scrollable main menu and each live session supplies its own unread badge; the collapsed list shows an aggregate badge for other accounts. Whether this remains usable with 30 real accounts needs GUI testing.
- The local passcode screen no longer offers a Log Out action that could reset the whole account bank. Visible call and Star Gift send controls were removed from the main chat, composer, profile and context menus. Incoming call and gift data paths remain in the protocol; some deep links and birthday suggestions can still reach gift flows.
- Manual Open With and Save actions now check the message's protected-content policy before writing files. Generic protected files may show a blocked-action toast; the label and localized text need a GUI polish pass.

## What UI removal does not prove

- Every saved account still constructs its own Telegram session, MTProto connection and managers. Baseline RAM and network load are roughly linear in the number of online accounts. Hiding settings does not make the core lightweight.
- View-once and protected media retain the upstream viewer path to avoid saving restricted content to disk. Round video streaming remains available in the view-once viewer so its image is visible before the message expires. Some deep links and specialized viewers may still expose upstream features.
- The embedded multimedia, calls and web libraries are still linked because they share code with incoming rendering, notifications and other core behavior. Further removal needs dependency mapping and regression tests.
- The current arm64 Release source compiled successfully and was packaged in `Builds.nosync/AccountBank-Minimal-Release-v10-macOS-arm64-Test` with a separate empty Data folder. The app is ad-hoc signed in a temporary directory and stored in a ZIP because the Desktop file provider adds a FinderInfo attribute that prevents bundle verification there. The launcher verified the ZIP checksum, extracted to a user-owned `/private/tmp` cache keyed by archive hash and verified the bundle signature. The v10 ZIP is 94,880,530 bytes, 3,437 bytes larger than v9. Linker dead stripping had reduced the stripped v9 executable from 281,033,296 to 264,657,592 bytes, but introduces a dynamic-registration risk until GUI testing. v10 Release GUI startup, login, restart and 5/10/20/30-account performance remain unverified; this is not a notarized public release.
- The later v11 Release source compiled successfully, and a second build reported `ninja: no work to do`. Its arm64 test package is in `Builds.nosync/AccountBank-Minimal-Release-v11-macOS-arm64-Test`, separate from v10, with an empty Data folder. The launcher verified the ZIP checksum and ad-hoc app signature. The v11 ZIP is 94,741,193 bytes. v11 GUI startup, login, restart, protected-media behavior and 5/10/20/30-account performance remain unverified; this is not a notarized public release.
- A minimal AppKit `.app` fails at `[NSApplication sharedApplication]` in this agent shell, while the same program as a standalone executable succeeds. The AccountBank headless abort at that call is therefore not evidence of an AccountBank-specific GUI defect. No desktop UI automation was used.
- Release Qt and the required static libraries were built in separate directories, preserving the original Debug dependencies. One Swift translation object generated a macOS 27 deployment warning, so compatibility with older macOS versions is not established by this build.
- The emoji/sticker selector and voice-recording UI are hidden but still constructed in some composers; shared call, WebView, wallet and media libraries remain linked. Further deletion requires reachable-path and GUI tests. The 128 MiB cache setting limits disk use per cache database, not memory per account.

## Acceptance gate before calling it lightweight

1. Done: run a v11 Release build with the project's own Telegram API credentials, package an arm64 test app, verify ZIP integrity and the ad-hoc bundle signature. Never commit or print the credentials.
2. Test 1, 5, 10, 20 and 30 accounts. Record RSS, idle CPU, process thread count, cache size and wake recovery; do not record account identifiers or message content.
3. At each count, verify receiving and sending from every account, correct unread and notification routing, manual file download, photos and text entities.
4. Confirm a corrupted copied test key causes a clear startup failure and leaves all key copies byte-for-byte intact. Use synthetic test data, never a live account directory.
5. Inspect macOS UI manually: main/Settings/search/macOS menus, primary and secondary composers, normal and protected media, and account switching. Compare against the prior working build.

## Independent Claude review

Claude identified two regressions in the first cut: forcing all document downloads off broke custom emoji and reactions, and the primary chat composer still showed the emoji button. Both paths were corrected before the final build. Claude also identified the local key reset risk and the need to retain send-key, power-saving, active-session and call-acceptance controls. A fresh CLI review of the v7 launcher and storage changes did not return a result, so those changes have only the independent source audits and checks listed above.

In a later independent review, Claude found that strict storage reads would reject a normal account left on the QR or phone login screen and lock out the other accounts on every restart. The mapless blank-account classifier and slot handling were corrected in source. Claude also found that disabling round-video playback hid the image in a view-once viewer; that viewer path was restored. Both fixes still require manual GUI regression checks. One damaged authorized slot still blocks the full profile; recovery needs a separate, safe design.

In the latest review, Claude found a protected-content Save/Open With bypass and a one-second window after login where authorization could be written before the account map. Both paths were corrected in source. Claude's follow-up found a null key on legacy map reads and an ineffective attempt to hide passcode-screen Log Out; those were corrected as well. Current source has not been tested with 20–30 real accounts. The next performance work is to measure startup and idle cost per account, then defer services that are proved unnecessary at launch. Removing shared protocol, decoder or sticker code before those measurements risks breaking incoming content.

## v12 update

The v12 Release build removes the chat-history export UI/manager, sticker-set authoring UI and local Swift translation dependency. Ordinary sticker messages now display a text placeholder; isolated animated emoji, inline custom emoji, reactions, dice, previews and background sticker loading remain. The old statement above about frozen ordinary sticker frames describes v11, not v12. ZIP size fell from 94,741,193 to 93,691,280 bytes. The build, no-op rebuild, archive checksum and signature checks passed. The GUI and 20–30-account behavior are still unverified. The v11 package is retained only as rollback until v12 login and restart have been checked; do not run both packages with their copied account profile at once.
