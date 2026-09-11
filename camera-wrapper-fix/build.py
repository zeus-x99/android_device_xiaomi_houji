#!/usr/bin/env python3
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess

parser = argparse.ArgumentParser(description='Reproduce the houji camera workaround and native test')
parser.add_argument('--tree', type=Path, required=True, help='Android source root')
parser.add_argument('--blob', type=Path, required=True, help='Original blob after existing fixups, before deduplication')
args = parser.parse_args()
base = Path(__file__).resolve().parent
tree = args.tree.resolve()
clang = tree / 'prebuilts/clang/host/linux-x86/clang-r584948b/bin'
out = base / 'out'
out.mkdir(exist_ok=True)
source = args.blob
original = source.read_bytes()
expected = '14c0a6ffdaba0d085d340cd1434b83b5597c76dc23899b58d929cd5d466b7900'
assert hashlib.sha256(original).hexdigest() == expected, 'Unrecognized camera blob'
site, resume, cave = 0x2f8ba4, 0x2f8ba8, 0xef5dd0
assert original[site:site + 8] == bytes.fromhex('6b1240b9cb010034')
assert not any(original[cave:0xef6000]), 'Code padding is not empty'

def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)

run(clang / 'clang', '--target=aarch64-linux-android35', '-c', base / 'dedup.S', '-o', out / 'dedup.o')
(out / 'stub.ld').write_text('SECTIONS { . = 0xef5dd0; .text.fix : { *(.text.fix) } }\n')
run(clang / 'ld.lld', '-T', out / 'stub.ld', '--entry=houji_dedup_outputs',
    '--defsym=houji_original_resume=0x2f8ba8', out / 'dedup.o', '-o', out / 'stub.elf')
run(clang / 'llvm-objcopy', '-O', 'binary', '--only-section=.text.fix', out / 'stub.elf', out / 'stub.bin')
stub = (out / 'stub.bin').read_bytes()
assert 0 < len(stub) <= 0xef6000 - cave
patched = bytearray(original)
patched[cave:cave + len(stub)] = stub
delta = cave - site
assert delta % 4 == 0 and -(1 << 27) <= delta < (1 << 27)
struct.pack_into('<I', patched, site, 0x14000000 | ((delta // 4) & 0x3ffffff))
phoff = struct.unpack_from('<Q', original, 32)[0]
phentsize, phnum = struct.unpack_from('<HH', original, 54)
assert phentsize == 56
changed_header = None
for index in range(phnum):
    pos = phoff + index * phentsize
    kind, flags, offset, va, pa, filesz, memsz, align = struct.unpack_from('<II6Q', original, pos)
    if kind == 1 and flags == 5 and offset == va == 0x294000:
        assert offset + filesz == va + memsz == cave
        struct.pack_into('<QQ', patched, pos + 32, filesz + len(stub), memsz + len(stub))
        changed_header = pos
assert changed_header is not None
allowed = set(range(site, site + 4)) | set(range(cave, cave + len(stub))) | set(range(changed_header + 32, changed_header + 48))
assert len(patched) == len(original)
assert all(i in allowed for i, (a, b) in enumerate(zip(original, patched)) if a != b)
(out / 'camera.qcom.so').write_bytes(patched)

run(clang / 'clang', '--target=aarch64-linux-android35', '-ffreestanding', '-fno-builtin',
    '-fno-stack-protector', '-fno-pic', '-O2', '-c', base / 'test.c', '-o', out / 'test.o')
run(clang / 'clang', '--target=aarch64-linux-android35', '-c', base / 'test-entry.S', '-o', out / 'test-entry.o')
(out / 'test.ld').write_text('''SECTIONS {
    . = 0x10000; .text : { *(.text) *(.text._start) }
    . = ALIGN(0x1000); .rodata : { *(.rodata*) }
    . = ALIGN(0x1000); .data : { *(.data*) } .bss : { *(.bss*) }
    . = 0x2f8ba8; .text.resume : { *(.text.resume) }
    . = 0xef5dd0; .text.fix : { *(.text.fix) }
}''')
run(clang / 'ld.lld', '-z', 'max-page-size=4096', '-T', out / 'test.ld', '--entry=_start', out / 'test.o',
    out / 'test-entry.o', out / 'dedup.o', '-o', out / 'dedup-test')
print('stub bytes:', len(stub))
print('original sha256:', expected)
print('patched sha256:', hashlib.sha256(patched).hexdigest())
