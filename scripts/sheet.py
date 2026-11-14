"""Képernyőképek összefűzése egy áttekintő lapra: sheet.py OUT.png IN1.png IN2.png ... (4 oszlop)"""
import sys
from PIL import Image
out, files = sys.argv[1], sys.argv[2:]
cols = min(4, len(files))
rows = (len(files) + cols - 1) // cols
W = Image.new("RGB", (cols * 370 - 10, rows * 370 - 10), (25, 25, 25))
for i, f in enumerate(files):
    W.paste(Image.open(f), ((i % cols) * 370, (i // cols) * 370))
W.save(out)
