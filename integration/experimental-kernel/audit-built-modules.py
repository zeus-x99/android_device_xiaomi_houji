#!/usr/bin/env python3
"""Audit built modules against generated symbol tables; not a boot test."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--wifi", choices=["kiwi_v2", "qca6750"], required=True)
args = parser.parse_args()
excluded = ".qca6750" if args.wifi == "kiwi_v2" else ".kiwi_v2"
work = Path(__file__).resolve().parent
out = work / "out"
provided = {}
conflicts = []
for table in [out / "kernel/Module.symvers", *sorted((out / "sm8650-modules").rglob("Module.symvers"))]:
    if excluded in table.parts:
        continue
    for line in table.read_text().splitlines():
        fields = line.split()
        if len(fields) < 3:
            continue
        crc, name = int(fields[0], 16), fields[1]
        if name in provided and provided[name] != crc:
            conflicts.append({"symbol": name, "table": str(table)})
        provided[name] = crc
modules = []
for path in sorted(out.rglob("*.ko")):
    if excluded in path.parts:
        continue
    result = subprocess.run(["modprobe", "--show-modversions", str(path)], check=True, text=True, capture_output=True)
    missing, mismatch = [], []
    for line in result.stdout.splitlines():
        crc, name = line.split()
        if name not in provided:
            missing.append(name)
        elif provided[name] != int(crc, 16):
            mismatch.append(name)
    def info(field):
        return subprocess.run(["modinfo", "-F", field, str(path)], check=True, text=True, capture_output=True).stdout.strip()
    modules.append({"path": str(path.relative_to(out)), "name": info("name"),
                    "vermagic": info("vermagic"), "depends": info("depends"),
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "missing": missing, "crc_mismatch": mismatch})
report = {"wifi_profile": args.wifi, "module_count": len(modules), "symbol_conflicts": conflicts, "modules": modules}
(work / f"out/module-audit-{args.wifi}.json").write_text(json.dumps(report, indent=2) + "\n")
failed = sum(bool(m["missing"] or m["crc_mismatch"]) for m in modules)
print(f"Modules: {len(modules)}; failing imports: {failed}; conflicting export CRCs: {len(conflicts)}")
raise SystemExit(bool(failed or conflicts))
