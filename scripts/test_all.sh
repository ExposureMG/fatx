#!/usr/bin/env bash
set -Eeuo pipefail

trap 'echo "Erreur à la ligne ${LINENO}: ${BASH_COMMAND}" >&2' ERR
export PYTHONDONTWRITEBYTECODE=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$ROOT_DIR/build"
export PYTHONPYCACHEPREFIX="$BUILD_DIR/.pycache"

ensure_build_prereqs() {
    local build_type=$1
    local build_tests=$2
    local sanitize=$3

    local cache_file="$BUILD_DIR/CMakeCache.txt"
    local tests_bin="$BUILD_DIR/fatx_tests"
    local need_rebuild=0

    if [[ -f "$cache_file" ]]; then
        grep -Eq "^CMAKE_BUILD_TYPE:.*=${build_type}$" "$cache_file" || need_rebuild=1
        grep -Eq "^BUILD_TESTS:.*=${build_tests}$" "$cache_file" || need_rebuild=1
        grep -Eq '^CMAKE_EXPORT_COMPILE_COMMANDS:.*=ON$' "$cache_file" || need_rebuild=1
        grep -Eq "^SANITIZE:.*=${sanitize}$" "$cache_file" || need_rebuild=1
        [[ -f "$BUILD_DIR/compile_commands.json" ]] || need_rebuild=1
    else
        need_rebuild=1
    fi

    if [[ "$build_tests" == "ON" && "$sanitize" == "ON" && -x "$tests_bin" ]]; then
        if command -v ldd &> /dev/null; then
            ldd "$tests_bin" 2>/dev/null | grep -Eqi 'libasan|libubsan' || need_rebuild=1
        fi
    fi

    if [[ $need_rebuild -eq 0 ]]; then
        echo "[i] Reuse existing build in $BUILD_DIR (${build_type}, BUILD_TESTS=${build_tests}, SANITIZE=${sanitize})"
        return
    fi

    printf "  Nettoyage..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    printf " ✓\n"
    echo "[i] Configuration CMake (${build_type}, BUILD_TESTS=${build_tests}, SANITIZE=${sanitize})..."
    cmake -B "$BUILD_DIR" -S "$ROOT_DIR" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DBUILD_TESTS="$build_tests" \
        -DSANITIZE="$sanitize" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null 2>&1
    echo "[i] Compilation..."
    cmake --build "$BUILD_DIR" -j"$(nproc)"
}

require_valgrind() {
    local tool=$1
    if ! command -v valgrind &> /dev/null; then
        echo "[!] valgrind not found: skipping $tool tests"
        echo ""
        return 1
    fi
}

print_valgrind_summary() {
    local label=$1
    local report_file=$2
    local tool_rc=$3

    echo "[i] ${label} résumé:"

    if [[ -f "$report_file" ]]; then
        grep -E "^(Failed tests:|Tests with Valgrind errors:|Total Valgrind errors:|Status:)" "$report_file" || true
        if [[ $tool_rc -eq 0 ]]; then
            echo "[✓] ${label}: aucune erreur détectée."
        else
            echo "[!] ${label}: des problèmes ont été détectés (code=$tool_rc)."
            echo "[i] Rapport global disponible: $report_file"
        fi
    else
        if [[ $tool_rc -eq 0 ]]; then
            echo "[!] ${label}: exécution réussie, mais rapport introuvable: $report_file"
        else
            echo "[!] ${label}: échec sans rapport global généré."
            return $tool_rc
        fi
    fi
}

phase_1() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 1/7] Build Release (binary, docs, install)  ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""
    ensure_build_prereqs Release OFF OFF

    echo "[1.1/3] Documentation (Doxygen)..."
    cmake --build "$BUILD_DIR" -j"$(nproc)" --target doxygen > /dev/null 2>&1

    echo "[1.2/3] Manpages..."
    cmake --build "$BUILD_DIR" -j"$(nproc)" --target man > /dev/null 2>&1

    echo "[1.3/3] Installation..."
    cmake --install "$BUILD_DIR" --prefix "$BUILD_DIR/install"
}

phase_2() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 2/7] Tests Release                          ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""
    ensure_build_prereqs Release ON OFF

    echo "[2.1/1] Exécution des tests (GoogleTest + scripts bash en parallèle)..."
    (
        cd "$BUILD_DIR"
        "$BUILD_DIR/fatx_tests" --gtest_brief_output &
        GTEST_PID=$!
        ctest --output-on-failure -j"$(nproc)" &
        CTEST_PID=$!
        wait $GTEST_PID $CTEST_PID
    ) || true
}

phase_3() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 3/7] Tests Debug (coverage + dead code)     ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""
    ensure_build_prereqs Debug ON OFF

    echo "[3.1/2] Analyse de couverture..."
    cmake --build "$BUILD_DIR" --target coverage -- -j"$(nproc)"
    echo "[3.2/2] Analyse du code mort..."
    cmake --build "$BUILD_DIR" --target deadcode -- -j"$(nproc)"
}

phase_4() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 4/7] Address/Undefined Behavior Sanitizer   ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""
    ensure_build_prereqs Debug ON ON

    echo "[4.1/1] Analyse ASAN/UBSAN..."
    cmake --build "$BUILD_DIR" --target sanitize
}

phase_5() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 5/7] Valgrind Memcheck                      ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""

    require_valgrind "memcheck" || return
    ensure_build_prereqs Debug ON OFF

    echo "[5.1/1] Analyse mémoire (memcheck)..."
    local memcheck_rc=0
    trap - ERR
    set +e
    cmake --build "$BUILD_DIR" --target memcheck
    memcheck_rc=$?
    set -e
    trap 'echo "Erreur à la ligne ${LINENO}: ${BASH_COMMAND}" >&2' ERR

    local report_file="$BUILD_DIR/memcheck_report/report.txt"
    print_valgrind_summary "Memcheck" "$report_file" "$memcheck_rc"
}

phase_6() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 6/7] Valgrind Helgrind                      ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""

    require_valgrind "helgrind" || return
    ensure_build_prereqs Debug ON OFF

    echo "[6.1/1] Analyse concurrence (helgrind)..."
    local helgrind_rc=0
    trap - ERR
    set +e
    cmake --build "$BUILD_DIR" --target helgrind
    helgrind_rc=$?
    set -e
    trap 'echo "Erreur à la ligne ${LINENO}: ${BASH_COMMAND}" >&2' ERR

    local report_file="$BUILD_DIR/helgrind_report/report.txt"
    print_valgrind_summary "Helgrind" "$report_file" "$helgrind_rc"
}

phase_7() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║ [Phase 7/7] Valgrind DRD                           ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""

    require_valgrind "drd" || return
    ensure_build_prereqs Debug ON OFF

    echo "[7.1/1] Analyse concurrence (drd)..."
    local drd_rc=0
    trap - ERR
    set +e
    cmake --build "$BUILD_DIR" --target drd
    drd_rc=$?
    set -e
    trap 'echo "Erreur à la ligne ${LINENO}: ${BASH_COMMAND}" >&2' ERR

    local report_file="$BUILD_DIR/drd_report/report.txt"
    print_valgrind_summary "DRD" "$report_file" "$drd_rc"
}

show_help() {
    echo "Usage: $0 [PHASE]"
    echo ""
    echo "Phases disponibles:"
    echo "  1   - Build Release (binary, docs, install)"
    echo "  2   - Tests Release"
    echo "  3   - Tests Debug (coverage + dead code)"
    echo "  4   - Address/Undefined Behavior Sanitizer"
    echo "  5   - Valgrind Memcheck"
    echo "  6   - Valgrind Helgrind"
    echo "  7   - Valgrind DRD"
    echo ""
    echo "Exemples:"
    echo "  $0      # Exécute toutes les phases (1-7)"
    echo "  $0 1    # Exécute uniquement la phase 1"
    echo ""
}

# Main execution
if [[ $# -gt 1 ]]; then
    show_help
    exit 1
fi

PHASE="${1:-all}"

case "$PHASE" in
    1)
        phase_1
        ;;
    2)
        phase_2
        ;;
    3)
        phase_3
        ;;
    4)
        phase_4
        ;;
    5)
        phase_5
        ;;
    6)
        phase_6
        ;;
    7)
        phase_7
        ;;
    all)
        phase_1
        phase_2
        phase_3
        phase_4
        phase_5
        phase_6
        phase_7
        ;;
    -h|--help|help)
        show_help
        exit 0
        ;;
    *)
        echo "❌ Phase invalide: $PHASE"
        echo ""
        show_help
        exit 1
        ;;
esac

echo ""
echo "╔════════════════════════════════════════════════════╗"
echo "║ ✅ Exécution complétée avec succès                 ║"
echo "╚════════════════════════════════════════════════════╝"
echo ""
