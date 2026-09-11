# houji CN camera output wrapper workaround

Tested on Xiaomi 14 (23127PN0CC), LineageOS 24.0, using
OS3.0.306.0.WNCCNXM blobs and the existing 6.1.118 prebuilt kernel.
This workaround is specific to the hashes below; other firmware needs a separate
analysis. It is not a claim of official LineageOS support or international-model validation.

## Failure and workaround

Photo-mode teardown/reconfiguration aborts in the Qualcomm camera provider with
Scudo invalid chunk state at `CamX::ChiContext::DestroyPipelineDescriptor+432`
(0x2f8bd0). Malloc-debug recorded two wrapper pointers freed twice by the same
output deletion loop. Logs associate these with MfsrPostFilter0/1 IPE5 and IPE66.
QR preview alone did not exercise the failure.

Before the deletion loop, clear later duplicate output wrapper pointers within
the same descriptor. The existing code still frees each unique wrapper.
Scudo and the input ownership checks remain enabled. This does not address
possible sharing across different descriptors.

Input SHA-256 after the existing dependency fixups:
`14c0a6ffdaba0d085d340cd1434b83b5597c76dc23899b58d929cd5d466b7900`

Output SHA-256:
`12a5430267eeda2591c5b44ae4f08fe4805c2c48e84ffd26e79610d0cd95c481`

`extract-files.py` rejects unknown hashes and accepts an already patched blob.
At 0x2f8ba4, a branch enters 92 bytes of verified zero padding at 0xef5dd0.
The RX segment file/memory sizes grow by 92 bytes without crossing 0xef6000.
The routine restores `ldr w11, [x19, #0x10]` and resumes at 0x2f8ba8.
Count is at +0x10, first wrapper pointer at +0x38, stride 0x28.
Only caller-saved x8-x16 are used; no stack accesses or calls are added.
Existing symbols and the vendor Build ID are unchanged; identify the patch by SHA-256.

## Reproduce

Supply the original blob matching the input hash above. Proprietary binaries are
not included in this directory. The helper uses the Android tree's clang-r584948b.

```sh
python3 camera-wrapper-fix/build.py --tree /path/to/android --blob /path/to/original/camera.qcom.so
adb push camera-wrapper-fix/out/dedup-test /data/local/tmp/houji-dedup-test
adb shell chmod 755 /data/local/tmp/houji-dedup-test
adb shell /data/local/tmp/houji-dedup-test
```

The native test returns 0 on success. It covers 68 cases for counts 0 through 16
with null, unique, repeated and mixed pointers, comparing all 1,024 descriptor bytes.
The helper generates a candidate library; running it does not install that library.

## Validation, 2026-09-11

- Native AArch64 test: exit 0; expected hash, idempotence and unknown-hash rejection checked.
- Normal allocator: photo preview, exit and reopen no longer reproduced the abort.
- Malloc-debug: the two duplicate frees recorded before patch were absent after patch.
- ROM rebuilt, installed without formatting and booted; partition library matched
  the output hash with no temporary bind mount. SELinux remained Enforcing.
- Rear capture saved a 4096×3072 JPEG; capture and exit produced no new fatal/Scudo crash.
- All lenses, recording and long-duration stability still need full testing.

The companion CN extraction-list changes are in `zeus-x99/device_xiaomi_sm8650-common`,
branch `fix/houji-cn-camera-20260911`. These device changes do not constitute a complete
source manifest or include the separate local Settings debugging customization.
