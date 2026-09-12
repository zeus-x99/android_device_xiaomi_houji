# Houji QSync timer adapter

This restores the legacy DisplayEffect backlight timer request using a private
Binder endpoint in the existing display composer. The device product includes
only libhouji_qsync_runtime and libhouji_qsync_client.
The fake SDM libraries and test binaries are manual test targets, not ROM packages.

Runtime.cpp is pinned to libsdmcore build ID 30713b1ff3e1c13e860a4d7c38282820.
The original implementation must support internal mode 4 and the checked startup
bindings. Do not update the build ID without rechecking the ABI and semantics.

QsyncServer waits for both display initialization and the original NDK Binder
thread-pool startup. It observes ABinderProcess_startThreadPool without changing
the composer's configured thread count. Both original HALs run with AT_SECURE=1,
which makes Bionic ignore LD_PRELOAD. Houji's extract-files.py adds DT_NEEDED
dependencies, and the libraries use DF_1_GLOBAL for symbol interposition.
The original init service options and secure-execution mode remain unchanged.

The logical Binder display ID is 0. SDM identifies the primary panel by its DRM
connector ID (67 on the tested phone), so attachment uses IsPrimaryDisplay()
rather than comparing the SDM ID with zero. Secondary built-in displays are ignored.

The controller runs under the original SDM locks. Its worker releases scheduling
locks before entering SDM. New mode/power requests cancel stale deadlines, and
destruction waits for in-flight leases before removing the routing entry.
Restore failures remain errors; owned backlight overrides are cleared with at
most eight attempts, with 250 ms between retries. Exhaustion is reported, not
treated as successful QSync restoration.

Host sanitizer tests, Android fake-SDM tests, cross-process Binder reset/expiry
tests and an early-registration negative test have passed. A real HAL trial with
the primary-display fix also verified requests from MiBrightness, reset/expiry,
new-mode and power cancellation under SELinux Enforcing and AT_SECURE=1.
In the panel's QSync timing (HWC mode 4), hardware QSync changed 1 -> 0 -> 1
across a backlight notification and timer expiry. The ordinary 24 Hz timing
does not demonstrate hardware QSync enablement. The final packaged build
1789208069 was installed and verified with no temporary mounts: service startup,
real brightness requests and the five-second expiry passed using the same
runtime library hash as the hardware trial. Root, Enforcing and kernel 6.1.118
were retained. Diagnostic files and temporary display policies were removed.

Manual tests create files in the product output; move test binaries and fake
libraries out of vendor/bin and vendor/lib64 before building a ROM image.
