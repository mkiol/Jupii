/* Copyright (C) 2020-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FOSDEMMODEL_H
#define FOSDEMMODEL_H

#include <QByteArray>
#include <QDebug>
#include <QDomElement>
#include <QDomNodeList>
#include <QHash>
#include <QString>
#include <QUrl>
#include <QVariant>

#include "itemmodel.h"
#include "listmodel.h"

class FosdemItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        TrackRole,
        UrlRole,
        SelectedRole
    };

   public:
    FosdemItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit FosdemItem(const QString &id, const QString &name,
                        const QString &track, const QUrl &url,
                        QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto name() const { return m_name; }
    inline auto track() const { return m_track; }
    inline auto url() const { return m_url; }

   private:
    QString m_id;
    QString m_name;
    QString m_track;
    QUrl m_url;
};

class FosdemModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)
   public:
    explicit FosdemModel(QObject *parent = nullptr);
    ~FosdemModel() override;
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE void refresh();
    inline auto refreshing() const { return m_refreshing; }
    inline auto year() const { return m_year; }
    void setYear(int value);

   signals:
    void yearChanged();
    void error();
    void refreshingChanged();

   private:
    static const QString m_url;
    static const QString m_url_archive;
    static const QString m_filename;
    int m_year = 2022;
    QDomNodeList m_entries;
    bool m_refreshing = false;

    QList<ListItem *> makeItems() override;
    bool parseData();
    void downloadDir();
    QUrl makeUrl() const;
    void setRefreshing(bool value);
};

#endif  // FOSDEMMODEL_H
