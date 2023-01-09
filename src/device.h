/* Copyright (C) 2019-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QString>
#include <libupnpp/device/device.hxx>
#include <memory>

class ContentDirectoryService;
class ConnectionManagerService;

class MediaServerDevice : public QObject, public UPnPProvider::UpnpDevice {
    Q_OBJECT
   public:
    static const QString descTemplate;
    static const QString csTemplate;
    static const QString cmTemplate;
    static QString desc();
    MediaServerDevice(QObject* parent = nullptr);
    ~MediaServerDevice() override;
    bool readLibFile(const std::string& name, std::string& contents) override;

   public slots:
    void updateDirectory();

   private:
    std::unique_ptr<ContentDirectoryService> cd;
    std::unique_ptr<ConnectionManagerService> cm;
    int actionHandler(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out);
    void start();

    // cd actions
    static int getSearchCapabilities(const UPnPP::SoapIncoming& in,
                                     UPnPP::SoapOutgoing& out);
    static int getSortCapabilities(const UPnPP::SoapIncoming& in,
                                   UPnPP::SoapOutgoing& out);
    int getSystemUpdateID(const UPnPP::SoapIncoming& in,
                          UPnPP::SoapOutgoing& out) const;
    int browse(const UPnPP::SoapIncoming& in, UPnPP::SoapOutgoing& out) const;

    // cm actions
    static int getProtocolInfo(const UPnPP::SoapIncoming& in,
                               UPnPP::SoapOutgoing& out);
    static int getCurrentConnectionIDs(const UPnPP::SoapIncoming& in,
                                       UPnPP::SoapOutgoing& out);
    static int getCurrentConnectionInfo(const UPnPP::SoapIncoming& in,
                                        UPnPP::SoapOutgoing& out);
};

#endif  // DEVICE_H
