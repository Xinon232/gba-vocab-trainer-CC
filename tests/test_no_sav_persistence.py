import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NoSavPersistenceTest(unittest.TestCase):
    def test_learning_state_does_not_use_sram_sav(self):
        main = (ROOT / "src" / "main.cpp").read_text()
        self.assertNotIn('"sav.h"', main)
        self.assertNotIn("sav_load(", main)
        self.assertNotIn("sav_save(", main)
        self.assertFalse((ROOT / "src" / "sav.cpp").exists())
        self.assertFalse((ROOT / "include" / "sav.h").exists())


if __name__ == "__main__":
    unittest.main()
