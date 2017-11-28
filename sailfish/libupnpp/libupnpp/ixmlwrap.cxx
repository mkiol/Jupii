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

#include "libupnpp/ixmlwrap.hxx"

#include <ixml.h>                  // for IXML_Node, etc

#include <string>                       // for string

using std::string;

namespace UPnPP {

#if 0
// Get the value for the first element in the document with the given name.
// There should be only one such element for this to make any sense.
string getFirstElementValue(IXML_Document *doc, const string& name)
{
    string ret;
    IXML_NodeList *nodes =
        ixmlDocument_getElementsByTagName(doc, name.c_str());

    if (nodes) {
        IXML_Node *first = ixmlNodeList_item(nodes, 0);
        if (first) {
            IXML_Node *dnode = ixmlNode_getFirstChild(first);
            if (dnode) {
                ret = ixmlNode_getNodeValue(dnode);
            }
        }
    }

    if(nodes)
        ixmlNodeList_free(nodes);
    return ret;
}
#endif

string ixmlwPrintDoc(IXML_Document* doc)
{
    DOMString s = ixmlPrintDocument(doc);
    if (s) {
        string cpps(s);
        ixmlFreeDOMString(s);
        return cpps;
    } else {
        return string();
    }
}

}
