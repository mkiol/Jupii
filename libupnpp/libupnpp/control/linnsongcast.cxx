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

#include "libupnpp/control/linnsongcast.hxx"

#include <algorithm>

#include "libupnpp/soaphelp.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/control/discovery.hxx"
#include "libupnpp/control/ohproduct.hxx"
#include "libupnpp/control/mediarenderer.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {
namespace Songcast {

static MRDH getRenderer(const string& name)
{
    UPnPDeviceDesc ddesc;
    if (UPnPDeviceDirectory::getTheDir()->getDevByUDN(name, ddesc)) {
        return MRDH(new MediaRenderer(ddesc));
    } else if (UPnPDeviceDirectory::getTheDir()->getDevByFName(name, ddesc)) {
        return MRDH(new MediaRenderer(ddesc));
    }
    LOGERR("getRenderer: getDevByFname failed for " << name << endl);
    return MRDH();
}

static DVCH getDevice(const string& name)
{
    UPnPDeviceDesc ddesc;
    if (UPnPDeviceDirectory::getTheDir()->getDevByUDN(name, ddesc)) {
        return DVCH(new MediaRenderer(ddesc));
    } else if (UPnPDeviceDirectory::getTheDir()->getDevByFName(name, ddesc)) {
        return DVCH(new MediaRenderer(ddesc));
    }
    LOGERR("getDevice: getDevByFname failed for " << name << endl);
    return DVCH();
}

OHSNH senderService(DVCH dev)
{
    OHSNH handle;
    for (vector<UPnPServiceDesc>::const_iterator it =
                dev->desc()->services.begin();
            it != dev->desc()->services.end(); it++) {
        if (OHSender::isOHSenderService(it->serviceType)) {
            handle = OHSNH(new OHSender(*(dev->desc()), *it));
            break;
        }
    }
    return handle;
}

OHSNH getSender(const string& nm, string& reason)
{
    OHSNH ret;
    DVCH dev = getDevice(nm);
    if (!dev) {
        reason = nm + " : can't connect or not a renderer";
        return ret;
    }
    return senderService(dev);
}

void getSenderState(const string& nm, SenderState& st, bool live)
{
    st.reset();
    st.nm = nm;

    DVCH dev = getDevice(nm);
    if (!dev) {
        st.reason = nm + " not a media renderer?";
        return;
    }
    st.nm = dev->desc()->friendlyName;
    st.UDN = dev->desc()->UDN;

    st.sender = senderService(dev);
    if (!st.sender) {
        return;
    }
    st.has_sender = true;
    int ret = st.sender->metadata(st.uri, st.meta);
    if (ret != 0) {
        st.reason = nm + " metadata() failed, code " + SoapHelp::i2s(ret);
        return;
    }
    if (!live) {
        st.sender.reset();
    }
    return;
}

void getReceiverState(const string& nm, ReceiverState& st, bool live)
{
    st.reset();
    st.nm = nm;

    MRDH rdr = getRenderer(nm);
    if (!rdr) {
        st.reason = nm + " not a media renderer?";
        return;
    }
    st.nm = rdr->desc()->friendlyName;
    st.UDN = rdr->desc()->UDN;

    OHPRH prod = rdr->ohpr();
    if (!prod) {
        st.state = ReceiverState::SCRS_NOOH;
        st.reason =  nm + ": device has no OHProduct service";
        return;
    }
    int currentindex;
    if (prod->sourceIndex(&currentindex)) {
        st.reason = nm + " : sourceIndex failed";
        return;
    }

    vector<OHProduct::Source> sources;
    if (prod->getSources(sources) || sources.size() == 0) {
        st.reason = nm + ": getSources failed";
        return;
    }
    unsigned int rcvi = 0;
    for (; rcvi < sources.size(); rcvi++) {
        if (!sources[rcvi].type.compare("Receiver"))
            break;
    }
    if (rcvi == sources.size()) {
        st.state = ReceiverState::SCRS_NOOH;
        st.reason = nm +  " has no Receiver service";
        return;
    }
    st.receiverSourceIndex = int(rcvi);

    if (currentindex < 0 || currentindex >= int(sources.size())) {
        st.reason = nm +  ": bad index " + SoapHelp::i2s(currentindex) +
                    " not inside sources of size " +  SoapHelp::i2s(sources.size());
        return;
    }

    // Looks like the device has a receiver service. We may have to return a
    // handle for it.
    OHRCH rcv = rdr->ohrc();

    // We used to check if the active source was the receiver service
    // here. But the Receiver can be active (connected to a sender)
    // without being the current source, and it forced playing tricks
    // with source names to handle upmpdcli in SenderReceiver mode. So
    // we don't do it any more and just query the Receiver state and
    // list the Receiver as available if it has no connected Uri

    if (!rcv) {
        st.reason = nm +  ": no receiver service??";
        goto out;
    }
    if (rcv->sender(st.uri, st.meta)) {
        LOGERR("getReceiverState: sender() failed\n");
        st.reason = nm +  ": Receiver::Sender failed";
        goto out;
    }

    if (st.uri.empty()) {
        st.state = ReceiverState::SCRS_NOTRECEIVER;
        st.reason = nm +  " not in receiver mode ";
        goto out;
    }

    OHPlaylist::TPState tpst;
    if (rcv->transportState(&tpst)) {
        LOGERR("getReceiverState: transportState() failed\n");
        st.reason = nm +  ": Receiver::transportState() failed";
        goto out;
    }

    if (tpst == OHPlaylist::TPS_Playing) {
        st.state = ReceiverState::SCRS_PLAYING;
    } else {
        st.state = ReceiverState::SCRS_STOPPED;
    }
out:
    if (live) {
        st.prod = prod;
        st.rcv = rcv;
    }

    return;
}

void listReceivers(vector<ReceiverState>& vreceivers)
{
    vreceivers.clear();
    vector<UPnPDeviceDesc> vdds;
    if (!MediaRenderer::getDeviceDescs(vdds)) {
        LOGERR("listReceivers::getDeviceDescs failed" << endl);
        return;
    }

    for (auto& entry : vdds) {
        ReceiverState st;
        getReceiverState(entry.UDN, st, false);
        if (st.state == ReceiverState::SCRS_NOTRECEIVER ||
                st.state == ReceiverState::SCRS_PLAYING ||
                st.state == ReceiverState::SCRS_STOPPED) {
            vreceivers.push_back(st);
        }
    }
}

// Look at all service descriptions and store udns of parent devices
// for Sender services.
static bool lookForSenders(vector<string>* sndudns,
                           const UPnPDeviceDesc& device,
                           const UPnPServiceDesc& service)
{
    if (OHSender::isOHSenderService(service.serviceType)) {
        sndudns->push_back(device.UDN);
    }
    return true;
}

void listSenders(vector<SenderState>& vsenders)
{
    vsenders.clear();
    // Search the directory for all devices with a Sender service
    vector<string> sndudns;
    UPnPDeviceDirectory::Visitor visitor =
        bind(lookForSenders, &sndudns, _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    sort(sndudns.begin(), sndudns.end());
    sndudns.erase(unique(sndudns.begin(), sndudns.end()), sndudns.end());

    // Get the details
    for (unsigned int i = 0; i < sndudns.size(); i++) {
        SenderState st;
        string udn = sndudns[i];
        getSenderState(udn, st, false);
        if (st.has_sender) {
            vsenders.push_back(st);
        }
    }
}

bool setReceiverPlaying(ReceiverState st,
                        const string& uri, const string& meta)
{
    if (!st.rcv || !st.prod) {
        string uuid = st.UDN;
        getReceiverState(uuid, st, true);
        if (!st.rcv || !st.prod) {
            st.reason = st.nm + " : can't connect";
            return false;
        }
    }

    if (st.rcv->setSender(uri, meta)) {
        st.reason = st.nm + " Receiver::setSender() failed";
        return false;
    }
    if (st.prod->setSourceIndex(st.receiverSourceIndex)) {
        st.reason = st.nm + " : can't set source index to " +
                    SoapHelp::i2s(st.receiverSourceIndex);
        return false;
    }
    if (st.rcv->play()) {
        st.reason = st.nm + " Receiver::play() failed";
        return false;
    }
    return true;
}

bool stopReceiver(ReceiverState st)
{
    LOGDEB("stopReceiver: st.nm " << st.nm << " st.UDN " << st.UDN << endl);
    if (!st.rcv || !st.prod) {
        string uuid = st.UDN;
        getReceiverState(uuid, st, true);
        if (!st.rcv || !st.prod) {
            st.reason = st.nm + " : can't connect";
            return false;
        }
    }
    if (st.rcv->stop()) {
        st.reason = st.nm + " Receiver::play() failed";
        return false;
    }
    if (st.prod->setSourceIndex(0)) {
        st.reason = st.nm + " : can't set source index to " +
                    SoapHelp::i2s(st.receiverSourceIndex);
        return false;
    }
    return true;
}

void setReceiversFromSender(const string& sendernm, const vector<string>& rcvs)
{
    string reason;
    OHSNH sender = getSender(sendernm, reason);
    if (!sender) {
        LOGERR(reason << endl);
        return;
    }
    string uri, meta;
    int iret;
    if ((iret = sender->metadata(uri, meta)) != 0) {
        LOGERR("Can't retrieve sender metadata. Error: " << SoapHelp::i2s(iret)
               << endl);
        return;
    }

    // Note: sequence sent from windows songcast when setting up a receiver:
    //   Product::SetSourceIndex / Receiver::SetSender / Receiver::Play
    // When stopping:
    //   Receiver::Stop / Product::SetStandby
    for (auto& sl: rcvs) {
        LOGERR("Setting up " << sl << endl);
        ReceiverState sstate;
        getReceiverState(sl, sstate);

        switch (sstate.state) {
        case ReceiverState::SCRS_GENERROR:
        case ReceiverState::SCRS_NOOH:
            LOGERR(sl << sstate.reason << endl);
            continue;
        case ReceiverState::SCRS_STOPPED:
        case ReceiverState::SCRS_PLAYING:
        case ReceiverState::SCRS_NOTRECEIVER:
            if (setReceiverPlaying(sstate, uri, meta)) {
                LOGERR(sl << " set up for playing " << uri << endl);
            } else {
                LOGERR(sstate.reason << endl);
            }
        }
    }
}

void setReceiversFromReceiver(const string& masterName,
                              const vector<string>& slaves)
{
    ReceiverState mstate;
    getReceiverState(masterName, mstate);
    if (mstate.state != ReceiverState::SCRS_PLAYING) {
        LOGERR("Required master not in Receiver Playing mode" << endl);
        return;
    }

    // Note: sequence sent from windows songcast when setting up a receiver:
    //   Product::SetSourceIndex / Receiver::SetSender / Receiver::Play
    // When stopping:
    //   Receiver::Stop / Product::SetStandby
    for (auto& sl: slaves) {
        LOGERR("Setting up " << sl << endl);
        ReceiverState sstate;
        getReceiverState(sl, sstate);

        switch (sstate.state) {
        case ReceiverState::SCRS_GENERROR:
        case ReceiverState::SCRS_NOOH:
            LOGERR(sl << sstate.reason << endl);
            continue;
        case ReceiverState::SCRS_STOPPED:
        case ReceiverState::SCRS_PLAYING:
            LOGERR(sl << ": already in receiver mode" << endl);
            continue;
        case ReceiverState::SCRS_NOTRECEIVER:
            if (setReceiverPlaying(sstate, mstate.uri, mstate.meta)) {
                LOGERR(sl << " set up for playing " << mstate.uri << endl);
            } else {
                LOGERR(sstate.reason << endl);
            }
        }
    }
}

void stopReceivers(const vector<string>& slaves)
{
    for (auto& sl: slaves) {
        LOGERR("Songcast: resetting " << sl << endl);
        ReceiverState sstate;
        getReceiverState(sl, sstate);

        switch (sstate.state) {
        case ReceiverState::SCRS_GENERROR:
        case ReceiverState::SCRS_NOOH:
            LOGERR(sl << sstate.reason << endl);
            continue;
        case ReceiverState::SCRS_NOTRECEIVER:
            LOGERR(sl << ": not in receiver mode" << endl);
            continue;
        case ReceiverState::SCRS_STOPPED:
        case ReceiverState::SCRS_PLAYING:
            if (stopReceiver(sstate)) {
                LOGERR(sl << " back from receiver mode " << endl);
            } else {
                LOGERR(sstate.reason << endl);
            }
        }
    }
}

}
}

