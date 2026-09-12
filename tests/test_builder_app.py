import json, os, subprocess, sys, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class AppTest(unittest.TestCase):
    def test_embedded_template_selftest(self):
        app=ROOT/'builder/app.py'
        self.assertTrue(app.exists(), 'GUI application not implemented')
        with tempfile.TemporaryDirectory() as tmp:
            p=subprocess.run([sys.executable,str(app),'--self-test',tmp,'--template',str(ROOT/'gbavocab.gba')],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
            report=json.loads((Path(tmp)/'self-test.json').read_text())
            self.assertEqual(report['entries'],40011)
            self.assertTrue(report['roundtrip'])
    @unittest.skipUnless(os.environ.get('DISPLAY') or sys.platform=='win32','GUI requires desktop')
    def test_gui_constructs(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=subprocess.run([sys.executable,str(ROOT/'builder/app.py'),'--gui-smoke',tmp],capture_output=True,text=True)
            self.assertEqual(p.returncode,0,p.stdout+p.stderr)
            self.assertTrue(json.loads((Path(tmp)/'gui-smoke.json').read_text())['window_created'])
if __name__=='__main__': unittest.main()
