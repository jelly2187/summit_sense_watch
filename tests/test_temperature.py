import ctypes
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/serial_logger'))
from temperature_model import parse_temperature,summarize_temperature

class TemperatureCodecTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which('gcc')
        if not cc:raise unittest.SkipTest('gcc required')
        cls.temp=tempfile.TemporaryDirectory();dll=Path(cls.temp.name)/'codec.dll'
        subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-shared','-I',str(ROOT/'components/as6221/include'),str(ROOT/'components/as6221/as6221_codec.c'),'-o',str(dll)],check=True)
        cls.lib=ctypes.CDLL(str(dll))
        cls.lib.as6221_decode_temperature.argtypes=[ctypes.c_uint8,ctypes.c_uint8]
        cls.lib.as6221_decode_temperature.restype=ctypes.c_float
        cls.lib.as6221_temperature_in_range.argtypes=[ctypes.c_float]
        cls.lib.as6221_temperature_in_range.restype=ctypes.c_bool
    @classmethod
    def tearDownClass(cls):
        # Windows keeps a loaded DLL locked until it is explicitly released.
        handle=cls.lib._handle
        del cls.lib
        import _ctypes
        _ctypes.FreeLibrary(handle)
        cls.temp.cleanup()
    def test_datasheet_vectors(self):
        for raw,expected in [(0x0c80,25),(0x2580,75),(0xec00,-40),(0xf380,-25),(0xffff,-1/128),(0,0),(0x3e80,125)]:
            self.assertAlmostEqual(self.lib.as6221_decode_temperature(raw>>8,raw&255),expected,places=6)
    def test_zero_negative_and_range(self):
        for value in [-40,-25,0,25,125]:self.assertTrue(self.lib.as6221_temperature_in_range(value))
        for value in [-40.01,125.01,float('nan'),float('inf')]:self.assertFalse(self.lib.as6221_temperature_in_range(value))

class TemperatureClientTests(unittest.TestCase):
    def test_validity_and_errors(self):
        self.assertEqual(parse_temperature('TEMP,1,1000000,-25.0,1,0,0x40A0,OK')['temperature_c'],-25)
        self.assertEqual(parse_temperature('TEMP,1,1000000,0,1,0,0x40A0,OK')['valid'],1)
        self.assertEqual(parse_temperature('TEMP,1,1000000,-999,0,1,0x40A0,I2C_ERROR')['valid'],0)
        self.assertIsNone(parse_temperature('TEMP,1,1000000,-999,1,1,0x40A0,OK'))
        self.assertIsNone(parse_temperature('TEMP,1,1000000,nan,1,0,0x40A0,OK'))
    def test_quality_excludes_failed_values(self):
        rows=[]
        for line in ['TEMP,0,1000000,25,1,0,0x40A0,OK','TEMP,1,1250000,-999,0,1,0x40A0,I2C_ERROR','TEMP,2,1500000,26,1,1,0x40A0,OK']:
            row=parse_temperature(line);row['session']=1;rows.append(row)
        result=summarize_temperature(rows)['sessions'][0]
        self.assertEqual(result['valid_records'],2)
        self.assertEqual(result['mean_c'],25.5)
        self.assertEqual(result['received_hz'],4)
        self.assertEqual(result['i2c_errors_increase'],1)

if __name__=='__main__':unittest.main()
