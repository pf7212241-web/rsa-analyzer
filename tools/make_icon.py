"""Векторная по построению пиктограмма: модуль и два множителя."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
image = Image.new('RGBA', (128, 128), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((1, 1, 126, 126), radius=23, fill='#2459A6')
draw.line([(64, 48), (64, 67), (34, 87)], fill='white', width=7)
draw.line([(64, 67), (94, 87)], fill='white', width=7)
for x, y, r in [(64, 35, 17), (31, 94, 13), (97, 94, 13)]:
    draw.ellipse((x-r, y-r, x+r, y+r), fill='white')
    draw.ellipse((x-r+6, y-r+6, x+r-6, y+r-6), fill='#2459A6')
(root / 'assets').mkdir(exist_ok=True)
image.save(root / 'assets/rsa.png')
image.save(root / 'assets/rsa.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128)])
