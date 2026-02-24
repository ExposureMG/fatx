cmake_minimum_required(VERSION 3.30..3.31)
project(
  fatx
  VERSION 1.18
  LANGUAGES C CXX
)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/scripts)

# Option pour activer les tests
option(BUILD_TESTS "Build test suite" ON)
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SANITIZE "Enable sanitizers" OFF)

include(CTest)

find_package(
  Boost 1.65.0
  COMPONENTS program_options
  REQUIRED
)
find_package(Doxygen COMPONENTS dot)
find_package(FUSE3 3.1 REQUIRED)

# Chercher GoogleTest pour les tests unitaires
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  # Si GTest n'est pas trouvé, essayer de le télécharger avec FetchContent
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  # Empêcher l'installation de GTest
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
  -D_FILE_OFFSET_BITS=64
  -fdiagnostics-color
  -DPACKAGE_VERSION=${PROJECT_VERSION}
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
  -Wnrvo
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
  -Wsuggest-final-methods
  -Wsuggest-final-types
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

add_compile_options($<$<OR:$<CONFIG:Debug>,$<BOOL:$ENV{CI}>>:$<$<CXX_COMPILER_ID:MSVC>:/WX>>)
add_compile_options($<$<OR:$<CONFIG:Debug>,$<BOOL:$ENV{CI}>>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>>)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
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
    -DDBGCR=32
    -DDBGLIMIT=256
    # -DDBG_FAT
    # -DDBG_GAPS
    # -DNO_WRITE
    # -DNO_LOCK
    # -DNO_CACHE
  )
  # Flags de couverture toujours actifs en Debug (génération .gcno/.gcda)
  add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs>)
  add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-ftest-coverage>)
  add_link_options($<$<CXX_COMPILER_ID:GNU,Clang>:--coverage>)
  if(SANITIZE)
    add_compile_options(
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
      # -fsanitize=bounds-strict
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
      --param=max-gcse-memory=256000
      --param=max-vartrack-size=100000000
    )
    add_link_options(
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
      -fsanitize=bounds-strict
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
endif()
endif()

# Bibliothèque objet partagée entre fatx et fatx_tests (exclut fatx.cpp qui contient main)
add_library(fatx_objects OBJECT
  src/types.hpp
  src/constants.hpp
  src/context.hpp
  src/context.cpp
  src/fuse_ops.hpp
  src/fuse_ops.cpp
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
)
target_include_directories(fatx_objects PUBLIC src)
target_link_libraries(fatx_objects PUBLIC Boost::program_options FUSE3::fuse3)

add_executable(fatx src/fatx.cpp)
target_link_libraries(fatx PRIVATE fatx_objects)

add_custom_target(
  test.sh
  ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/scripts/test.sh ${PROJECT_BINARY_DIR}
)
set(aliases fusefatx mkfs.fatx label.fatx fsck.fatx unrm.fatx)
foreach(i ${aliases})
  add_custom_target(
    ${i}
    ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink fatx ${i}
    DEPENDS fatx
  )
  install(FILES ${PROJECT_BINARY_DIR}/${i} TYPE SBIN)
endforeach(i)

enable_testing()
# Tests unitaires avec GoogleTest
if(BUILD_TESTS AND (GTest_FOUND OR googletest_POPULATED))
  add_executable(fatx_tests
    tests/test_context.cpp
    tests/test_device.cpp
    tests/test_diskmap.cpp
    tests/test_entry.cpp
    tests/test_frontend.cpp
    tests/test_fuse_ops.cpp
    tests/test_integration.cpp
    tests/test_partition.cpp
    tests/test_utils.cpp
  )
  # fatx_objects propage PUBLIC : src/ includes + Boost + FUSE3
  target_link_libraries(fatx_tests PRIVATE
    fatx_objects
    GTest::gtest_main GTest::gmock
  )
  if(ENABLE_COVERAGE)
    target_compile_options(fatx_tests PRIVATE --coverage)
    target_link_options(fatx_tests PRIVATE --coverage)
  endif()

  include(GoogleTest)
  gtest_discover_tests(fatx_tests WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

include(GNUInstallDirs)
install(TARGETS fatx RUNTIME DESTINATION sbin)

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
  add_custom_target(man ALL DEPENDS ${_man_outputs})
  install(DIRECTORY ${PROJECT_BINARY_DIR}/man8/
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
    FILES_MATCHING PATTERN "*.gz"
  )
else()
  message(STATUS "pandoc not found: using pre-built man pages from man8/")
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
  add_custom_target(doxygen EXCLUDE_FROM_ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_SOURCE_DIR}/doc/doxygen.cfg
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating Doxygen documentation"
    VERBATIM
  )
  install(DIRECTORY ${CMAKE_BINARY_DIR}/html TYPE DOC)
endif()

# Cible pour générer le rapport de couverture avec analyse par module (nécessite lcov)
find_program(PYTHON3_EXECUTABLE python3)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  find_program(LCOV_FOUND lcov)
  find_program(GENHTML_FOUND genhtml)
  if(LCOV_FOUND AND GENHTML_FOUND AND PYTHON3_EXECUTABLE)
    add_custom_target(coverage
      COMMAND ${PYTHON3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/generate_coverage.py --build-dir ${CMAKE_BINARY_DIR}
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
execute_process(
  COMMAND ${BASH} ${PROJECT_SOURCE_DIR}/scripts/test.sh
	OUTPUT_VARIABLE names
)
list(LENGTH names nbtst)
math(EXPR nb "${nbtst} - 1")
foreach(i RANGE 0 ${nb})
  list(GET names ${i} test_name)
  message("-- Found test: ${test_name}")
  add_test(NAME ${test_name} COMMAND ${BASH} test.sh ${i})
  set_tests_properties(${test_name} PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
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
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_deadcode.py
            --build-dir ${CMAKE_BINARY_DIR}
            --src-dir   ${PROJECT_SOURCE_DIR}/src
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Analyse de code mort → ${CMAKE_BINARY_DIR}/deadcode_report/report.txt"
    VERBATIM
  )
endif()
