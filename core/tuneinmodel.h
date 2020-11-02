/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TUNEINMODEL_H
#define TUNEINMODEL_H

#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QUrl>
#include <QDomDocument>
#include <memory>

#include "listmodel.h"
#include "itemmodel.h"

class TuneinItem : public SelectableItem
{
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
    TuneinItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit TuneinItem(const QString &id,
                      const QString &name,
                      const QString &description,
                      const QUrl &url,
                      const QUrl &icon,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString description() const { return m_description; }
    inline QUrl url() const { return m_url; }
    inline QUrl icon() const { return m_icon; }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
    QUrl m_icon;
};

class TuneinModel : public SelectableItemModel
{
    Q_OBJECT
public:
    explicit TuneinModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems();

signals:
    void error();

private:
    static const int httpTimeout = 10000;

    QList<ListItem*> makeItems();
    static QUrl makeSearchUrl(const QString &phrase);
    static std::unique_ptr<QDomDocument> parseXmlData(const QByteArray &data);
    QByteArray downloadData(const QUrl &url);
};

#endif // TUNEINMODEL_H
