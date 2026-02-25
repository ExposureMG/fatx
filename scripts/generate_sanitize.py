#!/usr/bin/env python3
"""
FATX Sanitize Report Script
Parses ASAN/UBSAN logs and generates a unified detailed report.
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
import glob
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from datetime import datetime


def read_text(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def parse_asan(log_content):
    incidents = []

    chunks = re.split(r"(?=^==\d+==ERROR: AddressSanitizer:)", log_content, flags=re.M)
    for chunk in chunks:
        chunk = chunk.strip()
        if not chunk.startswith("=="):
            continue

        first = chunk.splitlines()[0] if chunk.splitlines() else ""
        m_kind = re.search(r"ERROR: AddressSanitizer: ([^\n]+)", first)
        kind = m_kind.group(1).strip() if m_kind else "unknown"

        summary = ""
        m_sum = re.search(r"^SUMMARY: AddressSanitizer: (.+)$", chunk, flags=re.M)
        if m_sum:
            summary = m_sum.group(1).strip()

        incidents.append(
            {
                "tool": "ASAN",
                "kind": kind,
                "headline": first,
                "summary": summary,
            }
        )

    return incidents


def parse_ubsan(log_content):
    incidents = []

    for line in log_content.splitlines():
        if "runtime error:" in line:
            incidents.append(
                {
                    "tool": "UBSAN",
                    "kind": "runtime-error",
                    "headline": line.strip(),
                    "summary": "",
                }
            )

    for line in log_content.splitlines():
        if "SUMMARY: UndefinedBehaviorSanitizer:" in line:
            incidents.append(
                {
                    "tool": "UBSAN",
                    "kind": "summary",
                    "headline": line.strip(),
                    "summary": line.strip(),
                }
            )

    return incidents


def list_logs(report_dir):
    asan_logs = sorted(glob.glob(os.path.join(report_dir, "asan*")))
    ubsan_logs = sorted(glob.glob(os.path.join(report_dir, "ubsan*")))
    return asan_logs, ubsan_logs


def run_tests(build_dir, report_dir, ctest_cmd, jobs, detect_leaks):
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "halt_on_error=0:detect_leaks={}:log_path={}/asan".format(detect_leaks, report_dir)
    env["UBSAN_OPTIONS"] = "halt_on_error=0:print_stacktrace=1:log_path={}/ubsan".format(report_dir)

    junit = os.path.join(report_dir, "ctest_results.xml")
    cmd = [ctest_cmd, "--output-on-failure", "--output-junit", junit]
    if jobs:
        cmd.extend(["-j", str(jobs)])

    proc = subprocess.run(
        cmd,
        cwd=build_dir,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )

    test_output = os.path.join(report_dir, "test_output.log")
    with open(test_output, "w", encoding="utf-8", errors="replace") as f:
        f.write(proc.stdout or "")

    return proc.returncode


def parse_ctest_junit(junit_path):
    tests = []
    if not os.path.exists(junit_path):
        return tests

    try:
        root = ET.parse(junit_path).getroot()
    except ET.ParseError:
        return tests

    for tc in root.findall(".//testcase"):
        name = tc.attrib.get("name", "unknown")
        classname = tc.attrib.get("classname", "")
        time_s = tc.attrib.get("time", "0")
        failed = tc.find("failure") is not None or tc.find("error") is not None
        skipped = tc.find("skipped") is not None
        system_out = tc.find("system-out")
        output = system_out.text if system_out is not None and system_out.text else ""
        asan_incidents = parse_asan(output)
        ubsan_incidents = parse_ubsan(output)
        if failed:
            status = "failed"
            failure_node = tc.find("failure")
            if failure_node is None:
                failure_node = tc.find("error")
            failure_msg = failure_node.attrib.get("message", "") if failure_node is not None else ""
        elif skipped:
            status = "skipped"
            failure_msg = ""
        else:
            status = "passed"
            failure_msg = ""

        tests.append(
            {
                "name": name,
                "classname": classname,
                "time": time_s,
                "status": status,
                "failure": failure_msg,
                "output": output,
                "asan_incidents": asan_incidents,
                "ubsan_incidents": ubsan_incidents,
            }
        )

    return tests


def write_test_reports(report_dir, tests):
    tests_dir = os.path.join(report_dir, "tests")
    os.makedirs(tests_dir, exist_ok=True)

    for idx, test in enumerate(tests, start=1):
        safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", test["name"])[:120]
        path = os.path.join(tests_dir, f"{idx:03d}_{safe}.txt")
        with open(path, "w", encoding="utf-8") as f:
            f.write(f"name: {test['name']}\n")
            f.write(f"classname: {test['classname']}\n")
            f.write(f"status: {test['status']}\n")
            f.write(f"time: {test['time']}\n")
            f.write(f"asan_incidents: {len(test['asan_incidents'])}\n")
            f.write(f"ubsan_incidents: {len(test['ubsan_incidents'])}\n")
            if test["failure"]:
                f.write(f"failure: {test['failure']}\n")

            if test["asan_incidents"]:
                f.write("\nasan_details:\n")
                for idx, incident in enumerate(test["asan_incidents"], start=1):
                    f.write(f"  [{idx}] {incident['kind']}\n")
                    f.write(f"      {incident['headline']}\n")
                    if incident["summary"]:
                        f.write(f"      summary: {incident['summary']}\n")

            if test["ubsan_incidents"]:
                f.write("\nubsan_details:\n")
                for idx, incident in enumerate(test["ubsan_incidents"], start=1):
                    f.write(f"  [{idx}] {incident['kind']}\n")
                    f.write(f"      {incident['headline']}\n")
                    if incident["summary"]:
                        f.write(f"      summary: {incident['summary']}\n")

            if test["output"]:
                f.write("\noutput:\n")
                f.write(test["output"])


def write_report(report_file, build_dir, report_dir, test_rc, asan_logs, ubsan_logs, incidents, test_output, tests, detect_leaks):
    asan_incidents = [i for i in incidents if i["tool"] == "ASAN"]
    ubsan_incidents = [i for i in incidents if i["tool"] == "UBSAN"]
    tests_passed = [t for t in tests if t["status"] == "passed"]
    tests_failed = [t for t in tests if t["status"] == "failed"]
    tests_skipped = [t for t in tests if t["status"] == "skipped"]
    tests_with_asan = [t for t in tests if len(t.get("asan_incidents", [])) > 0]
    tests_with_ubsan = [t for t in tests if len(t.get("ubsan_incidents", [])) > 0]

    with open(report_file, "w", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write("FATX SANITIZE REPORT\n")
        f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write("=" * 80 + "\n\n")

        f.write("SUMMARY\n")
        f.write("-" * 80 + "\n")
        f.write(f"Build dir: {build_dir}\n")
        f.write(f"Report dir: {report_dir}\n")
        f.write(f"ASAN detect_leaks: {detect_leaks}\n")
        f.write(f"Test command exit code: {test_rc}\n")
        f.write(f"CTest total tests: {len(tests)}\n")
        f.write(f"CTest passed: {len(tests_passed)}\n")
        f.write(f"CTest failed: {len(tests_failed)}\n")
        f.write(f"CTest skipped: {len(tests_skipped)}\n")
        f.write(f"ASAN logs found: {len(asan_logs)}\n")
        f.write(f"UBSAN logs found: {len(ubsan_logs)}\n")
        f.write(f"ASAN incidents: {len(asan_incidents)}\n")
        f.write(f"UBSAN incidents: {len(ubsan_incidents)}\n")
        f.write(f"Tests with ASAN incidents: {len(tests_with_asan)}\n")
        f.write(f"Tests with UBSAN incidents: {len(tests_with_ubsan)}\n")
        f.write(f"Total incidents: {len(incidents)}\n")
        if test_rc == 0 and len(tests_failed) == 0 and len(incidents) == 0:
            status = "✓ No test/sanitizer issue detected"
        else:
            status = "✗ Test and/or sanitizer issues detected"
        f.write(f"Status: {status}\n\n")

        f.write("ARTIFACTS\n")
        f.write("-" * 80 + "\n")
        f.write(f"Test output: {test_output}\n")
        for p in asan_logs:
            f.write(f"ASAN log: {p}\n")
        for p in ubsan_logs:
            f.write(f"UBSAN log: {p}\n")
        f.write(f"JUnit: {os.path.join(report_dir, 'ctest_results.xml')}\n")
        f.write(f"Per-test reports dir: {os.path.join(report_dir, 'tests')}\n")
        f.write("\n")

        f.write("FAILED TESTS\n")
        f.write("=" * 80 + "\n")
        if not tests_failed:
            f.write("No failed CTest entries.\n\n")
        else:
            for t in tests_failed:
                f.write(f"- {t['name']} ({t['classname']})\n")
                if t["failure"]:
                    f.write(f"  reason: {t['failure']}\n")
            f.write("\n")

        f.write("TESTS WITH SANITIZER INCIDENTS\n")
        f.write("=" * 80 + "\n")
        if not tests_with_asan and not tests_with_ubsan:
            f.write("No per-test ASAN/UBSAN incidents found.\n\n")
        else:
            for t in tests:
                ac = len(t.get("asan_incidents", []))
                uc = len(t.get("ubsan_incidents", []))
                if ac == 0 and uc == 0:
                    continue
                f.write(f"- {t['name']} ({t['classname']}): ASAN={ac}, UBSAN={uc}\n")
            f.write("\n")

        f.write("INCIDENTS\n")
        f.write("=" * 80 + "\n")
        if not incidents:
            f.write("No ASAN/UBSAN incidents found.\n")
        else:
            for idx, incident in enumerate(incidents, start=1):
                f.write(f"[{idx}] {incident['tool']} | {incident['kind']}\n")
                f.write(f"    {incident['headline']}\n")
                if incident["summary"]:
                    f.write(f"    summary: {incident['summary']}\n")
                f.write("\n")


def main():
    parser = argparse.ArgumentParser(description="FATX ASAN/UBSAN report generator")
    parser.add_argument("--build-dir", required=True, help="CMake build directory")
    parser.add_argument("--report-dir", required=True, help="Directory containing asan*/ubsan* logs")
    parser.add_argument("--run-tests", action="store_true", help="Run ctest before generating report")
    parser.add_argument("--ctest", default="ctest", help="CTest command (default: ctest)")
    parser.add_argument("--jobs", type=int, help="CTest parallel jobs")
    parser.add_argument("--detect-leaks", type=int, choices=[0, 1], default=1, help="ASAN detect_leaks value (default: 1)")
    parser.add_argument("--test-rc", type=int, help="Exit code from test command")
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    report_dir = os.path.abspath(args.report_dir)
    test_output = os.path.join(report_dir, "test_output.log")
    report_file = os.path.join(report_dir, "report.txt")

    os.makedirs(report_dir, exist_ok=True)

    test_rc = args.test_rc
    if args.run_tests:
        jobs = args.jobs or os.cpu_count()
        test_rc = run_tests(build_dir, report_dir, args.ctest, jobs, args.detect_leaks)
    elif test_rc is None:
        print("error: --test-rc is required when --run-tests is not used", file=sys.stderr)
        return 2

    asan_logs, ubsan_logs = list_logs(report_dir)
    tests = parse_ctest_junit(os.path.join(report_dir, "ctest_results.xml"))
    write_test_reports(report_dir, tests)

    incidents = []
    for path in asan_logs:
        incidents.extend(parse_asan(read_text(path)))
    for path in ubsan_logs:
        incidents.extend(parse_ubsan(read_text(path)))

    write_report(
        report_file,
        build_dir,
        report_dir,
        test_rc,
        asan_logs,
        ubsan_logs,
        incidents,
        test_output,
        tests,
        args.detect_leaks,
    )

    print("=" * 80)
    print("Sanitize report summary")
    print("=" * 80)
    print(f"ASAN logs      : {len(asan_logs)}")
    print(f"UBSAN logs     : {len(ubsan_logs)}")
    print(f"CTest tests    : {len(tests)}")
    print(f"CTest failed   : {len([t for t in tests if t['status'] == 'failed'])}")
    print(f"ASAN incidents : {len([i for i in incidents if i['tool'] == 'ASAN'])}")
    print(f"UBSAN incidents: {len([i for i in incidents if i['tool'] == 'UBSAN'])}")
    print(f"Total incidents: {len(incidents)}")
    print(f"Report         : {report_file}")
    print("=" * 80)

    has_failed_tests = any(t["status"] == "failed" for t in tests)
    has_incidents = len(incidents) > 0
    return 0 if (test_rc == 0 and not has_failed_tests and not has_incidents) else 1


if __name__ == "__main__":
    sys.exit(main())
