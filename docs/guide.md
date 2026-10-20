---
title: User guide
permalink: /guide/
---

[Open the generator and analyzer]({{ '/' | relative_url }}).

## Generate a password

Choose a profile, enter a length and quantity, then select **Generate**. Copy the result into your password manager with its copy button. Copy feedback appears only after the browser confirms success. If clipboard access is unavailable, select and copy the displayed value manually.

Length means **Unicode code points**, not bytes or visible symbols. The generator produces the exact requested count or explains why the selected constraints cannot be satisfied. A maximum UTF-8 byte count is useful when a destination limits encoded bytes.

| Profile | Contents | Use |
| --- | --- | --- |
| Portable | ASCII uppercase and lowercase letters, digits, and symbols | Default for ordinary credentials; check the destination's symbol policy |
| Unicode | Assigned, visible, NFC-stable characters from the catalog | Systems whose full Unicode password workflow you have tested |
| Unspeakable | Historic scripts, game symbols, Braille, emoji, combining marks, tag characters, and ASCII | Authentication testing, demonstrations, and explicitly compatible destinations |

**The website requires ASCII uppercase, lowercase, digits, and symbols by default in every profile.** Unspeakable additionally requires at least one character from each of its six Unicode categories: tags, combining marks, emoji, historic scripts, Braille, and game symbols. Every Unspeakable result therefore includes all ten categories by default.

Allowed sets contribute to the alphabet; required sets must appear in every result. Choosing another profile restores that profile's default sets. For custom test data, turn off **Require STIG length & character rules**, then adjust Allow and Require. Turning the option off keeps the current requirements until you change them. Editing settings clears previous results so they cannot be mistaken for results of the new policy. Strict byte limits can still exceed the sampler's work limit; generation fails rather than returning a password that misses a requirement.

The default is **24 code points, one password, Portable**. Longer independent random strings increase the space an attacker must search. Exotic glyphs alone are not a measure of security.

## STIG length & character rules

The website enables this option by default in every profile. It enforces a minimum of **15 Unicode code points** and at least one of each ASCII character class: **A–Z, a–z, 0–9, and a symbol**. These four sets stay allowed and required while the option is enabled. Enabling the option raises a shorter length to 15; entering a shorter length afterward produces an error. An incompatible byte limit also produces an error.

These checks implement the following password length and composition rules from the [DISA Application Security and Development STIG, V6R5](https://dl.dod.cyber.mil/wp-content/uploads/stigs/zip/U_ASD_V6R5_STIG.zip), verified October 20, 2026:

| Rule | Requirement |
| --- | --- |
| V-222536 / APSC-DV-001680 | Minimum 15 characters |
| V-222537 / APSC-DV-001690 | At least one uppercase character |
| V-222538 / APSC-DV-001700 | At least one lowercase character |
| V-222539 / APSC-DV-001710 | At least one numeric character |
| V-222540 / APSC-DV-001720 | At least one special character |

This is a password composition option, not certification of a system's STIG compliance. The receiving system must enforce its applicable account, password history, lifetime, and authentication policies. Product-specific STIGs can impose other requirements. Unicode destinations may reject, normalize, or count characters differently; choose Portable for the broadest compatibility.

The native **0.1.0 CLI** keeps its explicit requirement behavior. To request this length and composition baseline there, use `upwgen --profile portable --length 24 --require upper,lower,digits,symbols`.

## Inspect Unicode

Paste text into the analyzer. It reports the Unicode name, code point, category, script, encoding length, and properties of each character, including invisible and combining characters. The totals distinguish:

- **Code points:** the Unicode values in the text.
- **UTF-8 bytes:** the size of the UTF-8 encoding.
- **UTF-16 units:** the units counted by JavaScript's `string.length`.
- **Graphemes:** user-perceived characters, when the browser supports `Intl.Segmenter`.

Emoji sequences and combining marks can contain several code points but display as one grapheme. Missing font glyphs may appear as boxes even when their code points and names are valid.

Normalization diagnostics compare the pasted text with NFC, NFD, NFKC, and NFKD. They explain transformations; they do not silently rewrite the input.

## Privacy and compatibility

Generation and analysis run on your device. The site fetches its own program and Unicode data; it does not upload your input, save it in browser storage, or use analytics. GitHub can still receive ordinary page and asset requests. The clipboard, browser extensions, password manager, and operating system are outside the application's control.

Try a Unicode password through registration, login, reset, and password-manager autofill before relying on it. Invisible characters can be dropped by a destination, and normalization can change the representation. The portable profile avoids those Unicode transformations, although each service can still impose its own policy.

See the [CLI reference]({{ '/cli/' | relative_url }}) for scripting and reproducible test data, and [Unicode & privacy]({{ '/unicode/' | relative_url }}) for the detailed model.
