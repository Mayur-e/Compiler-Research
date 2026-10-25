#!/usr/bin/env python3
"""Fetches real GCC testsuite C files directly from the GCC GitHub mirror into benchmarks/gcc/gcc/testsuite/."""

import urllib.request
import json
from pathlib import Path

DEST_DIR = Path("benchmarks/gcc/gcc/testsuite/gcc.c-torture/execute")
DEST_DIR.mkdir(parents=True, exist_ok=True)

def fetch_tests():
    print("[*] Querying GitHub API for GCC c-torture execute tests...")
    req = urllib.request.Request(
        "https://api.github.com/repos/gcc-mirror/gcc/contents/gcc/testsuite/gcc.c-torture/execute",
        headers={"User-Agent": "MDE-HRA-Research/1.0"}
    )
    
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            items = json.loads(resp.read().decode('utf-8'))
    except Exception as e:
        print(f"[-] GitHub API error: {e}")
        return

    # Select representative pointer, alias, struct, and general torture tests
    selected = []
    keywords = ['alias', 'ptr', 'struct', 'node', 'list', 'mem', '2000', '2001', '2002', '99']
    
    for item in items:
        name = item['name']
        if not name.endswith('.c'):
            continue
        if any(k in name for k in keywords):
            selected.append(item)
            if len(selected) >= 50:  # Download a solid 50-file corpus
                break

    print(f"[*] Downloading {len(selected)} real GCC testsuite benchmark files...")
    downloaded = 0
    for item in selected:
        name = item['name']
        raw_url = item['download_url']
        dest_file = DEST_DIR / name
        try:
            with urllib.request.urlopen(raw_url, timeout=10) as resp:
                content = resp.read()
                dest_file.write_bytes(content)
                downloaded += 1
                print(f"  + [{downloaded}/{len(selected)}] {name} ({len(content)} bytes)")
        except Exception as e:
            print(f"  - Failed {name}: {e}")

    print(f"[+] Successfully saved {downloaded} GCC testsuite files to {DEST_DIR}")

if __name__ == '__main__':
    fetch_tests()
