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
extern OHSNH senderService(DVCH dev);

/**
 * Find device with given name (try friendly, then uuid), and
 * return the result of senderService()
 */
extern OHSNH getSender(const std::string& nm, std::string& reason);

/** Everything you need to know about a Sender */
struct SenderState {
    std::string nm;
    std::string UDN;
    std::string uri;
    std::string meta;
    std::string reason;
    bool has_sender;
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
 *    field will hold a handle to it on return
 */
extern void getSenderState(const std::string& nm, SenderState& st,
                           bool live = true);

/** Retrieve status data for all found Senders */
extern void listSenders(std::vector<SenderState>& vscs);

/** Everything you need to know about a Receiver */
struct ReceiverState {
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


/** Retrieve the Receiver service status for device nm (friendly or uuid)
 *
 * @param live if true, if the service is found, the 'prod' and 'rcv'
 *    field will hold Service handles on return
 */
extern void getReceiverState(const std::string& nm, ReceiverState& st,
                             bool live = true);
/** Get status for all found Receiver services */
extern void listReceivers(std::vector<ReceiverState>& vscs);

extern bool setReceiverPlaying(ReceiverState st,
                               const std::string& uri,
                               const std::string& meta);
extern bool stopReceiver(ReceiverState st);

extern void setReceiversFromSender(const std::string& sendernm,
                                   const std::vector<std::string>&
                                   rcvnms);
extern void setReceiversFromReceiver(const std::string& rcvnm,
                                     const std::vector<std::string>&
                                     rcvnms);
extern void stopReceivers(const std::vector<std::string>& rcvnms);

}
}
#endif /* _LINNSONGCAST_H_X_INCLUDED_ */
