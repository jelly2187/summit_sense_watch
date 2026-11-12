"""Build the Rev B title/clock subsets using the existing OFL Noto Sans SC source."""
from pathlib import Path
import subprocess

here = Path(__file__).resolve().parent
root = here.parents[1]
src = here / "fonts/NotoSansSC-Medium.ttf"
conv = here / "node_modules/lv_font_conv/lv_font_conv.js"
out = root / "components/watch_ui/fonts"
for name, size, symbols in (
    ("watch_title_28", 28, "山知健康运动环境定位设备设置"),
    ("watch_clock_84", 84, "0123456789:"),
):
    dest = out / (name + ".c")
    subprocess.run(["node", str(conv), "--font", str(src), "--symbols", symbols, "--size", str(size),
                    "--bpp", "4", "--format", "lvgl", "--lv-include", "lvgl.h", "--no-compress",
                    "--no-kerning", "--lv-font-name", name, "--output", str(dest)], check=True)
    text = dest.read_text(encoding="utf-8")
    # Omit machine-dependent absolute paths recorded in the converter comment.
    text = "\n".join(line for line in text.splitlines() if not line.startswith(" * Opts:"))
    dest.write_text(text.rstrip() + "\n", encoding="utf-8", newline="\n")
