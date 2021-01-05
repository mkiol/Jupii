#ifndef _UPNPPEXPORTS_HXX_INCLUDED_
#define _UPNPPEXPORTS_HXX_INCLUDED_


#if defined _WIN32 || defined __CYGWIN__
  #define UPNPP_HELPER_DLL_IMPORT __declspec(dllimport)
  #define UPNPP_HELPER_DLL_EXPORT __declspec(dllexport)
  #define UPNPP_HELPER_DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define UPNPP_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define UPNPP_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define UPNPP_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define UPNPP_HELPER_DLL_IMPORT
    #define UPNPP_HELPER_DLL_EXPORT
    #define UPNPP_HELPER_DLL_LOCAL
  #endif
#endif

#ifdef UPNPP_DLL // defined if UPNPP is compiled as a DLL
  #ifdef UPNPP_DLL_EXPORTS // defined if we are building the UPNPP DLL
    #define UPNPP_API UPNPP_HELPER_DLL_EXPORT
  #else
    #define UPNPP_API UPNPP_HELPER_DLL_IMPORT
  #endif // UPNPP_DLL_EXPORTS
  #define UPNPP_LOCAL UPNPP_HELPER_DLL_LOCAL
#else // UPNPP_DLL is not defined: this means UPNPP is a static lib.
  #define UPNPP_API
  #define UPNPP_LOCAL
#endif // UPNPP_DLL

#endif /* _UPNPPEXPORTS_HXX_INCLUDED_ */
