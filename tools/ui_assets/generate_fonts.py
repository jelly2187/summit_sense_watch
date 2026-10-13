"""Rebuild the subset fonts. npm ci; pip install --target python_deps fonttools==4.60.1."""
from pathlib import Path
import re, sys, subprocess, urllib.request
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(HERE/'python_deps'))
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
fonts=HERE/'fonts';fonts.mkdir(exist_ok=True)
source=fonts/'NotoSansSC[wght].ttf'
if not source.exists():
    urllib.request.urlretrieve('https://raw.githubusercontent.com/google/fonts/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf',source)
static=fonts/'NotoSansSC-Medium.ttf'
if not static.exists():
    font=TTFont(source);instantiateVariableFont(font,{'wght':500},inplace=True);font.save(static)
text=(ROOT/'components/watch_ui/watch_pages.c').read_text(encoding='utf-8')
symbols=''.join(sorted(set(c for c in text if ord(c)>127)))
out=ROOT/'components/watch_ui/fonts';out.mkdir(exist_ok=True)
for size in (16,20):
    subprocess.run(['node',str(HERE/'node_modules/lv_font_conv/lv_font_conv.js'),
      '--font',str(static),'--range','0x20-0x7e','--symbols',symbols,'--size',str(size),
      '--bpp','4','--format','lvgl','--lv-include','lvgl.h','--no-compress','--no-kerning',
      '--lv-font-name',f'watch_cn_{size}','--output',str(out/f'watch_cn_{size}.c')],check=True)
    # Normalize converter output for clean, reproducible source diffs.
    generated=out/f'watch_cn_{size}.c'
    generated.write_text(generated.read_text(encoding='utf-8').rstrip()+'\n',encoding='utf-8',newline='\n')
