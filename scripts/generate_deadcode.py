#!/usr/bin/env python3
"""
FATX Dead Code Analysis Script
Combines 3 tools: cppcheck, GCC -fanalyzer, and lcov coverage analysis
to produce a unified dead code report.
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
import json
import os
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


# ───────────────────────────────────────────────────────── helpers ──

def run(cmd, *, capture=True, cwd=None):
    """Run a shell command, return (returncode, stdout, stderr)."""
    try:
        r = subprocess.run(
            cmd, shell=True, cwd=cwd,
            capture_output=capture, text=True, check=False,
            timeout=300
        )
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return 1, "", "TIMEOUT"
    except Exception as e:
        return 1, "", str(e)


def coverage_level(pct):
    if pct >= 90:   return "🟢", "EXCELLENT"
    if pct >= 80:   return "🟢", "TRÈS BON"
    if pct >= 70:   return "🟡", "BON"
    if pct >= 50:   return "🟠", "ACCEPTABLE"
    return "🔴", "À AMÉLIORER"


def bar(pct, width=20):
    filled = int(pct / 100 * width)
    return "█" * filled + "░" * (width - filled)


def group_consecutive(lines):
    if not lines:
        return []
    ranges, start, end = [], lines[0], lines[0]
    for n in lines[1:]:
        if n == end + 1:
            end = n
        else:
            ranges.append((start, end))
            start = end = n
    ranges.append((start, end))
    return ranges


def fmt_range(r):
    return str(r[0]) if r[0] == r[1] else f"{r[0]}-{r[1]}"


# ─────────────────────────────────────────────────── tool 1: cppcheck ──

def run_cppcheck(src_dir, build_dir, out_dir):
    print("  [1/3] cppcheck ...", flush=True)
    compile_commands = os.path.join(build_dir, "compile_commands.json")
    if not os.path.exists(compile_commands):
        return {"error": "compile_commands.json not found in build directory"}

    out_file = os.path.join(out_dir, "cppcheck.txt")
    cmd = (
        f'cppcheck '
        f'--project="{compile_commands}" '
        f'--enable=all '
        f'--check-level=exhaustive '
        f'--inconclusive '
        f'--suppress=missingInclude '
        f'--suppress=missingIncludeSystem '
        f'--suppress=unknownMacro '
        f'--inline-suppr '
        f'--template="{{file}}:{{line}}: [{{severity}}] {{id}}: {{message}}" '
        f'--output-file="{out_file}" '
    )
    rc, _, _ = run(cmd)

    try:
        with open(out_file, encoding="utf-8", errors="replace") as f:
            raw = f.read()
    except Exception:
        return {"error": "cppcheck output not found", "raw": ""}

    dead_ids = {"unreachable", "deadCode", "unusedFunction",
                "knownConditionTrueFalse", "alwaysTrue", "alwaysFalse"}
    findings = []
    for line in raw.splitlines():
        if src_dir not in line:
            continue
        for did in dead_ids:
            if did in line:
                rel = line.replace(src_dir + "/", "src/")
                findings.append(rel)
                break

    print(f"        → {len(findings)} finding(s) in project sources", flush=True)
    return {"findings": findings, "raw_file": out_file, "return_code": rc}


# ──────────────────────────────────────────── tool 2: GCC -fanalyzer ──

def run_gcc_analyzer(src_dir, build_dir, out_dir):
    print("  [2/3] GCC -fanalyzer ...", flush=True)
    compile_commands = os.path.join(build_dir, "compile_commands.json")
    if not os.path.exists(compile_commands):
        return {"error": "compile_commands.json not found"}

    # Extract compile flags from compile_commands.json
    with open(compile_commands, encoding="utf-8", errors="replace") as f:
        db = json.load(f)

    flags = []
    for entry in db:
        fn = entry.get("file", "")
        if "context.cpp" in fn and "tests/" not in fn:
            toks = entry.get("command", "").split()
            i = 0
            while i < len(toks):
                t = toks[i]
                if any(t.startswith(p) for p in ["-I", "-D", "-std", "-isystem"]):
                    if t in ["-I", "-D", "-isystem"]:
                        flags.append(t + toks[i + 1])
                        i += 2
                    else:
                        flags.append(t)
                        i += 1
                else:
                    i += 1
            break

    flags_str = " ".join(flags)
    sources = sorted(Path(src_dir).glob("*.cpp"))
    out_file = os.path.join(out_dir, "gcc_analyzer.txt")

    findings = []
    with open(out_file, "w", encoding="utf-8", errors="replace") as f:
        f.write("=== GCC -fanalyzer Dead Code Analysis ===\n\n")
        for src in sources:
            cmd = (
                f'g++ {flags_str} '
                f'-fanalyzer '
                f'-Wno-error '
                f'-Wunused '
                f'-Wunused-function '
                f'-Wunused-variable '
                f'-Wunused-but-set-variable '
                f'-fsyntax-only '
                f'"{src}" 2>&1'
            )
            rc2, out2, _ = run(cmd)
            f.write(f"=== {src.name} ===\n")
            f.write(out2 + "\n")

            for line in out2.splitlines():
                if "warning:" in line and str(src) in line:
                    rel = line.replace(str(src_dir) + "/", "src/")
                    findings.append(rel)

    print(f"        → {len(findings)} warning(s) in project sources", flush=True)
    return {"findings": findings, "raw_file": out_file}


# ──────────────────────────────────────────────── tool 3: lcov coverage ──

def run_coverage_analysis(src_dir, build_dir, out_dir):
    print("  [3/3] Coverage analysis (lcov) ...", flush=True)

    # Search for coverage info file
    info_candidates = [
        os.path.join(build_dir, "coverage_final.info"),
        os.path.join(build_dir, "coverage.info"),
    ]
    # Also search in parent directories
    parent = str(Path(build_dir).parent)
    for sibling in ["build.1", "build", "build.debug"]:
        info_candidates.append(os.path.join(parent, sibling, "coverage_final.info"))

    info_file = None
    for candidate in info_candidates:
        if os.path.exists(candidate):
            info_file = candidate
            break

    if info_file is None:
        print("        → no coverage info found, skipping", flush=True)
        return {"error": "no coverage_final.info found", "modules": {}, "uncovered": {}}

    print(f"        → using {info_file}", flush=True)

    # Parse lcov info
    modules = {}       # fname -> (covered, total)
    uncovered = {}     # fname -> [list of uncovered line numbers]
    current_file = None
    cov_lines = []
    uncov_lines = []

    with open(info_file, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line.startswith("SF:"):
                current_file = line[3:]
            elif line.startswith("DA:") and current_file:
                if "src/" in current_file:
                    parts = line[3:].split(",")
                    if len(parts) >= 2:
                        try:
                            lineno = int(parts[0])
                            count = int(parts[1])
                            if count == 0:
                                uncov_lines.append(lineno)
                            else:
                                cov_lines.append(lineno)
                        except ValueError:
                            pass
            elif line == "end_of_record" and current_file:
                if "src/" in current_file:
                    fname = os.path.basename(current_file)
                    total = len(cov_lines) + len(uncov_lines)
                    if total > 0:
                        existing = modules.get(fname, (0, 0))
                        modules[fname] = (
                            existing[0] + len(cov_lines),
                            existing[1] + total
                        )
                        if fname not in uncovered:
                            uncovered[fname] = []
                        uncovered[fname].extend(uncov_lines)
                current_file = None
                cov_lines = []
                uncov_lines = []

    total_cov = sum(v[0] for v in modules.values())
    total_lines = sum(v[1] for v in modules.values())
    total_uncov = total_lines - total_cov
    global_pct = total_cov / total_lines * 100 if total_lines else 0

    print(f"        → {total_uncov} uncovered lines ({global_pct:.1f}% covered)", flush=True)
    return {
        "modules": modules,
        "uncovered": uncovered,
        "total_cov": total_cov,
        "total_lines": total_lines,
        "info_file": info_file
    }


# ─────────────────────────────────────────────── report generation ──

def write_report(cppcheck_res, gcc_res, cov_res, out_dir, src_dir):
    report_path = os.path.join(out_dir, "report.txt")
    sep = "=" * 80
    sep2 = "-" * 80

    now = datetime.now().strftime("%Y-%m-%d %H:%M")

    with open(report_path, "w", encoding="utf-8", errors="replace") as f:
        def w(s=""):
            f.write(s + "\n")

        w(sep)
        w("RAPPORT D'ANALYSE DE CODE MORT — FATX")
        w(f"Généré le : {now}")
        w(sep)
        w()

        # ── Section 1: cppcheck ──
        w("■ SECTION 1 — cppcheck (analyse statique)")
        w(sep2)
        if "error" in cppcheck_res:
            w(f"  ERREUR: {cppcheck_res['error']}")
        else:
            findings = cppcheck_res.get("findings", [])
            if findings:
                w(f"  {len(findings)} finding(s) de code mort / conditions toujours vraies:\n")
                for f2 in findings:
                    w(f"    {f2}")
            else:
                w("  ✅ Aucun code mort détecté par cppcheck dans src/")
            w()
            w(f"  Rapport brut : {cppcheck_res.get('raw_file', 'N/A')}")
        w()

        # ── Section 2: GCC -fanalyzer ──
        w("■ SECTION 2 — GCC -fanalyzer (analyse de flot)")
        w(sep2)
        if "error" in gcc_res:
            w(f"  ERREUR: {gcc_res['error']}")
        else:
            findings = gcc_res.get("findings", [])
            if findings:
                w(f"  {len(findings)} warning(s) détecté(s):\n")
                for f2 in findings:
                    w(f"    {f2}")
            else:
                w("  ✅ Aucun warning de code mort détecté par GCC -fanalyzer dans src/")
            w()
            w(f"  Rapport brut : {gcc_res.get('raw_file', 'N/A')}")
        w()

        # ── Section 3: couverture ──
        w("■ SECTION 3 — Couverture de code (lcov)")
        w(sep2)
        if "error" in cov_res:
            w(f"  ERREUR: {cov_res['error']}")
        else:
            modules = cov_res.get("modules", {})
            uncovered = cov_res.get("uncovered", {})
            total_cov = cov_res.get("total_cov", 0)
            total_lines = cov_res.get("total_lines", 0)
            info_file = cov_res.get("info_file", "")

            total_uncov = total_lines - total_cov
            global_pct = total_cov / total_lines * 100 if total_lines else 0
            icon, label = coverage_level(global_pct)

            w(f"  Source données : {info_file}")
            w()
            w(f"  {'Module':<20} {'Couv.':>6}  {'Graphe':<22} {'Statut':<12} {'Détail'}")
            w(f"  {'-'*20} {'-'*6}  {'-'*22} {'-'*12} {'-'*15}")

            for fname in sorted(modules.keys()):
                cov, total = modules[fname]
                uncov = total - cov
                pct = cov / total * 100 if total else 0
                ico, lbl = coverage_level(pct)
                b = bar(pct, 20)
                w(f"  {fname:<20} {pct:>5.1f}%  {b}  {ico} {lbl:<10} {cov}/{total}")

            w(f"  {'':20} {'':6}  {'':22} {'':12}")
            b = bar(global_pct, 20)
            w(f"  {'TOTAL':<20} {global_pct:>5.1f}%  {b}  {icon} {label:<10} {total_cov}/{total_lines}")
            w()
            w(f"  Total lignes non couvertes : {total_uncov}")
            w()

            # Détail par module
            w(f"  Lignes non couvertes par module:")
            w(f"  {sep2[:60]}")
            for fname in sorted(uncovered.keys()):
                lines = sorted(set(uncovered[fname]))
                if not lines:
                    continue
                cov, total = modules.get(fname, (0, 0))
                pct = (cov / total * 100) if total else 0
                ranges = group_consecutive(lines)
                w(f"  {fname} ({pct:.1f}%) — {len(lines)} ligne(s):")
                chunks = []
                for r in ranges:
                    chunks.append(f"L.{fmt_range(r)}")
                # Print in rows of 8
                for i in range(0, len(chunks), 8):
                    w("    " + "  ".join(chunks[i:i+8]))
                w()

        # ── Section 4: synthèse ──
        w("■ SECTION 4 — Synthèse")
        w(sep2)
        w()

        total_dead = 0
        total_dead += len(cppcheck_res.get("findings", []))
        total_dead += len(gcc_res.get("findings", []))
        uncov_total = cov_res.get("total_lines", 0) - cov_res.get("total_cov", 0)

        w("  Code mort confirmé (cppcheck + GCC analyzer):")
        if total_dead == 0:
            w("    ✅  0 finding — les outils statiques n'ont pas détecté de code mort structurel")
        else:
            w(f"    ⚠️  {total_dead} finding(s) trouvé(s)")
        w()
        w(f"  Lignes jamais exécutées (couverture):  {uncov_total}")
        w()
        w(sep)

    return report_path


# ──────────────────────────────────────────────────────────── main ──

def main():
    parser = argparse.ArgumentParser(description="FATX dead code analysis")
    parser.add_argument(
        "--build-dir", required=True,
        help="CMake build directory (must contain compile_commands.json)"
    )
    parser.add_argument(
        "--src-dir",
        help="Source directory (default: <project_root>/src)"
    )
    parser.add_argument(
        "--output-dir",
        help="Output directory (default: <build-dir>/deadcode_report)"
    )
    args = parser.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    project_root = str(Path(build_dir).parent)

    src_dir = args.src_dir or os.path.join(project_root, "src")
    src_dir = os.path.abspath(src_dir)

    out_dir = args.output_dir or os.path.join(build_dir, "deadcode_report")
    os.makedirs(out_dir, exist_ok=True)

    print(f"\nFATX Dead Code Analysis")
    print(f"  Build dir : {build_dir}")
    print(f"  Src dir   : {src_dir}")
    print(f"  Output    : {out_dir}\n")

    # Run tools
    cppcheck_res = run_cppcheck(src_dir, build_dir, out_dir)
    gcc_res      = run_gcc_analyzer(src_dir, build_dir, out_dir)
    cov_res      = run_coverage_analysis(src_dir, build_dir, out_dir)

    # Generate report
    report_path = write_report(cppcheck_res, gcc_res, cov_res, out_dir, src_dir)

    print(f"\n✅ Rapport généré : {report_path}")
    print(f"   Répertoire    : {out_dir}\n")

    # Print report to stdout
    try:
        with open(report_path, encoding="utf-8", errors="replace") as f:
            print(f.read())
    except Exception:
        pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
