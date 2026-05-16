import csv
import os
import math
import subprocess
import sys
from collections import defaultdict

os.makedirs("gifs", exist_ok=True)

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Instalando Pillow para generar GIFs...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "--user", "pillow"])
    from PIL import Image, ImageDraw


def color_map(value, vmin, vmax):
    if vmax <= vmin:
        t = 0.5
    else:
        t = (value - vmin) / (vmax - vmin)

    if t < 0:
        t = 0
    if t > 1:
        t = 1

    r = int(255 * t)
    b = int(255 * (1 - t))
    g = int(80 * (1 - abs(2 * t - 1)))

    return (r, g, b)


def make_wave1d_gif():
    data = defaultdict(list)

    with open("resultados/wave1d_snapshots.csv", "r") as file:
        reader = csv.DictReader(file)

        for row in reader:
            step = int(row["step"])
            x = float(row["x"])
            u = float(row["u"])
            data[step].append((x, u))

    width = 900
    height = 360
    margin = 45

    frames = []

    for step in sorted(data.keys()):
        img = Image.new("RGB", (width, height), "white")
        draw = ImageDraw.Draw(img)

        draw.line((margin, height // 2, width - margin, height // 2), fill=(180, 180, 180), width=1)
        draw.rectangle((margin, margin, width - margin, height - margin), outline=(0, 0, 0))

        points = []

        for x, u in data[step]:
            px = margin + x * (width - 2 * margin)
            py = height // 2 - u * 120.0
            points.append((px, py))

        if len(points) > 1:
            draw.line(points, fill=(20, 70, 180), width=2)

        draw.text((margin, 15), f"Onda 1D - paso {step}", fill=(0, 0, 0))
        frames.append(img)

    frames[0].save(
        "gifs/wave1d.gif",
        save_all=True,
        append_images=frames[1:],
        duration=60,
        loop=0
    )


def make_wave3d_gif():
    raw = defaultdict(list)

    with open("resultados/wave3d_slice_snapshots.csv", "r") as file:
        reader = csv.DictReader(file)

        for row in reader:
            step = int(row["step"])
            x = int(row["x"])
            y = int(row["y"])
            u = float(row["u"])
            raw[step].append((x, y, u))

    all_values = [u for values in raw.values() for _, _, u in values]
    vmax = max(abs(min(all_values)), abs(max(all_values)))
    vmin = -vmax

    cell = 8
    Nx = max(x for values in raw.values() for x, _, _ in values) + 1
    Ny = max(y for values in raw.values() for _, y, _ in values) + 1

    width = Nx * cell
    height = Ny * cell + 30

    frames = []

    for step in sorted(raw.keys()):
        img = Image.new("RGB", (width, height), "white")
        draw = ImageDraw.Draw(img)

        for x, y, u in raw[step]:
            c = color_map(u, vmin, vmax)
            x0 = x * cell
            y0 = 30 + y * cell
            draw.rectangle((x0, y0, x0 + cell, y0 + cell), fill=c)

        draw.text((10, 8), f"Onda 3D - corte central z = L/2 - paso {step}", fill=(0, 0, 0))
        frames.append(img)

    frames[0].save(
        "gifs/wave3d_slice.gif",
        save_all=True,
        append_images=frames[1:],
        duration=70,
        loop=0
    )


make_wave1d_gif()
make_wave3d_gif()

print("GIFs generados:")
print("  gifs/wave1d.gif")
print("  gifs/wave3d_slice.gif")
