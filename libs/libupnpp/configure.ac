
# Software version. There is no direct link with the library version_info
# except that we sort or promise that no api and abi incompatibilities
# occur with revision (3rd number) changes.
# ! When changing the version, also change the defines in upnpplib.hxx !
AC_INIT([libupnpp], [0.15.3], [jfd@lesbonscomptes.com],
             [libupnpp], [http://www.lesbonscomptes.com/upmpdcli])

# Lib version info. See:
# https://www.sourceware.org/autobook/autobook/autobook_91.html#SEC91
# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
#
# - Start with version information of ‘0:0:0’ for each libtool library.
# - Update the version information only immediately before a public release
# - If the library source code has changed at all since the last update, then
#   increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# - If any interfaces have been added, removed, or changed since the last
#   update, increment current, and set revision to 0.
# - If any interfaces have been added since the last public release, then
#   increment age.
# - If any interfaces have been removed or changed since the last public
#   release, then set age to 0 AND CHANGE PACKAGE NAME. 
# libupnpp packages are named libupnppX where X is the .so major number
# (c-a). This allows packages for multiple incompatible ABIs to be
# installed
VERSION_INFO=6:2:0

AC_PREREQ([2.53])
AC_CONFIG_SRCDIR([libupnpp/upnpplib.hxx])
AC_CONFIG_HEADERS([libupnpp/config.h])
AH_BOTTOM([#include "libupnpp/conf_post.h"])

AM_INIT_AUTOMAKE([1.10 no-define subdir-objects foreign])
AC_DISABLE_STATIC
LT_INIT

AC_PROG_CXX
AC_PROG_LIBTOOL

# libupnp is configured with large file support, and we need to do the same,
# else a difference in off_t size impacts struct File_Info and prevents the
# vdir to work. This does make a difference, for exemple, for Raspbian
# on the Raspberry PI. Use the same directives as libupnp's configure.ac
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_DEFINE([_LARGE_FILE_SOURCE], [], [Large files support])
AC_DEFINE([_FILE_OFFSET_BITS], [64], [File Offset size])
AC_CHECK_LIB([rt], [clock_gettime], [], [])
AC_CHECK_LIB([pthread], [pthread_create], [], [])

# Check that std::future is available.
AC_LANG_PUSH([C++])
CXXFLAGS="-std=c++11 $CXXFLAGS"
AC_MSG_CHECKING([whether std::future is available])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <future>]],
		[[std::future<int> f;]])],
	[ AC_DEFINE([HAVE_STD_FUTURE], [1],
		[Define to 1 if you have the `std::future`.])
	  have_std_future=yes
	],
	[
	  have_std_future=no
	]
)
AC_MSG_RESULT([$have_std_future])
AC_LANG_POP

dnl The following check for libthreadutil is normally unnecessary and even
dnl problematic. libupnpp does not use it directly, Still you may need it to
dnl add it on some systems.
dnl AC_CHECK_LIB([threadutil], [TimerThreadRemove], [],
dnl     AC_MSG_ERROR([libthreadutil (part of libupnp) not found]))

PKG_CHECK_MODULES([upnp], [libupnp], [], AC_MSG_ERROR([libupnp not found]))
PKG_CHECK_MODULES([curl], [libcurl], [], AC_MSG_ERROR([libcurl not found]))
PKG_CHECK_MODULES([expat], [expat], [],AC_MSG_ERROR([expat not found]))

LIBUPNPP_LIBS="$LIBS $upnp_LIBS $curl_LIBS $expat_LIBS"
echo "LIBUPNPP_LIBS $LIBUPNPP_LIBS"

LIBS="$upnp_LIBS"
AC_CHECK_FUNCS([getifaddrs] [UpnpSetLogLevel])

LIBS=""
                     
AC_SUBST(LIBUPNPP_LIBS)
AC_SUBST(VERSION_INFO)

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libupnpp.pc])
AC_OUTPUT
