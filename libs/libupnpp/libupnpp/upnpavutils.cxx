/* Copyright (C) 2006-2016 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#include "libupnpp/config.h"
#ifndef WIN32
#include <netinet/in.h>                 // for ntohl
#else
#include "Winsock2.h"
#endif

#include <cstdio>
#include <string>

#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/base64.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/smallut.h"

using namespace std;

namespace UPnPP {

// Format duration in milliseconds into UPnP duration format
string upnpduration(int ms)
{
    int hours = ms / (3600 * 1000);
    ms -= hours * 3600 * 1000;
    int minutes = ms / (60 * 1000);
    ms -= minutes * 60 * 1000;
    int secs = ms / 1000;
    ms -= secs * 1000;

    char cbuf[100];

    // This is the format from the ref doc, but it appears that the
    // decimal part in the seconds field is an issue with some control
    // points. So drop it...
    //  sprintf(cbuf, "%d:%02d:%02d.%03d", hours, minutes, secs, ms);
    sprintf(cbuf, "%d:%02d:%02d", hours, minutes, secs);
    return cbuf;
}

// H:M:S to seconds
int upnpdurationtos(const string& dur)
{
    int hours, minutes, seconds;
    if (sscanf(dur.c_str(), "%d:%d:%d", &hours, &minutes, &seconds) != 3) {
        return 0;
    }
    return 3600 * hours + 60 * minutes + seconds;
}

// Decode OHPlaylist IdArray: base64-encoded array of binary msb
// 32bits integers, into vector<int>. This is used internally by the
// device side too, so it's more convenient to have it here than in
// the control ohplaylist
bool ohplIdArrayToVec(const string& _data, vector<int> *ids)
{
    string data;
    if (!base64_decode(_data, data)) {
        return false;
    }
    const char *cp = data.c_str();
    while (cp - data.c_str() <= int(data.size()) - 4) {
        unsigned int *ip = (unsigned int *)cp;
        ids->push_back(ntohl(*ip));
        cp += 4;
    }
    return true;
}

bool parseProtoInfEntry(const string& protoinfo, ProtocolinfoEntry& e)
{
    vector<string> toks;
    stringToTokens(protoinfo, toks, ":");
    if (toks.size() != 4) {
        LOGINF("parseProtoInfEntry: bad format: [" << protoinfo << "]\n");
        return false;
    }
    e.protocol = stringtolower((const std::string&)toks[0]);
    e.network = stringtolower((const std::string&)toks[1]);
    e.additional = stringtolower((const std::string&)toks[3]);

    if (e.protocol.compare("http-get")) {
        e.contentFormat = stringtolower((const std::string&)toks[2]);
    } else {
        // Parse MIME type
        vector<string> mtoks;
        stringToStrings(stringtolower((const std::string&)toks[2]), mtoks,
                        ";=");
        if (mtoks.size() == 0) {
            LOGINF("parseProtoInfEntry: bad format (no content type?): [" <<
                   protoinfo << "]\n");
            return false;
        }
        e.contentFormat = mtoks[0];
        if ((mtoks.size() - 1) % 4 != 0) {
            LOGINF("parseProtoInfEntry: bad format "
                   "missing param element in: [" << protoinfo << "]\n");
            return false;
        }
        // ';' name '=' value
        for (unsigned int i = 1; i < mtoks.size(); i += 4) {
            e.content_params[mtoks[i+1]] = mtoks[i+3];
        }
    }

    string params;
    for (const auto& it: e.content_params) {
        params += it.first + "=" + it.second + " ";
    }
    LOGDEB1("parseProtoInfEntry: got protocol[" << e.protocol << "] net[" <<
            e.network << "] contentFormat[" << e.contentFormat <<
            "] params[" << params << "] additional[" << e.additional << "]\n");
    return true;
}

bool parseProtocolInfo(const std::string& pinfo,
                       std::vector<ProtocolinfoEntry>& entries)
{
    vector<string> rawentries;
    stringToTokens(pinfo, rawentries, ",");
    for (unsigned int i = 0; i < rawentries.size(); i++) {
        ProtocolinfoEntry e;
        if (parseProtoInfEntry(rawentries[i], e)) {
            entries.push_back(std::move(e));
        }
    }
    return true;
}
    
}
