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
// An XML parser which constructs an UPnP device object from the
// device descriptor
#include "libupnpp/config.h"

#include "description.hxx"

#include <algorithm>

#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UpnpDownload...

#include "libupnpp/upnpplib.hxx"
#include "libupnpp/expatmm.hxx"         // for inputRefXMLParser
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/smallut.h"
#include "libupnpp/log.hxx"

using namespace std;
using namespace UPnPP;

namespace UPnPClient {

class UPnPDeviceParser : public inputRefXMLParser {
public:
    UPnPDeviceParser(const string& input, UPnPDeviceDesc& device)
        : inputRefXMLParser(input), m_device(device)
    {}

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **)
    {
        m_path.push_back(name);
    }
    virtual void EndElement(const XML_Char *name)
    {
        m_path.pop_back();
        trimstring(m_chardata, " \t\n\r");

        UPnPDeviceDesc *dev;
        bool ismain = false;
        const string dl("devicelist");
        if (find(m_path.begin(), m_path.end(), dl) == m_path.end()) {
            dev = &m_device;
            ismain = true;
        } else {
            dev = &m_tdevice;
            ismain = false;
        }

        if (!strcmp(name, "service")) {
            dev->services.push_back(m_tservice);
            m_tservice.clear();
        } else if (!strcmp(name, "device")) {
            if (ismain == false) {
                m_device.embedded.push_back(m_tdevice);
            }
            m_tdevice.clear();
        } else if (!strcmp(name, "controlURL")) {
            m_tservice.controlURL = m_chardata;
        } else if (!strcmp(name, "eventSubURL")) {
            m_tservice.eventSubURL = m_chardata;
        } else if (!strcmp(name, "serviceType")) {
            m_tservice.serviceType = m_chardata;
        } else if (!strcmp(name, "serviceId")) {
            m_tservice.serviceId = m_chardata;
        } else if (!strcmp(name, "SCPDURL")) {
            m_tservice.SCPDURL = m_chardata;
        } else if (!strcmp(name, "deviceType")) {
            dev->deviceType = m_chardata;
        } else if (!strcmp(name, "friendlyName")) {
            dev->friendlyName = m_chardata;
        } else if (!strcmp(name, "manufacturer")) {
            dev->manufacturer = m_chardata;
        } else if (!strcmp(name, "modelName")) {
            dev->modelName = m_chardata;
        } else if (!strcmp(name, "UDN")) {
            dev->UDN = m_chardata;
        } else if (!strcmp(name, "URLBase")) {
            m_device.URLBase = m_chardata;
        } else if (!strcmp(name, "icon")) {
            dev->iconList.push_back(m_ticon);
            m_ticon = UPnPDeviceDesc::Icon();
        } else if (!strcmp(name, "url")) {
            /*if (!m_path.empty() && m_path.back() == "icon") {
                m_device.iconList.push_back(m_chardata);
            }*/
            m_ticon.url = m_chardata;
        } else if (!strcmp(name, "mimetype")) {
            m_ticon.mimeType = m_chardata;
        } else if (!strcmp(name, "width")) {
            m_ticon.width = std::stoi(m_chardata);
        } else if (!strcmp(name, "height")) {
            m_ticon.height = std::stoi(m_chardata);
        } else if (!strcmp(name, "depth")) {
            m_ticon.depth = std::stoi(m_chardata);
        }

        m_chardata.clear();
    }

    virtual void CharacterData(const XML_Char *s, int len)
    {
        if (s == 0 || *s == 0)
            return;

        string str(s, len);
        m_chardata += str;
    }

private:
    UPnPDeviceDesc& m_device;
    std::vector<std::string> m_path;
    string m_chardata;
    UPnPServiceDesc m_tservice;
    UPnPDeviceDesc m_tdevice;
    UPnPDeviceDesc::Icon m_ticon;
};

UPnPDeviceDesc::UPnPDeviceDesc(const string& url, const string& description)
    : ok(false)
{
    //cerr << "UPnPDeviceDesc::UPnPDeviceDesc: url: " << url << endl;
    //cerr << " description " << endl << description << endl;

    //cout << "UPnPDeviceDesc::UPnPDeviceDesc: url: " << url << endl;
    //cout << " description " << endl << description << endl;

    UPnPDeviceParser mparser(description, *this);
    if (!mparser.Parse())
        return;
    if (URLBase.empty()) {
        // The standard says that if the URLBase value is empty, we
        // should use the url the description was retrieved
        // from. However this is sometimes something like
        // http://host/desc.xml, sometimes something like http://host/
        // (rare, but e.g. sent by the server on a dlink nas).
        URLBase = baseurl(url);
    }
    for (auto& dev: embedded) {
        dev.URLBase = URLBase;
    }

    XMLText = description;
    
    ok = true;
    //cerr << "URLBase: [" << URLBase << "]" << endl;
    //cerr << dump() << endl;
}




// XML parser for the service description document (SCPDURL)
class ServiceDescriptionParser : public inputRefXMLParser {
public:
    ServiceDescriptionParser(UPnPServiceDesc::Parsed& out, const string& input)
        : inputRefXMLParser(input), m_parsed(out)
    {
    }

protected:
    class StackEl {
    public:
        StackEl(const string& nm) : name(nm) {}
        string name;
        XML_Size start_index;
        std::unordered_map<string,string> attributes;
        string data;
    };

    virtual void StartElement(const XML_Char *name, const XML_Char **attrs)
    {
        //LOGDEB("startElement: name [" << name << "]" << " bpos " <<
        //             XML_GetCurrentByteIndex(expat_parser) << endl);

        m_path.push_back(StackEl(name));
        StackEl& lastelt = m_path.back();
        lastelt.start_index = XML_GetCurrentByteIndex(expat_parser);
        for (int i = 0; attrs[i] != 0; i += 2) {
            lastelt.attributes[attrs[i]] = attrs[i+1];
        }

        switch (name[0]) {
        case 'a':
            if (!strcmp(name, "action")) {
                m_tact.clear();
            } else if (!strcmp(name, "argument")) {
                m_targ.clear();
            }
            break;
        case 's':
            if (!strcmp(name, "stateVariable")) {
                m_tvar.clear();
                std::unordered_map<string,string>::iterator it =
                    lastelt.attributes.find("sendEvents");
                if (it != lastelt.attributes.end()) {
                    stringToBool(it->second, &m_tvar.sendEvents);
                }
            }
            break;
        default:
            break;
        }
    }

    virtual void EndElement(const XML_Char *name)
    {
        string parentname;
        if (m_path.size() == 1) {
            parentname = "root";
        } else {
            parentname = m_path[m_path.size()-2].name;
        }
        StackEl& lastelt = m_path.back();
        //LOGINF("ServiceDescriptionParser: Closing element " << name
        //<< " inside element " << parentname <<
        //" data " << m_path.back().data << endl);

        switch (name[0]) {
        case 'a':
            if (!strcmp(name, "action")) {
                m_parsed.actionList[m_tact.name] = m_tact;
            } else if (!strcmp(name, "argument")) {
                m_tact.argList.push_back(m_targ);
            }
            break;
        case 'd':
            if (!strcmp(name, "direction")) {
                if (!lastelt.data.compare("in")) {
                    m_targ.todevice = false;
                } else {
                    m_targ.todevice = true;
                }
            } else if (!strcmp(name, "dataType")) {
                m_tvar.dataType = lastelt.data;
                trimstring(m_tvar.dataType);
            }
            break;
        case 'm':
            if (!strcmp(name, "minimum")) {
                m_tvar.hasValueRange = true;
                m_tvar.minimum = atoi(lastelt.data.c_str());
            } else if (!strcmp(name, "maximum")) {
                m_tvar.hasValueRange = true;
                m_tvar.maximum = atoi(lastelt.data.c_str());
            }
            break;
        case 'n':
            if (!strcmp(name, "name")) {
                if (!parentname.compare("argument")) {
                    m_targ.name = lastelt.data;
                    trimstring(m_targ.name);
                } else if (!parentname.compare("action")) {
                    m_tact.name = lastelt.data;
                    trimstring(m_tact.name);
                } else if (!parentname.compare("stateVariable")) {
                    m_tvar.name = lastelt.data;
                    trimstring(m_tvar.name);
                }
            }
            break;
        case 'r':
            if (!strcmp(name, "relatedStateVariable")) {
                m_targ.relatedVariable = lastelt.data;
                trimstring(m_targ.relatedVariable);
            }
            break;
        case 's':
            if (!strcmp(name, "stateVariable")) {
                m_parsed.stateTable[m_tvar.name] = m_tvar;
            } else if (!strcmp(name, "step")) {
                m_tvar.hasValueRange = true;
                m_tvar.step = atoi(lastelt.data.c_str());
            }
            break;
        }

        m_path.pop_back();
    }

    virtual void CharacterData(const XML_Char *s, int len)
    {
        if (s == 0 || *s == 0)
            return;
        m_path.back().data += string(s, len);
    }

private:
    vector<StackEl> m_path;
    UPnPServiceDesc::Parsed& m_parsed;
    UPnPServiceDesc::Argument m_targ;
    UPnPServiceDesc::Action m_tact;
    UPnPServiceDesc::StateVariable m_tvar;
};

bool UPnPServiceDesc::fetchAndParseDesc(const string& urlbase,
                                        Parsed& parsed) const
{
    char *buf = 0;
    char contentType[LINE_SIZE];
    string url = caturl(urlbase, SCPDURL);
    int code = UpnpDownloadUrlItem(url.c_str(), &buf, contentType);
    if (code != UPNP_E_SUCCESS) {
        LOGERR("UPnPServiceDesc::fetchAndParseDesc: error fetching " <<
               url << " : " << LibUPnP::errAsString("", code) << endl);
        return false;
    }
    string sdesc(buf);
    free(buf);
    ServiceDescriptionParser parser(parsed, sdesc);
    return parser.Parse();
}

} // namespace
