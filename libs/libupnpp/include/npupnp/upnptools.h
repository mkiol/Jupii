/*******************************************************************************
 *
 * Copyright (c) 2000-2003 Intel Corporation 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 *
 * * Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer. 
 * * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * * Neither name of Intel Corporation nor the names of its contributors 
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


#ifndef UPNP_TOOLS_H
#define UPNP_TOOLS_H


/** @file upnptools.h
 *
 * @brief UPnPTools Optional Tool API: dditional, optional utility API
 * that can be helpful in writing applications.
 */


#include "upnpconfig.h"    /* for UPNP_HAVE_TOOLS */

/* Function declarations only if tools compiled into the library */
#if UPNP_HAVE_TOOLS

#include "UpnpGlobal.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Converts an SDK error code into a text explanation.
 *
 * @return An ASCII text string representation of the error message associated
 *     with the error code or the string "Unknown error code"
 */
EXPORT_SPEC const char *UpnpGetErrorMessage(
    /** [in] The SDK error code to convert. */
    int rc);


/**
 * @brief Combines a base URL and a relative URL into a single absolute URL.
 *
 * The memory for \b AbsURL needs to be allocated by the caller and must
 * be large enough to hold the \b BaseURL and \b RelURL combined.
 *
 * @return An integer representing one of the following:
 *    \li <tt>UPNP_E_SUCCESS</tt>: The operation completed successfully.
 *    \li <tt>UPNP_E_INVALID_PARAM</tt>: \b RelURL is <tt>NULL</tt>.
 *    \li <tt>UPNP_E_INVALID_URL</tt>: The \b BaseURL / \b RelURL 
 *              combination does not form a valid URL.
 *    \li <tt>UPNP_E_OUTOF_MEMORY</tt>: Insufficient resources exist to 
 *              complete this operation.
 */
EXPORT_SPEC int UpnpResolveURL(
    /** [in] The base URL to combine. */
    const char *BaseURL,
    /** [in] The relative URL to \b BaseURL. */
    const char *RelURL,
    /** [out] A pointer to a buffer to store the absolute URL. */
    char *AbsURL);


/**
 * @brief Combines a base URL and a relative URL into a single absolute URL.
 *
 * The memory for \b AbsURL becomes owned by the caller and should be freed
 * later.
 *
 * @return An integer representing one of the following:
 *    \li <tt>UPNP_E_SUCCESS</tt>: The operation completed successfully.
 *    \li <tt>UPNP_E_INVALID_PARAM</tt>: \b RelURL is <tt>NULL</tt>.
 *    \li <tt>UPNP_E_INVALID_URL</tt>: The \b BaseURL / \b RelURL 
 *              combination does not form a valid URL.
 *    \li <tt>UPNP_E_OUTOF_MEMORY</tt>: Insufficient resources exist to 
 *              complete this operation.
 */
EXPORT_SPEC int UpnpResolveURL2(
    /** [in] The base URL to combine. */
    const char *BaseURL,
    /** [in] The relative URL to \b BaseURL. */
    const char *RelURL,
    /** [out] A pointer to a pointer to a buffer to store the
     * absolute URL. Must be freed later by the caller. */
    char **AbsURL);

#ifdef __cplusplus
}
#endif


/** @} */


#endif /* UPNP_HAVE_TOOLS */


#endif /* UPNP_TOOLS_H */

