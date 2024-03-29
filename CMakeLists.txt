cmake_minimum_required(VERSION 3.0)
project(
  fatx
  VERSION 1.17
  LANGUAGES C CXX)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE
	  Release
	  CACHE STRING "Build type" FORCE)
endif(CMAKE_BUILD_TYPE STREQUAL "")

set(SANITIZE false)

include(FindBoost)
include(CTest)

find_package(
  Boost 1.65.0
  COMPONENTS program_options thread
  REQUIRED)
find_package(Doxygen COMPONENTS dot)
find_package(FUSE 2.9 REQUIRED)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(gcc_like_cxx "$<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>")
set(msvc_cxx "$<COMPILE_LANG_AND_ID:CXX,MSVC>")

include_directories(src)
link_directories(${Boost_LIBRARY_DIRS})

add_compile_options(
  -D_FILE_OFFSET_BITS=64
  -std=c++2b
  -fdiagnostics-color
  -Werror
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
  # -Wnull-dereference
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
  # -Wsuggest-attribute=pure
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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(
	-DDBG_INIT
	-DDBG_READ -DDBG_WRITE
	# -DDBG_DIFF
	# -DDBG_SEM
	# -DDBGSEM=\"Entry:fatx\"
	# -DDBG_BUFFER -DDBGBUFDMP=64 -DDBG_CACHE -DDBG_CACHDMP -DDBG_AREAS
	# -DDBG_GUESS
	-DDBGCR=32
	-DDBGLIMIT=256
	# -DDBG_FAT
	# -DDBG_GAPS -DNO_WRITE -DNO_LOCK -DNO_CACHE
)
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
	  --param=max-vartrack-size=100000000)
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
	  -fsanitize=pointer-overflow)
endif(SANITIZE)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

add_executable(
  fatx
  src/fatx.hpp
  src/fatx.cpp
  test.sh
  AUTHORS
  BUGS
  TODO
  README
  NEWS
  INSTALL
  doxygen.cfg
  ChangeLog)
target_link_libraries(fatx ${Boost_LIBRARIES} ${FUSE_LIBRARIES})

add_custom_target(
  test.sh ALL COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${PROJECT_SOURCE_DIR}/test.sh ${PROJECT_BINARY_DIR})
set(aliases fusefatx mkfs.fatx label.fatx fsck.fatx unrm.fatx)
foreach(i ${aliases})
  add_custom_target(
	${i} ALL
	COMMAND ${CMAKE_COMMAND} -E create_symlink fatx ${i}
	DEPENDS fatx)
install(FILES ${PROJECT_BINARY_DIR}/${i} TYPE SBIN)
endforeach(i)

include(GNUInstallDirs)
install(TARGETS fatx RUNTIME DESTINATION sbin)
install(DIRECTORY man8 TYPE MAN)
install(
  FILES README
  COPYING
  TODO
  AUTHORS
  BUGS
  NEWS
		INSTALL
		ChangeLog
		doc/fatx.jpg
  TYPE DOC)

set(DOXYGEN_PROJECT_LOGO doc/fatx.jpg)
doxygen_add_docs(doc src)
if(DOXYGEN_FOUND)
  # install(DIRECTORY ${CMAKE_BINARY_DIR}/html TYPE DOC)
endif(DOXYGEN_FOUND)

enable_testing()
set(CTEST_OUTPUT_ON_FAILURE 1)
find_program(BASH NAMES bash)
execute_process(COMMAND ${BASH} ${PROJECT_SOURCE_DIR}/test.sh
	OUTPUT_VARIABLE names)
list(LENGTH names nbtst)
math(EXPR nb "${nbtst} - 1")
foreach(i RANGE 0 ${nb})
  list(GET names ${i} test_name)
  message("-- Found test: ${test_name}")
  add_test(NAME ${test_name} COMMAND ${BASH} test.sh ${i})
endforeach(i)
