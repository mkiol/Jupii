/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FOSDEMMODEL_H
#define FOSDEMMODEL_H

#include <QString>
#include <QHash>
#include <QDebug>
#include <QByteArray>
#include <QVariant>
#include <QUrl>
#include <QPair>
#include <QDir>
#include <QNetworkAccessManager>
#include <QDomNodeList>
#include <QDomElement>
#include <memory>

#include "listmodel.h"
#include "itemmodel.h"

class FosdemItem : public SelectableItem
{
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
    FosdemItem(QObject *parent = nullptr): SelectableItem(parent) {}
    explicit FosdemItem(const QString &id,
                      const QString &name,
                      const QString &track,
                      const QUrl &url,
                      QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString name() const { return m_name; }
    inline QString track() const { return m_track; }
    inline QUrl url() const { return m_url; }
    void refresh();
private:
    QString m_id;
    QString m_name;
    QString m_track;
    QUrl m_url;
};

class FosdemModel : public SelectableItemModel
{
    Q_OBJECT
    Q_PROPERTY (int year READ getYear WRITE setYear NOTIFY yearChanged)
    Q_PROPERTY (bool refreshing READ isRefreshing NOTIFY refreshingChanged)
public:
    explicit FosdemModel(QObject *parent = nullptr);
    bool isRefreshing();
    Q_INVOKABLE QVariantList selectedItems();

    int getYear();
    void setYear(int value);

public slots:
    void refresh();

signals:
    void yearChanged();
    void refreshingChanged();
    void error();

private:
    static const QString m_url;
    static const QString m_url_archive;
    static const QString m_filename;
    static const int httpTimeout = 100000;

    int m_year = 2020;
    std::unique_ptr<QNetworkAccessManager> nam;
    QDomNodeList m_entries;
    bool m_refreshing = false;

    QList<ListItem*> makeItems();
    bool parseData();
    void refreshItem(const QString &id);
    QUrl makeUrl();
    void init();
};

#endif // FOSDEMMODEL_H
