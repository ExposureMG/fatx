cmake_minimum_required(VERSION 3.30..3.31)
project(
  fatx
  VERSION 1.18
  LANGUAGES C CXX
)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

# Option pour activer les tests
option(BUILD_TESTS "Build test suite" ON)
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(
    CMAKE_BUILD_TYPE
    Release
    CACHE STRING "Build type" FORCE
  )
endif(CMAKE_BUILD_TYPE STREQUAL "")

set(SANITIZE false)

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

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

add_compile_options(
  -D_FILE_OFFSET_BITS=64
  # -fdiagnostics-color
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
  -Winline
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
  -Wstrict-overflow=2
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
  -Wzero-as-null-pointer-constant
  -Wno-inline)

add_compile_options($<$<OR:$<CONFIG:Debug>,$<BOOL:$ENV{CI}>>:$<$<CXX_COMPILER_ID:MSVC>:/WX>>)
add_compile_options($<$<OR:$<CONFIG:Debug>,$<BOOL:$ENV{CI}>>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>>)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
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
  # Ajouter les flags de couverture pour GCC/Clang en mode Debug
  add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs>)
  add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-ftest-coverage>)
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
  endif(SANITIZE)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

add_executable(
  fatx
  src/types.hpp
  src/constants.hpp
  src/fatx.hpp
  src/fatx.cpp
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
target_compile_features(fatx PRIVATE cxx_std_26)
target_include_directories(fatx PRIVATE src)
target_link_options(fatx PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs>)
target_link_options(fatx PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-ftest-coverage>)
if(Boost_FOUND)
  target_link_libraries(fatx Boost::program_options)
endif()
if(FUSE3_FOUND)
  target_link_libraries(fatx FUSE3::fuse3)
endif()

add_custom_target(
  test.sh
  ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/test.sh ${PROJECT_BINARY_DIR}
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
if(GTest_FOUND OR googletest_POPULATED)
  # Créer l'exécutable de test pour les constantes et composants de base
  add_executable(fatx_tests
    tests/test_basic.cpp
  )

  target_compile_features(fatx_tests PRIVATE cxx_std_26)
  target_include_directories(fatx_tests PRIVATE src)

  # Lier avec GoogleTest
  target_link_libraries(fatx_tests GTest::gtest_main GTest::gmock)

  # Ajouter les tests à CTest
  include(GoogleTest)
  gtest_discover_tests(fatx_tests)

  # Ajouter une dépendance pour que les tests soient compilés après l'exécutable principal
  add_dependencies(fatx_tests fatx)
endif()

option(CODE_COVERAGE "Enable code coverage reporting" OFF)

if(CODE_COVERAGE)
    message(STATUS "Enabling code coverage")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -lgcov --coverage")
    
    # Ajouter les flags uniquement à la cible de test
    if(TARGET fatx_tests)
        target_compile_options(fatx_tests PRIVATE --coverage)
        target_link_libraries(fatx_tests PRIVATE --coverage)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS fatx RUNTIME DESTINATION sbin)
install(DIRECTORY man8 TYPE MAN)
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

set(DOXYGEN_PROJECT_LOGO doc/fatx.jpg)
doxygen_add_docs(doc src)
if(DOXYGEN_FOUND)
  # install(DIRECTORY ${CMAKE_BINARY_DIR}/html TYPE DOC)
endif(DOXYGEN_FOUND)

# Cible pour générer le rapport de couverture (nécessite lcov)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  find_program(LCOV_FOUND lcov)
  if(LCOV_FOUND)
    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E make_directory coverage-report
      COMMAND lcov --capture --directory ${CMAKE_BINARY_DIR} --output-file coverage.info --no-external
      COMMAND lcov --remove coverage.info '/usr/*' '*/test_*' '*/tests/*' --output-file coverage.info
      COMMAND genhtml coverage.info --output-directory coverage-report
      COMMENT "Generating coverage report with lcov"
      DEPENDS fatx
    )
    add_custom_target(coverage-clean
      COMMAND ${CMAKE_COMMAND} -E remove_directory coverage-report
      COMMAND ${CMAKE_COMMAND} -E remove coverage.info
      COMMENT "Cleaning coverage report"
    )
  endif(LCOV_FOUND)
endif()

set(CTEST_OUTPUT_ON_FAILURE 1)
find_program(BASH NAMES bash)
execute_process(
  COMMAND ${BASH} ${PROJECT_SOURCE_DIR}/test.sh
	OUTPUT_VARIABLE names
)
list(LENGTH names nbtst)
math(EXPR nb "${nbtst} - 1")
foreach(i RANGE 0 ${nb})
  list(GET names ${i} test_name)
  message("-- Found test: ${test_name}")
  add_test(NAME ${test_name} COMMAND ${BASH} test.sh ${i})
endforeach(i)
