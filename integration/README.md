# houji experimental LineageOS 24 integration snapshot

This source snapshot records the changes used to build the booted Android 17 ROM
`1789213062` on 2026-09-12. It is an unofficial development build, not a claim of
LineageOS device approval or complete hardware validation.

## Source layout

- `manifest.xml` pins the synced Android projects and the five device repair forks.
  The houji revision intentionally points to the runtime source before this
  documentation commit, so the manifest does not need a self-referential hash.
- `projects.json` and `patches/` preserve additional changes to frameworks/base,
  Settings, build/make and build/soong against their exact original commits.
- `PORT-SOURCES.md` credits the upstream settings ports and documents adaptations.
- `experimental-kernel/` contains the separate 6.1.130 source build experiments.
  Those changes are not applied by the ROM script and were not flashed. The
  working ROM continues to use the pinned existing 6.1.118 kernel.

The forked device/common/hardware/telephony/Aperture commits preserve CN blob
extraction fixups, camera and codec workarounds, FOD display synchronization,
PowerHAL storage cleanup and QSync backlight timing. Proprietary binaries and
personal test data are not included.

## Reconstruct the source snapshot

Keep a separate copy of this integration directory before syncing, because the
manifest pins houji to the earlier runtime-source commit.

1. Initialize a fresh LineageOS repo workspace with the usual LineageOS manifest
   tooling. Use the supplied manifest as `.repo/manifests/houji-snapshot.xml`,
   select it with `repo init -m houji-snapshot.xml`, then run `repo sync`.
   Do not use this to overwrite a workspace containing local changes.
2. Supply the proprietary vendor trees with the normal extract-utils workflow
   using the checked-out device and common extraction scripts. The CN input used
   here was `houji-ota_full-OS3.0.306.0.WNCCNXM-user-16.0-db6e778102.zip`, SHA256
   `7016ec0d641e0dc0c55de3672b531a0a02540581a309d3504da9b1679a96a159`.
   Vendor trees generated from this firmware are not distributed here.
3. Run `python3 /path/to/integration/apply-patches.py /path/to/android` first.
   It checks all four project bases and refuses dirty worktrees. Add `--apply`
   only after checks pass; patches will be staged for inspection. Do not also
   apply older standalone framework or Settings patches on top.
4. From the source root, build using the standard environment:

   ```bash
   export GOMEMLIMIT=18GiB GOGC=50
   export SOONG_NINJA=ninja NINJA_HIGHMEM_NUM_JOBS=1
   source build/envsetup.sh
   breakfast houji userdebug
   m -j16 bacon
   ```

This records source versions and build changes, not a proven bit-for-bit
reproducible release. Toolchain downloads, extraction prerequisites and available
disk/RAM remain necessary. A clean end-to-end rebuild from this published
manifest has not yet been run.

## Framework and build changes

- Framework: optical fingerprint timing/brightness parameters and launcher sleep
  interface compatibility, plus battery styles/percentages, network traffic,
  clock placement/AM-PM/home hiding, Compose QS brightness options and carrier
  padding after clock width changes.
- Settings: backup transport selection and the existing, permission-protected
  local ADB root toggle activity. The latter is a development helper; launching
  it alone does not enable root.
- build/make: recovery touch rotation export, missing/duplicate public snapshot
  module handling, and the negative inode-count automatic allocation case.
- build/soong: forwarding the configured Go memory/GC environment to bootstrap.

## Validation and remaining work

Production SystemUI and the complete ROM built successfully. The ROM booted;
targeted status bar/QS settings, FOD timing and QSync tests are recorded locally.
This does not mean all features were retested together on the latest build.

The multivalent BatteryRepository test still uses the old percentage-setting
interface and needs migration. No full unit-test suite or CTS pass is claimed.
Volume-key/edge-gesture setting consumers, several other settings, rotated and
large-font layouts, multiuser, real call audio, outdoor GNSS, NFC tags, deep sleep,
charging/thermal behavior and an occasional PowerHAL request failure remain open.
The previously reported missing battery text at startup was a screenshot-reading
error; the original image already contained the percentage.

MindTheGapps 17 ARM64 was installed separately after an explicitly authorized
factory reset. Its applicable 37 files matched the release hashes, the Google
services processes started and Play Store opened its login page. Account sign-in
and downloads were not tested. No GApps binaries are included in this snapshot.

Published code retains the original copyright notices. Do not interpret inclusion
of experimental patches as endorsement for flashing unmatched kernel/modules.
