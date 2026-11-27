#!/usr/bin/env python3
"""Build a private enrollment profile from a verbose startup and candump log."""
import argparse
import json
from pathlib import Path
import re

def build(startup, identity_trace):
    text = startup.read_text()
    match = re.search(r'1FB00000\s+\[8\]\s+((?:[0-9A-F]{2} ){7}[0-9A-F]{2})', text)
    if not match:
        raise ValueError("Startup trace has no slot-0 identity")
    identity = bytes.fromhex(match[1])
    own_word = int.from_bytes(identity[:2], 'little')
    peers = {int(word, 16) for word in re.findall(r'can0\s+1F93([0-9A-F]{4})\s+\[0\]', text)}
    peers.discard(own_word)
    if len(peers) != 4:
        raise ValueError("This captured-system profile requires four other identity words")
    identities = {}
    tables = []
    for line in text.splitlines():
        identity_row = re.search(r'1FB0000([0-4])\s+\[8\]\s+((?:[0-9A-F]{2} ){7}[0-9A-F]{2})', line)
        if identity_row:
            identities[int(identity_row[1])] = bytes.fromhex(identity_row[2]).hex()
        row = re.match(r'\s*\(([0-9.]+)\)\s+can0\s+(1EC[0-4][0-9A-F]{4})\s+\[8\]\s+((?:[0-9A-F]{2} ){7}[0-9A-F]{2})', line)
        if row:
            tables.append({"ms": round(float(row[1])*1000), "id": row[2], "data": bytes.fromhex(row[3]).hex()})
    if len(identities) != 5 or not tables or tables[-1]["id"] != "1EC4FFF0":
        raise ValueError("Startup trace has no complete five-slot startup tables")
    burst = {}
    for line in identity_trace.read_text().splitlines():
        match = re.match(r'^\(([0-9.]+)\)\s+\w+\s+(1F[0-9A-F]{6})#$', line)
        if not match:
            continue
        stamp, identifier = float(match[1]), int(match[2], 16)
        sequence, slot = (identifier >> 20) & 15, (identifier >> 16) & 15
        if slot != 0 or sequence > 7:
            continue
        if sequence == 0:
            burst = {}
        burst[sequence] = (stamp, (identifier >> 8) & 255, identifier & 255)
        if sequence == 7 and len(burst) == 8:
            if stamp - burst[0][0] > 0.1:
                continue
            if bytes(burst[i][2] for i in range(8)) == identity:
                return {
                    "identity": identity.hex(),
                    "announcement_keys": bytes(burst[i][1] for i in range(8)).hex(),
                    "other_high_words": sorted(peers),
                    "identities": [identities[i] for i in range(5)],
                    "startup_tables": tables,
                }
    raise ValueError("No complete matching identity burst in identity trace")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("startup", type=Path)
    parser.add_argument("identity_trace", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        profile = build(args.startup, args.identity_trace)
        if args.output.exists():
            if json.loads(args.output.read_text()) != profile:
                raise ValueError("Refusing to overwrite a different enrollment profile")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            with args.output.open("x") as output:
                args.output.chmod(0o600)
                output.write(json.dumps(profile, indent=2) + "\n")
        print("Enrollment profile verified:", args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + "\n")

if __name__ == "__main__":
    main()
