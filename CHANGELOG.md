# Changelog

## Unreleased

- Require all ten character categories by default in the website's Unspeakable profile.
- Enable DISA ASD STIG length and composition rules by default in every web profile: at least 15 code points and ASCII uppercase, lowercase, digits, and symbols.
- Show the enforced rules beside the generator, preserve an explicit custom mode, and clear old results when settings change.
- Sample short passwords uniformly with exact completion counts so required categories remain practical at the minimum length.

## 0.1.0

- Add a browser-local password generator and Unicode analyzer on GitHub Pages.
- Add dark, responsive documentation with generator and analyzer on the front page.
- Replace implicit category requirements with portable, Unicode, and unspeakable profiles and explicit requirements.
- Use operating-system cryptographic randomness and unbiased bounded selection.
- Validate numeric arguments, honor exact code-point lengths, and support UTF-8 byte budgets.
- Add raw, numbered, escaped, and JSONL output and a deterministic, test-only corpus mode.
- Pin shared native/browser Unicode catalog and analyzer data to Unicode 16.0.0.
- Introduce an internal generator core, checked secret buffers, streaming output, and meaningful tests.
- Add CMake/CTest, cross-platform CI, and tagged-release Windows x64 and Ubuntu 26.04 amd64 packages.
- Change defaults to Portable, 24 code points, and raw output. Preserve legacy exclusion flags and positional arguments.

## 0.0.1

- Initial native Unicode password generator with Windows support.
