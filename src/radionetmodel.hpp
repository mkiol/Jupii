/* Copyright (C) 2023-2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef RADIONETMODEL_H
#define RADIONETMODEL_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>

#include "itemmodel.h"
#include "listmodel.h"

class RadionetItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        IconRole = Qt::DecorationRole,
        IdRole = Qt::UserRole,
        CountryRole,
        CityRole,
        FormatRole,
        GenresRole,
        SelectedRole
    };

   public:
    RadionetItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit RadionetItem(QString id, QString name, QString country,
                          QString city, QString format, QStringList genres,
                          QUrl icon, QUrl url, QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline auto name() const { return m_name; }
    inline auto country() const { return m_country; }
    inline auto city() const { return m_city; }
    inline auto format() const { return m_format; }
    inline auto genres() const { return m_genres; }
    inline auto icon() const { return m_icon; }
    inline auto url() const { return m_url; }
    QUrl iconThumb() const;

   private:
    QString m_id;
    QString m_name;
    QString m_country;
    QString m_city;
    QString m_format;
    QStringList m_genres;
    QUrl m_icon;
    QUrl m_url;
};

class RadionetModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(bool canShowMore READ canShowMore NOTIFY canShowMoreChanged)
   public:
    explicit RadionetModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE inline void requestMoreItems() {
        m_lastIndex = getCount();
        m_showMoreRequested = true;
    }
    Q_INVOKABLE inline int lastIndex() const { return m_lastIndex; }

   signals:
    void canShowMoreChanged();

   private:
    bool m_showMoreRequested = false;
    int m_lastIndex = 0;
    QList<ListItem *> makeItems() override;
    bool canShowMore() const;
};

#endif  // TUNEINMODEL_H
