# UI中文字库构建

固件直接编译`components/watch_ui/fonts`内生成的C字库，无需在每次构建时运行字体工具。只有修改中文文案/字形时才重新生成。

```powershell
npm ci --prefix tools/ui_assets
python -m pip install --target tools/ui_assets/python_deps fonttools==4.60.1
python tools/ui_assets/generate_fonts.py
```

需要Node与Python在PATH。脚本从Google Fonts官方仓库下载Noto Sans SC变量字体，实例化wght=500，再从页面源码提取中文及符号，附ASCII生成16/20px、4bpp字库。完整字体、node_modules和python_deps不纳入Git；生成C文件、脚本、package-lock、OFL许可纳入Git。

源字体：[Noto Sans SC](https://github.com/google/fonts/tree/main/ofl/notosanssc)，SIL OFL 1.1，见fonts/OFL.txt。工具lv_font_conv1.5.3使用MIT；LVGL9.2.2使用MIT。源码字体下载地址跟随官方main；已经生成的C文件固定当前字形，若需严格重建相同字形应另外保存源字体版本/校验信息。
