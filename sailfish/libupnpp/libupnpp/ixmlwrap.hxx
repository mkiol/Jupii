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
#ifndef _IXMLWRAP_H_INCLUDED_
#define _IXMLWRAP_H_INCLUDED_

#include <ixml.h>                  // for IXML_Document

#include <string>                       // for string

namespace UPnPP {

#if 0
/** Retrieve the text content for the first element of given name.
 * Returns an empty string if the element does not contain a text node */
std::string getFirstElementValue(IXML_Document *doc,
                                 const std::string& name);
*
#endif

/** Return the result of ixmlPrintDocument as a string and take
 * care of freeing the memory. This is inefficient of course (one
 * more alloc+copy), and destined to debug statements */
std::string ixmlwPrintDoc(IXML_Document*);

}

#endif /* _IXMLWRAP_H_INCLUDED_ */
