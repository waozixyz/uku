"""Prepare high-contrast captions for rendered canvas integration checks."""
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops, ImageDraw, ImageOps

source = Path(sys.argv[1])
image = Image.open(source).convert('RGB')
red, _, blue = image.split()
mask = ImageChops.subtract(blue, red).point(lambda value: 255 if value > 40 else 0)
output = source.parent / 'ocr'
output.mkdir(exist_ok=True)
regions = []
while mask.getbbox():
    _, top, _, _ = mask.getbbox()
    left = mask.crop((0, top, mask.width, top + 1)).tobytes().index(255)
    previous = mask.copy()
    ImageDraw.floodfill(mask, (left, top), 0)
    bounds = ImageChops.difference(previous, mask).getbbox()
    x, y, right, bottom = bounds
    if right - x < 48 or bottom - y < 24:
        continue
    # Borders confuse full-page OCR, so recognize the caption separately.
    inset = max(2, round(image.width / 500))
    x, y, right, bottom = x + inset, y + inset, right - inset, bottom - inset
    # Accent buttons have light captions on blue; a red-channel threshold
    # removes their fill and focus borders without erasing antialiased text.
    crop = image.crop((x, y, right, bottom)).getchannel('R').point(
        lambda value: 0 if value > 190 else 255)
    file = output / f'{len(regions)}.png'
    crop.save(file)
    regions.append({'file': str(file), 'x': x, 'y': y})

# Tesseract misses small light text on the dark canvas unless its polarity is
# normalized. Keep the original pixel dimensions for reliable click positions.
page = image.convert('L')
if page.getpixel((10, min(70, page.height - 1))) < 128:
    page = ImageOps.invert(page)
page = ImageOps.autocontrast(page)
file = output / 'page.png'
page.save(file)
regions.extend({'file': str(file), 'x': 0, 'y': 0, 'mode': mode} for mode in (6, 11))
print(json.dumps(regions))
