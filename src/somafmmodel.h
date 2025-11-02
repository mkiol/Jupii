/* Copyright (C) 2018-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SOMAFMMODEL_H
#define SOMAFMMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QDomElement>
#include <QDomNodeList>
#include <QHash>
#include <QNetworkAccessManager>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "itemmodel.h"
#include "listmodel.h"

class SomafmItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IconRole = Qt::DecorationRole,
        IdRole = Qt::UserRole,
        UrlRole,
        DescriptionRole,
        SelectedRole
    };

   public:
    SomafmItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit SomafmItem(QString id, QString name, QString description, QUrl url,
                        QUrl icon, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString id() const override { return m_id; }
    auto name() const { return m_name; }
    auto description() const { return m_description; }
    auto url() const { return m_url; }
    auto icon() const { return m_icon; }
    QUrl iconThumb() const;

   private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
    QUrl m_icon;
};

class SomafmModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
   public:
    explicit SomafmModel(QObject *parent = nullptr);
    ~SomafmModel() override;
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE void refresh();
    auto refreshing() const { return m_refreshing; }

   signals:
    void error();
    void progressChanged(int n, int total);
    void refreshingChanged();

   private:
    static const QUrl m_dirUrl;
    static const QString m_dirFilename;
    QDomNodeList m_entries;
    bool m_refreshing = false;

    QList<ListItem *> makeItems() override;
    bool parseData();
    bool parseData(const QByteArray &data);
    void downloadDir();
    static QString bestImage(const QDomElement &entry);
    void setRefreshing(bool value);
};

#endif  // SOMAFMMODEL_H
