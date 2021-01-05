/*******************************************************************************
 *
 * Copyright (c) 2000-2003 Intel Corporation
 * Copyright (c) 2006 Rémi Turboult <r3mi@users.sourceforge.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * - Neither name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef UPNP_DEBUG_H
#define UPNP_DEBUG_H

/** @file upnpdebug.h
 * @brief libnpupnp message log definitions */

#include "upnpconfig.h"
#include "UpnpGlobal.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Describe the code area generating the message */
typedef enum Upnp_Module {
    /** SSDP (discovery) client and server. */
    SSDP,
    /** SOAP (actions) client and server. */
    SOAP,
    /** GENA (events) client and server */
    GENA,
    /** Thread pool */
    TPOOL,
    /** Network dispatcher */
    MSERV,
    DOM,
    /** Interface. */
    API,
    /** WEB server. */
    HTTP
} Dbg_Module;

/** @brief Log verbosity level, from UPNP_CRITICAL to UPNP_ALL, in
 * increasing order of verbosity.
 */
typedef enum Upnp_LogLevel {
    /** Fatal error, the library is probably not functional any more. */
    UPNP_CRITICAL,
    /** Regular operational, usually local error. */
    UPNP_ERROR,
    /** Interesting information, error caused by the remote. */
    UPNP_INFO,
    /** Debugging traces. */
    UPNP_DEBUG,
    /** Very verbose debugging traces. */
    UPNP_ALL
} Upnp_LogLevel;

/** Default log level */
#define UPNP_DEFAULT_LOG_LEVEL  UPNP_ERROR

/** @brief Initialize the log output. Can be called before @ref UpnpInit2.
 *
 * @return -1 for failure or UPNP_E_SUCCESS for success.
 */
EXPORT_SPEC int UpnpInitLog(void);

/** @brief Set the log verbosity level. */
EXPORT_SPEC void UpnpSetLogLevel(
    /** [in] Log level. */
    Upnp_LogLevel log_level);

/** @brief Closes the log output, if appropriate. */
EXPORT_SPEC void UpnpCloseLog(void);

/** @brief Set the name for the log file. You will then need to call
 * @ref UpnpInitLog to close the old file if needed, and open the new one. */
EXPORT_SPEC void UpnpSetLogFileNames(
    /** [in] Name of the log file. NULL or empty to use stderr. */
    const char *fileName,
    /** Ignored, used to be a second file. */
    const char *Ignored);

/**
 * @brief Use the level/module to determine if a message should be emitted.
 *
 * @return NULL if the log is not active for this module / level, else
 *  the output file pointer.
 */
EXPORT_SPEC FILE *UpnpGetDebugFile(
    /** [in] The level of the debug logging. It will decide whether debug
     * statement will go to standard output, or any of the log files. */
    Upnp_LogLevel level,
    /** [in] debug will go in the name of this module. */
    Dbg_Module module);

/** @brief Prints the debug statement to the current output */
EXPORT_SPEC void UpnpPrintf(
    /** [in] Message level, to be compared to the current verbosity. */
    Upnp_LogLevel DLevel,
    /** [in] Emitting code area. */
    Dbg_Module Module,
    /** [in] Source file name (usually __FILE__). */
    const char *DbgFileName,
    /** [in] Source line number (usually __LINE__). */
    int DbgLineNo,
    /** [in] Printf-like format specification. */
    const char *FmtStr,
    /** [in] Printf-like arguments. */
    ...)
#if (__GNUC__ >= 3)
/* This enables printf like format checking by the compiler. */
    __attribute__ ((format(__printf__, 5, 6)))
#endif
    ;


#ifdef __cplusplus
}
#endif

#endif /* UPNP_DEBUG_H */
