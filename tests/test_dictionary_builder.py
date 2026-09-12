import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'builder'))
import dictionary_builder as b
class BuilderTests(unittest.TestCase):
 def test_dictcc_import(self):
  self.assertEqual(b.parse_export('\ufeff# synthetic\r\n====\r\n\r\nNew York\tNueva York\t[noun]\r\ncafé au lait\tMilchkaffee\r\n'),[('New York','Nueva York'),('café au lait','Milchkaffee')])
 def test_box_labels_and_unicode_separators(self):
  self.assertEqual(b.parse_export('Box 1\n[BOX 2]\n=== Box 3 ===\nfirst\u2028field\ttranslation\n'),[('first\u2028field','translation')])
 def test_limits(self):
  with self.assertRaises(ValueError):b.parse_export('a'*191+'\tb')
  with self.assertRaises(ValueError):b.parse_export('word without tab')
  with self.assertRaises(ValueError):b.build_dict(dict(name='Test',front='en',back='en',entries=[('a','b')]))
if __name__=='__main__':unittest.main()
