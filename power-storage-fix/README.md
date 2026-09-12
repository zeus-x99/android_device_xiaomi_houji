# Unsupported storage clock scaling requests

The current houji prebuilt UFS module does not enable the clock-scaling
capability. It consequently exposes neither `clkscale_enable` nor a UFS
devfreq device. The vendor performance library falls back from the absent UFS
node to an eMMC node that also does not exist. Launch hints then report a
failed storage resource `[11, 4]`.

During houji extraction, register a fixup on the common module's
`perfboostsconfig.xml` and `perfboostselection.xml`. Remove only resource/value
pairs for opcode `0x42C10000`. Keep every other resource, timeout, enable flag,
target and hint type unchanged. Other devices invoking common extraction
directly do not receive this fixup.

The selection file matters: fixing only the base configuration still produced
the storage failure in three launch requests. With both files fixed, three
100 ms launch requests returned valid handles, released successfully and
produced no storage-node or `[11, 4]` errors in the new service's log.

Validation against OS3.0.306.0.WNCCNXM inputs removed 12 pairs from the base
file and 3 from selection overrides. XML parsing, preservation of unrelated
resource pairs, and idempotence were checked. The callback rejects malformed
resource lists and storage-only hints instead of silently creating an empty
hint. Extraction dependencies were also imported successfully.

This removes unsupported requests; it does not enable UFS clock scaling or
claim a storage throughput improvement. Reassess the fixup when intentionally
changing to a driver that supports this capability. Private device logs and
proprietary configuration files are not included here.
