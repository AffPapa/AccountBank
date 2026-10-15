# AccountBank

AccountBank is an experimental, separate macOS fork of Telegram Desktop. It targets current Apple Silicon Macs. This repository does not contain a tested AccountBank app or a download.

## Current implementation

- The macOS app uses the AccountBank name, bundle identifier, URL scheme, and application data folder. Development runs must also pass a dedicated `-workdir` inside the project workspace.
- The source permits up to 30 account slots when adding accounts and reading saved account records. Real accounts, connection behavior, memory use, and sleep recovery have not been tested.
- Both chat composers hide the voice and round-video recording button and its guarded keyboard shortcuts. The underlying media code and incoming media handling remain present.
- The unchanged upstream source compiled as an arm64 Debug baseline. The AccountBank changes also compiled as arm64 Debug. Local dependency preparation used a Rust build without ThinLTO because the bundled Rust toolchain failed to load proc macros with ThinLTO on this Mac. The local icon conversion used Pillow because `iconutil` rejected the upstream iconset.
- The development build used Telegram Desktop's published test API credentials, which are unsuitable for users or release. Other removed-feature UI work has not started.

## Product boundary

- Keep 20–30 authorized accounts connected while the app runs and the Mac is awake. Show the selected account's chats and a compact status and unread summary for every account.
- Keep private chats, groups, channels, text, photos, read state, typing state, and notifications correct. Channel access must retain official sponsored messages.
- Remove voice recording, built-in audio and video playback, sticker and emoji pickers, calls, and Stories from the product UI. Preserve incoming message types, captions, text entities, and Unicode emoji in messages.
- Exclude account automation and bulk actions from the first version.

## First implementation gates

1. Build the pinned, unmodified upstream source for arm64 macOS and record the exact source revision and build inputs.
2. Measure the stock client's startup, idle CPU, memory, energy, disk use, and sleep/wake recovery with authorized accounts. Start with one account, then test 5, 10, 20, and 30. Record the real number tested.
3. Change the account limit only after mapping all related UI, storage, and lifecycle checks. Verify that every account remains connected and that notification and send actions route to the right account.
4. Remove visible features one at a time and compare each result with the stock baseline. Keep protocol and storage compatibility even for content the UI cannot play.
5. Release only with a separate app identity, own Telegram API credentials, license notices, signing, and real account acceptance. Never copy sessions or secrets from another client.

The project follows Telegram Desktop's GPLv3 license. The name AccountBank is a working repository name, not a claim of Telegram affiliation.
