/* Copyright (C) 2018-2021 Michal Kosciesza <michal@mkiol.net>
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
#include <QDomNodeList>

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
public:
    explicit IcecastModel(QObject *parent = nullptr);
    ~IcecastModel();
    Q_INVOKABLE QVariantList selectedItems() override;

public slots:
    void refresh();

signals:
    void error();

private:
    static const QUrl m_dirUrl;
    static const QString m_dirFilename;
    QDomNodeList m_entries;

    QList<ListItem*> makeItems() override;
    bool parseData();
    void downloadDir();
};

#endif // ICECASTMODEL_H
