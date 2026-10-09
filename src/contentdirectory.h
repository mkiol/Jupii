/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef CONTENTDIRECTORY_H
#define CONTENTDIRECTORY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "libupnpp/control/cdirectory.hxx"
#include "libupnpp/control/service.hxx"

#include "service.h"

class ContentDirectory : public Service
{
    Q_OBJECT
public:
    explicit ContentDirectory(QObject* parent = nullptr);

    bool readItems(const QString &id, UPnPClient::UPnPDirContent &content);
    bool readItem(const QString &id, const QString &pid,
                  UPnPClient::UPnPDirContent &content);

    /** Search the content directory service from root ("0").
     *  @param criteria an UPnP searchcriteria string (see
     *         UPnP-av-ContentDirectory-v1-Service section 2.5.5) */
    bool search(const QString &criteria, UPnPClient::UPnPDirContent &content);

    /** Search capabilities advertised by the server (S: field).
     *  Empty list means no search support or service not inited. Cached after
     *  first successful query for the current device connection. */
    QStringList searchCapabilities();

   private:
    void changed(const QString &name, const QVariant &value);
    UPnPClient::Service* createUpnpService(const UPnPClient::UPnPDeviceDesc &ddesc,
                                           const UPnPClient::UPnPServiceDesc &sdesc);
    void postInit();
    void reset();
    std::string type() const;

    UPnPClient::ContentDirectory* s();

    QStringList m_searchCaps;
    bool m_searchCapsLoaded = false;
};

#endif // CONTENTDIRECTORY_H
