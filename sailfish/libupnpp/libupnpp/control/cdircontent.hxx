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
#ifndef _UPNPDIRCONTENT_H_X_INCLUDED_
#define _UPNPDIRCONTENT_H_X_INCLUDED_

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/upnpavutils.hxx"

namespace UPnPClient {

/**
 * UPnP resource. A resource describes one of the entities associated with
 * a directory entry. This would be typically the audio file URI, and
 * its characteristics (sample rate etc.) as attributes, but there can
 * be several resources associated to one entry, for example for
 * multiple encoding formats.
 */
class UPnPResource {
public:
    /// URI Value
    std::string m_uri;

    /// Resource attributes
    std::map<std::string, std::string> m_props;

    /// Get the protocolinfo attribute in cooked form
    bool protoInfo(UPnPP::ProtocolinfoEntry& e) const {
        const auto it = m_props.find("protocolInfo");
        if (it == m_props.end()) {
            return false;
        }
        return UPnPP::parseProtoInfEntry(it->second, e);
    }
};


/**
 * UPnP Media Server directory entry, converted from XML data.
 *
 * This is a dumb data holder class, a struct with helpers.
 */
class UPnPDirObject {
public:
    enum ObjType {item, container};
    // There are actually several kinds of containers:
    // object.container.storageFolder, object.container.person,
    // object.container.playlistContainer etc., but they all seem to
    // behave the same as far as we're concerned. Otoh, musicTrack
    // items are special to us, and so should playlists, but I've not
    // seen one of the latter yet (servers seem to use containers for
    // playlists).
    enum ItemClass {ITC_audioItem = 0, ITC_playlist = 1,
                    ITC_unknown = 2,
                    ITC_videoItem = 3,
                    ITC_audioItem_musicTrack = ITC_audioItem, // hist compat
                    ITC_audioItem_playlist = ITC_playlist // hist compat
                   };

    std::string m_id; // ObjectId
    std::string m_pid; // Parent ObjectId
    std::string m_title; // dc:title. Directory name for a container.
    ObjType m_type; // item or container
    ItemClass m_iclass;
    // Properties as gathered from the XML document (album, artist, etc.),
    // except for title which has a proper field.
    // The map keys are the XML tag names
    std::map<std::string, std::string> m_props;

    // Resources: there may be several, for example for different
    // audio formats of the same track, each with an URI and descriptor fields
    std::vector<UPnPResource> m_resources;

    /** Get named property
     * @param name (e.g. upnp:artist, upnp:album,
     *     upnp:originalTrackNumber, upnp:genre). Use m_title instead
     *     for dc:title.
     * @param[out] value the parameter value if found
     * @return true if found.
     */
    bool getprop(const std::string& name, std::string& value) const
    {
        std::map<std::string, std::string>::const_iterator it =
            m_props.find(name);
        if (it == m_props.end())
            return false;
        value = it->second;
        return true;
    }
    const std::string& getprop(const std::string& name) const
    {
        std::map<std::string, std::string>::const_iterator it =
            m_props.find(name);
        if (it == m_props.end())
            return nullstr;
        return it->second;
    }

    /** Get named property for resource
     * Field names: "bitrate", "duration" (H:mm:ss.ms), "nrAudioChannels",
     * "protocolInfo", "sampleFrequency" (Hz), "size" (bytes)
     */
    bool getrprop(unsigned int ridx, const std::string& nm, std::string& val)
    const
    {
        if (ridx >= m_resources.size())
            return false;
        std::map<std::string, std::string>::const_iterator it =
            m_resources[ridx].m_props.find(nm);
        if (it == m_resources[ridx].m_props.end())
            return false;
        val = it->second;
        return true;

    }

    /** Simplified interface to retrieving values: we don't distinguish
     * between non-existing and empty, and we only use the first ressource
     */
    std::string f2s(const std::string& nm, bool isresfield) {
        std::string val;
        if (isresfield) {
            getrprop(0, nm, val);
        } else {
            getprop(nm, val);
        }
        return val;
    }

    int getDurationSeconds(unsigned ridx = 0) const
    {
        std::string sdur;
        if (!getrprop(ridx, "duration", sdur)) {
            //?? Avoid returning 0, who knows...
            return 1;
        }
        return UPnPP::upnpdurationtos(sdur);
    }

    /**
     * Get a DIDL document suitable for sending to a mediaserver. Only
     * works for items, not containers. The idea is that we may have
     * missed useful stuff while parsing the data from the content
     * directory, so we send the original if we can.
     */
    std::string getdidl() const;

    void clear()
    {
        m_id.clear();
        m_pid.clear();
        m_title.clear();
        m_type = (ObjType)-1;
        m_iclass = (ItemClass)-1;
        m_props.clear();
        m_resources.clear();
        m_didlfrag.clear();
    }

    std::string dump() const
    {
        std::ostringstream os;
        os << "UPnPDirObject: " << (m_type == item ? "item" : "container") <<
           " id [" << m_id << "] pid [" << m_pid <<
           "] title [" << m_title << "]" << std::endl;
        os << "Properties: " << std::endl;
        for (std::map<std::string,std::string>::const_iterator it =
                    m_props.begin();
                it != m_props.end(); it++) {
            os << "[" << it->first << "]->[" << it->second << "] "
               << std::endl;
        }
        os << "Resources:" << std::endl;
        for (std::vector<UPnPResource>::const_iterator it =
                    m_resources.begin(); it != m_resources.end(); it++) {
            os << "  Uri [" << it->m_uri << "]" << std::endl;
            os << "  Resource attributes:" << std::endl;
            for (std::map<std::string, std::string>::const_iterator it1 =
                        it->m_props.begin();
                    it1 != it->m_props.end(); it1++) {
                os << "    [" << it1->first << "]->[" << it1->second << "] "
                   << std::endl;
            }
        }
        os << std::endl;
        return os.str();
    }

private:
    friend class UPnPDirParser;
    // didl text for element, sans header
    std::string m_didlfrag;
    static std::string nullstr;
};

/**
 * Image of a MediaServer Directory Service container (directory),
 * possibly containing items and subordinate containers.
 */
class UPnPDirContent {
public:
    std::vector<UPnPDirObject> m_containers;
    std::vector<UPnPDirObject> m_items;

    void clear()
    {
        m_containers.clear();
        m_items.clear();
    }

    /**
     * Parse from DIDL-Lite XML data.
     *
     * Normally only used by ContentDirectory::readDir()
     * This is cumulative: in general, the XML data is obtained in
     * several documents corresponding to (offset,count) slices of the
     * directory (container). parse() can be called repeatedly with
     * the successive XML documents and will accumulate entries in the item
     * and container vectors. This makes more sense if the different
     * chunks are from the same container, but given that UPnP Ids are
     * actually global, nothing really bad will happen if you mix
     * up...
     */
    bool parse(const std::string& didltext);
};

} // namespace

#endif /* _UPNPDIRCONTENT_H_X_INCLUDED_ */
