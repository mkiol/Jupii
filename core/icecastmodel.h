/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ICECASTMODEL_H
#define ICECASTMODEL_H

#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QUrl>
#include <QDir>
#include <QDomNodeList>
#include <QNetworkReply>

#include "contentserver.h"
#include "listmodel.h"
#include "itemmodel.h"

class IcecastItem : public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        TypeRole,
        UrlRole,
        DescriptionRole,
        SelectedRole
    };

public:
    IcecastItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit IcecastItem(const QString &id,
                      const QString &name,
                      const QString &description,
                      const QUrl &url,
                      ContentServer::Type type,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString description() const { return m_description; }
    inline QUrl url() const { return m_url; }
    inline ContentServer::Type type() const { return m_type; }
private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
    ContentServer::Type m_type;
};

class IcecastModel : public SelectableItemModel
{
    Q_OBJECT
    Q_PROPERTY (bool refreshing READ isRefreshing NOTIFY refreshingChanged)

public:
    explicit IcecastModel(QObject *parent = nullptr);
    bool isRefreshing();
    Q_INVOKABLE QVariantList selectedItems();

public slots:
    void refresh();

signals:
    void refreshingChanged();
    void error();

private slots:
    void handleDataDownloadFinished();

private:
    static const QString m_dirUrl;
    static const QString m_dirFilename;
    static const int httpTimeout = 100000;

    QDomNodeList m_entries;
    bool m_refreshing = false;

    QList<ListItem*> makeItems();
    bool parseData();
};

#endif // ICECASTMODEL_H
