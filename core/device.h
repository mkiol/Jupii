/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QThread>
#include <QString>
#include <memory>

#include <device/device.hxx>

class ContentDirectoryService : public UPnPProvider::UpnpService
{
public:
    ContentDirectoryService(const std::string& stp, const std::string& sid, UPnPProvider::UpnpDevice *dev);
    void update();
    std::string systemUpdateId();
    virtual bool getEventData(bool all, std::vector<std::string>& names,
                              std::vector<std::string>& values);
private:
    int updateCounter = 0;
    bool updateNeeded = true;
};

class ConnectionManagerService : public UPnPProvider::UpnpService
{
public:
    ConnectionManagerService(const std::string& stp, const std::string& sid, UPnPProvider::UpnpDevice *dev);
    virtual bool getEventData(bool all, std::vector<std::string>& names,
                              std::vector<std::string>& values);
};

class MediaServerDevice : public QThread, public UPnPProvider::UpnpDevice
{
    Q_OBJECT
    Q_PROPERTY (bool running READ isRunning NOTIFY runningChanged)
public:
    static const QString descTemplate;
    static const QString csTemplate;
    static const QString cmTemplate;
    static QString desc();
    static std::unordered_map<std::string, UPnPProvider::VDirContent> desc_files();
    MediaServerDevice(QObject *parent = nullptr);
    ~MediaServerDevice();
    bool getRunning();
    void sendAdvertisement();

signals:
    void runningChanged();

public slots:
    void updateDirectory();
    void restart();
    void stop();

private:
    std::unique_ptr<ContentDirectoryService> cd;
    std::unique_ptr<ConnectionManagerService> cm;
    void run();
    static int actionHandler(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    int actionHandler2(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);

    // cd actions
    int getSearchCapabilities(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    int getSortCapabilities(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    int getSystemUpdateID (const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    int browse(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);

    // cm actions
    int getProtocolInfo(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    int getCurrentConnectionIDs(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    int getCurrentConnectionInfo(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
};

#endif // DEVICE_H
