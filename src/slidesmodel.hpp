/* Copyright (C) 2025 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SLIDESMODEL_H
#define SLIDESMODEL_H

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QList>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "itemmodel.h"

class SlidesItem : public SelectableItem {
    Q_OBJECT
   public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        PathRole,
        SelectedRole,
        DateRole,
        FriendlyDateRole,
        IconRole,
        SizeRole
    };

    SlidesItem(QObject *parent = nullptr) : SelectableItem(parent) {}
    explicit SlidesItem(const QString &id, const QUrl &slidesUrl,
                        const QString &path, const QString &title,
                        const QDateTime &date, unsigned int size,
                        QObject *parent = nullptr);
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString id() const override { return m_id; }
    auto slidesUrl() const { return m_slidesUrl; }
    auto path() const { return m_path; }
    auto title() const { return m_title; }
    auto date() const { return m_date; }
    QString friendlyDate() const;
    auto size() const { return m_size; }
    QUrl iconThumb() const;

   private:
    QString m_id;
    QUrl m_slidesUrl;
    QString m_path;
    QString m_title;
    QDateTime m_date;
    unsigned int m_size;
};

class SlidesModel : public SelectableItemModel {
    Q_OBJECT
    Q_PROPERTY(int queryType READ getQueryType WRITE setQueryType NOTIFY
                   queryTypeChanged)
   public:
    explicit SlidesModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void deleteItem(const QString &id);
    Q_INVOKABLE void createItem(const QString &id, QString name,
                                const QList<QUrl> &urls);
    Q_INVOKABLE QVariantMap parseItem(const QString &id) const;
    auto getQueryType() const { return m_queryType; };
    void setQueryType(int value);

   signals:
    void queryTypeChanged();

   private:
    struct Item {
        QString path;
        QString title;
        QString iconPath;
        QUrl slidesUrl;
        QDateTime date;
        unsigned int size;
    };
    QList<Item> m_items;
    QDir m_dir;
    QList<ListItem *> makeItems() override;
    int m_queryType = 0;
};

#endif  // SLIDESMODEL_H
