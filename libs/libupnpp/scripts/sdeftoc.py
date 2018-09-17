#!/usr/bin/env python
from __future__ import print_function

'''Turn UPnP service defintion XML file into a C++ skeleton based
on libupnpp'''

import sys
import os
import xml.sax.handler

cprolog_text = \
'''/* Copyright (C) 2016 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "%s"

#include <upnp/upnp.h>

#include <functional>
#include <iostream>
#include <map>
#include <utility>

#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/device/device.hxx"

using namespace std;
using namespace std::placeholders;

'''

hprolog_text = \
'''/* Copyright (C) 2016 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef _%s_H_INCLUDED_
#define _%s_H_INCLUDED_

#include <string>
#include <vector>

#include "libupnpp/device/device.hxx"
#include "libupnpp/soaphelp.hxx"

using namespace UPnPP;

'''

def back(l):
    return l[len(l)-1]

def stripsuff(s):
    dotpos = s.rfind('.')
    if dotpos != -1:
        s = s[:dotpos]
    return s
    


class MyHandler(xml.sax.handler.ContentHandler):
    def __init__(self, classname, cfn, hfn):
        self.classname = classname
        self.cfn = cfn
        self.hfn = hfn
        self.elstack = []
        self.chardata = u""
        self.attrs = {}
        self.inargs = []
        self.outargs = []
        self.actions = []
        self.statevars = {}
        self.cprolog = cprolog_text % os.path.basename(hfn)
        self.cprolog += 'static const string sTp%s("urn:XXX:1");\n' % classname
        self.cprolog += 'static const string sId%s("urn:XXX");\n\n' % classname
        self.cprolog += '%s::%s(UPnPProvider::UpnpDevice *dev)\n' % \
                        (classname, classname)
        self.cprolog += '    : UpnpService(sTp%s, sId%s, dev)\n'  % \
                        (classname, classname)
        self.cprolog += '{\n'
        uh = stripsuff(os.path.basename(hfn)).upper()
        self.hprolog = hprolog_text % (uh, uh)
        self.hprolog += 'class %s : public UPnPProvider::UpnpService {\n' % classname
        self.hprolog += 'public:\n'
        self.hprolog += '    %s(UPnPProvider::UpnpDevice *dev);\n\n' % classname
        self.hprolog += 'private:\n'

    def setDocumentLocator(self, locator):
        self.locator = locator
        
    def startElement(self, name, attrs):
        self.attrs = attrs
        if name == 'stateVariable':
            self.stvarev = False
            self.stvarallow = []
            if 'sendEvents' in attrs and attrs['sendEvents'] == 'yes':
                self.stvarev = True
        self.elstack.append(name)

    def endElement(self, name):
        self.elstack.pop()
        chardata = self.chardata.strip()
        self.chardata = u""

        if name == 'action':
            self.actions.append([self.actionname, self.inargs, self.outargs])
            self.inargs = []
            self.outargs = []
        elif name == 'stateVariable':
            self.statevars[self.stvarname] = [self.stvartype, self.stvarev, \
                                              self.stvarallow]
            self.stvartype = u''
            self.stvarev = False
            self.stvarallow = []
        elif name == 'name':
            if back(self.elstack) == 'action':
                self.actionname = chardata
            elif back(self.elstack) == 'argument':
                self.argname = chardata
            elif back(self.elstack) == 'stateVariable':
                self.stvarname = chardata
        elif name == 'dataType':
            self.stvartype = chardata
        elif name == 'allowedValue':
            self.stvarallow.append(chardata)
        elif name == 'sendEventsAttribute' and chardata == 'yes':
            self.stvarev = True
        elif name == 'direction':
            self.argdir = chardata
        elif name == 'relatedStateVariable':
            self.relvar = chardata
        elif name == 'argument':
            if self.argdir == 'in':
                self.inargs.append([self.argname, self.relvar])
            else:
                self.outargs.append([self.argname, self.relvar])
            self.argname = u''
            self.argdir = u''
            self.relvar = u''
            
    def characters(self, chars):
        self.chardata += chars

    def actionMethodText(self, actname, inargs, outargs):
        methname = "%s::act%s" % (self.classname, actname)
        txt = u''
        txt += "int %s(const SoapIncoming& sc, SoapOutgoing& data)\n"\
               % methname
        txt += "{\n"
        if len(inargs) != 0:
            txt += "    bool ok = false;\n"
        for arg in inargs:
            relvar = arg[1]
            if relvar in self.statevars:
                atype = self.statevars[relvar][0]
            else:
                atype = 'string'
            if atype == 'string':
                txt += '    std::string in_%s;\n' % arg[0]
            elif atype == 'boolean':
                txt += '    bool in_%s;\n' % arg[0]
            else:
                txt += '    int in_%s;\n' % arg[0]
            txt += '    ok = sc.get("%s", &in_%s);\n' % (arg[0], arg[0])
            txt += '    if (!ok) {\n'
            txt += '        LOGERR("%s: no %s in params\\n");\n' %\
                   (methname, arg[0])
            txt += '        return UPNP_E_INVALID_PARAM;\n'
            txt += '    }\n'
                
        txt += "\n"

        txt += '    LOGDEB("%s: "' % methname
        for arg in inargs:
            txt += ' << " %s " << in_%s' % (arg[0], arg[0])
        txt += ' << endl);\n\n'
        
        for arg in outargs:
            txt += '    std::string out_%s;\n' % arg[0]
        for arg in outargs:
            txt += '    data.addarg("%s", out_%s);\n' % (arg[0], arg[0])
                    
        txt += "    return UPNP_E_SUCCESS;\n"
        txt += "}\n\n"
        return txt

    def endDocument(self):
        act_text = u''
        for act in self.actions:
            self.cprolog += '    dev->addActionMapping(\n'
            self.cprolog += '        this, "%s",\n'% act[0]
            self.cprolog += '        bind(&%s::act%s, this, _1, _2));\n' % \
                          (self.classname, act[0])
            act_text += self.actionMethodText(act[0], act[1], act[2])

        self.cprolog += '}\n\n'

        f = open(self.cfn, "w")
        print(self.cprolog, file = f)
        print(act_text, file = f)
        f.close()
        
        f = open(self.hfn, "w")
        for nm in self.actions:
            self.hprolog += '    int act%s(const SoapIncoming& sc, SoapOutgoing& data);\n' % nm[0]
        self.hprolog += '};\n'
        self.hprolog += '#endif\n'
        print(self.hprolog, file = f)
        f.close
        
def Usage():
    print("Usage: %s xmlfilename cfn hfn" % os.path.basename(sys.argv[0]), file=sys.stderr)
    sys.exit(1)
    
if len(sys.argv) != 4:
    Usage()
    
inputname = sys.argv[1]
cfn = sys.argv[2]
hfn = sys.argv[3]

classname = stripsuff(os.path.basename(inputname))

handler = MyHandler(classname, cfn, hfn)
parser = xml.sax.make_parser()

xml.sax.parse(inputname, handler)

