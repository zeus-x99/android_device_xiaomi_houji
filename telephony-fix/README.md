# Pending outgoing-call cancellation

With OS3.0.306.0.WNCCNXM radio blobs, a GSM DIAL can remain outstanding when
4G calling is disabled. The original tracker waits for a call index before
sending hangup, so Telecom eventually returns from DISCONNECTING to DIALING.

This device enables `ro.vendor.radio.hangup_pending_mo`. It requires the
framework change below; the property alone does not fix the issue.

- Repository: https://github.com/zeus-x99/android_frameworks_opt_telephony
- Branch: `fix/houji-pending-call-20260912`
- Commit: `2c9fa3bb5e749ad2a0a5e2dbdc09f918afe048ed`
- Path: `frameworks/opt/telephony`
- Base: `2fa698c859a26c2f0d79b089aa5aee335c365f1a`

For this LineageOS 24.0 baseline, a local manifest may replace the upstream
project with the pinned fork (first preserve any local framework edits):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote name="houji-call-fix" fetch="https://github.com/" />
  <remove-project name="LineageOS/android_frameworks_opt_telephony" />
  <project path="frameworks/opt/telephony"
           name="zeus-x99/android_frameworks_opt_telephony"
           remote="houji-call-fix"
           revision="2c9fa3bb5e749ad2a0a5e2dbdc09f918afe048ed" />
</manifest>
```

The framework sends one foreground cancellation for a pending ordinary GSM
call only when no background or ringing call exists. Emergency calls and
other devices retain the original behavior. It does not fabricate a local
idle state or restart the radio.

## Validation

On houji, normal Dialer hangup sent HANGUP_FOREGROUND_RESUME_BACKGROUND,
received success, and released the pending DIAL with INVALID_MODEM_STATE.
Telecom reported LOCAL disconnect; both slots returned to IDLE with zero
pending operations. No airplane-mode toggle or external HAL call was used.
IMS self-call cancellation also passed after restoring 4G calling.

The two regression tests in FrameworksTelephonyTests passed using
`am instrument --no-hidden-api-checks`. They simulate delayed responses,
check duplicate hangup handling, and verify the disabled default behavior.
These checks do not validate answered-call audio, incoming calls, emergency
calling, or every carrier/network transition. Keep testing those separately.
