#!/usr/bin/env python3
"""Compile houji DT components and check overlay composition (not flash images)."""

import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path)
    parser.add_argument("tools", type=Path)
    args = parser.parse_args()
    work = args.work.resolve()
    kernel = work / "kernel/sm8650"
    dt = work / "kernel/sm8650-devicetrees"
    modules = work / "kernel/sm8650-modules"
    out = work / "out/devicetrees"
    out.mkdir(parents=True, exist_ok=True)
    clang = args.tools.resolve() / "prebuilts/clang/host/linux-x86/clang-r487747c/bin/clang"
    dtc = work / "out/kernel/scripts/dtc/dtc"
    overlay = work / "out/kernel/scripts/dtc/fdtoverlay"
    includes = [kernel / "include", kernel / "scripts/dtc/include-prefixes", dt / "qcom",
                modules / "qcom/opensource/audio-kernel/include", modules / "qcom/opensource/camera-kernel"]
    inputs = ["qcom/pineapple.dts", "qcom/pineapple-v2.dts", "qcom/houji-sm8650-overlay.dts",
              "qcom/audio/pineapple-audio.dts", "qcom/audio/houji-audio-mtp.dts",
              "qcom/camera/pineapple-camera.dts", "qcom/camera/pineapple-camera-v2.dts",
              "qcom/camera/houji-sm8650-camera-sensor.dts", "qcom/display/display/pineapple-sde.dts",
              "qcom/display/display/houji-sde-display-mtp-overlay.dts"]
    results = []

    def run(cmd, name):
        result = subprocess.run([str(x) for x in cmd], text=True, capture_output=True)
        (out / (name + ".log")).write_text(result.stdout + result.stderr)
        results.append({"step": name, "command": [str(x) for x in cmd], "exit": result.returncode})
        (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(f"{name} failed: see {out / (name + '.log')}")

    for rel in inputs:
        src = dt / rel
        name = src.stem
        pp = out / (name + ".pp.dts")
        suffix = ".dtb" if name in ("pineapple", "pineapple-v2") else ".dtbo"
        cmd = [clang, "-E", "-P", "-nostdinc", "-undef", "-D__DTS__", "-x", "assembler-with-cpp"]
        for include in includes:
            cmd.extend(["-I", include])
        run(cmd + [src, "-o", pp], name + ".cpp")
        run([dtc, "-@", "-I", "dts", "-O", "dtb", "-o", out / (name + suffix), pp], name + ".dtc")

    for base in ("pineapple", "pineapple-v2"):
        camera = "pineapple-camera-v2" if base.endswith("v2") else "pineapple-camera"
        names = ["pineapple-audio", "pineapple-sde", camera, "houji-sm8650-overlay",
                 "houji-audio-mtp", "houji-sde-display-mtp-overlay", "houji-sm8650-camera-sensor"]
        run([overlay, "-i", out / (base + ".dtb"), "-o", out / ("combined-" + base + ".dtb")]
            + [out / (name + ".dtbo") for name in names], base + ".combine")
    print("Compiled 10 DT inputs; both component-composition checks passed. Not flash images.")


if __name__ == "__main__":
    main()
