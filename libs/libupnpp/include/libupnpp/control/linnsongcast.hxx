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
#ifndef _LINNSONGCAST_H_X_INCLUDED_
#define _LINNSONGCAST_H_X_INCLUDED_

#include <string>
#include <vector>

#include "libupnpp/control/device.hxx"
#include "libupnpp/control/ohsender.hxx"
#include "libupnpp/control/ohreceiver.hxx"
#include "libupnpp/control/ohproduct.hxx"

/**
 * Helper functions for dealing with Linn Songcast Sender and Receiver
 * UPnP services. These deal with plumbing set up, and do not touch the
 * audio stream at all.
 */

namespace UPnPClient {
namespace Songcast {

/**
 * Search the device for a Sender service and, if found, create and
 * return a service object.
 */
OHSNH UPNPP_API senderService(DVCH dev);

/**
 * Find device with given name (try friendly, then uuid), and
 * return the result of senderService()
 */
OHSNH UPNPP_API getSender(const std::string& nm, std::string& reason);

/** Everything you need to know about a Sender */
struct UPNPP_API SenderState {
    std::string nm;
    std::string UDN;
    std::string uri;
    std::string meta;
    std::string reason;
    bool has_sender;

    OHPRH prod;
    OHSNH sender;

    SenderState() : has_sender(false) { }
    void reset() {
        nm = UDN = uri = meta = reason = std::string();
        has_sender = false;
        sender.reset();
    }
};

/** Retrieve the Sender service status for device nm (friendly or uuid)
 *
 * @param live if true, if the service is found, the 'sender'
 *    field will hold a handle to it on return.
 * @return st.reason is empty for success, else it holds an explanation string.
 */
void UPNPP_API getSenderState(const std::string& nm, SenderState& st,
                              bool live = true);

/** Retrieve status data for all found Senders */
void UPNPP_API listSenders(std::vector<SenderState>& vscs);

/** Everything you need to know about a Receiver */
struct UPNPP_API ReceiverState {
    enum SCState {SCRS_GENERROR, SCRS_NOOH, SCRS_NOTRECEIVER,
                  SCRS_STOPPED, SCRS_PLAYING
    };
    SCState state;
    int receiverSourceIndex;
    std::string nm;
    std::string UDN;
    std::string uri;
    std::string meta;
    std::string reason;

    OHPRH prod;
    OHRCH rcv;

    ReceiverState()
        : state(SCRS_GENERROR), receiverSourceIndex(-1) {
    }

    void reset() {
        state = ReceiverState::SCRS_GENERROR;
        receiverSourceIndex = -1;
        nm = UDN = uri = meta = reason = std::string();
        prod.reset();
        rcv.reset();
    }
};

/** Set the source index for the renderer nm (friendly or uuid)
 *
 * @param nm friendly name or uuid of the UPnP media renderer
 * @param sourceindex index of the source
 *
 */
bool UPNPP_API setSourceIndex(const std::string& rdrnm, int sourceindex);

/** Set the source index by name for the renderer nm (friendly or uuid)
 *
 * @param nm friendly name or uuid of the UPnP media renderer
 * @param name name of the source
 *
 */
bool UPNPP_API setSourceIndexByName(const std::string& rdrnm,
                                    const std::string& name);

/** Retrieve the Receiver service status for device nm (friendly or uuid)
 *
 * @param live if true, if the service is found, the 'prod' and 'rcv'
 *    field will hold Service handles on return
 * @return st.reason will be empty on success.
 */
void UPNPP_API getReceiverState(const std::string& nm, ReceiverState& st,
                                bool live = true);
/** Get status for all found Receiver services */
void UPNPP_API listReceivers(std::vector<ReceiverState>& vscs);

bool UPNPP_API setReceiverPlaying(ReceiverState st);
bool UPNPP_API setReceiverPlaying(ReceiverState st,
                                  const std::string& uri,
                                  const std::string& meta);
bool UPNPP_API stopReceiver(ReceiverState st);

void UPNPP_API setReceiversFromSender(const std::string& sendernm,
                                      const std::vector<std::string>& rcvnms);
/**
 * @return false for initial error. True if any slave setup was attempted. 
 *        Errors are indicated by a non-empty string in the reasons slot.
 */
bool UPNPP_API setReceiversFromSenderWithStatus(
    const std::string& sendernm,
    const std::vector<std::string>& rcvnms,
    std::vector<std::string>& reasons);

void UPNPP_API setReceiversFromReceiver(const std::string& rcvnm,
                                        const std::vector<std::string>& rcvnms);
/**
 * @return false for initial error. True if any slave setup was attempted. 
 *        Errors are indicated by a non-empty string in the reasons slot.
 */
bool UPNPP_API setReceiversFromReceiverWithStatus(
    const std::string& rcvnm,
    const std::vector<std::string>& rcvnms,
    std::vector<std::string>& reasons);

void UPNPP_API stopReceivers(const std::vector<std::string>& rcvnms);
/**
 * @return false for initial error. True if any slave setup was attempted. 
 *        Errors are indicated by a non-empty string in the reasons slot.
 */
bool UPNPP_API stopReceiversWithStatus(const std::vector<std::string>& rcvnms,
                                       std::vector<std::string>& reasons);

void UPNPP_API setReceiversPlaying(const std::vector<std::string>& rcvnms);

bool UPNPP_API setReceiversPlayingWithStatus(
    const std::vector<std::string>& rcvnms,
    std::vector<std::string>& reasons);

}
}
#endif /* _LINNSONGCAST_H_X_INCLUDED_ */
