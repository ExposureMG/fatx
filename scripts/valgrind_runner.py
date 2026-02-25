#!/usr/bin/env python3
"""
Shared Valgrind runner for FATX test suites.
Runs each CTest test command under a chosen Valgrind tool and writes
per-test reports plus a global summary.
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

import json
import os
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime


def _run(cmd, cwd=None, timeout=0):
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            timeout=(None if timeout <= 0 else timeout),
        )
        return result.returncode, result.stdout or ""
    except subprocess.TimeoutExpired:
        return 124, "TIMEOUT"
    except Exception as exc:
        return 1, str(exc)


def _parse_valgrind_log(log_file):
    data = {
        "errors": 0,
        "summary": "",
        "has_errors": False,
        "lines": 0,
        "definitely_lost": "0",
        "indirectly_lost": "0",
        "possibly_lost": "0",
    }

    if not os.path.exists(log_file):
        return data

    try:
        with open(log_file, encoding="utf-8", errors="replace") as f:
            content = f.read()
    except OSError:
        return data

    lines = content.splitlines()
    data["lines"] = len(lines)

    summary_line = ""
    for line in lines:
        if "ERROR SUMMARY" in line:
            summary_line = line.strip()
    data["summary"] = summary_line

    m = re.search(r"ERROR SUMMARY:\s*(\d+)\s+errors?", summary_line)
    if m:
        data["errors"] = int(m.group(1))
        data["has_errors"] = data["errors"] > 0

    m = re.search(r"definitely lost:\s*([\d,]+) bytes", content)
    if m:
        data["definitely_lost"] = m.group(1)
    m = re.search(r"indirectly lost:\s*([\d,]+) bytes", content)
    if m:
        data["indirectly_lost"] = m.group(1)
    m = re.search(r"possibly lost:\s*([\d,]+) bytes", content)
    if m:
        data["possibly_lost"] = m.group(1)

    return data


def _safe_name(value):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value)[:140]


def _list_ctest_tests(build_dir, ctest_cmd):
    rc, out = _run([ctest_cmd, "--show-only=json-v1"], cwd=build_dir, timeout=120)
    if rc != 0:
        return []

    try:
        payload = json.loads(out)
    except json.JSONDecodeError:
        return []

    tests = []
    for item in payload.get("tests", []):
        name = item.get("name")
        cmd = item.get("command")
        if name and isinstance(cmd, list) and len(cmd) > 0:
            tests.append({"name": name, "command": cmd})
    return tests


def run_valgrind_suite(*, tool, tool_label, tool_args, report_dir, build_dir, ctest_cmd="ctest", jobs=None):
    build_dir = os.path.abspath(build_dir)
    report_dir = os.path.abspath(report_dir)
    tests_dir = os.path.join(report_dir, "tests")
    os.makedirs(tests_dir, exist_ok=True)

    if jobs is None or jobs <= 0:
        jobs = max(1, os.cpu_count() or 1)

    tests = _list_ctest_tests(build_dir, ctest_cmd)
    if not tests:
        report_file = os.path.join(report_dir, "report.txt")
        with open(report_file, "w", encoding="utf-8") as f:
            f.write("No CTest tests discovered.\n")
        print(f"error: no tests found (report: {report_file})")
        return 2

    print("=" * 80)
    print(f"FATX {tool_label} test report")
    print("=" * 80)
    print(f"Build dir : {build_dir}")
    print(f"Report dir: {report_dir}")
    print(f"Tests     : {len(tests)}")
    print(f"Jobs      : {jobs}")

    start = time.monotonic()

    def _run_one(index, test):
        stem = f"{index:03d}_{_safe_name(test['name'])}"
        log_file = os.path.join(tests_dir, f"{stem}.{tool}.log")
        txt_file = os.path.join(tests_dir, f"{stem}.{tool}.txt")

        cmd = ["valgrind", f"--tool={tool}"] + list(tool_args) + [f"--log-file={log_file}"] + test["command"]
        test_start = time.monotonic()
        rc, output = _run(cmd, cwd=build_dir, timeout=1800)
        elapsed = time.monotonic() - test_start
        log_data = _parse_valgrind_log(log_file)

        with open(txt_file, "w", encoding="utf-8") as f:
            f.write(f"name: {test['name']}\n")
            f.write(f"tool: {tool}\n")
            f.write(f"test_rc: {rc}\n")
            f.write(f"errors: {log_data['errors']}\n")
            f.write(f"summary: {log_data['summary']}\n")
            f.write(f"time_s: {elapsed:.3f}\n")
            f.write(f"log: {log_file}\n")
            if output.strip():
                f.write("\noutput:\n")
                f.write(output)

        return {
            "name": test["name"],
            "index": index,
            "test_rc": rc,
            "errors": log_data["errors"],
            "summary": log_data["summary"],
            "time_s": elapsed,
            "log": log_file,
            "txt": txt_file,
            "mem": log_data,
        }

    results = []
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(_run_one, idx, test) for idx, test in enumerate(tests, start=1)]
        for future in as_completed(futures):
            results.append(future.result())

    results.sort(key=lambda item: item["index"])

    elapsed_total = time.monotonic() - start
    failed_tests = [r for r in results if r["test_rc"] != 0]
    valgrind_errors = [r for r in results if r["errors"] > 0]
    total_errors = sum(r["errors"] for r in results)

    slowest = sorted(results, key=lambda item: item["time_s"], reverse=True)[:10]

    report_file = os.path.join(report_dir, "report.txt")
    with open(report_file, "w", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write(f"FATX {tool_label.upper()} REPORT\n")
        f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write("=" * 80 + "\n\n")

        f.write("SUMMARY\n")
        f.write("-" * 80 + "\n")
        f.write(f"Build dir: {build_dir}\n")
        f.write(f"Report dir: {report_dir}\n")
        f.write(f"Tool: {tool}\n")
        f.write(f"Tests discovered: {len(tests)}\n")
        f.write(f"Parallel jobs: {jobs}\n")
        f.write(f"Elapsed total: {elapsed_total:.2f} s\n")
        f.write(f"Failed tests: {len(failed_tests)}\n")
        f.write(f"Tests with Valgrind errors: {len(valgrind_errors)}\n")
        f.write(f"Total Valgrind errors: {total_errors}\n")
        status_ok = (len(failed_tests) == 0 and total_errors == 0)
        f.write(f"Status: {'✓ OK' if status_ok else '✗ ISSUES DETECTED'}\n\n")

        f.write("SLOWEST TESTS\n")
        f.write("=" * 80 + "\n")
        for item in slowest:
            f.write(f"- {item['name']}: {item['time_s']:.3f} s (errors={item['errors']}, rc={item['test_rc']})\n")
        f.write("\n")

        f.write("FAILING TESTS\n")
        f.write("=" * 80 + "\n")
        if not failed_tests:
            f.write("No failing tests.\n\n")
        else:
            for item in failed_tests:
                f.write(f"- {item['name']} (rc={item['test_rc']})\n")
            f.write("\n")

        f.write("TESTS WITH VALGRIND ERRORS\n")
        f.write("=" * 80 + "\n")
        if not valgrind_errors:
            f.write("No Valgrind errors.\n\n")
        else:
            for item in valgrind_errors:
                f.write(f"- {item['name']}: errors={item['errors']}\n")
                if item["summary"]:
                    f.write(f"  {item['summary']}\n")
            f.write("\n")

        if tool == "memcheck":
            f.write("LEAK SUMMARY (memcheck)\n")
            f.write("=" * 80 + "\n")
            for item in valgrind_errors:
                mem = item["mem"]
                f.write(
                    f"- {item['name']}: definitely={mem['definitely_lost']}B, "
                    f"indirectly={mem['indirectly_lost']}B, possibly={mem['possibly_lost']}B\n"
                )
            f.write("\n")

        f.write("ARTIFACTS\n")
        f.write("-" * 80 + "\n")
        f.write(f"Per-test reports dir: {tests_dir}\n")
        f.write(f"Global report: {report_file}\n")

    print(f"Report: {report_file}")
    print(f"Per-test reports: {tests_dir}")
    print(f"Elapsed: {elapsed_total:.2f} s")
    print("=" * 80)

    return 0 if (len(failed_tests) == 0 and total_errors == 0) else 1
