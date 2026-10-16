#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Install the exact official Android Clang content, ignoring tar timestamps."""
import argparse
import sys
sys.dont_write_bytecode = True
import json
from pathlib import Path
from toolchain_integrity import prepare_toolchain

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path(__file__).resolve().parents[3] / '.toolchains/android-16-r547379')
    args = parser.parse_args()
    pins = json.loads(Path(__file__).with_name('toolchain-pins.json').read_text())
    print(prepare_toolchain(args.directory.resolve(), pins))
