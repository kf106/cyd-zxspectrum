# Web flasher (ESP Web Tools)

Published to **GitHub Pages** when a release is published or when the
`Deploy web flasher (GitHub Pages)` workflow is run manually.

## One-time repo setup (current GitHub UI)

You do **not** need **Verified domains** unless you want a custom domain. There is often **no “Source” branch** picker when using Actions-based Pages.

1. **Settings → Actions → General → Workflow permissions:** **Read and write permissions**.
2. **Actions** → **Deploy web flasher (GitHub Pages)** → **Run workflow** (or publish a GitHub Release).
3. After a green run, **Settings → Pages** should show: **Your site is live at** `https://kf106.github.io/cyd-zxspectrum/`

Public URL: https://kf106.github.io/cyd-zxspectrum/

## Local test

Build firmware, copy bins next to `index.html`, generate `manifest.json` from
`manifest.template.json`, and serve over HTTPS (Web Serial requires a secure context).
A simple approach is to run the GitHub Actions workflow on a test tag.
