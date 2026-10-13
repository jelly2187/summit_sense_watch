import csv
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/serial_logger'))
from imu_model import parse_imu,parse_status,rotate_vector

def packet(kind,payload):
    data=bytes([0x55,kind])+payload
    return data+bytes([sum(data)&255])

class IMUParserTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc=shutil.which('gcc')
        if not cc:raise unittest.SkipTest('gcc required')
        cls.temp=tempfile.TemporaryDirectory();cls.path=Path(cls.temp.name)
        source=cls.path/'host.c'
        source.write_text(r'''
#include "imu_protocol.h"
#include <stdio.h>
#include <stdlib.h>
static void received(const imu_frame_t *p,void *user) {
    (void)user;
    printf("%s,%u,%.6f,%.6f,%.6f,%.6f\n",p->kind,p->valid_mask,p->values[0],p->values[1],p->values[2],p->values[3]);
}
int main(int argc,char **argv) {
    unsigned chunk=argc>1?atoi(argv[1]):7;if(!chunk || chunk>64)return 2;
    imu_parser_t parser;imu_parser_init(&parser);
    uint8_t bytes[64];unsigned value,n=0;
    while(scanf("%x",&value)==1) {
        bytes[n++]=value;
        if(n==chunk) { imu_parser_feed(&parser,bytes,n,received,NULL);n=0; }
    }
    if(n)imu_parser_feed(&parser,bytes,n,received,NULL);
    printf("STAT,%llu,%llu\n",(unsigned long long)parser.frames,(unsigned long long)parser.checksum_errors);
    return 0;
}
''',encoding='utf-8')
        cls.exe=cls.path/'host.exe'
        subprocess.run([cc,'-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'components/yahboom_imu/include'),str(ROOT/'components/yahboom_imu/imu_protocol.c'),str(source),'-o',str(cls.exe)],check=True)

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def decode(self,data,chunk=7):
        p=subprocess.run([str(self.exe),str(chunk)],input=' '.join(f'{b:02x}' for b in data),text=True,capture_output=True,check=True)
        return [line.split(',') for line in p.stdout.splitlines()]

    def test_signed_units_and_fragmentation(self):
        data=packet(0x51,struct.pack('<hhhh',2048,-2048,0,-1234))+packet(0x52,struct.pack('<hhhh',16384,-32768,0,77))+packet(0x53,struct.pack('<hhhh',16384,-16384,8192,0x55))
        for chunk in [1,3,7,64]:
            rows=self.decode(data,chunk)
            self.assertEqual(rows[0][:2],['ACC','15'])
            self.assertEqual(list(map(float,rows[0][2:])),[1,-1,0,-12.34])
            self.assertEqual(list(map(float,rows[1][2:])),[1000,-2000,0,0])
            self.assertEqual(list(map(float,rows[2][2:])),[90,-90,45,85])

    def test_pressure_and_signed_height(self):
        row=self.decode(packet(0x56,struct.pack('<Ii',101325,-12345)))[0]
        self.assertEqual(row[:2],['PRESS','3'])
        self.assertEqual(list(map(float,row[2:])),[101325,-123.45,0,0])
        row=self.decode(packet(0x56,struct.pack('<Ii',0,500)))[0]
        self.assertEqual(row[1],'0')

    def test_corruption_then_resync(self):
        good=packet(0x51,struct.pack('<hhhh',0,0,2048,2500))
        bad=bytearray(good);bad[-1]^=1
        rows=self.decode(bytes([1,2,3])+bytes(bad)+good,3)
        self.assertEqual(rows[0][0],'ACC')
        self.assertEqual(rows[-1][1:],[ '1','1'])

    def test_unused_packet_not_mislabelled(self):
        rows=self.decode(packet(0x50,bytes(8)))
        self.assertEqual(rows,[['STAT','1','0']])

class IMUClientTests(unittest.TestCase):
    def test_telemetry_and_valid_mask(self):
        row=parse_imu('IMU,PRESS,5,100000,101325,-12.34,0,0,3')
        self.assertEqual(row['v2'],-12.34)
        self.assertEqual(row['valid_mask'],3)
        self.assertIsNone(parse_imu('IMU,PRESS,5,100000,nan,0,0,0,3'))
        self.assertIsNone(parse_imu('IMU,INVALID,5,100000,0,0,0,0,3'))
        self.assertEqual(parse_status('IMU_STAT,100000,10,0,0,0,2,2,1,1')['configured'],1)

    def test_rotation_axes(self):
        for vector,angles,expected in [((1,0,0),(0,0,90),(0,1,0)),((1,0,0),(0,90,0),(0,0,-1)),((0,1,0),(90,0,0),(0,0,1))]:
            for actual,want in zip(rotate_vector(vector,*angles),expected):self.assertAlmostEqual(actual,want,places=6)

if __name__=='__main__':unittest.main()
