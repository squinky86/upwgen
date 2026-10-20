---
title: Command-line reference
permalink: /cli/
---

Download the Windows x64 archive or Ubuntu 26.04 amd64 package from the [0.1.0 release](https://github.com/squinky86/upwgen/releases/tag/0.1.0). Each release includes SHA-256 checksums.

## Install

On Ubuntu 26.04:

```sh
sudo apt install ./upwgen-0.1.0-linux-amd64.deb
upwgen --version
```

On Windows, extract the ZIP and run `.\bin\upwgen.exe` from PowerShell or add its `bin` directory to `PATH`. A standalone `.exe` is also available. The release build uses a static Microsoft C runtime.

To build from source on Linux or macOS with CMake and a C++17 compiler:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
```

On Windows in a Visual Studio developer shell:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Generate

```sh
upwgen generate --profile portable --length 32 --count 5
upwgen --profile unicode --length 20 --max-bytes 64
upwgen --profile unspeakable --length 24 --format escaped
upwgen --include upper,lower,digits --require digits --length 24
```

Default invocation is equivalent to `generate --profile portable --length 24 --count 1 --format raw`.

| Option | Meaning |
| --- | --- |
| `--profile portable\|unicode\|unspeakable` | Choose the starting alphabet; `stress` is an alias for `unspeakable` |
| `--length N` | Exact Unicode code-point count |
| `--count N` | Number of results |
| `--include LIST` | Replace the profile's selected category list with a comma-separated list |
| `--exclude LIST` | Remove categories |
| `--require LIST` | Require at least one character from each listed, selected category |
| `--max-bytes N` | Limit each password's UTF-8 encoding |
| `--format raw\|numbered\|escaped\|jsonl` | Select the output format |
| `--help` | Usage and supported options |
| `--version` | Release and Unicode data versions |

Use `upwgen sets` to list categories. Category IDs are `symbols`, `upper`, `lower`, `digits`, `tags`, `combining`, `emoji`, `historic`, `braille`, and `games`. The `unicode` profile filters each category; `tags` and `combining` have no allowed characters in that profile.

Limits are 4,096 code points per result, 10,000 results, and 1,000,000 total requested code points. Requirements and byte limits can make a policy infeasible or too expensive to sample. Such requests fail instead of truncating or silently changing length.

## Output

`raw` emits one UTF-8 password per line, without labels. The newline is a record delimiter and is not part of the password. `numbered` is for interactive display. `escaped` exposes code points for inspection. `jsonl` emits a JSON object per result for scripts.

Errors go to standard error with a nonzero exit code. A streaming batch may already have emitted complete results when a later operation fails; consumers must check the exit status.

## Reproducible authentication test data

```sh
upwgen corpus --profile unspeakable --length 24 --count 100 --seed 42 --format jsonl
```

`corpus` uses a deterministic test generator and defaults to the unspeakable profile, JSONL output, and seed zero. Its outputs are **public test data, never credentials**. `generate` rejects `--seed` and always uses operating-system cryptographic randomness.

Also see the [authentication edge-case corpus]({{ '/assets/authentication-cases.jsonl' | relative_url }}) for canonical equivalents, invisible characters, whitespace, supplementary-plane text, and encoding boundaries.

## Migration from 0.0.1

Short exclusion flags and positional length/count arguments remain accepted. Flags can now be combined, for example `upwgen -s 24 5`. Their meanings remain `-s` symbols, `-c` uppercase, `-l` lowercase, `-d` digits, `-n` tags, `-e` emoji, `-x` historic scripts, `-g` games, `-m` combining marks, and `-o` Braille.

The default profile, length, and output changed: **Portable / 24 / raw** replaces the old all-categories / 15 / numbered behavior. Category presence is now optional unless requested. Use `--profile unspeakable --format numbered` to retain the broad alphabet and display style. Length is now exact, and malformed arguments fail clearly.
