import sys, unittest, struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'builder'))
import dictionary_builder as b
class External(unittest.TestCase):
 def test_standalone(self):
  self.assertTrue(hasattr(b,'build_dict'),'standalone .dict producer missing')
  pairs=[(f'front {i:05}',f'back {40009-i:05}') for i in range(40010)]
  spec=dict(name='English German',front='en',back='de',entries=pairs)
  data=b.build_dict(spec)
  self.assertEqual(data[:8],b'GVDIDX01')
  self.assertEqual(b.read_dict(data)['entries'],pairs)
  self.assertEqual(b.build_dict(spec),data)
  for side,order in enumerate(b.read_dict(data)['indexes']):
   self.assertEqual(order,sorted(range(len(pairs)),key=lambda i:(b.key(pairs[i][side]),i)))
 def test_reader_rejects_rechecksummed_invalid_header(self):
  import zlib
  original=b.build_dict(dict(name='Test',front='en',back='de',entries=[('a','b')]))
  for offset,value in [(140,1),(48,ord('!')),(16,9)]:
   data=bytearray(original);data[offset]=value;struct.pack_into('<I',data,156,zlib.crc32(data[:156]))
   with self.assertRaises(ValueError):b.read_dict(data)
 def test_gba_additions_preserve_comment_characters(self):
  import zlib
  base=b.build_dict(dict(name='Test',front='en',back='de',entries=[('a','b')]))
  row=b'#tag\ttranslation'
  slot=bytearray(b'ADD1'+struct.pack('<I',0)+row+bytes(192-len(row)))
  slot.extend(struct.pack('<I',zlib.crc32(slot))+b'OK01')
  decoded=b.read_dict(base+slot)
  self.assertEqual(decoded['additions'],[('#tag','translation')])
  decoded['entries']+=decoded['additions']
  self.assertEqual(b.read_dict(b.build_dict(decoded))['entries'],decoded['entries'])
if __name__=='__main__':unittest.main()
