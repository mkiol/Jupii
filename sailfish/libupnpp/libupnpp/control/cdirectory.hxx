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
#ifndef _UPNPDIR_HXX_INCLUDED_
#define _UPNPDIR_HXX_INCLUDED_

#include "libupnpp/config.h"

#include <unordered_map>
#include <set>                          // for set
#include <string>                       // for string
#include <vector>                       // for vector

#include "libupnpp/control/service.hxx"  // for Service

namespace UPnPClient {
class ContentDirectory;    // lines 30-30
}
namespace UPnPClient {
class UPnPDeviceDesc;
}
namespace UPnPClient {
class UPnPDirContent;
}
namespace UPnPClient {
class UPnPServiceDesc;
}

namespace UPnPClient {

typedef std::shared_ptr<ContentDirectory> CDSH;

/**
 * Content Directory Service client class.
 *
 * This stores identity data from a directory service
 * and the device it belongs to, and has methods to query
 * the directory, using libupnp for handling the UPnP protocols.
 *
 * Note: m_rdreqcnt: number of entries requested per directory read.
 * 0 means all entries. The device can still return less entries than
 * requested, depending on its own limits. In general it's not optimal
 * becauses it triggers issues, and is sometimes actually slower, e.g. on
 * a D-Link NAS 327
 *
 * The value chosen may be affected by the UpnpSetMaxContentLength
 * (2000*1024) done during initialization, but this should be ample.
 */
class ContentDirectory : public Service {
public:

    /** Construct by copying data from device and service objects. */
    ContentDirectory(const UPnPDeviceDesc& device,
                     const UPnPServiceDesc& service);
    virtual ~ContentDirectory();

    /** An empty one */
    ContentDirectory() : m_rdreqcnt(200), m_serviceKind(CDSKIND_UNKNOWN) {}

    enum ServiceKind {CDSKIND_UNKNOWN, CDSKIND_BUBBLE, CDSKIND_MEDIATOMB,
                      CDSKIND_MINIDLNA, CDSKIND_MINIM, CDSKIND_TWONKY
                     };

    ServiceKind getKind() {
        return m_serviceKind;
    }

    /** Test service type from discovery message */
    static bool isCDService(const std::string& st);

    /** Retrieve the directory services currently seen on the network */
    static bool getServices(std::vector<CDSH>&);

    /** Retrieve specific service designated by its friendlyName */
    static bool getServerByName(const std::string& friendlyName,
                                CDSH& server);

    /** Read a full container's children list
     *
     * @param objectId the UPnP object Id for the container. Root has Id "0"
     * @param[out] dirbuf stores the entries we read.
     * @return UPNP_E_SUCCESS for success, else libupnp error code.
     */
    int readDir(const std::string& objectId, UPnPDirContent& dirbuf);

    /** Read a partial slice of a container's children list
     *
     * The entries read are concatenated to the input dirbuf.
     *
     * @param objectId the UPnP object Id for the container. Root has Id "0"
     * @param offset the offset of the first entry to read
     * @param count the maximum number of entries to read
     * @param[out] dirbuf a place to store the entries we read. They are
     *        appended to the existing ones.
     * @param[out] didread number of entries actually read.
     * @param[out] total total number of children.
     * @return UPNP_E_SUCCESS for success, else libupnp error code.
     */
    int readDirSlice(const std::string& objectId, int offset,
                     int count, UPnPDirContent& dirbuf,
                     int *didread, int *total);

    int goodSliceSize()
    {
        return m_rdreqcnt;
    }

    /** Search the content directory service.
     *
     * @param objectId the UPnP object Id under which the search
     * should be done. Not all servers actually support this below
     * root. Root has Id "0"
     * @param searchstring an UPnP searchcriteria string. Check the
     * UPnP document: UPnP-av-ContentDirectory-v1-Service-20020625.pdf
     * section 2.5.5. Maybe we'll provide an easier way some day...
     * @param[out] dirbuf stores the entries we read.
     * @return UPNP_E_SUCCESS for success, else libupnp error code.
     */
    int search(const std::string& objectId, const std::string& searchstring,
               UPnPDirContent& dirbuf);
    /** Same to search() as readDirSlice to readDir() */
    int searchSlice(const std::string& objectId,
                    const std::string& searchstring,
                    int offset, int count, UPnPDirContent& dirbuf,
                    int *didread, int *total);

    /** Read metadata for a given node.
     *
     * @param objectId the UPnP object Id. Root has Id "0"
     * @param[out] dirbuf stores the entries we read. At most one entry will be
     *   returned.
     * @return UPNP_E_SUCCESS for success, else libupnp error code.
     */
    int getMetadata(const std::string& objectId, UPnPDirContent& dirbuf);

    /** Retrieve search capabilities
     *
     * @param[out] result an empty vector: no search, or a single '*' element:
     *     any tag can be used in a search, or a list of usable tag names.
     * @return UPNP_E_SUCCESS for success, else libupnp error code.
     */
    int getSearchCapabilities(std::set<std::string>& result);

protected:
    /* My service type string */
    static const std::string SType;

private:
    int m_rdreqcnt; // Slice size to use when reading
    ServiceKind m_serviceKind;

    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();
};

}

#endif /* _UPNPDIR_HXX_INCLUDED_ */
