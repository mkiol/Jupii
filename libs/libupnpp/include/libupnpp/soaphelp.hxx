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
#ifndef _SOAPHELP_H_X_INCLUDED_
#define _SOAPHELP_H_X_INCLUDED_

// This module used to be necessary to decode/encode XML DOM trees
// from/to the libupnp interface. The npupnp interface uses C++
// objects instead, so there is not much left in here, but it has
// successfully isolated the client code from the change.

#include <unordered_map>
#include <memory>
#include <string>

#include "upnppexports.hxx"

namespace UPnPP {

/** Decode incoming Soap call data */
class UPNPP_API SoapIncoming {
public:
    SoapIncoming();
    ~SoapIncoming();

    /** Get action name */
    const std::string& getName() const;

    /** Get boolean parameter value */
    bool get(const char *nm, bool *value) const;
    /** Get integer parameter value */
    bool get(const char *nm, int *value) const;
    /** Get string parameter value */
    bool get(const char *nm, std::string *value) const;

    void getMap(std::unordered_map<std::string, std::string>& out);

    class UPNPP_LOCAL Internal;
    Internal *m;
};

namespace SoapHelp {
std::string UPNPP_API xmlQuote(const std::string& in);
std::string UPNPP_API xmlUnquote(const std::string& in);
std::string UPNPP_API i2s(int val);
inline std::string val2s(const std::string& val)
{
    return val;
}
inline std::string val2s(int val)
{
    return i2s(val);
}
inline std::string val2s(bool val)
{
    return i2s(int(val));
}
template <class InputIterator>
std::string UPNPP_API argsToStr(InputIterator first, InputIterator last)
{
    std::string out;
    for (auto it = first; it != last; it++) {
        out += it->first + " = " + it->second + "\n";
    }
    return out;
}
}

/** Store the values to be encoded in a SOAP response.
 *
 * The elements in the response must be in a defined order, so we
 * can't use a map as container, we use a vector of pairs instead.
 * The generic UpnpDevice callback fills up name and service type, the
 * device call only needs to fill the data vector.
 */
class UPNPP_API SoapOutgoing {
public:
    SoapOutgoing();
    SoapOutgoing(const std::string& st, const std::string& nm);
    ~SoapOutgoing();

    SoapOutgoing& addarg(const std::string& k, const std::string& v);

    SoapOutgoing& operator()(const std::string& k, const std::string& v);

    const std::string& getName() const;

    class UPNPP_LOCAL Internal;
    Internal *m;
};


} // namespace UPnPP

#endif /* _SOAPHELP_H_X_INCLUDED_ */
