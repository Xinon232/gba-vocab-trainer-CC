import sys,unittest,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'builder'))
import app
class AppTests(unittest.TestCase):
 def test_standalone_verified_output(self):
  with tempfile.TemporaryDirectory() as tmp:
   path=Path(tmp)/'English-German.dict'
   spec=dict(name='English German',front='en',back='de',entries=[('word','Wort')])
   self.assertTrue(hasattr(app,'build_dictionary'),'standalone GUI backend missing')
   report=app.build_dictionary(spec,path)
   self.assertEqual(report['entries'],1)
   self.assertEqual(app.pack.read_dict(path.read_bytes())['entries'],spec['entries'])
   old=path.read_bytes()
   with self.assertRaises(ValueError):app.build_dictionary(dict(spec,entries=[]),path)
   self.assertEqual(path.read_bytes(),old)
if __name__=='__main__':unittest.main()
