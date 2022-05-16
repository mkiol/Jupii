/* Copyright (C) 2020-2021 Michal Kosciesza <michal@mkiol.net>
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
#include <QDomNodeList>
#include <QDomElement>

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
    QVariant data(int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    inline QString id() const override { return m_id; }
    inline QString name() const { return m_name; }
    inline QString track() const { return m_track; }
    inline QUrl url() const { return m_url; }
private:
    QString m_id;
    QString m_name;
    QString m_track;
    QUrl m_url;
};

class FosdemModel : public SelectableItemModel
{
    Q_OBJECT
    Q_PROPERTY (int year READ year WRITE setYear NOTIFY yearChanged)
public:
    explicit FosdemModel(QObject *parent = nullptr);
    ~FosdemModel();
    Q_INVOKABLE QVariantList selectedItems() override;
    int year() const;
    void setYear(int value);

public slots:
    void refresh();

signals:
    void yearChanged();
    void error();

private:
    static const QString m_url;
    static const QString m_url_archive;
    static const QString m_filename;
    int m_year = 2022;
    QDomNodeList m_entries;
    bool m_refreshing = false;

    QList<ListItem*> makeItems() override;
    bool parseData();
    void downloadDir();
    QUrl makeUrl() const;
};

#endif // FOSDEMMODEL_H
