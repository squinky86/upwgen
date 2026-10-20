---
title: Release notes
permalink: /releases/
---

## 0.1.0

[Download release assets](https://github.com/squinky86/upwgen/releases/tag/0.1.0) · [Source](https://github.com/squinky86/upwgen/tree/0.1.0)

- New browser generator and Unicode analyzer, with local processing and copy controls.
- Dark terminal-inspired, responsive documentation and tool interface on GitHub Pages.
- Portable, Unicode, and unspeakable profiles backed by pinned Unicode 16 data.
- Exact code-point length, checked arguments, explicit required categories, and UTF-8 byte budgets.
- Cryptographic OS randomness and unbiased selection for native secret generation.
- Raw, numbered, escaped, and JSONL output plus deterministic test-corpus generation.
- Internal testable core, bounded resource use, and best-effort secret-buffer cleanup.
- CMake/CTest, cross-platform CI, Windows x64 release binaries, and Ubuntu 26.04 amd64 Debian packages.

### Behavior changes

Defaults are now **Portable, 24 code points, raw output**. The prior broad alphabet is available as `--profile unspeakable`, and the old output style as `--format numbered`. Enabled categories no longer imply required characters. Short exclusion flags and positional length/count remain accepted.

## 0.0.1

The original native generator introduced selectable Unicode ranges and Windows console output. It remains available in the [release archive](https://github.com/squinky86/upwgen/releases/tag/0.0.1).
