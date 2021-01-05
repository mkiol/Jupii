/*******************************************************************************
 *
 * Copyright (C) 2020 J.F. Dockes <jf@dockes.org>
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
#ifndef _UPNPDEV_HXX_INCLUDED_
#define _UPNPDEV_HXX_INCLUDED_

/*
 * Store parsed XML description document into more convenient c++ structures.
 * Keep field names compatible with the libupnpp version to facilitate
 * a possible future merge.
 */

#include <vector>
#include <string>

#include "UpnpGlobal.h"

class EXPORT_SPEC UPnPServiceDesc {
public:
    /// Service Type e.g. urn:schemas-upnp-org:service:ConnectionManager:1
    std::string serviceType;
    /// Service Id inside device: e.g. urn:upnp-org:serviceId:ConnectionManager
    std::string serviceId; 
    /// Service description URL.
    std::string SCPDURL;
    /// Service control URL.
    std::string controlURL; 
    /// Service event URL.
    std::string eventSubURL;
};

class EXPORT_SPEC UPnPDeviceDesc {
public:
    UPnPDeviceDesc(const std::string& url, const std::string& description);
    UPnPDeviceDesc() {}

    /// Parse success status.
    bool ok{false};
    /// Device Type: e.g. urn:schemas-upnp-org:device:MediaServer:1
    std::string deviceType;
    /// User-configurable name (usually), e.g. Lounge-streamer
    std::string friendlyName;
    /// Unique Device Number. This is the same as the deviceID in the
    /// discovery message. e.g. uuid:a7bdcd12-e6c1-4c7e-b588-3bbc959eda8d
    std::string UDN;
    /// Base for all relative URLs. e.g. http://192.168.4.4:49152/
    std::string URLBase;
    /// Manufacturer: e.g. D-Link, PacketVideo
    std::string manufacturer;
    /// Model name: e.g. MediaTomb, DNS-327L
    std::string modelName;
    /// Raw downloaded document.
    std::string XMLText;
    
    /// Services provided by this device.
    std::vector<UPnPServiceDesc> services;

    /// Embedded devices. We use UPnPDeviceDesc for convenience, but
    /// they can't recursively have embedded devices (and they just get
    /// a copy of the root URLBase).
    std::vector<UPnPDeviceDesc> embedded;
};

#endif /* _UPNPDEV_HXX_INCLUDED_ */
