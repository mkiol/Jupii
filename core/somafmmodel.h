/* Copyright (C) 2018-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SOMAFMMODEL_H
#define SOMAFMMODEL_H

#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QUrl>
#include <QDomNodeList>
#include <QDomElement>
#include <QNetworkAccessManager>
#include <memory>

#include "listmodel.h"
#include "itemmodel.h"

class SomafmItem : public SelectableItem
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
    SomafmItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit SomafmItem(const QString &id,
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

class SomafmModel : public SelectableItemModel
{
    Q_OBJECT
public:
    explicit SomafmModel(QObject *parent = nullptr);
    ~SomafmModel();
    Q_INVOKABLE QVariantList selectedItems();

public slots:
    void refresh();

signals:
    void error();

private:
    static const QUrl m_dirUrl;
    static const QString m_dirFilename;
    static const QString m_imageFilename;
    QDomNodeList m_entries;

    QList<ListItem*> makeItems();
    bool parseData();
    void downloadImages(std::shared_ptr<QNetworkAccessManager> nam);
    void downloadDir();
    static QString bestImage(const QDomElement& entry);
};

#endif // SOMAFMMODEL_H
