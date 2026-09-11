# Aperture lens configuration

Camera 0 on the tested houji China firmware exposes physical cameras 2 (main,
6.55 mm) and 3 (ultrawide, 2.16 mm). Add its 0.6 zoom ratio through the product
resource overlay; Aperture adds 1.0 itself. Auxiliary-camera selection stays at
the application default.

Validated on 23127PN0CC with OS3.0.306.0.WNCCNXM blobs and LineageOS 24.0:

- Camera2 preview at 1.0 / 0.6 / 1.0 / 0.6 / 1.0. After one second of settling,
  each stage returned 99–100 results with the expected active physical ID.
- Aperture exposes the 0.6 selector. Both ultrawide and main JPEGs decode at
  4096 x 3072; EXIF focal lengths are 2.16 and 6.55 mm respectively.
- An 18-second ultrawide 4K recording saves and decodes a video frame.

This configuration does not expose or validate the telephoto camera. Do not
interpret digital zoom on camera 0 as proof of telephoto operation. Other
firmware baselines and camera modes need their own validation.
