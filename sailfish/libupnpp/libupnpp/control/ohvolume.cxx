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
#include "libupnpp/control/ohvolume.hxx"

#include <stdlib.h>                     // for atoi
#include <string.h>                     // for strcmp
#include <math.h>
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for _Bind, bind, _1
#include <ostream>                      // for endl, basic_ostream, etc
#include <string>                       // for string, basic_string, etc

#include "libupnpp/control/service.hxx"  // for VarEventReporter, Service
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGINF
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHVolume::SType("urn:av-openhome-org:service:Volume:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHVolume::isOHVLService(const string& st)
{
    const string::size_type sz(SType.size() - 2);
    return !SType.compare(0, sz, st, 0, sz);
}

void OHVolume::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHVolume::evtCallback: getReporter(): " << getReporter() << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHVolume::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("Volume")) {
            int vol = devVolTo0100(atoi(it->second.c_str()));
            getReporter()->changed(it->first.c_str(), vol);
        } else if (!it->first.compare("VolumeLimit")) {
            m_volmax = atoi(it->second.c_str());
        } else if (!it->first.compare("Mute")) {
            bool val = false;
            stringToBool(it->second, &val);
            getReporter()->changed(it->first.c_str(), val ? 1 : 0);
        } else {
            LOGDEB1("OHVolume event: untracked variable: name [" <<
                    it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

void OHVolume::registerCallback()
{
    Service::registerCallback(bind(&OHVolume::evtCallback, this, _1));
}

int OHVolume::maybeInitVolmax()
{
    // We only query volumelimit once. We should get updated by events
    // later on
    if (m_volmax < 0) {
        volumeLimit(&m_volmax);
    }
    // If volumeLimit fails, we're lost really. Hope it will get
    // better.
    if (m_volmax > 0)
        return m_volmax;
    else
        return 100;
}

// Translate device volume to 0-100
int OHVolume::devVolTo0100(int dev_vol)
{
    int volmin = 0;
    int volmax = maybeInitVolmax();

    int volume;
    if (dev_vol < 0)
        dev_vol = 0;
    if (dev_vol > volmax)
        dev_vol = volmax;
    if (volmax != 100) {
        double fact = double(volmax - volmin) / 100.0;
        if (fact <= 0.0) // ??
            fact = 1.0;
        volume = int((dev_vol - volmin) / fact);
    } else {
        volume = dev_vol;
    }
    return volume;
}

// Translate 0-100 to device volume
int OHVolume::vol0100ToDev(int ivol)
{
    int volmin = 0;
    int volmax = maybeInitVolmax();
    int volstep = 1;

    int desiredVolume = ivol;
    int currentVolume;
    volume(&currentVolume);

    bool goingUp = desiredVolume > currentVolume;
    if (volmax != 100) {
        double fact = double(volmax - volmin) / 100.0;
        // Round up when going up, down when going down. Else the user
        // will be surprised by the GUI control going back if he does
        // not go a full step
        desiredVolume = volmin + (goingUp ? int(ceil(ivol * fact)) :
                                  int(floor(ivol * fact)));
    }
    // Insure integer number of steps (are there devices where step != 1?)
    int remainder = (desiredVolume - volmin) % volstep;
    if (remainder) {
        if (goingUp)
            desiredVolume += volstep - remainder;
        else
            desiredVolume -= remainder;
    }

    return desiredVolume;
}

int OHVolume::volume(int *value)
{
    int mval;
    int ret = runSimpleGet("Volume", "Value", &mval);
    if (ret == 0) {
        *value = devVolTo0100(mval);
    } else {
        *value = 20;
    }
    return ret;
}

int OHVolume::setVolume(int value)
{
    int mval = vol0100ToDev(value);
    return runSimpleAction("SetVolume", "Value", mval);
}

int OHVolume::volumeLimit(int *value)
{
    return runSimpleGet("VolumeLimit", "Value", value);
}

int OHVolume::mute(bool *value)
{
    return runSimpleGet("Mute", "Value", value);
}

int OHVolume::setMute(bool value)
{
    return runSimpleAction("SetMute", "Value", value);
}

} // End namespace UPnPClient
