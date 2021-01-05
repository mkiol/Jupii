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
#ifndef _OHPLAYLIST_HXX_INCLUDED_
#define _OHPLAYLIST_HXX_INCLUDED_

#include <unordered_map>
#include <string>
#include <vector>

#include "libupnpp/control/service.hxx"
#include "libupnpp/control/cdircontent.hxx"


namespace UPnPClient {

class OHPlaylist;
class UPnPDeviceDesc;
class UPnPServiceDesc;

typedef std::shared_ptr<OHPlaylist> OHPLH;

/**
 * OHPlaylist Service client class.
 *
 */
class UPNPP_API OHPlaylist : public Service {
public:

    OHPlaylist(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
    }
    OHPlaylist() {}
    virtual ~OHPlaylist() {}

    /** Test service type from discovery message */
    static bool isOHPlService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);

    int play();
    int pause();
    int stop();
    int next();
    int previous();
    int setRepeat(bool onoff);
    int repeat(bool *on);
    int setShuffle(bool onoff);
    int shuffle(bool *on);
    int seekSecondAbsolute(int value);
    int seekSecondRelative(int value);
    int seekId(int value);
    int seekIndex(int value);
    enum TPState {TPS_Unknown, TPS_Buffering, TPS_Paused, TPS_Playing,
                  TPS_Stopped
                 };
    int transportState(TPState *tps);
    int id(int *value);
    int read(int id, std::string* uri, UPnPDirObject *dirent);

    struct TrackListEntry {
        int id;
        std::string url;
        UPnPDirObject dirent;
        void clear() {
            id = -1;
            url.clear();
            dirent.clear();
        }
    };
    int readList(const std::vector<int>& ids,
                 std::vector<TrackListEntry>* entsp);

    int insert(int afterid, const std::string& uri, const std::string& didl,
               int *nid);
    int deleteId(int id);
    int deleteAll();
    int tracksMax(int *);
    int idArray(std::vector<int> *ids, int *tokp);
    int idArrayChanged(int token, bool *changed);
    int protocolInfo(std::string *proto);

    static int stringToTpState(const std::string& va, TPState *tpp);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void UPNPP_LOCAL evtCallback(
        const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback();
};

} // namespace UPnPClient

#endif /* _OHPLAYLIST_HXX_INCLUDED_ */
