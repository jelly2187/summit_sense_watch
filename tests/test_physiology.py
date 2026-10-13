"""Host-test the exact firmware C algorithm with known signals and failure cases."""
import csv
import math
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class PhysiologyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which('gcc')
        if not cc: raise unittest.SkipTest('gcc required for host C replay')
        cls.temp=tempfile.TemporaryDirectory()
        cls.folder=Path(cls.temp.name)
        cls.exe=cls.folder/'replay.exe'
        subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
            '-I',str(ROOT/'components/physiology/include'),
            str(ROOT/'components/physiology/physiology.c'),
            str(ROOT/'tools/physiology_replay/replay.c'),'-lm','-o',str(cls.exe)],check=True)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def replay(self,rows):
        path=self.folder/'input.csv'
        with path.open('w',newline='') as f:
            writer=csv.writer(f)
            for seq,red,ir,flags,epoch in rows:
                writer.writerow([1,seq*10_000_000,seq,seq*10000,red,ir,flags,0,0,0,epoch])
        result=subprocess.run([str(self.exe),str(path)],capture_output=True,text=True,check=True)
        return list(csv.DictReader(result.stdout.splitlines()))

    def signal(self,bpm=72,ratio=.5,count=3000):
        return [(i,int(50000+1000*ratio*math.sin(2*math.pi*bpm/60*i/100)),
                 int(100000+2000*math.sin(2*math.pi*bpm/60*i/100)),0,0) for i in range(count)]

    def test_known_rates_and_formula(self):
        for bpm in [60,72,90,120,150]:
            results=self.replay(self.signal(bpm))
            available=[r for r in results if r['hr_available']=='1']
            self.assertTrue(available, f'no HR at {bpm}')
            for r in available:
                self.assertLess(abs(float(r['hr_bpm'])-bpm),2)
                self.assertLess(abs(float(r['spo2_est_pct'])-98.757),.2)
                self.assertEqual(r['calibrated'],'0')

    def test_no_contact_flat_and_saturation(self):
        for red,ir,reason in [(800,650,'NO_CONTACT'),(50000,100000,'WEAK_SIGNAL'),(262143,100000,'SATURATED')]:
            results=self.replay([(i,red,ir,0,0) for i in range(1500)])
            self.assertTrue(any(r['reason']==reason for r in results))
            self.assertTrue(all(r['hr_available']=='0' and r['spo2_est_available']=='0' for r in results))

    def test_gap_invalidates_and_requires_new_window(self):
        rows=self.signal(count=3000)
        rows=[r for r in rows if r[0]!=1200]
        results=self.replay(rows)
        self.assertTrue(any(r['reason']=='DATA_GAP' for r in results))
        self.assertTrue(all(r['hr_available']=='0' for r in results if 12_010_000<=int(r['time_us'])<20_000_000))
        self.assertTrue(any(r['hr_available']=='1' and int(r['time_us'])>=20_000_000 for r in results))

    def test_out_of_range_spo2_not_clamped(self):
        results=self.replay(self.signal(ratio=2))
        good=[r for r in results if r['hr_available']=='1']
        self.assertTrue(good)
        self.assertTrue(all(r['spo2_est_available']=='0' and float(r['spo2_est_pct'])==-1 for r in good))

    def test_random_noise_rejected(self):
        rng=random.Random(17)
        results=self.replay([(i,50000+rng.randrange(-1000,1001),100000+rng.randrange(-2000,2001),0,0) for i in range(3000)])
        self.assertTrue(all(r['hr_available']=='0' for r in results))

if __name__=='__main__': unittest.main()
