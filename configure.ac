AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIR([m4])
AC_INIT([fatx], [1.15], [fatx@christopheduverger.fr], [fatx], [http://sourceforge.net/projects/fatx/])
AM_INIT_AUTOMAKE
AC_ARG_ENABLE([dbg], [AS_HELP_STRING([--enable-dbg], [build with additional debugging code])], [CPPFLAGS+=' -D DEBUG'])
AM_CONDITIONAL([ENABLE_DBG], [test "$enable_dbg" = "yes"])
#AC_ARG_ENABLE([gdb], [AS_HELP_STRING([--enable-gdb], [build with -g and low optimisation])], [CXXFLAGS+='-g'; CPPFLAGS+='-O1'], [CPPFLAGS+='-O6'])
#AM_CONDITIONAL([ENABLE_GDB], [test "$enable_gdb" = "yes"])
AC_ARG_ENABLE([xbe], [AS_HELP_STRING([--enable-xbe], [produce .xbe files])])
AM_CONDITIONAL([ENABLE_XBE], [test "$enable_xbe" = "yes"])
AC_CHECK_LIB([fuse], [main])
AM_CONDITIONAL([USE_LIBFUSE], [grep HAVE_LIBFUSE confdefs.h > /dev/null && ls /dev/fuse >/dev/null])
AM_MAINTAINER_MODE([disable])
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX([14], [noext], [mandatory])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AC_HEADER_STDC
AX_BOOST_BASE([1.55])
AX_BOOST_PROGRAM_OPTIONS
AC_SUBST([BOOST_PROGRAM_OPTIONS_A], [${BOOST_LDFLAGS:2}/lib${BOOST_PROGRAM_OPTIONS_LIB:2}.a])
AC_CHECK_LIB([pthread], [main])
AM_CONDITIONAL([USE_LIBPTHREAD], [grep HAVE_LIBPTHREAD confdefs.h > /dev/null])
AC_CHECK_HEADERS([fcntl.h string.h unistd.h])
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset])
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([$PACKAGE_NAME], [doxygen.cfg], [doc])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

