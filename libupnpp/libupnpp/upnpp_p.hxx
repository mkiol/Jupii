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
#ifndef _UPNPP_H_X_INCLUDED_
#define _UPNPP_H_X_INCLUDED_
#include "libupnpp/config.h"

/* Private shared defs for the library. Clients need not and should
   not include this */

#include <pthread.h>
#include <time.h>
#include <string>

namespace UPnPP {

// Concatenate paths. Caller should make sure it makes sense.
extern std::string caturl(const std::string& s1, const std::string& s2);
// Return the scheme://host:port[/] part of input, or input if it is weird
extern std::string baseurl(const std::string& url);
extern std::string path_getfather(const std::string &s);
extern std::string path_getsimple(const std::string &s);
template <class T> bool csvToStrings(const std::string& s, T &tokens);

// @return false if s does not look like a bool at all (does not begin
// with [FfNnYyTt01]
extern bool stringToBool(const std::string& s, bool *v);

extern void timespec_now(struct timespec *ts);

} // namespace

#endif /* _UPNPP_H_X_INCLUDED_ */
