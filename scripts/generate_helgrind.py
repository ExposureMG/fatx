#!/usr/bin/env python3
"""
FATX Helgrind report generator.
Runs each CTest test under Valgrind helgrind and generates per-test reports.
"""
#
#   FATX filesystem support (Xbox 360)
#
#   Copyright (C) 2012-2026 Christophe Duverger
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, version 3 of the License.
#

import argparse
import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from valgrind_runner import run_valgrind_suite


def main():
    parser = argparse.ArgumentParser(description="FATX helgrind report generator")
    parser.add_argument("--build-dir", required=True, help="CMake build directory")
    parser.add_argument("--binary", help="Deprecated: kept for compatibility")
    parser.add_argument("--ctest", default="ctest", help="CTest command")
    parser.add_argument("--jobs", type=int, default=0, help="Parallel jobs (default: cpu count)")
    args = parser.parse_args()

    report_dir = os.path.join(args.build_dir, "helgrind_report")
    return run_valgrind_suite(
        tool="helgrind",
        tool_label="Helgrind",
        tool_args=["--history-level=full"],
        report_dir=report_dir,
        build_dir=args.build_dir,
        ctest_cmd=args.ctest,
        jobs=args.jobs,
    )


if __name__ == "__main__":
    sys.exit(main())
