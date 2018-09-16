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

#include <string.h>                     // for strcmp

#include "libupnpp/control/avlastchg.hxx"
#include "libupnpp/expatmm.hxx"         // for inputRefXMLParser

using namespace std;
using namespace UPnPP;

namespace UPnPClient {


class LastchangeParser : public inputRefXMLParser {
public:
    LastchangeParser(const string& input, std::unordered_map<string,string>& props)
        : inputRefXMLParser(input), m_props(props)
    {}

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
    {
        //LOGDEB("LastchangeParser: begin " << name << endl);
        for (int i = 0; attrs[i] != 0; i += 2) {
            //LOGDEB("    " << attrs[i] << " -> " << attrs[i+1] << endl);
            if (!strcmp("val", attrs[i])) {
                m_props[name] = attrs[i+1];
            }
        }
    }
private:
    std::unordered_map<string, string>& m_props;
};


bool decodeAVLastChange(const string& xml,
                        std::unordered_map<string, string>& props)
{
    LastchangeParser mparser(xml, props);
    if (!mparser.Parse())
        return false;
    return true;
}


}
