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

Each selected category contributes to the allowed alphabet. Selecting a category does **not** require its presence in every result. Mark a category as required when a destination has a composition rule. Requiring many small categories in a short password can be expensive; the generator stops with an explanation when a policy cannot be sampled within its work limit.

The default is **24 code points, one password, Portable**. Longer independent random strings increase the space an attacker must search. Exotic glyphs alone are not a measure of security.

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
