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
#ifndef _VDIR_H_X_INCLUDED_
#define _VDIR_H_X_INCLUDED_

#include "libupnpp/config.h"

#include <time.h>
#include <sys/types.h>
#include <stddef.h>

#include <string>
#include <functional>


/// Virtual directory handler to satisfy libupnp miniserver GETs.
///
/// Two types of objects can be defined:
///  - local 'files' which are actually memory strings, and will be
///    served from within the module. This is used for UPnP XML
///    interface definition files and such.
///  - external "virtual directories" for which a set of operations is
///    supplied by the caller. Any request from libupnp on a file
///    inside one of these directories will be forwarded.
///
/// As libupnp only lets us defines the api calls (open/read/etc.),
/// without any data cookie, this is a global singleton object.


namespace UPnPProvider {

class VirtualDir {
public:

    /// Get hold of the global object.
    static VirtualDir* getVirtualDir();

    /// Add file entry, to be served internally.
    /// @param path absolute /-separated parent directory path
    /// @param name file name
    /// @param content data content
    /// @param mimetype MIME type
    bool addFile(const std::string& path, const std::string& name,
                 const std::string& content, const std::string& mimetype);

    class FileInfo {
    public:
	FileInfo()
	    : file_length(0), last_modified(0), is_directory(false),
	      is_readable(true) {
	}
	
	off_t file_length;
	time_t last_modified;
	bool is_directory;
	bool is_readable;
	std::string mime;
    };

    class FileOps {
    public:
	std::function<int (const std::string&, FileInfo*)> getinfo;
	std::function<void *(const std::string&)> open;
	std::function<int (void *hdl, char* buf, size_t cnt)> read;
	std::function<off_t (void *hdl, off_t offs, int whence)> seek;
	std::function<void (void *hdl)> close;
    };

    /// Add virtual directory entry. Any request for a path below this point
    /// will be forwarded to @param fops. Only read operations are defined.
    bool addVDir(const std::string& path, FileOps fops);

private:
    VirtualDir() {}
    VirtualDir(VirtualDir const&) = delete;
    VirtualDir& operator=(VirtualDir const&) = delete;
};

}

#endif /* _VDIR_H_X_INCLUDED_ */
