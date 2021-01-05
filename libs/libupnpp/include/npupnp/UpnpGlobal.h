#ifndef UPNPGLOBAL_H
#define UPNPGLOBAL_H

/*!
 * \file
 *
 * \brief Visibility defines and such
 */

#ifdef _WIN32

#  ifdef UPNP_STATIC_LIB
#    define EXPORT_SPEC
#  else /* UPNP_STATIC_LIB */
#    ifdef LIBUPNP_EXPORTS
#      define EXPORT_SPEC __declspec(dllexport)
#    else /* LIBUPNP_EXPORTS */
#        define EXPORT_SPEC __declspec(dllimport)
#    endif /* LIBUPNP_EXPORTS */
#  endif /* UPNP_STATIC_LIB */

#else /* Not windows -> */

#  if __GNUC__ >= 4
#    define EXPORT_SPEC __attribute__ ((visibility ("default")))
#  else
#    define EXPORT_SPEC
#  endif

#endif

/* Sized integer types. */
#include <stdint.h>

#endif /* UPNPGLOBAL_H */
