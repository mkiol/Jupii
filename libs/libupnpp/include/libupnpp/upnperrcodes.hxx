/* Duplicate upnp.h errcodes, which are the sole reason for a libupnpp
   client to include upnp.h. Makes things much easier now that upnp.h
   can be found either in npupnp/ or upnp/ */

#ifndef UPNP_E_NO_WEB_SERVER
#define UPNP_E_SUCCESS            0
#define UPNP_E_INVALID_HANDLE        -100
#define UPNP_E_INVALID_PARAM        -101
#define UPNP_E_OUTOF_HANDLE        -102
#define UPNP_E_OUTOF_CONTEXT        -103
#define UPNP_E_OUTOF_MEMORY        -104
#define UPNP_E_INIT            -105
#define UPNP_E_BUFFER_TOO_SMALL        -106
#define UPNP_E_INVALID_DESC        -107
#define UPNP_E_INVALID_URL        -108
#define UPNP_E_INVALID_SID        -109
#define UPNP_E_INVALID_DEVICE        -110
#define UPNP_E_INVALID_SERVICE        -111
#define UPNP_E_BAD_RESPONSE        -113
#define UPNP_E_BAD_REQUEST        -114
#define UPNP_E_INVALID_ACTION        -115
#define UPNP_E_FINISH            -116
#define UPNP_E_INIT_FAILED        -117
#define UPNP_E_URL_TOO_BIG        -118
#define UPNP_E_BAD_HTTPMSG        -119
#define UPNP_E_ALREADY_REGISTERED    -120
#define UPNP_E_INVALID_INTERFACE    -121
#define UPNP_E_NETWORK_ERROR        -200
#define UPNP_E_SOCKET_WRITE        -201
#define UPNP_E_SOCKET_READ        -202
#define UPNP_E_SOCKET_BIND        -203
#define UPNP_E_SOCKET_CONNECT        -204
#define UPNP_E_OUTOF_SOCKET        -205
#define UPNP_E_LISTEN            -206
#define UPNP_E_TIMEDOUT            -207
#define UPNP_E_SOCKET_ERROR        -208
#define UPNP_E_FILE_WRITE_ERROR        -209
#define UPNP_E_CANCELED            -210
#define UPNP_E_EVENT_PROTOCOL        -300
#define UPNP_E_SUBSCRIBE_UNACCEPTED    -301
#define UPNP_E_UNSUBSCRIBE_UNACCEPTED    -302
#define UPNP_E_NOTIFY_UNACCEPTED    -303
#define UPNP_E_INVALID_ARGUMENT        -501
#define UPNP_E_FILE_NOT_FOUND        -502
#define UPNP_E_FILE_READ_ERROR        -503
#define UPNP_E_EXT_NOT_XML        -504
#define UPNP_E_NO_WEB_SERVER        -505
#define UPNP_E_OUTOF_BOUNDS        -506
#define UPNP_E_NOT_FOUND        -507
#define UPNP_E_INTERNAL_ERROR        -911
#define UPNP_SOAP_E_INVALID_ACTION    401
#define UPNP_SOAP_E_INVALID_ARGS    402
#define UPNP_SOAP_E_OUT_OF_SYNC        403
#define UPNP_SOAP_E_INVALID_VAR        404
#define UPNP_SOAP_E_ACTION_FAILED    501
#endif /* not defined(UPNP_E_SUCCESS) */
