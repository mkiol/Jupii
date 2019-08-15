/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QUrl>
#include <QDir>
#include <QCoreApplication>

#include <unordered_map>
#include <string>

#include "device.h"
#include "settings.h"
#include "info.h"
#include "utils.h"
#include "contentserver.h"
#include "playlistmodel.h"
#include "iconprovider.h"

const QString MediaServerDevice::descTemplate = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
    <specVersion>
        <major>1</major>
        <minor>0</minor>
    </specVersion>
    <device>
        <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>
        <friendlyName>%1</friendlyName>
        <manufacturer>%1</manufacturer>
        <manufacturerURL>%2</manufacturerURL>
        <modelDescription>%1</modelDescription>
        <modelName>%1</modelName>
        <modelNumber>%3</modelNumber>
        <modelURL>%2</modelURL>
        <dlna:X_DLNADOC xmlns:dlna="urn:schemas-dlna-org:device-1-0">DMS-1.50</dlna:X_DLNADOC>
        <UDN>uuid:%4</UDN>
        <iconList>
            <icon>
                <mimetype>image/png</mimetype>
                <width>256</width>
                <height>256</height>
                <depth>8</depth>
                <url>%5</url>
            </icon>
            <icon>
                <mimetype>image/png</mimetype>
                <width>120</width>
                <height>120</height>
                <depth>8</depth>
                <url>%6</url>
            </icon>
            <icon>
                <mimetype>image/png</mimetype>
                <width>48</width>
                <height>48</height>
                <depth>8</depth>
                <url>%7</url>
            </icon>
            <icon>
                <mimetype>image/png</mimetype>
                <width>32</width>
                <height>32</height>
                <depth>8</depth>
                <url>%8</url>
            </icon>
            <icon>
                <mimetype>image/png</mimetype>
                <width>16</width>
                <height>16</height>
                <depth>8</depth>
                <url>%9</url>
            </icon>
            <icon>
                <mimetype>image/jpeg</mimetype>
                <width>256</width>
                <height>256</height>
                <depth>8</depth>
                <url>%10</url>
            </icon>
            <icon>
                <mimetype>image/jpeg</mimetype>
                <width>120</width>
                <height>120</height>
                <depth>8</depth>
                <url>%11</url>
            </icon>
            <icon>
                <mimetype>image/jpeg</mimetype>
                <width>48</width>
                <height>48</height>
                <depth>8</depth>
                <url>%12</url>
            </icon>
            <icon>
                <mimetype>image/jpeg</mimetype>
                <width>32</width>
                <height>32</height>
                <depth>8</depth>
                <url>%13</url>
            </icon>
            <icon>
                <mimetype>image/jpeg</mimetype>
                <width>16</width>
                <height>16</height>
                <depth>8</depth>
                <url>%14</url>
            </icon>
        </iconList>
        <serviceList>
            <service>
                <serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:ContentDirectory</serviceId>
                <SCPDURL>/ContentDirectory/scpd.xml</SCPDURL>
                <controlURL>/ContentDirectory/control.xml</controlURL>
                <eventSubURL>/ContentDirectory/event.xml</eventSubURL>
            </service>
            <service>
                <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>
                <serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>
                <SCPDURL>/ConnectionManager/scpd.xml</SCPDURL>
                <controlURL>/ConnectionManager/control.xml</controlURL>
                <eventSubURL>/ConnectionManager/event.xml</eventSubURL>
            </service>
        </serviceList>
    </device>
</root>)";

const QString MediaServerDevice::csTemplate = R"(<?xml version="1.0"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
    <specVersion>
        <major>1</major>
        <minor>0</minor>
    </specVersion>
    <actionList>
        <action>
            <name>GetSearchCapabilities</name>
            <argumentList>
                <argument>
                    <name>SearchCaps</name>
                    <direction>out</direction>
                    <relatedStateVariable>SearchCapabilities</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>GetSortCapabilities</name>
            <argumentList>
                <argument>
                    <name>SortCaps</name>
                    <direction>out</direction>
                    <relatedStateVariable>SortCapabilities</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>GetSystemUpdateID</name>
            <argumentList>
                <argument>
                    <name>Id</name>
                    <direction>out</direction>
                    <relatedStateVariable>SystemUpdateID</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>Browse</name>
            <argumentList>
                <argument>
                    <name>ObjectID</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_ObjectID</relatedStateVariable>
                </argument>
                <argument>
                    <name>BrowseFlag</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_BrowseFlag</relatedStateVariable>
                </argument>
                <argument>
                    <name>Filter</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_Filter</relatedStateVariable>
                </argument>
                <argument>
                    <name>StartingIndex</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_Index</relatedStateVariable>
                </argument>
                <argument>
                    <name>RequestedCount</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable>
                </argument>
                <argument>
                    <name>SortCriteria</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_SortCriteria</relatedStateVariable>
                </argument>
                <argument>
                    <name>Result</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_Result</relatedStateVariable>
                </argument>
                <argument>
                    <name>NumberReturned</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable>
                </argument>
                <argument>
                    <name>TotalMatches</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable>
                </argument>
                <argument>
                    <name>UpdateID</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_UpdateID</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
    </actionList>
    <serviceStateTable>
        <stateVariable sendEvents="yes">
            <name>TransferIDs</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_ObjectID</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_Result</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_SearchCriteria</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_BrowseFlag</name>
            <dataType>string</dataType>
            <allowedValueList>
                <allowedValue>BrowseMetadata</allowedValue>
                <allowedValue>BrowseDirectChildren</allowedValue>
            </allowedValueList>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_Filter</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_SortCriteria</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_Index</name>
            <dataType>ui4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_Count</name>
            <dataType>ui4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_UpdateID</name>
            <dataType>ui4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>SearchCapabilities</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>SortCapabilities</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="yes">
            <name>SystemUpdateID</name>
            <dataType>ui4</dataType>
        </stateVariable>
    </serviceStateTable>
</scpd>)";

const QString MediaServerDevice::cmTemplate = R"(<?xml version="1.0"?>
<scpd xmlns="urn:schemas-upnp-org:service-1-0">
    <specVersion>
        <major>1</major>
        <minor>0</minor>
    </specVersion>
    <actionList>
        <action>
            <name>GetProtocolInfo</name>
            <argumentList>
                <argument>
                    <name>Source</name>
                    <direction>out</direction>
                    <relatedStateVariable>SourceProtocolInfo</relatedStateVariable>
                </argument>
                <argument>
                    <name>Sink</name>
                    <direction>out</direction>
                    <relatedStateVariable>SinkProtocolInfo</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>GetCurrentConnectionIDs</name>
            <argumentList>
                <argument>
                    <name>ConnectionIDs</name>
                    <direction>out</direction>
                    <relatedStateVariable>CurrentConnectionIDs</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
        <action>
            <name>GetCurrentConnectionInfo</name>
            <argumentList>
                <argument>
                    <name>ConnectionID</name>
                    <direction>in</direction>
                    <relatedStateVariable>A_ARG_TYPE_ConnectionID</relatedStateVariable>
                </argument>
                <argument>
                    <name>RcsID</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_RcsID</relatedStateVariable>
                </argument>
                <argument>
                    <name>AVTransportID</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_AVTransportID</relatedStateVariable>
                </argument>
                <argument>
                    <name>ProtocolInfo</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_ProtocolInfo</relatedStateVariable>
                </argument>
                <argument>
                    <name>PeerConnectionManager</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_ConnectionManager</relatedStateVariable>
                </argument>
                <argument>
                    <name>PeerConnectionID</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_ConnectionID</relatedStateVariable>
                </argument>
                <argument>
                    <name>Direction</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_Direction</relatedStateVariable>
                </argument>
                <argument>
                    <name>Status</name>
                    <direction>out</direction>
                    <relatedStateVariable>A_ARG_TYPE_ConnectionStatus</relatedStateVariable>
                </argument>
            </argumentList>
        </action>
    </actionList>
    <serviceStateTable>
        <stateVariable sendEvents="yes">
            <name>SourceProtocolInfo</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="yes">
            <name>SinkProtocolInfo</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="yes">
            <name>CurrentConnectionIDs</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_ConnectionStatus</name>
            <dataType>string</dataType>
            <allowedValueList>
                <allowedValue>OK</allowedValue>
                <allowedValue>ContentFormatMismatch</allowedValue>
                <allowedValue>InsufficientBandwidth</allowedValue>
                <allowedValue>UnreliableChannel</allowedValue>
                <allowedValue>Unknown</allowedValue>
            </allowedValueList>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_ConnectionManager</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_Direction</name>
            <dataType>string</dataType>
            <allowedValueList>
                <allowedValue>Input</allowedValue>
                <allowedValue>Output</allowedValue>
            </allowedValueList>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_ProtocolInfo</name>
            <dataType>string</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_ConnectionID</name>
            <dataType>i4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_AVTransportID</name>
            <dataType>i4</dataType>
        </stateVariable>
        <stateVariable sendEvents="no">
            <name>A_ARG_TYPE_RcsID</name>
            <dataType>i4</dataType>
        </stateVariable>
    </serviceStateTable>
</scpd>)";

MediaServerDevice* MediaServerDevice::inst = nullptr;

MediaServerDevice* MediaServerDevice::instance(QObject *parent)
{
    if (MediaServerDevice::inst == nullptr) {
        MediaServerDevice::inst = new MediaServerDevice(parent);
    }

    return MediaServerDevice::inst;
}

void MediaServerDevice::updateDirectory()
{
    qDebug() << "updateDirectory";
    if (cd) {
        cd->update();
    }
}

QString MediaServerDevice::desc()
{
    qDebug() << "UUID:" << Settings::instance()->mediaServerDevUuid();

    auto desc = descTemplate.arg(Jupii::APP_NAME,
                     Jupii::PAGE,
                     Jupii::APP_VERSION,
                     Settings::instance()->mediaServerDevUuid());
    // -- icons --
    QString ifname, addr;
    if (!Utils::instance()->getNetworkIf(ifname, addr)) {
        qWarning() << "Cannot find valid network interface";
        return desc;
    }

    QList<QUrl> icons{IconProvider::urlToImg("upnp-256.png"),
                      IconProvider::urlToImg("upnp-120.png"),
                      IconProvider::urlToImg("upnp-48.png"),
                      IconProvider::urlToImg("upnp-32.png"),
                      IconProvider::urlToImg("upnp-16.png"),
                      IconProvider::urlToImg("upnp-256.jpg"),
                      IconProvider::urlToImg("upnp-120.jpg"),
                      IconProvider::urlToImg("upnp-48.jpg"),
                      IconProvider::urlToImg("upnp-32.jpg"),
                      IconProvider::urlToImg("upnp-16.jpg")};
    for (const auto& icon : icons) {
        auto id = Utils::idFromUrl(icon, ContentServer::artCookie);
        QUrl iconUrl;
        if (ContentServer::makeUrl(id, iconUrl)) {
            desc = desc.arg(iconUrl.toString());
        } else {
            qWarning() << "Cannot make Url form icon url";
            return desc;
        }
    }
    // --

    return desc;
}

std::unordered_map<std::string, UPnPProvider::VDirContent> MediaServerDevice::desc_files()
{
    std::unordered_map<std::string, UPnPProvider::VDirContent> files;
    files.insert({"/MediaServer/description.xml", {desc().toStdString(), "text/xml"}});
    files.insert({"/ContentDirectory/scpd.xml", {csTemplate.toStdString(), "text/xml"}});
    files.insert({"/ConnectionManager/scpd.xml", {cmTemplate.toStdString(), "text/xml"}});
    return files;
}

MediaServerDevice::MediaServerDevice(QObject *parent) :
    QThread(parent),
    UPnPProvider::UpnpDevice(
        "uuid:" + Settings::instance()->mediaServerDevUuid().toStdString(),
        desc_files())
{
    qDebug() << "Creating UPnP Media Server device";

    cd = std::unique_ptr<ContentDirectoryService>(
                new ContentDirectoryService("urn:schemas-upnp-org:service:ContentDirectory:1",
                                "urn:upnp-org:serviceId:ContentDirectory", this));
    addActionMapping(cd.get(), "GetSearchCapabilities", actionHandler);
    addActionMapping(cd.get(), "GetSortCapabilities", actionHandler);
    addActionMapping(cd.get(), "GetSystemUpdateID", actionHandler);
    addActionMapping(cd.get(), "Browse", actionHandler);

    cm = std::unique_ptr<ConnectionManagerService>(
                new ConnectionManagerService("urn:schemas-upnp-org:service:ConnectionManager:1",
                                "urn:upnp-org:serviceId:ConnectionManager", this));
    addActionMapping(cm.get(), "GetProtocolInfo", actionHandler);
    addActionMapping(cm.get(), "GetCurrentConnectionIDs", actionHandler);
    addActionMapping(cm.get(), "GetCurrentConnectionInfo", actionHandler);

    auto pl = PlaylistModel::instance();
    connect(pl, &PlaylistModel::itemsAdded, this, &MediaServerDevice::updateDirectory);
    connect(pl, &PlaylistModel::itemsRemoved, this, &MediaServerDevice::updateDirectory);
    connect(pl, &PlaylistModel::itemsLoaded, this, &MediaServerDevice::updateDirectory);

    auto s = Settings::instance();
    connect(s, &Settings::contentDirSupportedChanged, this, &MediaServerDevice::restart);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &MediaServerDevice::stop);

    restart();
}

void MediaServerDevice::run()
{
    qDebug() << "UPnP Media Server device starting";
    emit runningChanged();
    eventloop();
    qDebug() << "UPnP Media Server device ending";
    emit runningChanged();
}

void MediaServerDevice::restart()
{
    bool supported = Settings::instance()->getContentDirSupported();
    if (isRunning()) {
        if (!supported)
            stop();
    } else {
        if (supported)
            start(QThread::LowPriority);
    }
}

void MediaServerDevice::stop()
{
    shouldExit();
}

int MediaServerDevice::actionHandler(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    auto action = QString::fromStdString(in.getName());
    qDebug() << "actionHandler:" << action;

    auto dev = MediaServerDevice::instance();

    if (action == "GetSortCapabilities") {
        return dev->getSortCapabilities(in, out);
    } else if (action == "Browse") {
        return dev->browse(in, out);
    } else if (action == "GetSearchCapabilities") {
        return dev->getSearchCapabilities(in, out);
    } else if (action == "GetSystemUpdateID") {
        return dev->getSystemUpdateID(in, out);
    } else if (action == "GetProtocolInfo") {
        return dev->getProtocolInfo(in, out);
    } else if (action == "GetCurrentConnectionIDs") {
        return dev->getCurrentConnectionIDs(in, out);
    } else if (action == "GetCurrentConnectionInfo") {
        return dev->getCurrentConnectionInfo(in, out);
    }

    return UPNP_E_BAD_REQUEST;
}

int MediaServerDevice::getSearchCapabilities(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    Q_UNUSED(in)
    out.addarg("SortCaps","dc:title");
    return UPNP_E_SUCCESS;
}

int MediaServerDevice::getSortCapabilities(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    Q_UNUSED(in)
    out.addarg("SearchCaps","dc:title");
    return UPNP_E_SUCCESS;
}

int MediaServerDevice::getSystemUpdateID(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    Q_UNUSED(in)
    out.addarg("Id", cd->systemUpdateId());
    return UPNP_E_SUCCESS;
}

int MediaServerDevice::browse(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    std::string objectId; in.get("ObjectID", &objectId);
    std::string browseFlag; in.get("BrowseFlag", &browseFlag);
    std::string requestedCount; in.get("RequestedCount", &requestedCount);

    qDebug() << "ObjectID:" << QString::fromStdString(objectId);
    qDebug() << "BrowseFlag:" << QString::fromStdString(browseFlag);
    qDebug() << "RequestedCount:" << QString::fromStdString(requestedCount);

    //std::string filter; in.get("Filter", &filter);
    //std::string startingIndex; in.get("StartingIndex", &startingIndex);
    //std::string sortCriteria; in.get("SortCriteria", &sortCriteria);
    //qDebug() << "Filter:" << QString::fromStdString(filter);
    //qDebug() << "StartingIndex:" << QString::fromStdString(startingIndex);
    //qDebug() << "SortCriteria:" << QString::fromStdString(sortCriteria);

    if (objectId == "0") {
        if (browseFlag == "BrowseMetadata") {
            out.addarg("Result", "<DIDL-Lite xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\" "
                                 "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
                                 "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
                                 "xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\">"
                                 "<container id=\"0\" parentID=\"-1\" restricted=\"1\" searchable=\"0\">"
                                 "<dc:title>Root</dc:title><upnp:class>object.container</upnp:class>"
                                 "</container></DIDL-Lite>");
            out.addarg("NumberReturned", "1");
            out.addarg("TotalMatches", "1");
        } else { //BrowseDirectChildren
            auto list = PlaylistModel::instance()->getDidlList(std::stoi(requestedCount));
            out.addarg("Result", list.second.toStdString());
            std::string count = std::to_string(list.first);
            out.addarg("NumberReturned", count);
            out.addarg("TotalMatches", count);

        }
    } else {
        if (browseFlag == "BrowseMetadata") {
            auto list = PlaylistModel::instance()->getDidlList(1, QString::fromStdString(objectId));
            out.addarg("Result", list.second.toStdString());
            std::string count = std::to_string(list.first);
            out.addarg("NumberReturned", count);
            out.addarg("TotalMatches", count);
        } else { //BrowseDirectChildren
            qWarning() << "Requested objectID is not root for BrowseDirectChildren";
            return UPNP_E_BAD_REQUEST;
        }
    }

    out.addarg("UpdateID", cd->systemUpdateId());

    return UPNP_E_SUCCESS;
}

int MediaServerDevice::getProtocolInfo(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    Q_UNUSED(in)
    out.addarg("Source","http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN,"
                        "http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_SM,"
                        "http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_MED,"
                        "http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_LRG,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_HD_50_AC3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_HD_60_AC3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_PS_NTSC,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_PS_PAL,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_HD_NA_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_SD_NA_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=MPEG_TS_SD_EU_ISO,"
                        "http-get:*:video/mpeg:DLNA.ORG_PN=MPEG1,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AAC_MULT5,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_SD_AC3,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF15_AAC_520,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_CIF30_AAC_940,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L31_HD_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L32_HD_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_BL_L3L_SD_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_HP_HD_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_1080i_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=AVC_MP4_MP_HD_720p_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=MPEG4_P2_MP4_ASP_AAC,"
                        "http-get:*:video/mp4:DLNA.ORG_PN=MPEG4_P2_MP4_SP_VGA_AAC,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_50_AC3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HD_60_AC3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_HP_HD_AC3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AAC_MULT5_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_AC3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_HD_MPEG1_L3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AAC_MULT5_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_AC3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=AVC_TS_MP_SD_MPEG1_L3_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_HD_NA_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_EU_T,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA,"
                        "http-get:*:video/vnd.dlna.mpeg-tts:DLNA.ORG_PN=MPEG_TS_SD_NA_T,"
                        "http-get:*:audio/mpeg:DLNA.ORG_PN=MP3,"
                        "http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO_320,"
                        "http-get:*:audio/3gpp:DLNA.ORG_PN=AAC_ISO_320,"
                        "http-get:*:audio/mp4:DLNA.ORG_PN=AAC_ISO,"
                        "http-get:*:audio/mp4:DLNA.ORG_PN=AAC_MULT5_ISO,"
                        "http-get:*:audio/L16;rate=44100;channels=2:DLNA.ORG_PN=LPCM,"
                        "http-get:*:image/jpeg:*,"
                        "http-get:*:video/avi:*,"
                        "http-get:*:video/divx:*,"
                        "http-get:*:video/x-matroska:*,"
                        "http-get:*:video/mpeg:*,"
                        "http-get:*:video/mp4:*,"
                        "http-get:*:video/x-ms-wmv:*,"
                        "http-get:*:video/x-msvideo:*,"
                        "http-get:*:video/x-flv:*,"
                        "http-get:*:video/x-tivo-mpeg:*,"
                        "http-get:*:video/quicktime:*,"
                        "http-get:*:audio/mp4:*,"
                        "http-get:*:audio/x-wav:*,"
                        "http-get:*:audio/x-flac:*,"
                        "http-get:*:audio/x-dsd:*,"
                        "http-get:*:application/ogg:*");
    out.addarg("Sink","");
    return UPNP_E_SUCCESS;
}

int MediaServerDevice::getCurrentConnectionIDs(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    Q_UNUSED(in)
    out.addarg("ConnectionIDs","");
    return UPNP_E_SUCCESS;
}

int MediaServerDevice::getCurrentConnectionInfo(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out)
{
    std::string connectionID; in.get("ConnectionID", &connectionID);
    qDebug() << "ConnectionID:" << QString::fromStdString(connectionID);
    out.addarg("RcsID","");
    out.addarg("AVTransportID","");
    out.addarg("ProtocolInfo","");
    out.addarg("PeerConnectionManage","");
    out.addarg("PeerConnectionID","");
    out.addarg("Direction","");
    out.addarg("Status","");
    return UPNP_E_SUCCESS;
}

ContentDirectoryService::ContentDirectoryService(const std::string& stp, const std::string& sid,
                         UPnPProvider::UpnpDevice *dev) :
    UPnPProvider::UpnpService(stp, sid, dev)
{
}

std::string ContentDirectoryService::systemUpdateId()
{
    return std::to_string(updateCounter);
}

void ContentDirectoryService::update()
{
    updateCounter++;
    updateNeeded = true;
}

bool ContentDirectoryService::getEventData(bool all, std::vector<std::string>& names,
                          std::vector<std::string>& values)
{
    Q_UNUSED(all)
    if (updateNeeded) {
        names.push_back("SystemUpdateID");
        values.push_back(systemUpdateId());
        updateNeeded = false;
    }
    return true;
}

ConnectionManagerService::ConnectionManagerService(const std::string& stp, const std::string& sid,
                         UPnPProvider::UpnpDevice *dev) :
    UPnPProvider::UpnpService(stp, sid, dev)
{
}

bool ConnectionManagerService::getEventData(bool all, std::vector<std::string>& names,
                          std::vector<std::string>& values)
{
    Q_UNUSED(all)
    Q_UNUSED(names)
    Q_UNUSED(values)
    return true;
}
