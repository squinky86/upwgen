"""Tests for the pinned Unicode profile contract and browser lookup data."""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_unicode_data", ROOT / "scripts/generate_unicode_data.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class UnicodeDataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        MODULE.verify_sources()
        cls.data = MODULE.load_data()
        cls.categories = {entry["id"]: entry for entry in cls.data["categories"]}
        cls.characters = {entry[0]: entry for entry in cls.data["characters"]}

    def test_legacy_registry_and_ascii_alphabet(self):
        self.assertEqual(len(self.categories), 10)
        ascii_pool = {
            cp for name in ["symbols", "upper", "lower", "digits"]
            for cp in self.categories[name]["characters"]
        }
        self.assertEqual(ascii_pool, set(range(0x21, 0x7F)))
        self.assertEqual(len({entry["flag"] for entry in self.categories.values()}), 10)

    def test_unassigned_historic_values_are_excluded(self):
        self.assertNotIn(0x1034B, self.categories["historic"]["characters"])
        self.assertNotIn(0x1039E, self.categories["historic"]["characters"])
        self.assertNotIn(0x16FB, self.categories["historic"]["characters"])
        self.assertIn(0x10330, self.categories["historic"]["characters"])
        self.assertIn(0x10570, self.categories["historic"]["characters"])

    def test_visible_profiles_exclude_problematic_values(self):
        self.assertEqual(self.categories["tags"]["visible"], [])
        self.assertEqual(self.categories["combining"]["visible"], [])
        self.assertNotIn(0x1680, self.categories["historic"]["visible"])
        self.assertNotIn(0x2800, self.categories["braille"]["visible"])
        self.assertNotIn(0x10B00, self.categories["historic"]["visible"])
        nfc_non_yes = MODULE.property_set(
            "DerivedNormalizationProps.txt", "NFC_QC", {"N", "M"}
        )
        ignorables = MODULE.property_set(
            "DerivedCoreProperties.txt", "Default_Ignorable_Code_Point"
        )
        for entry in self.categories.values():
            with self.subTest(category=entry["id"]):
                self.assertEqual(entry["characters"], sorted(set(entry["characters"])))
                self.assertTrue(set(entry["visible"]).issubset(entry["characters"]))
                for cp in entry["visible"]:
                    record = self.characters[cp]
                    self.assertEqual(record[3], 0)
                    self.assertNotIn(record[4], {"R", "AL", "AN"})
                    self.assertNotIn(record[2][0], {"M", "C", "Z"})
                    self.assertNotIn(cp, nfc_non_yes)
                    self.assertNotIn(cp, ignorables)

    def test_names_cover_controls_supplementary_and_new_assignments(self):
        self.assertEqual(self.characters[0x0A][1], "LINE FEED")
        self.assertEqual(self.characters[0x1F600][1], "GRINNING FACE")
        self.assertEqual(self.characters[0xE0020][1], "TAG SPACE")
        self.assertEqual(self.characters[0x1FAE9][1], "FACE WITH BAGS UNDER EYES")
        prefixes = {entry[2] for entry in self.data["nameRanges"]}
        self.assertEqual(prefixes, {
            "CJK UNIFIED IDEOGRAPH", "HANGUL SYLLABLE", "TANGUT IDEOGRAPH",
            "PRIVATE USE", "SURROGATE",
        })

    def test_ranges_are_sorted_and_disjoint_for_binary_lookup(self):
        for field in ["scripts", "nameRanges", "defaultIgnorable"]:
            with self.subTest(field=field):
                ranges = self.data[field]
                self.assertEqual(ranges, sorted(ranges))
                for previous, current in zip(ranges, ranges[1:]):
                    self.assertLess(previous[1], current[0])


if __name__ == "__main__":
    unittest.main()
