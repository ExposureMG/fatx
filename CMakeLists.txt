cmake_minimum_required(VERSION 3.25...3.31)
project(
  fatx
  VERSION 1.19
  LANGUAGES C CXX
)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/scripts)

# Fixer le type de build par défaut AVANT toute option qui en dépend
# (only when building FATX itself: an application embedding fatx_core through
# add_subdirectory() keeps its own build type, warnings and tests)
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Option pour activer les tests (OFF par défaut en Release)
if(PROJECT_IS_TOP_LEVEL AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  option(BUILD_TESTS "Build test suite" ON)
else()
  option(BUILD_TESTS "Build test suite" OFF)
endif()
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

option(SANITIZE "Enable sanitizers" OFF)
set(FATX_DBGCR 48 CACHE STRING "Bytes per debug line for DBGCR (Debug builds)")

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
endif()

# fatx_core (the filesystem library) needs only the Boost headers. The
# command-line tools also need Boost.Program_options, and fusefatx needs FUSE3.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
# Cross builds can point at header-only Boost instead (the host's
# find_package(Boost) result would add the host's system headers)
set(FATX_BOOST_INCLUDE_DIR "" CACHE PATH "Boost headers to use instead of find_package(Boost)")
if(FATX_BOOST_INCLUDE_DIR)
  add_library(fatx_boost_headers INTERFACE)
  target_include_directories(fatx_boost_headers SYSTEM INTERFACE ${FATX_BOOST_INCLUDE_DIR})
  add_library(Boost::headers ALIAS fatx_boost_headers)
else()
  find_package(Boost 1.65.0 REQUIRED)
endif()
option(FATX_BUILD_TOOLS "Build the fatx command-line tools (needs Boost.Program_options)" ${PROJECT_IS_TOP_LEVEL})
if(FATX_BUILD_TOOLS)
  find_package(Boost 1.65.0 COMPONENTS program_options REQUIRED)
  find_package(FUSE3 3.1)
  option(FATX_WITH_FUSE "Build fusefatx (needs FUSE3)" ${FUSE3_FOUND})
  if(FATX_WITH_FUSE AND NOT FUSE3_FOUND)
    message(FATAL_ERROR "FATX_WITH_FUSE=ON needs FUSE3 (libfuse3 development files)")
  endif()
else()
  set(FATX_WITH_FUSE OFF)
endif()
if(PROJECT_IS_TOP_LEVEL)
  find_package(Doxygen COMPONENTS dot)
endif()

# Marquer les bibliothèques tierces comme SYSTEM : leurs headers seront
# compilés avec -isystem et n'amèneront pas de warnings
if(TARGET Boost::headers AND NOT FATX_BOOST_INCLUDE_DIR)
  set_target_properties(Boost::headers PROPERTIES SYSTEM TRUE)
endif()
if(FATX_BUILD_TOOLS)
  set_target_properties(Boost::program_options PROPERTIES SYSTEM TRUE)
endif()
if(FATX_WITH_FUSE)
  set_target_properties(FUSE3::fuse3 PROPERTIES SYSTEM TRUE)
endif()

# Chercher GoogleTest uniquement si les tests sont activés
if(BUILD_TESTS)
  find_package(GTest QUIET)
  if(GTest_FOUND)
    set_target_properties(GTest::gtest GTest::gtest_main GTest::gmock PROPERTIES SYSTEM TRUE)
  else()
    # Si GTest n'est pas trouvé, essayer de le télécharger avec FetchContent
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
      SYSTEM          # headers compilés avec -isystem (CMake 3.25+)
    )
    # Empêcher l'installation de GTest
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
endif()

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 26)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

add_compile_options(
  -D_FILE_OFFSET_BITS=64
  -DPACKAGE_VERSION=${PROJECT_VERSION}
)
# Warnings (GCC flags) and -Werror only when building FATX itself
if(PROJECT_IS_TOP_LEVEL)
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wnrvo FATX_HAVE_WNRVO)
add_compile_options(
  -fdiagnostics-color
  -Wall
  -Wextra
  -Wpedantic
  -Walloca
  -Walloc-zero
  -Warray-bounds=2
  -Wattribute-alias=2
  -Wcast-align
  -Wcast-qual
  -Wcatch-value=3
  -Wconversion
  -Wctor-dtor-privacy
  -Wdisabled-optimization
  -Wdouble-promotion
  -Wduplicated-branches
  -Wduplicated-cond
  -Wextra-semi
  -Wformat=1
  -Wfloat-equal
  -Wlogical-op
  -Wmissing-declarations
  -Wmissing-format-attribute
  -Wmissing-noreturn
  -Wnoexcept
  -Wnoexcept-type
  -Wnon-virtual-dtor
  $<$<BOOL:${FATX_HAVE_WNRVO}>:-Wnrvo>
  -Wnull-dereference
  -Wold-style-cast
  -Woverloaded-virtual
  -Wplacement-new=2
  -Wredundant-decls
  -Wshadow
  -Wsign-conversion
  -Wsign-promo
  -Wstrict-null-sentinel
  -Wstrict-overflow=5
  -Wstringop-overflow=4
  -Wstringop-truncation
  -Wsuggest-attribute=pure
  -Wsuggest-attribute=const
  -Wsuggest-attribute=noreturn
  -Wsuggest-attribute=malloc
  -Wsuggest-attribute=format
  -Wsuggest-attribute=cold
  # -Wsuggest-final-methods
  # -Wsuggest-final-types
  -Wsuggest-override
  -Wswitch-default
  -Wswitch-enum
  -Wtrampolines
  -Wundef
  -Wuninitialized
  -Wunused
  -Wunused-macros
  -Wuseless-cast
  -Wunreachable-code
  -Wzero-as-null-pointer-constant
  -Wno-inline
)

add_compile_options($<$<CONFIG:Debug>:$<$<CXX_COMPILER_ID:MSVC>:/WX>>)
add_compile_options($<$<CONFIG:Debug>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>>)
# -Wnull-dereference génère des faux positifs en Release via l'inlining agressif
# de Boost.Bimap/multi_index dans les headers STL — supprimé en Release uniquement
add_compile_options($<$<CONFIG:Release>:-Wno-null-dereference>)
endif()
if(PROJECT_IS_TOP_LEVEL AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(NOT FATX_DBGCR MATCHES "^[0-9]+$")
    message(FATAL_ERROR "FATX_DBGCR must be a positive integer")
  endif()

  if(ENABLE_COVERAGE)
    message(STATUS "Enabling code coverage")
    add_compile_options(--coverage)
    add_link_options(--coverage)
  endif()

  add_compile_options(
    # -DDBG_INIT
    # -DDBG_READ
    # -DDBG_WRITE
    # -DDBG_DIFF
    # -DDBG_SEM
    # -DDBGSEM=\"Entry:fatx\"
    # -DDBG_BUFFER
    # -DDBGBUFDMP=64
    # -DDBG_CACHE
    # -DDBG_CACHDMP
    # -DDBG_AREAS
    # -DDBG_GUESS
    -DDBGCR=${FATX_DBGCR}
    -DDBGLIMIT=256
    # -DDBG_FAT
    # -DDBG_GAPS
    # -DNO_WRITE
    # -DNO_LOCK
    # -DNO_CACHE
  )
  set(FATX_SANITIZE_FLAGS
    -fsanitize=address
    -fsanitize=pointer-compare
    -fsanitize=pointer-subtract
    -fsanitize=leak
    -fsanitize=undefined
    -fsanitize=shift
    -fsanitize=shift-exponent
    -fsanitize=shift-base
    -fsanitize=integer-divide-by-zero
    -fsanitize=unreachable
    -fsanitize=vla-bound
    -fsanitize=null
    -fsanitize=return
    -fsanitize=signed-integer-overflow
    -fsanitize=bounds
    -fsanitize=alignment
    -fsanitize=object-size
    -fsanitize=float-divide-by-zero
    -fsanitize=float-cast-overflow
    -fsanitize=nonnull-attribute
    -fsanitize=returns-nonnull-attribute
    -fsanitize=bool
    -fsanitize=enum
    -fsanitize=vptr
    -fsanitize=pointer-overflow
  )

  if(SANITIZE)
    add_compile_options(
      ${FATX_SANITIZE_FLAGS}
      --param=max-gcse-memory=256000
      --param=max-vartrack-size=100000000
    )
    add_link_options(
      ${FATX_SANITIZE_FLAGS}
    )
endif()
endif()

# fatx_core: the filesystem library, without FUSE and without command-line
# parsing. Embedding applications link it (see src/volume.hpp and src/io.hpp).
add_library(fatx_core STATIC
  src/types.hpp
  src/constants.hpp
  src/context.hpp
  src/context.cpp
  src/device.hpp
  src/device.cpp
  src/frontend.hpp
  src/frontend.cpp
  src/partition.hpp
  src/partition.cpp
  src/entry.hpp
  src/entry.cpp
  src/diskmap.hpp
  src/diskmap.cpp
  src/utils.hpp
  src/utils.cpp
  src/io.hpp
  src/io.cpp
  src/actions.hpp
  src/actions.cpp
  src/volume.hpp
  src/volume.cpp
)
target_include_directories(fatx_core PUBLIC src)
target_compile_features(fatx_core PUBLIC cxx_std_20)
target_link_libraries(fatx_core PUBLIC Boost::headers)

if(FATX_BUILD_TOOLS)
# Bibliothèque objet partagée entre fatx et fatx_tests (exclut fatx.cpp qui contient main)
add_library(fatx_objects OBJECT
  src/frontend_args.cpp
  $<$<BOOL:${FATX_WITH_FUSE}>:src/fuse_ops.hpp>
  $<$<BOOL:${FATX_WITH_FUSE}>:src/fuse_ops.cpp>
)
target_link_libraries(fatx_objects PUBLIC fatx_core Boost::program_options)
if(FATX_WITH_FUSE)
  target_compile_definitions(fatx_objects PUBLIC FATX_WITH_FUSE=1)
  target_link_libraries(fatx_objects PUBLIC FUSE3::fuse3)
endif()

add_executable(fatx src/fatx.cpp)
target_link_libraries(fatx PRIVATE fatx_objects)

add_custom_target(
  test.sh
  ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/scripts/test.sh ${PROJECT_BINARY_DIR}
)
set(aliases mkfs.fatx label.fatx fsck.fatx unrm.fatx)
if(FATX_WITH_FUSE)
  list(PREPEND aliases fusefatx)
endif()
foreach(i ${aliases})
  add_custom_target(
    ${i}
    ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink fatx ${i}
    DEPENDS fatx
  )
  if(PROJECT_IS_TOP_LEVEL)
    install(FILES ${PROJECT_BINARY_DIR}/${i} TYPE SBIN)
  endif()
endforeach(i)
endif()

if(PROJECT_IS_TOP_LEVEL)
enable_testing()
# Tests of the embedding seams (log sink, contexts, io_backend, volume API):
# need only fatx_core
if(BUILD_TESTS AND (GTest_FOUND OR googletest_POPULATED))
  add_executable(fatx_core_tests tests/test_core.cpp)
  target_link_libraries(fatx_core_tests PRIVATE fatx_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(fatx_core_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1")
endif()
# Tests unitaires avec GoogleTest
if(BUILD_TESTS AND FATX_BUILD_TOOLS AND (GTest_FOUND OR googletest_POPULATED))
  add_executable(fatx_tests
    tests/test_context.cpp
    tests/test_device.cpp
    tests/test_diskmap.cpp
    tests/test_entry.cpp
    tests/test_frontend.cpp
    $<$<BOOL:${FATX_WITH_FUSE}>:tests/test_fuse_ops.cpp>
    $<$<BOOL:${FATX_WITH_FUSE}>:tests/test_integration.cpp>
    tests/test_partition.cpp
    tests/test_utils.cpp
  )
  # fatx_objects propage PUBLIC : fatx_core (src/ includes, Boost) + FUSE3
  target_link_libraries(fatx_tests PRIVATE
    fatx_objects
    GTest::gtest_main GTest::gmock
  )
  if(ENABLE_COVERAGE)
    target_compile_options(fatx_tests PRIVATE --coverage)
    target_link_options(fatx_tests PRIVATE --coverage)
  endif()

  include(GoogleTest)
  if(SANITIZE)
    gtest_discover_tests(
      fatx_tests
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
    )
  else()
    gtest_discover_tests(fatx_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  endif()
endif()

include(GNUInstallDirs)
if(FATX_BUILD_TOOLS)
  install(TARGETS fatx RUNTIME DESTINATION sbin)
endif()

# Génération des man pages depuis les sources Markdown (doc/man/*.8.md)
find_program(PANDOC pandoc)
if(PANDOC)
  set(_man_tools fatx fsck.fatx fusefatx label.fatx mkfs.fatx unrm.fatx)
  set(_man_outputs)
  file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/man8)
  foreach(_tool IN LISTS _man_tools)
    set(_src ${PROJECT_SOURCE_DIR}/doc/man/${_tool}.8.md)
    set(_out ${PROJECT_BINARY_DIR}/man8/${_tool}.8.gz)
    set(_troff ${PROJECT_BINARY_DIR}/man8/${_tool}.8)
    add_custom_command(
      OUTPUT ${_out}
      COMMAND ${PANDOC} -s -t man -o ${_troff} ${_src}
      COMMAND gzip -9 -f ${_troff}
      DEPENDS ${_src}
      COMMENT "Generating man page ${_tool}(8)"
      VERBATIM
    )
    list(APPEND _man_outputs ${_out})
  endforeach()
  add_custom_target(man DEPENDS ${_man_outputs})
  install(DIRECTORY ${PROJECT_BINARY_DIR}/man8/
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
    FILES_MATCHING PATTERN "*.gz"
  )
else()
  message(STATUS "pandoc not found: using pre-built man pages from man8/")
  add_custom_target(man
    COMMENT "pandoc not found: using pre-built man pages from man8/"
    VERBATIM
  )
  install(DIRECTORY man8 TYPE MAN)
endif()
install(
  FILES
  README
  COPYING
  TODO
  AUTHORS
  BUGS
  NEWS
  INSTALL
  ChangeLog
  doc/fatx.jpg
  TYPE DOC
)

if(DOXYGEN_FOUND)
  add_custom_target(doxygen
    COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/doc/doxygen.cfg
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating Doxygen documentation"
    VERBATIM
  )
  install(DIRECTORY ${CMAKE_BINARY_DIR}/html TYPE DOC OPTIONAL)
endif()

# Cible pour générer le rapport de couverture avec analyse par module (nécessite lcov)
find_program(PYTHON3_EXECUTABLE python3)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  find_program(LCOV_FOUND lcov)
  find_program(GENHTML_FOUND genhtml)
  if(LCOV_FOUND AND GENHTML_FOUND AND PYTHON3_EXECUTABLE)
    # Calculate number of parallel jobs for tests
    include(ProcessorCount)
    ProcessorCount(TEST_JOBS)
    if(NOT TEST_JOBS OR TEST_JOBS EQUAL 0)
      set(TEST_JOBS 1)
    endif()
    
    add_custom_target(coverage
          COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
            PYTHONPYCACHEPREFIX=${CMAKE_BINARY_DIR}/.pycache
              ${PYTHON3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/generate_coverage.py --build-dir ${CMAKE_BINARY_DIR} --jobs ${TEST_JOBS}
      COMMENT "Generating coverage report with module analysis"
      DEPENDS fatx_tests
    )
    add_custom_target(coverage-clean
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/coverage-report
      COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/coverage_final_report
      COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage.info
      COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/coverage_final.info
      COMMENT "Cleaning coverage reports"
    )
  else()
    message(STATUS "Coverage report target skipped: missing lcov (${LCOV_FOUND}), genhtml (${GENHTML_FOUND}), or python3 (${PYTHON3_EXECUTABLE})")
  endif()
endif()

set(CTEST_OUTPUT_ON_FAILURE 1)
find_program(BASH NAMES bash)
if(FATX_BUILD_TOOLS)
execute_process(
  COMMAND ${BASH} ${PROJECT_SOURCE_DIR}/scripts/test.sh
	OUTPUT_VARIABLE names
)
else()
  set(names "")
endif()
list(LENGTH names nbtst)
math(EXPR nb "${nbtst} - 1")
foreach(i RANGE 0 ${nb})
  if(nbtst EQUAL 0)
    break()
  endif()
  list(GET names ${i} test_name)
  if(NOT FATX_WITH_FUSE AND test_name MATCHES "^Fuse_|^Label_check_noname$|^Unrm_remote|^Unrm_local")
    message("-- Skipping test (needs fusefatx): ${test_name}")
    continue()
  endif()
  message("-- Found test: ${test_name}")
  add_test(NAME ${test_name} COMMAND ${BASH} test.sh ${i})
  if(SANITIZE)
    set_tests_properties(
      ${test_name}
      PROPERTIES
        TIMEOUT 180
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
    )
  else()
    set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR} TIMEOUT 180)
  endif()
endforeach(i)

# Add a convenient target to run all tests via CTest
add_custom_target(run_all_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS fatx_tests fatx
  COMMENT "Run all tests with CTest"
)

# Alias `run_tests` -> `run_all_tests` for convenience
add_custom_target(run_tests DEPENDS run_all_tests)

# Cible pour l'analyse de code mort (cppcheck + GCC -fanalyzer + couverture lcov)
if(PYTHON3_EXECUTABLE)
  add_custom_target(deadcode
      COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        PYTHONPYCACHEPREFIX=${CMAKE_BINARY_DIR}/.pycache
            ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_deadcode.py
            --build-dir ${CMAKE_BINARY_DIR}
            --src-dir   ${PROJECT_SOURCE_DIR}/src
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Analyse de code mort → ${CMAKE_BINARY_DIR}/deadcode_report/report.txt"
    VERBATIM
  )
endif()

# Cible pour les tests de fuites mémoire avec valgrind (memcheck + helgrind + drd)
find_program(VALGRIND_EXECUTABLE valgrind)
find_program(PYTHON3_EXECUTABLE python3)
include(ProcessorCount)
ProcessorCount(VALGRIND_JOBS)
if(NOT VALGRIND_JOBS OR VALGRIND_JOBS EQUAL 0)
  set(VALGRIND_JOBS 1)
endif()
if(PYTHON3_EXECUTABLE AND BUILD_TESTS)
  # Calculate number of parallel jobs for tests
  include(ProcessorCount)
  ProcessorCount(TEST_JOBS)
  if(NOT TEST_JOBS OR TEST_JOBS EQUAL 0)
    set(TEST_JOBS 1)
  endif()
  
  add_custom_target(sanitize
      COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        PYTHONPYCACHEPREFIX=${CMAKE_BINARY_DIR}/.pycache
            ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_sanitize.py
            --build-dir ${CMAKE_BINARY_DIR}
            --report-dir ${CMAKE_BINARY_DIR}/sanitize_report
            --run-tests
            --ctest ${CMAKE_CTEST_COMMAND}
            --jobs ${TEST_JOBS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fatx_tests
    COMMENT "Running sanitizer tests (ASAN/UBSAN)"
    VERBATIM
  )
endif()
if(VALGRIND_EXECUTABLE AND PYTHON3_EXECUTABLE AND BUILD_TESTS)
  add_custom_target(memcheck
      COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        PYTHONPYCACHEPREFIX=${CMAKE_BINARY_DIR}/.pycache
            ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_memcheck.py
            --build-dir ${CMAKE_BINARY_DIR}
            --binary ${CMAKE_BINARY_DIR}/fatx_tests
            --ctest ${CMAKE_CTEST_COMMAND}
            --jobs ${VALGRIND_JOBS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fatx_tests
    COMMENT "Running memory tests (memcheck)"
    VERBATIM
  )
  add_custom_target(helgrind
      COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        PYTHONPYCACHEPREFIX=${CMAKE_BINARY_DIR}/.pycache
            ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_helgrind.py
            --build-dir ${CMAKE_BINARY_DIR}
            --binary ${CMAKE_BINARY_DIR}/fatx_tests
            --ctest ${CMAKE_CTEST_COMMAND}
            --jobs ${VALGRIND_JOBS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fatx_tests
    COMMENT "Running memory tests (helgrind)"
    VERBATIM
  )
  add_custom_target(drd
      COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        PYTHONPYCACHEPREFIX=${CMAKE_BINARY_DIR}/.pycache
            ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_drd.py
            --build-dir ${CMAKE_BINARY_DIR}
            --binary ${CMAKE_BINARY_DIR}/fatx_tests
            --ctest ${CMAKE_CTEST_COMMAND}
            --jobs ${VALGRIND_JOBS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS fatx_tests
    COMMENT "Running memory tests (drd)"
    VERBATIM
  )
endif()
endif() # PROJECT_IS_TOP_LEVEL
