---
title: Unicode, randomness & privacy
permalink: /unicode/
---

## A versioned catalog

The native application and website share category tables generated from the **Unicode 16.0.0** Character Database. Names, assignment status, scripts, and relevant properties come from pinned upstream data. Unicode data carries its own license alongside the project license.

The historic-script category contains assigned characters from the original Gothic, Ugaritic, Avestan, Parthian, Pahlavi, Cuneiform, Egyptian Hieroglyph, Vithkuqi, Cypro-Minoan, Runic, Ogham, and Linear B ranges. The catalog is a deliberate selection, not a claim to contain every Unicode character.

The analyzer covers text beyond the generation catalog. Characters assigned after Unicode 16 can be displayed by a newer browser while remaining “unassigned” in this pinned database. Grapheme segmentation and JavaScript normalization use the browser's Unicode implementation; its version may differ from the catalog.

## Compatibility

The Unicode profile excludes unassigned characters, combining marks, default-ignorables, whitespace, Braille blank, right-to-left characters, and characters that change under NFC. The unspeakable profile retains assigned tag and combining characters intentionally. It is useful for checking how software preserves text.

Unicode normalization can reorder combining marks or turn an equivalent sequence into another representation. Two strings that look identical can therefore have different code points or bytes. See the [Unicode normalization FAQ](https://www.unicode.org/faq/normalization.html).

NIST recommends that password verifiers accept Unicode and apply NFC normalization, while warning that endpoint differences can affect successful authentication. Its guidance favors password-manager support and does not require periodic changes without evidence of compromise. These are verifier requirements, not a certification of this generator. See [NIST SP 800-63B](https://pages.nist.gov/800-63-4/sp800-63b.html).

## Randomness and constraints

Native secret generation draws from the operating system: Linux `getrandom`, Apple/BSD `arc4random_buf`, or Windows `BCryptGenRandom`. Browser generation uses Web Crypto `crypto.getRandomValues`. Neither silently falls back to a predictable source.

Selection uses rejection sampling to avoid modulo bias. Whole-candidate rejection satisfies category and byte constraints without giving some valid strings extra weight. Constraints are bounded by a work limit so difficult policies cannot hang the application. No strength score is inferred from how unusual the output looks.

For independent uniform draws from an alphabet of size *A*, an unconstrained length-*L* string has *L × log₂(A)* bits of selection entropy. Requirements and encoding limits change that distribution's size; the application does not present the unconstrained formula as a guarantee for constrained outputs.

## Secret lifetime

The native utility generates and emits one result at a time and clears its owned secret buffers on destruction. This is a best-effort reduction in memory lifetime. Terminal scrollback, output files, pipes, operating-system buffers, and downstream programs can retain copies.

The website does not persist passwords or analyzer input. JavaScript strings are managed by the browser and cannot be reliably wiped. Clipboard content can be read or retained by other software; the site does not promise automatic erasure of the clipboard.

No input is uploaded for generation or analysis. All scripts and Unicode assets are served from this GitHub Pages site, with no third-party JavaScript or analytics.
