# SPDX-License-Identifier: Apache-2.0

import re
import xml.etree.ElementTree as ET


def remove_storage_clock_scaling(data: str) -> str:
    """Remove unsupported storage scaling requests from houji's perf config."""
    ET.fromstring(data)

    def fix_resources(match):
        resources = match.group(1)
        if not re.search(r'\b0x42c10000\b', resources, re.IGNORECASE):
            return match.group(0)
        tokens = [value.strip() for value in resources.split(',')]
        if len(tokens) % 2:
            raise ValueError('Malformed performance resource/value list')
        pairs = [(int(tokens[i], 0), tokens[i + 1])
                 for i in range(0, len(tokens), 2)]
        if all(opcode == 0x42C10000 for opcode, _ in pairs):
            raise ValueError('Storage-only hint needs explicit handling')
        # Keep formatting and every unrelated resource/value pair intact.
        resources = re.sub(
            r'\b0x42c10000\s*,\s*(?:0x[0-9a-f]+|[0-9]+)\s*,\s*',
            '', resources, flags=re.IGNORECASE,
        )
        resources = re.sub(
            r',\s*0x42c10000\s*,\s*(?:0x[0-9a-f]+|[0-9]+)\s*$',
            '', resources, flags=re.IGNORECASE,
        )
        remaining = [value.strip() for value in resources.split(',')]
        actual = [(int(remaining[i], 0), remaining[i + 1])
                  for i in range(0, len(remaining), 2)]
        if actual != [(op, value) for op, value in pairs if op != 0x42C10000]:
            raise ValueError('Unexpected change to performance resources')
        return f'Resources="{resources}"'

    result = re.sub(r'Resources="([^"]*)"', fix_resources, data)
    ET.fromstring(result)
    return result


def blob_fixup_perf_storage(ctx, file, file_path, *args, **kwargs):
    with open(file_path, encoding='utf-8') as stream:
        original = stream.read()
    fixed = remove_storage_clock_scaling(original)
    if fixed != original:
        with open(file_path, 'w', encoding='utf-8') as stream:
            stream.write(fixed)
