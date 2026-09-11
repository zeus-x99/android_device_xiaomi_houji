# Auxiliary cameras on houji

Camera 0 is a logical main/ultrawide device. Listing its logical zoom steps
hides other back cameras in Aperture, including the separately exposed
telephoto. Enable the auxiliary selector and use IDs 0, 3 and 4; ignore ID 2,
which duplicates the main sensor, and keep the default logical-aux filter.
IDs 3 and 4 need Aperture's backward-compatible CameraX override.

| Camera | Role | Tested focal length | Aperture JPEG |
| --- | --- | --- | --- |
| 0 | Main | 6.55 mm | 4096×3072 |
| 3 | Ultrawide | 2.16 mm | 4080×3060 |
| 4 | Telephoto | 9 mm | 4080×3060 |
| 1 | Front (unchanged) | 2.83 mm | 3264×2448 |

CameraX currently computes selector labels `.7 / 1 / 2.4` from sensor metadata.
These are not manually replaced with marketing zoom ratios. Actual camera IDs,
capture-result focal lengths and JPEG EXIF were checked independently.

## Validation and limits

On OS3.0.306.0.WNCCNXM blobs, both auxiliary cameras recorded and decoded short
clips at 480p, 720p, 1080p and 2160p, each at 24/30 FPS in SDR and HLG: 32 distinct
combinations. Container timestamps were checked against the selected frame rate.
HLG files identify HEVC Main10, BT.2020 and HLG transfer, rather than merely an
HLG UI selection. Camera2 telephoto preview/JPEG capture passed twice.

The first fixed-delay script stopped one HLG recording before it had accumulated
a full test clip. Waiting for actual recording progress before measuring and
stopping resolved that test failure. Short clips do not prove long recordings,
all lighting conditions, stabilization quality or simultaneous camera use.

## Aperture lifecycle dependency

An independent Aperture crash was observed when configuration was replayed while
the camera was not IDLE. Keep the associated application fix for this baseline:

- Repository: https://github.com/zeus-x99/android_packages_apps_Aperture
- Branch: `fix/capture-rebind-20260912`
- Commit: `a2f7ab0f9d8cfab0ffecb25858a522f066467f63`
- Base: `cf1bc0a72524318d0e06ebe938a20b170f1eb878`
- Android source path: `packages/apps/Aperture`

The collector waits for IDLE before rebinding and retains only the latest
pending configuration. Recording followed by Home and return at 0.1, 1 and
6 seconds retained the same app process in device regression tests.

Aperture directly reads its generated product/vendor RRO packages. A successful
ordinary overlay lookup alone does not prove its runtime configuration changed.
Build and install the product RRO as part of the ROM; temporary bind mounts are
only a diagnostic tool and are not part of the final configuration.
