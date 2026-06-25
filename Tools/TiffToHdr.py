#!/usr/bin/env python3
"""
Convert TIFF to EHdr (HDR+RAW) format.
GDAL generates the data file without extension, then we rename it to .raw.
"""

import os
import sys
import argparse
import subprocess
import json
import shutil
from pathlib import Path

def find_gdal_translate():
    """Locate gdal_translate executable."""
    # Try PATH
    try:
        subprocess.run(['gdal_translate', '--version'], capture_output=True, check=True)
        return 'gdal_translate'
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # Common OSGeo4W paths
    paths = [
        r'C:\OSGeo4W\bin\gdal_translate.exe',
        r'C:\OSGeo4W64\bin\gdal_translate.exe',
        r'C:\Program Files\QGIS 3.xx\bin\gdal_translate.exe',
    ]
    for p in paths:
        if os.path.exists(p):
            return p

    # OSGeo4W batch wrapper
    osgeo4w_bat = r'C:\OSGeo4W\osgeo4w.bat'
    if os.path.exists(osgeo4w_bat):
        return osgeo4w_bat

    raise RuntimeError("gdal_translate not found. Install GDAL/OSGeo4W.")

def get_tiff_info(tiff_path, gdal_cmd):
    """Get min/max from TIFF using gdalinfo."""
    if gdal_cmd.endswith('osgeo4w.bat'):
        cmd = [gdal_cmd, 'gdalinfo', tiff_path]
    else:
        cmd = [gdal_cmd.replace('gdal_translate', 'gdalinfo'), tiff_path]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        import re
        min_match = re.search(r'Minimum=([\d.-]+)', result.stdout)
        max_match = re.search(r'Maximum=([\d.-]+)', result.stdout)
        return (float(min_match.group(1)) if min_match else None,
                float(max_match.group(1)) if max_match else None)
    except subprocess.CalledProcessError:
        return None, None

def convert_tiff_to_ehdr(tiff_path, output_dir, normalize=False,
                         min_val=None, max_val=None, nodata=None,
                         output_name=None):
    """
    Convert TIFF to EHdr (HDR+RAW) using gdal_translate.
    """
    tiff_path = Path(tiff_path)
    if not tiff_path.exists():
        raise FileNotFoundError(f"TIFF not found: {tiff_path}")

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    if output_name is None:
        output_name = tiff_path.stem

    # Use output WITHOUT extension (GDAL's default for EHdr)
    output_base = output_dir / output_name

    gdal_cmd = find_gdal_translate()
    print(f"Using GDAL: {gdal_cmd}")

    # Build command
    if gdal_cmd.endswith('osgeo4w.bat'):
        cmd = [gdal_cmd, 'gdal_translate']
    else:
        cmd = [gdal_cmd]

    # EHdr format: generates .hdr and data file WITHOUT extension
    cmd.extend(['-of', 'EHdr'])
    cmd.extend(['-ot', 'Float32'])
    cmd.extend(['-co', 'INTERLEAVE=BIL'])

    # Normalization (optional)
    if normalize:
        if min_val is None or max_val is None:
            print("Auto-detecting min/max from TIFF...")
            min_val, max_val = get_tiff_info(str(tiff_path), gdal_cmd)
            if min_val is None or max_val is None:
                raise ValueError("Could not auto-detect min/max. Specify --min and --max.")

        print(f"Normalizing to 0-1 (range: {min_val:.2f} - {max_val:.2f})")
        cmd.extend(['-scale', str(min_val), str(max_val), '0', '1'])
    else:
        print("Keeping original values (no normalization)")

    # NODATA (optional)
    if nodata is not None:
        cmd.extend(['-a_nodata', str(nodata)])

    # Input and output (NO extension)
    cmd.append(str(tiff_path))
    cmd.append(str(output_base))

    print(f"\nExecuting: {' '.join(cmd)}")
    try:
        subprocess.run(cmd, capture_output=True, text=True, check=True)
        print("\n✅ Conversion successful!")

        # GDAL generates:
        #   - output_name      (data file, NO extension)
        #   - output_name.hdr  (header file)

        # Rename data file to .raw
        data_file = output_base  # No extension
        raw_file = output_base.with_suffix('.raw')
        
        if data_file.exists():
            print(f"Renaming: {data_file} -> {raw_file}")
            data_file.rename(raw_file)
        else:
            print(f"Warning: Data file not found: {data_file}")

        # The .hdr file should stay as .hdr (it references the data file by name)
        hdr_file = output_base.with_suffix('.hdr')
        if hdr_file.exists():
            print(f"   HDR: {hdr_file}")
            print(f"   RAW: {raw_file}")

            print("\n📄 Generated HDR content:")
            with open(hdr_file, 'r') as f:
                print(f.read())
        else:
            print(f"Warning: HDR file not found: {hdr_file}")

        return str(raw_file)

    except subprocess.CalledProcessError as e:
        print(f"❌ Error: {e.stderr}")
        raise

def main():
    parser = argparse.ArgumentParser(
        description="Convert TIFF to EHdr (HDR+RAW) format.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python convert_tiff_to_hdr.py terrain.tif ./output
  python convert_tiff_to_hdr.py terrain.tif ./output --normalize
  python convert_tiff_to_hdr.py terrain.tif ./output --normalize --min 0 --max 1000
  python convert_tiff_to_hdr.py terrain.tif ./output --name heightmap --nodata -32767
        """
    )
    parser.add_argument('input', help='Input TIFF file')
    parser.add_argument('output_dir', help='Output directory')
    parser.add_argument('--name', '-n', help='Output base name (without extension)')
    parser.add_argument('--normalize', action='store_true', help='Normalize values to 0-1')
    parser.add_argument('--min', type=float, help='Min value for normalization (auto-detect)')
    parser.add_argument('--max', type=float, help='Max value for normalization (auto-detect)')
    parser.add_argument('--nodata', type=float, help='NODATA value')

    args = parser.parse_args()

    try:
        raw_file = convert_tiff_to_ehdr(
            tiff_path=args.input,
            output_dir=args.output_dir,
            normalize=args.normalize,
            min_val=args.min,
            max_val=args.max,
            nodata=args.nodata,
            output_name=args.name
        )

        # Save metadata (optional)
        metadata = {
            'input': args.input,
            'output': str(raw_file),
            'normalized': args.normalize,
            'min_val': args.min,
            'max_val': args.max,
            'nodata': args.nodata
        }
        json_path = Path(raw_file).parent / f"{Path(raw_file).stem}.json"
        with open(json_path, 'w') as f:
            json.dump(metadata, f, indent=2)
        print(f"\n📝 Metadata saved: {json_path}")

    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()