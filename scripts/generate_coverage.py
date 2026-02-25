#!/usr/bin/env python3
"""
Generate code coverage report with module statistics
"""
import subprocess
import sys
import os
import re
from pathlib import Path
from datetime import datetime
from typing import Dict, Tuple

def run_command(cmd, cwd=None, capture=False, filter_output=None):
    """Run a shell command.

    filter_output: list of substrings — lines containing any of them are
    suppressed from both stdout and stderr.
    """
    try:
        if capture or filter_output:
            result = subprocess.run(cmd, shell=True, cwd=cwd,
                                  capture_output=True, text=True, check=False)
            if filter_output:
                for stream, dest in ((result.stdout, sys.stdout), (result.stderr, sys.stderr)):
                    if not stream:
                        continue
                    filtered = "\n".join(
                        line for line in stream.splitlines()
                        if not any(pat in line for pat in filter_output)
                    )
                    if filtered.strip():
                        print(filtered, file=dest)
            elif not capture:
                # no filtering but capture was False — print normally
                if result.stdout:
                    print(result.stdout, end="")
                if result.stderr:
                    print(result.stderr, end="", file=sys.stderr)
            if capture:
                return result.returncode, result.stdout, result.stderr
            return result.returncode, result.stdout, ""
        else:
            result = subprocess.run(cmd, shell=True, cwd=cwd, check=False)
            return result.returncode, "", ""
    except Exception as e:
        print(f"Error running command: {e}")
        return 1, "", str(e)

def extract_coverage_stats(info_file: str, build_dir: str) -> Dict[str, Tuple[int, int, float]]:
    """Extract coverage statistics from lcov info file."""
    stats = {}
    
    try:
        with open(info_file, 'r') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading info file: {e}")
        return stats
    
    # Parse coverage data by source file
    current_file = None
    lf = 0  # Lines found
    lh = 0  # Lines hit
    
    for line in content.split('\n'):
        if line.startswith('SF:'):
            # Save previous file if any
            if current_file and 'src/' in current_file and current_file.endswith('.cpp'):
                if lf > 0:
                    percent = (lh / lf) * 100
                    filename = os.path.basename(current_file)
                    stats[filename] = (lh, lf, percent)
            
            current_file = line[3:]
            lf = 0
            lh = 0
        elif line.startswith('LF:'):
            lf = int(line[3:])
        elif line.startswith('LH:'):
            lh = int(line[3:])
        elif line == 'end_of_record':
            # Save final file
            if current_file and 'src/' in current_file and current_file.endswith('.cpp'):
                if lf > 0:
                    percent = (lh / lf) * 100
                    filename = os.path.basename(current_file)
                    stats[filename] = (lh, lf, percent)
            current_file = None
    
    return stats

def classify_coverage(percent: float) -> Tuple[str, str]:
    """Classify coverage level and return level name and color code."""
    if percent >= 90:
        return "EXCELLENT", "🟢"
    elif percent >= 80:
        return "TRÈS BON", "🟢"
    elif percent >= 70:
        return "BON", "🟡"
    elif percent >= 50:
        return "ACCEPTABLE", "🟠"
    else:
        return "À AMÉLIORER", "🔴"

def print_coverage_report(stats: Dict[str, Tuple[int, int, float]]):
    """Print a formatted coverage report."""
    print("\n" + "="*80)
    print("RAPPORT DE COUVERTURE DE CODE - MODULES FATX")
    print("="*80)
    print(f"{'Module':<25} {'Couverture':<15} {'Niveau':<15} {'Détail':<15}")
    print("-"*80)
    
    # Sort by filename
    total_hit = 0
    total_lines = 0
    
    for filename in sorted(stats.keys()):
        lh, lf, percent = stats[filename]
        level, emoji = classify_coverage(percent)
        
        total_hit += lh
        total_lines += lf
        
        print(f"{filename:<25} {percent:>6.1f}%{'':<8} {emoji} {level:<12} {lh}/{lf}")
    
    print("-"*80)
    
    if total_lines > 0:
        avg_percent = (total_hit / total_lines) * 100
        avg_level, avg_emoji = classify_coverage(avg_percent)
        print(f"{'MOYENNE':<25} {avg_percent:>6.1f}%{'':<8} {avg_emoji} {avg_level:<12} {total_hit}/{total_lines}")
    
    print("="*80)
    print("\nLégend:")
    print("  🟢 EXCELLENT (≥90%) | 🟢 TRÈS BON (≥80%)  | 🟡 BON (≥70%)")
    print("  🟠 ACCEPTABLE (≥50%) | 🔴 À AMÉLIORER (<50%)")
    print("\n")


def write_coverage_report(
    report_file: Path,
    build_dir: Path,
    info_file: Path,
    html_index: Path,
    stats: Dict[str, Tuple[int, int, float]],
    ctest_rc: int,
):
    total_hit = sum(v[0] for v in stats.values())
    total_lines = sum(v[1] for v in stats.values())
    avg_percent = (total_hit / total_lines) * 100 if total_lines else 0.0

    with open(report_file, "w", encoding="utf-8", errors="replace") as f:
        f.write("=" * 80 + "\n")
        f.write("FATX COVERAGE REPORT\n")
        f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write("=" * 80 + "\n\n")

        f.write("SUMMARY\n")
        f.write("-" * 80 + "\n")
        f.write(f"Build dir: {build_dir}\n")
        f.write(f"CTest exit code: {ctest_rc}\n")
        f.write(f"Modules analyzed: {len(stats)}\n")
        f.write(f"Total covered lines: {total_hit}\n")
        f.write(f"Total lines: {total_lines}\n")
        f.write(f"Global coverage: {avg_percent:.1f}%\n")
        status = "✓ Coverage report generated" if stats else "✗ Coverage report generation incomplete"
        f.write(f"Status: {status}\n\n")

        f.write("MODULES\n")
        f.write("=" * 80 + "\n")
        for filename in sorted(stats.keys()):
            lh, lf, percent = stats[filename]
            level, emoji = classify_coverage(percent)
            f.write(f"- {filename}: {percent:.1f}% ({lh}/{lf}) [{emoji} {level}]\n")
        f.write("\n")

        f.write("ARTIFACTS\n")
        f.write("-" * 80 + "\n")
        f.write(f"LCOV info: {info_file}\n")
        f.write(f"HTML report: {html_index}\n")
        f.write(f"Text report: {report_file}\n")


def main():
    """Main function."""
    import argparse
    parser = argparse.ArgumentParser(description="FATX coverage report generator")
    parser.add_argument("--build-dir", default=None,
                        help="Build directory (default: <project>/build)")
    parser.add_argument("--report-dir", default=None,
                        help="Coverage report output directory (default: <build>/coverage_final_report)")
    parser.add_argument("--jobs", type=int, default=1,
                        help="Number of parallel jobs for CTest (default: 1)")
    args = parser.parse_args()

    project_dir = Path(__file__).parent.parent
    if args.build_dir:
        build_dir = Path(args.build_dir)
    else:
        build_dir = project_dir / "build"
    
    if not build_dir.exists():
        print(f"Build directory not found: {build_dir}")
        print("Running cmake configuration...")
        rc, _, _ = run_command(f"cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON", 
                              cwd=str(project_dir))
        if rc != 0:
            print("CMake configuration failed")
            return 1
    
    print("Generating coverage report...")
    print("-" * 80)
    
    # Run tests to ensure we have fresh coverage data. Build unit tests,
    # copy the `test.sh` script into the build dir, then run CTest so
    # both GoogleTest and shell integration tests registered with CTest
    # are executed and contribute to coverage data.
    print("1. Building tests and preparing test scripts...")
    rc, _, _ = run_command(f"cmake --build {build_dir} --target fatx_tests", 
                          cwd=str(project_dir))
    if rc != 0:
        print("Build failed")
        return 1

    # Ensure the test.sh copy target exists in the build directory
    rc, _, _ = run_command(f"cmake --build {build_dir} --target test.sh", cwd=str(project_dir))
    if rc != 0:
        print("Warning: failed to build/copy test.sh target (continuing...)")

    print("2. Running all tests via CTest...")
    rc, out, err = run_command(f"ctest --output-on-failure -j {args.jobs}", cwd=str(build_dir), capture=True)
    ctest_rc = rc
    if rc != 0:
        print("   Warning: Some tests failed. Continuing to capture coverage data.")
        print(out)
        if err:
            print(err)
    else:
        # Print a short summary of the ctest output
        summary_match = re.search(r"(\d+ tests from \d+ test cases ran|Total Test time:)", out)
        if summary_match:
            print("   ✓ Tests completed (see CTest output above)")
        else:
            print("   ✓ Tests completed")
    
    # Generate coverage data
    print("2. Capturing coverage data with lcov...")
    # Use current directory (.) instead of build_dir to capture all gcda files properly
    # --ignore-errors path: ignore missing .gcno files (e.g. from non-test binaries)
    rc, _, _ = run_command(f"lcov --capture --directory . --output-file {build_dir}/coverage_final.info --ignore-errors inconsistent,inconsistent,path --no-external",
                          cwd=str(project_dir),
                          filter_output=["Dropping ", "dropping ", "ignoring data for external"])
    if rc != 0:
        print("   Warning: lcov capture had issues (continuing...)")
    else:
        print("   ✓ Coverage data captured")
    
    # Generate HTML report
    print("3. Generating HTML report...")
    report_dir = Path(args.report_dir) if args.report_dir else (build_dir / "coverage_final_report")
    rc, _, _ = run_command(f"genhtml {build_dir}/coverage_final.info --output-directory {report_dir} --ignore-errors inconsistent,inconsistent",
                          cwd=str(project_dir),
                          filter_output=["Dropping ", "dropping ", "ignoring data for external"])
    if rc == 0:
        print(f"   ✓ HTML report generated at: {report_dir}/index.html")
    
    # Extract and display statistics
    print("4. Analyzing coverage statistics...")
    stats = extract_coverage_stats(str(build_dir / "coverage_final.info"), str(build_dir))
    
    if stats:
        report_file = report_dir / "report.txt"
        write_coverage_report(
            report_file=report_file,
            build_dir=build_dir,
            info_file=build_dir / "coverage_final.info",
            html_index=report_dir / "index.html",
            stats=stats,
            ctest_rc=ctest_rc,
        )
        print_coverage_report(stats)
        print(f"Report: {report_file}")
        return 0
    else:
        print("Could not extract coverage statistics")
        return 1

if __name__ == "__main__":
    sys.exit(main())
