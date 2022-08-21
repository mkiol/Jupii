/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
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
#include <memory>

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
    inline QString id() const override { return m_id; }
    inline auto name() const { return m_name; }
    inline auto description() const { return m_description; }
    inline auto url() const { return m_url; }
    inline auto icon() const { return m_icon; }

   private:
    QString m_id;
    QString m_name;
    QString m_description;
    QUrl m_url;
    QUrl m_icon;
};

class SomafmModel : public SelectableItemModel {
    Q_OBJECT
   public:
    explicit SomafmModel(QObject *parent = nullptr);
    ~SomafmModel() override;
    Q_INVOKABLE QVariantList selectedItems() override;

   public slots:
    void refresh();

   signals:
    void error();
    void progressChanged(int n, int total);

   private:
    static const QUrl m_dirUrl;
    static const QString m_dirFilename;
    static const QString m_imageFilename;
    QDomNodeList m_entries;

    QList<ListItem *> makeItems() override;
    bool parseData();
    bool parseData(const QByteArray &data);
    bool downloadImages(std::shared_ptr<QNetworkAccessManager> nam);
    void downloadDir();
    static QString bestImage(const QDomElement &entry);
};

#endif  // SOMAFMMODEL_H
