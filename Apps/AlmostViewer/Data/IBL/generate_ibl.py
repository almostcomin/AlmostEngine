#!/usr/bin/env python3
"""Generate GGX and Lambertian IBL cubemaps from an HDR environment.

Usage:
    py generate_ibl.py path/to/environment.hdr
    py generate_ibl.py path/to/environment.hdr --output-path path/to/output

Requires gltf-ibl-sampler.exe to be located next to this script (or
available as a command on PATH as a fallback).
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate GGX and Lambertian KTX2 cubemaps from an HDR image."
    )
    parser.add_argument(
        "texture",
        type=Path,
        help="Input environment texture (.hdr or .exr).",
    )
    parser.add_argument(
        "-o", "--output-path",
        type=Path,
        default=None,
        help="Output directory (default: same directory as the input texture).",
    )
    parser.add_argument(
        "--lambertian-resolution",
        type=int,
        default=128,
        help="Lambertian cubemap face resolution (default: 128).",
    )
    parser.add_argument(
        "--ggx-resolution",
        type=int,
        default=None,
        help="Optional GGX cubemap face resolution. If omitted, the tool chooses it automatically.",
    )
    parser.add_argument(
        "--sky-cubemap",
        action="store_true",
        help="Also generate an unfiltered cubemap of the environment (usable as skybox).",
    )
    parser.add_argument(
        "--sky-resolution",
        type=int,
        default=1024,
        help="Face resolution of the sky cubemap (default: 1024).",
    )
    args = parser.parse_args()

    input_path = args.texture.expanduser().resolve()
    if not input_path.is_file():
        parser.error(f"Input texture does not exist: {input_path}")

    if args.lambertian_resolution < 1:
        parser.error("--lambertian-resolution must be greater than zero.")
    if args.ggx_resolution is not None and args.ggx_resolution < 1:
        parser.error("--ggx-resolution must be greater than zero.")
    if args.sky_resolution < 1:
        parser.error("--sky-resolution must be greater than zero.")

    sampler = Path(__file__).resolve().parent / "gltf-ibl-sampler.exe"
    if not sampler.is_file():
        sampler = shutil.which("gltf-ibl-sampler")
        if sampler is None:
            print(
                "Error: could not find 'gltf-ibl-sampler.exe' next to this "
                "script nor 'gltf-ibl-sampler' on PATH.",
                file=sys.stderr,
            )
            return 1

    name = input_path.stem
    output_dir = (args.output_path.expanduser().resolve()
                  if args.output_path is not None
                  else input_path.parent)
    output_dir.mkdir(parents=True, exist_ok=True)

    # The sampler writes temporary files (outputLUT.png, pipeline.cache) into
    # its working directory. Use a temp folder inside the output dir and
    # clean it up afterwards so nothing untracked is left behind.
    temp_dir = output_dir / "_temp"
    temp_dir.mkdir(parents=True, exist_ok=True)

    ggx_output = output_dir / f"{name}_GGX.ktx2"
    ggx_lut_output = output_dir / f"{name}_GGX_LUT.ktx2"
    lambertian_output = output_dir / f"{name}_Lambertian.ktx2"
    sky_output = output_dir / f"{name}_Sky.ktx2"

    common = [
        sampler,
        "-inputPath",
        str(input_path),
        "-targetFormat",
        "R16G16B16A16_SFLOAT",
    ]

    ggx_command = common + [
        "-outCubeMap",
        str(ggx_output),
        "-outLUT",
        str(ggx_lut_output),
        "-distribution",
        "GGX",
    ]
    if args.ggx_resolution is not None:
        ggx_command += ["-cubeMapResolution", str(args.ggx_resolution)]

    lambertian_command = common + [
        "-outCubeMap",
        str(lambertian_output),
        "-distribution",
        "Lambertian",
        "-cubeMapResolution",
        str(args.lambertian_resolution),
    ]

    sky_command = (
        common
        + [
            "-outCubeMap",
            str(sky_output),
            "-distribution",
            "GGX",
            "-sampleCount",
            "1",
            "-cubeMapResolution",
            str(args.sky_resolution),
        ]
        if args.sky_cubemap
        else None
    )

    passes = (
        (("GGX", ggx_command), ("Lambertian", lambertian_command))
        + ((("Sky", sky_command),) if sky_command is not None else ())
    )
    for label, command in passes:
        print(f"\nGenerating {label} cubemap...")
        print(" ".join(f'"{p}"' if " " in str(p) else str(p) for p in command))
        result = subprocess.run(command, check=False, cwd=temp_dir)
        if result.returncode != 0:
            print(
                f"\nError: {label} generation failed (exit code {result.returncode}).",
                file=sys.stderr,
            )
            return result.returncode or 1

    for temp_file in (
        temp_dir / "outputLUT.png",
        temp_dir / "pipeline.cache",
    ):
        try:
            temp_file.unlink(missing_ok=True)
        except OSError:
            pass
    try:
        temp_dir.rmdir()
    except OSError:
        pass

    print(f"\nDone. Output folder: {output_dir}")
    print(f"  {ggx_output.name}")
    print(f"  {ggx_lut_output.name}")
    print(f"  {lambertian_output.name}")
    if args.sky_cubemap:
        print(f"  {sky_output.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
