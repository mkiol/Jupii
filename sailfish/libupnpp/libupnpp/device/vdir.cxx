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

#include "vdir.hxx"

#include <string.h>
#include <ixml.h>
#include <upnp/upnp.h>

#include <iostream>
#include <utility>
#include <unordered_map>

#include "libupnpp/log.hxx"
#include "libupnpp/upnpp_p.hxx"

using namespace std;
using namespace UPnPP;

namespace UPnPProvider {

static VirtualDir *theDir;

class FileEnt {
public:
    time_t mtime;
    std::string mimetype;
    std::string content;
};

struct DirEnt {
public:
    DirEnt(bool vd = false) : isvd(vd) {}
    bool isvd;
    unordered_map<string, FileEnt> files;
    VirtualDir::FileOps ops;
};
static unordered_map<string,  DirEnt> m_dirs;

static void pathcatslash(string& path)
{
    if (path.empty() || path[path.size() - 1] != '/') {
        path += '/';
    }
}

// Look up entry for pathname
static FileEnt *vdgetentry(const char *pathname, DirEnt **de)
{
    //LOGDEB("vdgetentry: [" << pathname << "]" << endl);
    *de = nullptr;
    
    VirtualDir *thedir = VirtualDir::getVirtualDir();
    if (thedir == 0) {
        return 0;
    }

    string path = path_getfather(pathname);
    string name= path_getsimple(pathname);

    pathcatslash(path);
    
    auto dir = m_dirs.find(path);
    if (dir == m_dirs.end()) {
        LOGERR("VirtualDir::vdgetentry: no dir: " << path << endl);
        return 0;
    }
    *de = &dir->second;
    if (dir->second.isvd) {
	return 0;
    } else {
	auto f = dir->second.files.find(name);
	if (f == dir->second.files.end()) {
	    LOGERR("VirtualDir::vdgetentry: no file: " << path << endl);
	    return 0;
	}
	return &(f->second);
    }
}

bool VirtualDir::addFile(const string& _path, const string& name,
                         const string& content, const string& mimetype)
{
    string path(_path);
    pathcatslash(path);
    
    //LOGDEB("VirtualDir::addFile: path " << path << " name " << name << endl);

    if (m_dirs.find(path) == m_dirs.end()) {
	m_dirs[path] = DirEnt();
        UpnpAddVirtualDir(path.c_str());
    }

    FileEnt entry;
    entry.mtime = time(0);
    entry.mimetype = mimetype;
    entry.content = content;
    m_dirs[path].files[name] = entry;
    // LOGDEB("VirtualDir::addFile: added entry for dir " <<
    // path << " name " << name << endl);
    return true;
}

bool VirtualDir::addVDir(const std::string& _path, FileOps fops)
{
    string path(_path);
    pathcatslash(path);
    if (m_dirs.find(path) == m_dirs.end()) {
	m_dirs[path] = DirEnt(true);
        UpnpAddVirtualDir(path.c_str());
    }
    m_dirs[path].ops = fops;
    return true;
}


//////////
// Methods for the libupnp interface:

struct Handle {
    Handle(FileEnt *e, DirEnt *d = nullptr, void *vh = 0)
        : entry(e), dir(d), vhandle(vh), offset(0) {
    }
    FileEnt *entry;
    DirEnt *dir;
    void *vhandle;
    size_t offset;
};

static int vdclose(UpnpWebFileHandle fileHnd)
{
    Handle *h = (Handle*)fileHnd;
    if (h->vhandle) {
	h->dir->ops.close(h->vhandle);
    }
    delete h;
    return 0;
}

static int vdgetinfo(const char *fn, struct File_Info* info)
{
    //LOGDEB("vdgetinfo: [" << fn << "] off_t " << sizeof(off_t) <<
    // " time_t " << sizeof(time_t) << endl);
    DirEnt *dir;
    FileEnt *entry = vdgetentry(fn, &dir);
    if (dir && dir->isvd) {
	VirtualDir::FileInfo inf;
	int ret = dir->ops.getinfo(fn, &inf);
	if (ret >= 0) {
	    info->file_length = inf.file_length;
	    info->last_modified = inf.last_modified;
	    info->is_directory = inf.is_directory;
	    info->is_readable =  inf.is_readable;
	    info->content_type = ixmlCloneDOMString(inf.mime.c_str());
	}
	return ret;
    }
    if (entry == 0) {
        LOGERR("vdgetinfo: no entry for " << fn << endl);
        return -1;
    }

    info->file_length = entry->content.size();
    info->last_modified = entry->mtime;
    info->is_directory = 0;
    info->is_readable = 1;
    info->content_type = ixmlCloneDOMString(entry->mimetype.c_str());

    return 0;
}

static UpnpWebFileHandle vdopen(const char* fn, enum UpnpOpenFileMode)
{
    //LOGDEB("vdopen: " << fn << endl);
    DirEnt *dir;
    FileEnt *entry = vdgetentry(fn, &dir);

    if (dir && dir->isvd) {
	void *vh = dir->ops.open(fn);
	if (vh) {
	    return new Handle(nullptr, dir, vh);
	} else {
	    return NULL;
	}
    }

    if (entry == 0) {
        LOGERR("vdopen: no entry for " << fn << endl);
        return NULL;
    }
    return new Handle(entry);
}

static int vdread(UpnpWebFileHandle fileHnd, char* buf, size_t buflen)
{
    // LOGDEB("vdread: " << endl);
    if (buflen == 0) {
        return 0;
    }
    Handle *h = (Handle *)fileHnd;
    if (h->vhandle) {
	return h->dir->ops.read(h->vhandle, buf, buflen);
    }

    if (h->offset >= h->entry->content.size()) {
        return 0;
    }
    size_t toread = buflen > h->entry->content.size() - h->offset ?
                    h->entry->content.size() - h->offset : buflen;
    memcpy(buf, h->entry->content.c_str() + h->offset, toread);
    h->offset += toread;
    return toread;
}

static int vdseek(UpnpWebFileHandle fileHnd, off_t offset, int origin)
{
    // LOGDEB("vdseek: " << endl);
    Handle *h = (Handle *)fileHnd;
    if (h->vhandle) {
	return h->dir->ops.seek(h->vhandle, offset, origin);
    }
    if (origin == 0) {
        h->offset = offset;
    } else if (origin == 1) {
        h->offset += offset;
    } else if (origin == 2) {
        h->offset = h->entry->content.size() + offset;
    } else {
        return -1;
    }
    return offset;
}

static int vdwrite(UpnpWebFileHandle, char*, size_t)
{
    LOGERR("vdwrite" << endl);
    return -1;
}

static struct UpnpVirtualDirCallbacks myvdcalls = {
    vdgetinfo, vdopen, vdread, vdwrite, vdseek, vdclose
};

VirtualDir *VirtualDir::getVirtualDir()
{
    if (theDir == 0) {
        theDir = new VirtualDir();
        if (UpnpSetVirtualDirCallbacks(&myvdcalls) != UPNP_E_SUCCESS) {
            LOGERR("SetVirtualDirCallbacks failed" << endl);
            delete theDir;
            theDir = 0;
            return 0;
        }
    }
    return theDir;
}

}
