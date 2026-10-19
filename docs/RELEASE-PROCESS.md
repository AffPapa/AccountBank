# NeTelegram release and website update

Use this checklist for each program release. The approved marketing copy lives on the NeTelegram landing page. Keep that copy stable between releases.

## Release sequence

1. Build the macOS Apple Silicon app and create a versioned GitHub Release in `AffPapa/AccountBank`.
2. Before linking it publicly, verify the uploaded ZIP contains the complete `AccountBank.app` bundle at its root. Verify the Developer ID signature and Apple notarization on the extracted app, and calculate SHA-256 from the exact uploaded ZIP.
3. Confirm the GitHub Release asset filename, size, checksum, tag, and direct download URL. Do not copy a URL from an older release.
4. Update the download button URL on every NeTelegram landing page to the new release asset. Leave the landing page's approved marketing text, layout, SEO copy, and other sections unchanged. Change release-specific labels or checksums only when they are part of the download control and would otherwise be incorrect.
5. Publish the website update through the existing Sites project. Keep the same public site URL and access setting.
6. Update the GitHub repository README with a concise version of the landing page's approved product description and feature list. Keep a prominent link to `https://netelegram-mac.iryadom.chatgpt.site/` and point the download link to the latest GitHub Release.
7. Check the live landing page, README links, release asset URL, and checksum after publication.

## Current release note

The beta 2 asset currently attached to GitHub Release is named `AccountBank.app.zip`. An audit on 2026-10-19 found that the ZIP extracts to a top-level `Contents/` directory rather than `AccountBank.app/Contents/`; its code signature reports `adhoc` and has no Team ID. Rebuild, sign, notarize, and replace this asset before treating beta 2 as a ready-to-install public download. The landing page button should only be switched to a verified release asset.

## Safety

- Never add Telegram API credentials, Apple passwords, signing keys, session files, or chat archives to source, release notes, logs, or the ZIP.
- Keep the Telegram API hash out of the public source repository. It may be embedded in a distributed client binary as required for a user-ready build; do not treat the binary as a place to keep a secret.
- Do not import Telegram sessions or data from another client.
- Keep release claims tied to the exact tagged source and uploaded artifact. A successful compile or synthetic smoke check alone does not prove real-account behavior.
