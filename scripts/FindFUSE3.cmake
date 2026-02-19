# FindFUSE3.cmake - Find the FUSE3 library and headers
#
# Defines the following variables:
#   FUSE3_FOUND
#   FUSE3_INCLUDE_DIRS
#   FUSE3_LIBRARIES
#   FUSE3_DEFINITIONS
#   FUSE3_VERSION
#   FUSE3_MAJOR_VERSION
#   FUSE3_MINOR_VERSION
#
# Provides the imported target:
#   FUSE3::fuse3
#
# Usage:
#   find_package(FUSE3 3.1 REQUIRED)
#   target_link_libraries(your_target PRIVATE FUSE3::fuse3)

include(FindPackageHandleStandardArgs)

set(FUSE3_FOUND FALSE)
set(FUSE3_INCLUDE_DIRS "")
set(FUSE3_LIBRARIES "")
set(FUSE3_DEFINITIONS "")
set(FUSE3_VERSION "")

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_FUSE3 fuse3 QUIET)
  if(PC_FUSE3_FOUND)
    set(FUSE3_DEFINITIONS "${PC_FUSE3_CFLAGS_OTHER}")
    set(FUSE3_VERSION "${PC_FUSE3_VERSION}")
    # Collect include dirs from pkg-config
    set(_F3_PKG_INCDIRS "${PC_FUSE3_INCLUDE_DIRS}")
    if(NOT _F3_PKG_INCDIRS AND PC_FUSE3_INCLUDEDIR)
      set(_F3_PKG_INCDIRS "${PC_FUSE3_INCLUDEDIR}")
    endif()
    if(NOT _F3_PKG_INCDIRS)
      foreach(flag IN LISTS PC_FUSE3_CFLAGS PC_FUSE3_CFLAGS_OTHER)
        if(flag MATCHES "^-I(.+)")
          list(APPEND _F3_PKG_INCDIRS "${CMAKE_MATCH_1}")
        endif()
      endforeach()
    endif()

    # Collect library dirs from pkg-config
    set(_F3_PKG_LIBDIRS "${PC_FUSE3_LIBRARY_DIRS}")
    if(NOT _F3_PKG_LIBDIRS AND PC_FUSE3_LIBDIR)
      set(_F3_PKG_LIBDIRS "${PC_FUSE3_LIBDIR}")
    endif()
    if(NOT _F3_PKG_LIBDIRS)
      foreach(flag IN LISTS PC_FUSE3_LDFLAGS PC_FUSE3_LDFLAGS_OTHER)
        if(flag MATCHES "^-L(.+)")
          list(APPEND _F3_PKG_LIBDIRS "${CMAKE_MATCH_1}")
        endif()
      endforeach()
    endif()
  endif()
endif()

# Find headers
find_path(
  FUSE3_INCLUDE_DIRS
  NAMES fuse3/fuse.h fuse.h
  HINTS ${_F3_PKG_INCDIRS}
  PATH_SUFFIXES fuse3
  DOC "FUSE3 include directory"
)

# Fallback common locations
if(NOT FUSE3_INCLUDE_DIRS)
  foreach(_cand 
    "/usr/include/fuse3" 
    "/usr/local/include/fuse3" 
    "/opt/homebrew/include/fuse3" 
    "/opt/local/include/fuse3" 
    "/usr/include" 
    "/usr/local/include")
    if(EXISTS "${_cand}/fuse.h" OR EXISTS "${_cand}/fuse3/fuse.h")
      set(FUSE3_INCLUDE_DIRS "${_cand}")
      break()
    endif()
  endforeach()
endif()

# Find library
find_library(
  FUSE3_LIBRARIES
  NAMES fuse3
  HINTS ${_F3_PKG_LIBDIRS}
  PATHS
    /usr/lib
    /usr/lib64
    /lib
    /lib64
    /usr/local/lib
    /usr/local/lib64
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /lib/x86_64-linux-gnu
    /lib/aarch64-linux-gnu
  DOC "FUSE3 library"
)

# Fallback: use pkg-config libs if absolute path not found
if(NOT FUSE3_LIBRARIES AND PC_FUSE3_FOUND)
  if(PC_FUSE3_LIBRARIES)
    # A list of bare library names, e.g., fuse3;pthread
    set(FUSE3_LIBRARIES "${PC_FUSE3_LIBRARIES}")
  elseif(PC_FUSE3_LDFLAGS)
    # A string of -l/-L flags; acceptable as a last resort
    set(FUSE3_LIBRARIES "${PC_FUSE3_LDFLAGS}")
  else()
    set(FUSE3_LIBRARIES "fuse3")
  endif()
endif()

# Extract version from headers if needed
if(FUSE3_INCLUDE_DIRS)
  if(EXISTS "${FUSE3_INCLUDE_DIRS}/fuse_common.h")
    set(_F3_HEADER "${FUSE3_INCLUDE_DIRS}/fuse_common.h")
  elseif(EXISTS "${FUSE3_INCLUDE_DIRS}/fuse3/fuse_common.h")
    set(_F3_HEADER "${FUSE3_INCLUDE_DIRS}/fuse3/fuse_common.h")
  endif()
  if(_F3_HEADER)
    file(READ "${_F3_HEADER}" _f3_contents)
    string(REGEX REPLACE ".*# *define *FUSE_MAJOR_VERSION *([0-9]+).*" "\\1" FUSE3_MAJOR_VERSION "${_f3_contents}")
    string(REGEX REPLACE ".*# *define *FUSE_MINOR_VERSION *([0-9]+).*" "\\1" FUSE3_MINOR_VERSION "${_f3_contents}")
    if(NOT FUSE3_VERSION AND FUSE3_MAJOR_VERSION AND FUSE3_MINOR_VERSION)
      set(FUSE3_VERSION "${FUSE3_MAJOR_VERSION}.${FUSE3_MINOR_VERSION}")
    endif()
  endif()
endif()

# Evaluate result
if(FUSE3_INCLUDE_DIRS AND FUSE3_LIBRARIES)
  set(FUSE3_FOUND TRUE)
endif()

find_package_handle_standard_args(
  FUSE3
  REQUIRED_VARS FUSE3_LIBRARIES FUSE3_INCLUDE_DIRS
  VERSION_VAR FUSE3_VERSION)

# Imported target
if(FUSE3_FOUND AND NOT TARGET FUSE3::fuse3)
  add_library(FUSE3::fuse3 INTERFACE IMPORTED)
  # Support both <fuse3/fuse.h> and <fuse.h>
  get_filename_component(_F3_PARENT "${FUSE3_INCLUDE_DIRS}" DIRECTORY)
  if(EXISTS "${FUSE3_INCLUDE_DIRS}/fuse.h")
    set_property(TARGET FUSE3::fuse3 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FUSE3_INCLUDE_DIRS};${_F3_PARENT}")
  else()
    set_property(TARGET FUSE3::fuse3 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${FUSE3_INCLUDE_DIRS};${FUSE3_INCLUDE_DIRS}/fuse3")
  endif()
  set_property(TARGET FUSE3::fuse3 PROPERTY INTERFACE_LINK_LIBRARIES "${FUSE3_LIBRARIES}")
  if(FUSE3_DEFINITIONS)
    set_property(TARGET FUSE3::fuse3 PROPERTY INTERFACE_COMPILE_DEFINITIONS "${FUSE3_DEFINITIONS}")
  endif()
endif()
