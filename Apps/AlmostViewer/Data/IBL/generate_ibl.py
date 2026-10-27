#!/usr/bin/env python3
"""Generate IBL textures from an HDR environment using Filament's cmgen.

Usage:
    py generate_ibl.py path/to/environment.hdr
    py generate_ibl.py path/to/environment.hdr --output-path path/to/output

Outputs:
    <name>_GGX.ktx2          prefiltered specular cubemap (mip chain, GGX)
    <name>_Lambertian.ktx2   diffuse irradiance cubemap
    <name>_Sky.ktx2          environment cubemap for the skybox

cmgen handles the panorama -> prefiltered cubemap filtering with proper
cross-face handling (no seams). KTX2 packing is done with the official Khronos
'ktx' tool (ktx create --cubemap), so the files are guaranteed valid KTX2.

Requires next to this script:
    cmgen.exe (Apache 2.0, see FILAMENT_LICENSE.txt)
    ktx.exe + ktx.dll (Apache 2.0, see KTX_LICENSE.html)
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

CMGEN = "cmgen.exe"
TARGET_FORMAT = "R16G16B16A16_SFLOAT"
MIN_LOD_SIZE = 2  # mip chain down to 2x2, like glTF-IBL-Sampler does

FACE_FILES = ["px", "nx", "py", "ny", "pz", "nz"]  # cmgen naming -> KTX2 face order

HALF_MAX = 65504.0


def scan_hdr_max(path: Path) -> float:
    """Return the maximum RGB value in a Radiance .hdr file (fast, with per-pixel quick reject)."""
    import math as _m
    with open(path, "rb") as f:
        data = f.read()
    pos = 0
    while True:
        eol = data.index(b"\n", pos)
        line = data[pos:eol].decode("ascii", "ignore").strip()
        pos = eol + 1
        if line.startswith("-Y") or line.startswith("+Y"):
            parts = line.split()
            h, w = int(parts[1]), int(parts[3])
            break
    maxv = 0.0
    for y in range(h):
        if w >= 8 and w < 32768 and data[pos] == 2 and data[pos+1] == 2 and (data[pos+2] << 8 | data[pos+3]) == w:
            pos += 4
            row = [0] * (w * 4)
            for c in range(4):
                x = 0
                while x < w:
                    cnt = data[pos]
                    if cnt > 128:
                        n = cnt - 128; val = data[pos+1]; pos += 2
                        for k in range(n):
                            row[x*4+c] = val; x += 1
                    else:
                        n = cnt; pos += 1
                        for k in range(n):
                            row[x*4+c] = data[pos]; pos += 1; x += 1
        else:
            row = list(data[pos:pos+w*4]); pos += w*4
        for x in range(w):
            i = x*4
            ex = row[i+3]
            if 255.0 * 2.0**(ex-136) <= maxv:  # quick reject
                continue
            m = _m.ldexp(1.0, ex - 136)
            for c in range(3):
                v = row[i+c] * m
                if v > maxv:
                    maxv = v
    return maxv


def write_hdr_clamped(src: Path, dst: Path, max_value: float) -> None:
    """Write a copy of a Radiance .hdr with all RGB values clamped to max_value (uncompressed rows)."""
    import math as _m
    with open(src, "rb") as f:
        data = f.read()
    pos = 0
    while True:
        eol = data.index(b"\n", pos)
        line = data[pos:eol].decode("ascii", "ignore").strip()
        pos = eol + 1
        if line.startswith("-Y") or line.startswith("+Y"):
            parts = line.split()
            h, w = int(parts[1]), int(parts[3])
            break
    with open(dst, "wb") as out:
        res_line_start = data.rfind(b"\n", 0, pos - 1) + 1  # header without the resolution line
        out.write(data[:res_line_start])
        out.write(f"-Y {h} +X {w}\n".encode("ascii"))
        for y in range(h):
            if w >= 8 and w < 32768 and data[pos] == 2 and data[pos+1] == 2 and (data[pos+2] << 8 | data[pos+3]) == w:
                pos += 4
                row = [0] * (w * 4)
                for c in range(4):
                    x = 0
                    while x < w:
                        cnt = data[pos]
                        if cnt > 128:
                            n = cnt - 128; val = data[pos+1]; pos += 2
                            for k in range(n):
                                row[x*4+c] = val; x += 1
                        else:
                            n = cnt; pos += 1
                            for k in range(n):
                                row[x*4+c] = data[pos]; pos += 1; x += 1
            else:
                row = list(data[pos:pos+w*4]); pos += w*4
            # convert RGBE -> float, clamp, re-encode RGBE
            for x in range(w):
                i = x*4
                ex = row[i+3]
                m = _m.ldexp(1.0, ex - 136) if ex > 0 else 0.0
                rgb = [min(row[i+c] * m, max_value) for c in range(3)]
                # re-encode
                v = max(rgb)
                if v <= 1e-32:
                    out.write(b"\x00\x00\x00\x00")
                    continue
                e = _m.ceil(_m.log2(v))
                mult = 256.0 * 2.0**(-e)
                enc = bytes(min(255, int(c * mult + 0.5)) for c in rgb)
                out.write(enc + bytes([e + 128]))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate GGX/Lambertian IBL KTX2 cubemaps from an HDR image (via Filament cmgen)."
    )
    parser.add_argument("texture", type=Path, help="Input environment texture (.hdr or .exr).")
    parser.add_argument("-o", "--output-path", type=Path, default=None,
                        help="Output directory (default: same directory as the input texture).")
    parser.add_argument("--lambertian-resolution", type=int, default=256,
                        help="Lambertian irradiance cubemap face resolution (default: 256).")
    parser.add_argument("--ggx-resolution", type=int, default=256,
                        help="GGX cubemap face resolution (default: 1024).")
    parser.add_argument("--sky-resolution", type=int, default=1024,
                        help="Face resolution of the sky cubemap (default: 1024).")
    parser.add_argument("--mirror", action="store_true",
                        help="Let cmgen mirror the cubemaps (its internal Filament convention). "
                             "Try this if faces look horizontally mirrored in PVRTexTool.")
    args = parser.parse_args()

    input_path = args.texture.expanduser().resolve()
    if not input_path.is_file():
        parser.error(f"Input texture does not exist: {input_path}")

    here = Path(__file__).resolve().parent
    cmgen = here / CMGEN
    ktx = here / "ktx.exe"
    if not cmgen.is_file():
        print(f"Error: {CMGEN} not found next to this script.", file=sys.stderr)
        return 1
    if not ktx.is_file():
        print("Error: ktx.exe (KTX-Software tools) not found next to this script.", file=sys.stderr)
        return 1

    name = input_path.stem
    output_dir = (args.output_path.expanduser().resolve()
                  if args.output_path is not None else input_path.parent)
    output_dir.mkdir(parents=True, exist_ok=True)

    temp_dir = output_dir / "_temp"
    temp_dir.mkdir(parents=True, exist_ok=True)

    mirror_flag = [] if args.mirror else ["--no-mirror"]
    ggx_output = output_dir / f"{name}_GGX.ktx2"
    lambertian_output = output_dir / f"{name}_Lambertian.ktx2"
    sky_output = output_dir / f"{name}_Sky.ktx2"
    ok = True

    # Clamp HDR values that exceed half-float range (e.g. a very bright sun disk);
    # otherwise the R16G16B16A16_SFLOAT KTX2 ends up with inf texels.
    actual_input = input_path
    max_value = scan_hdr_max(input_path)
    if max_value > HALF_MAX:
        if input_path.suffix.lower() == ".hdr":
            print(f"\nInput has values above half-float range (max {max_value:.0f} > {HALF_MAX:.0f}); "
                  "using a clamped copy to avoid inf texels in the KTX2 output.")
            clamped = temp_dir / (name + "_clamped.hdr")
            write_hdr_clamped(input_path, clamped, HALF_MAX)
            actual_input = clamped
        else:
            print(f"\nWarning: input has values above half-float range (max {max_value:.0f}); "
                  "the KTX2 output may contain inf texels (auto-clamp only supports .hdr).",
                  file=sys.stderr)

    def run_cmgen(label: str, size: int, extra: list[str]) -> bool:
        cmd = [str(cmgen), "--quiet", f"--size={size}", "--format=exr", *extra, str(actual_input)]
        print(f"\nGenerating {label} (cmgen, {size}x{size})...")
        result = subprocess.run(cmd, check=False, cwd=temp_dir)
        if result.returncode != 0:
            print(f"Error: {label} generation failed (cmgen exit code {result.returncode}).",
                  file=sys.stderr)
            return False
        return True

    def pack_cubemap(hdr_dir: Path, out_file: Path, prefix: str, label: str) -> bool:
        """Pack cmgen face files (mN_face.exr, is_mN_face.exr or i_face.exr) into one KTX2 via 'ktx create'."""
        subdirs = [d for d in hdr_dir.iterdir() if d.is_dir()]
        if len(subdirs) != 1:
            print(f"Error: expected exactly one cmgen output dir in {hdr_dir}", file=sys.stderr)
            return False
        d = subdirs[0]
        if prefix == "i_":
            mips = [0]
        else:
            num_start = len(prefix)  # file name: prefix + <num> + _face.exr
            mips = sorted(int(f.stem[num_start:].split("_")[0]) for f in d.glob(f"{prefix}*_px.exr"))
        inputs = []
        for m in mips:
            for face in FACE_FILES:
                p = d / (f"i_{face}.exr" if prefix == "i_" else f"{prefix}{m}_{face}.exr")
                if not p.is_file():
                    print(f"Error: missing cmgen output {p}", file=sys.stderr)
                    return False
                inputs.append(str(p))
        cmd = [str(ktx), "create", "--cubemap", "--format", TARGET_FORMAT,
               "--levels", str(len(mips)), *inputs, str(out_file)]
        result = subprocess.run(cmd, check=False, cwd=temp_dir)
        if result.returncode != 0:
            print(f"Error: {label} packing failed (ktx exit code {result.returncode}).",
                  file=sys.stderr)
            return False
        print(f"Wrote {out_file} ({len(mips)} mip levels)")
        return True

    # GGX prefiltered cubemap (full mip chain)
    ggx_dir = temp_dir / "ggx"
    ok = run_cmgen("GGX cubemap", args.ggx_resolution,
                   mirror_flag + [f"--ibl-ld={ggx_dir}",
                                  "--ibl-samples=1024",
                                  f"--ibl-min-lod-size={MIN_LOD_SIZE}"]) and ok
    if ok:
        ok = pack_cubemap(ggx_dir, ggx_output, "m", "GGX cubemap")

    # Lambertian (diffuse irradiance) cubemap, single mip
    lam_dir = temp_dir / "lambertian"
    ok = run_cmgen("Lambertian cubemap", args.lambertian_resolution,
                   [f"--ibl-irradiance={lam_dir}"]) and ok
    if ok:
        ok = pack_cubemap(lam_dir, lambertian_output, "i_", "Lambertian cubemap")

    # Sky environment cubemap: UNFILTERED base level + natural mip chain
    # (Filament's edge-aware mipmap generator; smooth progressive blur, no GGX).
    # Used with implicit-LOD sampling, so the mip chain must be a natural blur,
    # not the specular prefilter (which jumps in roughness between levels).
    sky_dir = temp_dir / "sky"
    ok = run_cmgen("Sky cubemap", args.sky_resolution,
                   mirror_flag + [f"--ibl-is-mipmap={sky_dir}"]) and ok
    if ok:
        ok = pack_cubemap(sky_dir, sky_output, "is_m", "Sky cubemap")

    # cleanup temp
    shutil.rmtree(temp_dir, ignore_errors=True)

    # final validation (best effort)
    for f in (ggx_output, lambertian_output, sky_output):
        if f.exists():
            v = subprocess.run([str(ktx), "validate", str(f)], capture_output=True, text=True)
            status = "OK" if v.returncode == 0 else "FAILED"
            print(f"  validate {f.name}: {status}")
            if v.returncode != 0:
                ok = False

    print(f"\nDone. Output folder: {output_dir}")
    print(f"  {ggx_output.name}")
    print(f"  {lambertian_output.name}")
    print(f"  {sky_output.name}")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
