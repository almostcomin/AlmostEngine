#!/usr/bin/env python3
"""
Combine separate AO, Roughness, and Metalness texture files into a single ORM
texture following the glTF convention:
    R = Occlusion (AO)
    G = Roughness
    B = Metalness

Supports PNG, JPG, JPEG, TGA, BMP, TIFF as input formats. Output is always PNG.

Looks for files in a directory matching common naming patterns used by
Polyhaven and ambientCG (case-insensitive).

Usage:
    python combine_orm.py <directory>
    python combine_orm.py <directory> --output-name MyMaterial_ORM.png
    python combine_orm.py <directory> --recursive

If AO is missing, R channel defaults to white (no occlusion).
If Metalness is missing, B channel defaults to black (non-metallic).
If Roughness is missing, the script aborts (it's the essential map).
"""

import argparse
import re
import sys
from pathlib import Path
from PIL import Image


# Supported input image extensions
IMAGE_EXTENSIONS = r'(png|jpg|jpeg|tga|bmp|tif|tiff)'

# Naming patterns Polyhaven / ambientCG / FreePBR commonly use.
# Order matters: more specific first.
PATTERNS = {
    'ao': [
        rf'.*_ao\.{IMAGE_EXTENSIONS}$',
        rf'.*_ambient[_-]?occlusion\.{IMAGE_EXTENSIONS}$',
        rf'.*_occlusion\.{IMAGE_EXTENSIONS}$',
        rf'.*ambientocclusion.*\.{IMAGE_EXTENSIONS}$',
    ],
    'roughness': [
        rf'.*_rough(ness)?\.{IMAGE_EXTENSIONS}$',
        rf'.*roughness.*\.{IMAGE_EXTENSIONS}$',
    ],
    'metalness': [
        rf'.*_metal(ness|lic)?\.{IMAGE_EXTENSIONS}$',
        rf'.*metal(ness|lic).*\.{IMAGE_EXTENSIONS}$',
    ],
}


def find_map(directory: Path, kind: str) -> Path | None:
    """Find the first file in `directory` matching one of the patterns for `kind`."""
    patterns = [re.compile(p, re.IGNORECASE) for p in PATTERNS[kind]]
    for file in directory.iterdir():
        if not file.is_file():
            continue
        for pat in patterns:
            if pat.match(file.name):
                return file
    return None


def combine(directory: Path, output_name: str | None) -> bool:
    """Combine maps in `directory` into a single ORM file.
    Returns True on success, False otherwise.
    """
    print(f"\n→ Processing: {directory}")

    ao_path = find_map(directory, 'ao')
    rough_path = find_map(directory, 'roughness')
    metal_path = find_map(directory, 'metalness')

    if rough_path is None:
        print(f"  ✗ No roughness map found. Skipping.")
        return False

    print(f"  AO:        {ao_path.name if ao_path else '(missing → white)'}")
    print(f"  Roughness: {rough_path.name}")
    print(f"  Metalness: {metal_path.name if metal_path else '(missing → black)'}")

    rough_img = Image.open(rough_path).convert('L')
    size = rough_img.size

    if ao_path:
        ao_img = Image.open(ao_path).convert('L')
        if ao_img.size != size:
            print(f"  ! Resizing AO from {ao_img.size} to {size}")
            ao_img = ao_img.resize(size, Image.LANCZOS)
    else:
        ao_img = Image.new('L', size, 255)

    if metal_path:
        metal_img = Image.open(metal_path).convert('L')
        if metal_img.size != size:
            print(f"  ! Resizing Metalness from {metal_img.size} to {size}")
            metal_img = metal_img.resize(size, Image.LANCZOS)
    else:
        metal_img = Image.new('L', size, 0)

    orm = Image.merge('RGB', (ao_img, rough_img, metal_img))

    # Output name: use provided, else derive from roughness filename
    if output_name:
        out_path = directory / output_name
    else:
        # Strip extension and roughness suffix
        base = rough_path.stem  # filename without extension
        base = re.sub(r'_rough(ness)?$', '', base, flags=re.IGNORECASE)
        base = re.sub(r'roughness', '', base, flags=re.IGNORECASE)
        base = base.strip('_- ')
        out_path = directory / f"{base}_ORM.png"

    orm.save(out_path, optimize=True)
    print(f"  ✓ Wrote: {out_path.name}  ({size[0]}x{size[1]})")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Combine AO/Roughness/Metalness into glTF ORM textures."
    )
    parser.add_argument('directory', type=Path,
                        help='Directory containing the material textures.')
    parser.add_argument('--output-name', type=str, default=None,
                        help='Custom output filename (default: derived from roughness file).')
    parser.add_argument('--recursive', action='store_true',
                        help='Process subdirectories. Each subdir is treated as a material.')

    args = parser.parse_args()

    if not args.directory.exists():
        print(f"Error: '{args.directory}' does not exist.", file=sys.stderr)
        sys.exit(1)

    if not args.directory.is_dir():
        print(f"Error: '{args.directory}' is not a directory.", file=sys.stderr)
        sys.exit(1)

    if args.recursive:
        dirs = [d for d in args.directory.iterdir() if d.is_dir()]
        if not dirs:
            print(f"No subdirectories found in '{args.directory}'. Processing it directly.")
            dirs = [args.directory]
    else:
        dirs = [args.directory]

    success = 0
    failed = 0
    for d in dirs:
        if combine(d, args.output_name):
            success += 1
        else:
            failed += 1

    print(f"\nDone. {success} succeeded, {failed} skipped.")


if __name__ == '__main__':
    main()