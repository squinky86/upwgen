# upwgen

The Unspeakable Password Generator: cryptographic passwords, unusual Unicode, and tools to understand what you are copying.

**[Open the generator & Unicode analyzer](https://squinky86.github.io/upwgen/)** · [Documentation](https://squinky86.github.io/upwgen/guide/) · [Download 0.1.0](https://github.com/squinky86/upwgen/releases/tag/0.1.0)

The website generates passwords and analyzes pasted text entirely in your browser. It includes per-character Unicode names, scripts, encoding lengths, normalization diagnostics, and clipboard controls. No passwords or analyzer input are uploaded or saved.

The website now requires **ASCII uppercase, lowercase, digits, and symbols**, with a **15-character minimum**, by default in every profile. Unspeakable also requires every Unicode category. Its default-on STIG option implements the DISA Application Security and Development password length and composition rules; see [the rule references and scope](https://squinky86.github.io/upwgen/guide/#stig-length--character-rules). Custom test policies remain available by turning that option off. Native 0.1.0 behavior is described below.

## Native quick start

```sh
upwgen
upwgen --profile portable --length 32 --count 5
upwgen --profile unicode --length 20 --max-bytes 64
upwgen --profile unspeakable --length 24 --format escaped
upwgen --include upper,lower,digits --require digits --length 24
upwgen corpus --seed 42 --count 100 --format jsonl
upwgen sets
```

Default: **Portable / 24 Unicode code points / one result / raw UTF-8 output**. Length is exact or the command fails. Corpus outputs are deterministic test data and must never be used as credentials.

| Profile | Intended use |
| --- | --- |
| `portable` | ASCII letters, digits, and symbols for ordinary credentials |
| `unicode` | Assigned, visible, NFC-stable characters for tested Unicode workflows |
| `unspeakable` (alias `stress`) | Assigned tag characters, combining marks, emoji, historic scripts, Braille, game symbols, and ASCII for compatibility testing |

Category selection controls the allowed alphabet. Use `--require` when a particular category must appear. `--include` replaces selected categories; `--exclude` removes them. `--max-bytes` imposes a UTF-8 byte budget.

Output formats are `raw`, `numbered`, `escaped`, and `jsonl`. Raw output contains one password per line. Diagnostics use stderr; errors return a nonzero status. See [the CLI reference](https://squinky86.github.io/upwgen/cli/) or `upwgen --help` for all options and limits.

## Install and build

Tagged releases contain a Windows x64 ZIP with a static Microsoft C runtime and a `.deb` built for Ubuntu 26.04 amd64, plus SHA-256 checksums.

```sh
# Ubuntu 26.04, after downloading the package:
sudo apt install ./upwgen-0.1.0-linux-amd64.deb
```

Build from source with CMake and a C++17 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
```

Windows developer shell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The installed executable has no JavaScript or Python runtime dependency. JavaScript is used by the separate website, and Python maintains checked-in Unicode data.

## Security and Unicode

Secret generation uses the OS cryptographic source on native platforms and Web Crypto in the browser, with rejection sampling to avoid modulo bias. Entropy failure is fatal. `--seed` is accepted only in the explicitly deterministic `corpus` mode.

Native secret buffers are cleared as a best-effort measure. Terminal scrollback, redirected files, clipboard history, browser-managed strings, and other applications can retain copies.

Unicode code points, UTF-8 bytes, UTF-16 units, and displayed graphemes are different measures of length. Some destinations strip invisible characters, normalize text, or impose byte limits. Test the entire registration/login/reset/autofill workflow before relying on exotic characters. Visual obscurity is not a security guarantee. See [the Unicode and privacy model](https://squinky86.github.io/upwgen/unicode/).

The native and browser catalogs use pinned **Unicode 16.0.0** data. The analyzer's names and properties use that version; browser normalization and grapheme segmentation may use a newer Unicode version.

## Development

```sh
python3 scripts/generate_unicode_data.py --check
node --test tests/web*.mjs
cmake -S . -B build-sanitize -DUPWGEN_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

The Pages and release workflows also test the built Jekyll site in Chromium: generation, exact clipboard contents, permission-denied fallback, Unicode analysis, malicious markup as text, offline operation, and mobile layout. To run that check locally, build `docs/` into `_site/`, run `npm ci`, install Chromium with `npx playwright-core install chromium`, then run `npm run test:browser`. `UPWGEN_BROWSER_EXECUTABLE` can select an already installed Chrome/Chromium binary.

GitHub Actions tests native builds on Linux, macOS, and Windows and builds the Jekyll site from `docs/`. Release tags such as `0.1.0` trigger tested Windows x64 and Ubuntu 26.04 amd64 artifacts. The release is published only after its builds and artifact download checks succeed. Pull requests changing the release workflow run the same packaging checks without publishing. To validate packaging manually, run the **Release binaries** workflow with `workflow_dispatch` on `main`; branch runs build and test artifacts without creating a release or changing existing assets.

## Migration

Version 0.0.1 defaulted to 15 code points, every category required, and numbered output. Version 0.1.0 defaults to 24 code points, Portable, and raw output. Use `--profile unspeakable --format numbered` for the old broad alphabet and display style. Explicit `--require` selects mandatory categories.

Legacy flags `-scldnexgmo` and positional length/count remain accepted; combined flags now work. Malformed, negative, or overflowing numbers fail instead of silently becoming defaults.

[Changelog](CHANGELOG.md) · GPL-3.0-or-later; see [LICENSE](LICENSE). Unicode data is distributed under its upstream license.
