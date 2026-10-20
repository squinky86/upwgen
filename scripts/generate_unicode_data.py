#!/usr/bin/env python3
"""Rebuild the shared C++/browser tables from the vendored Unicode 16 UCD.

No network access or Python unicodedata module is used. --check verifies both
the input SHA-256 manifest and byte-for-byte reproducibility of every output.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
VERSION = "16.0.0"
SOURCE = ROOT / "data" / "unicode" / VERSION

CATEGORY_DEFINITIONS = [
    ("symbols", "s", "ASCII punctuation", [
        (0x21, 0x2F), (0x3A, 0x40), (0x5B, 0x60), (0x7B, 0x7E)]),
    ("upper", "c", "ASCII uppercase letters", [(0x41, 0x5A)]),
    ("lower", "l", "ASCII lowercase letters", [(0x61, 0x7A)]),
    ("digits", "d", "ASCII digits", [(0x30, 0x39)]),
    ("tags", "n", "Invisible tag characters", [(0xE0020, 0xE007E)]),
    ("combining", "m", "Combining diacritical marks", [(0x300, 0x36F)]),
    ("emoji", "e", "Emoticons", [(0x1F600, 0x1F64F)]),
    ("historic", "x", "Historic scripts", [
        (0x10330, 0x1034A), (0x10380, 0x1039D), (0x10B00, 0x10B35),
        (0x10B40, 0x10B55), (0x10B60, 0x10B72), (0x10B80, 0x10B91),
        (0x12000, 0x120FF), (0x13000, 0x130FF),
        (0x10570, 0x1057A), (0x1057C, 0x1058A), (0x1058C, 0x10592),
        (0x10594, 0x10595), (0x10597, 0x105A1), (0x105A3, 0x105B1),
        (0x105B3, 0x105B9), (0x105BB, 0x105BC),
        (0x12F90, 0x12FF2), (0x16A0, 0x16FF), (0x1680, 0x169F),
        (0x10000, 0x1007F)]),
    ("braille", "o", "Braille patterns", [(0x2800, 0x28FF)]),
    ("games", "g", "Game symbols", [
        (0x2654, 0x265F), (0x1FA00, 0x1FA53), (0x1F000, 0x1F02B),
        (0x1F030, 0x1F093), (0x1F0A0, 0x1F0AE), (0x1F0B1, 0x1F0BF),
        (0x1F0C1, 0x1F0CF), (0x1F0D1, 0x1F0F5), (0x1FA60, 0x1FA6D)]),
]


def verify_sources() -> None:
    """Reject damaged or unreviewed Unicode input, including during regeneration."""
    for line in (SOURCE / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        expected, filename = line.split()
        actual = hashlib.sha256((SOURCE / filename).read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError(f"Unicode source checksum mismatch: {filename}")


def property_rows(filename: str):
    for line in (SOURCE / filename).read_text(encoding="utf-8").splitlines():
        content = line.split("#", 1)[0].strip()
        if not content:
            continue
        fields = [part.strip() for part in content.split(";")]
        bounds = fields[0].split("..")
        yield int(bounds[0], 16), int(bounds[-1], 16), fields[1:]


def property_set(filename: str, name: str, values: set[str] | None = None):
    return {
        cp
        for first, last, properties in property_rows(filename)
        if properties[0] == name and (values is None or properties[1] in values)
        for cp in range(first, last + 1)
    }


def compact_ranges(values: set[int]):
    result = []
    for value in sorted(values):
        if result and value == result[-1][1] + 1:
            result[-1][1] = value
        else:
            result.append([value, value])
    return result


def name_prefix(name: str) -> str:
    if name.startswith("<CJK Ideograph"):
        return "CJK UNIFIED IDEOGRAPH"
    if name.startswith("<Hangul Syllable"):
        return "HANGUL SYLLABLE"
    if name.startswith("<Tangut Ideograph"):
        return "TANGUT IDEOGRAPH"
    if "Surrogate" in name:
        return "SURROGATE"
    if "Private Use" in name:
        return "PRIVATE USE"
    raise ValueError(f"Unhandled algorithmic Unicode range: {name}")


def load_data():
    aliases = {}
    for line in (SOURCE / "NameAliases.txt").read_text(encoding="utf-8").splitlines():
        content = line.split("#", 1)[0].strip()
        if content:
            cp, alias, kind = content.split(";")
            if kind == "control":
                aliases.setdefault(int(cp, 16), alias)

    characters = []
    names = {}
    name_ranges = []
    pending_range = None
    for line in (SOURCE / "UnicodeData.txt").read_text(encoding="utf-8").splitlines():
        fields = line.split(";")
        cp = int(fields[0], 16)
        name, category, ccc, bidi = fields[1:5]
        if name.endswith(", First>"):
            pending_range = (cp, name, category)
            continue
        if name.endswith(", Last>"):
            if pending_range is None or pending_range[1][:-8] != name[:-7]:
                raise ValueError(f"Malformed UnicodeData range ending at {cp:X}")
            name_ranges.append([pending_range[0], cp, name_prefix(name), category])
            pending_range = None
            continue
        if name == "<control>":
            name = aliases.get(cp, fields[10] or "CONTROL")
        record = [cp, name, category, int(ccc), bidi]
        names[cp] = record
        characters.append(record)
    if pending_range is not None:
        raise ValueError("Unterminated UnicodeData range")

    ignorables = property_set("DerivedCoreProperties.txt", "Default_Ignorable_Code_Point")
    whitespace = property_set("PropList.txt", "White_Space")
    nfc_non_yes = property_set("DerivedNormalizationProps.txt", "NFC_QC", {"N", "M"})

    def is_visible(cp):
        record = names[cp]
        return (
            not record[2].startswith(("M", "C", "Z"))
            and record[3] == 0
            and record[4] not in {"R", "AL", "AN"}
            and cp not in ignorables
            and cp not in whitespace
            and cp not in nfc_non_yes
            and cp != 0x2800
        )

    categories = []
    for identifier, flag, description, ranges in CATEGORY_DEFINITIONS:
        candidates = {cp for first, last in ranges for cp in range(first, last + 1)}
        assigned = sorted(cp for cp in candidates if cp in names and names[cp][2] != "Cn")
        categories.append({
            "id": identifier,
            "flag": flag,
            "description": description,
            "characters": assigned,
            "visible": [cp for cp in assigned if is_visible(cp)],
        })
    scripts = sorted(
        [first, last, properties[0]]
        for first, last, properties in property_rows("Scripts.txt")
    )
    return {
        "unicodeVersion": VERSION,
        "categories": categories,
        "characters": characters,
        "nameRanges": name_ranges,
        "scripts": scripts,
        "defaultIgnorable": compact_ranges(ignorables),
    }


def cpp_literal(codepoints):
    def escaped(cp):
        if cp == ord('"'):
            return r"\""
        if cp == ord("\\"):
            return r"\\"
        if 0x20 <= cp <= 0x7E:
            return chr(cp)
        if cp <= 0xFFFF:
            return f"\\u{cp:04X}"
        return f"\\U{cp:08X}"
    groups = [codepoints[i:i + 12] for i in range(0, len(codepoints), 12)] or [[]]
    return "\n".join('    U"' + "".join(escaped(cp) for cp in group) + '"' for group in groups)


def render_header(data):
    lines = [
        "// Generated by scripts/generate_unicode_data.py; do not edit.",
        "// Unicode data copyright Unicode, Inc.; see data/unicode/16.0.0/LICENSE.txt.",
        "#pragma once",
        "",
        "#include <string_view>",
        "",
        "namespace upwgen::unicode_data {",
        f'inline constexpr std::string_view unicode_version = "{VERSION}";',
        "",
        "struct Category {",
        "    std::string_view id;",
        "    char legacy_flag;",
        "    std::string_view description;",
        "    std::u32string_view characters;",
        "    std::u32string_view visible;",
        "};",
        "",
    ]
    for category in data["categories"]:
        for kind in ["characters", "visible"]:
            lines.append(f'inline constexpr std::u32string_view {category["id"]}_{kind} =')
            lines.append(cpp_literal(category[kind]) + ";")
        lines.append("")
    lines.append("inline constexpr Category categories[] = {")
    for category in data["categories"]:
        identifier = category["id"]
        lines.append(
            f'    {{"{identifier}", \'{category["flag"]}\', "{category["description"]}", '
            f"{identifier}_characters, {identifier}_visible}},"
        )
    lines.extend(["};", "} // namespace upwgen::unicode_data", ""])
    return "\n".join(lines).encode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify without changing files")
    args = parser.parse_args()
    verify_sources()
    data = load_data()
    outputs = {
        ROOT / "include/upwgen/categories_generated.hpp": render_header(data),
        ROOT / "docs/assets/unicode-data.json": (
            json.dumps(data, ensure_ascii=True, separators=(",", ":")) + "\n"
        ).encode("utf-8"),
        ROOT / "docs/assets/unicode-LICENSE.txt": (SOURCE / "LICENSE.txt").read_bytes(),
    }
    stale = []
    for path, content in outputs.items():
        if args.check:
            if not path.is_file() or path.read_bytes() != content:
                stale.append(str(path.relative_to(ROOT)))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            print(f"Generated {path.relative_to(ROOT)} ({len(content):,} bytes)")
    if stale:
        print("Unicode outputs need regeneration: " + ", ".join(stale), file=sys.stderr)
        return 1
    if args.check:
        print(f"Unicode {VERSION} sources and generated outputs verified.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"Unicode generation failed: {error}", file=sys.stderr)
        sys.exit(1)
