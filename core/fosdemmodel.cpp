/* Copyright (C) 2020 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QDate>
#include <QFile>
#include <QDir>
#include <QList>
#include <QTimer>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <memory>

#include "fosdemmodel.h"
#include "utils.h"
#include "iconprovider.h"

const QString FosdemModel::m_url = "https://fosdem.org/%1/schedule/xml";
const QString FosdemModel::m_url_archive = "https://archive.fosdem.org/%1/schedule/xml";
const QString FosdemModel::m_filename = "fosdem%1.xml";

FosdemModel::FosdemModel(QObject *parent) :
    SelectableItemModel(new FosdemItem, parent),
    m_year(0)
{
}

void FosdemModel::init()
{
    auto filename = m_filename.arg(m_year);
    if (Utils::cacheFileExists(filename))
        parseData();
    else
        refresh();
}

int FosdemModel::getYear()
{
    return m_year;
}

void FosdemModel::setYear(int value)
{
    if (value != m_year) {
        m_year = value;
        init();
        emit yearChanged();
    }
}

QUrl FosdemModel::makeUrl()
{
    return QDate::currentDate().year() == m_year ?
                QUrl(m_url.arg(m_year)) : QUrl(m_url_archive.arg(m_year));
}

bool FosdemModel::parseData()
{
    QByteArray data;
    if (Utils::readFromCacheFile(m_filename.arg(m_year), data)) {
        qDebug() << "Parsing Fosdem events";
        QDomDocument doc; QString error;
        if (doc.setContent(data, false, &error)) {
            m_entries = doc.elementsByTagName("event");
            return true;
        } else {
            qWarning() << "Parse error:" << error;
            return false;
        }
    }

    qWarning() << "Cannot read fosdem data";
    return false;
}

bool FosdemModel::isRefreshing()
{
    return m_refreshing;
}

void FosdemModel::refresh()
{
    if (!m_refreshing) {
        setBusy(true);
        m_refreshing = true;
        emit refreshingChanged();

        QNetworkRequest request;
        request.setUrl(makeUrl());
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        if (!nam)
            nam = std::unique_ptr<QNetworkAccessManager>(new QNetworkAccessManager(this));
        auto reply = nam->get(request);
        QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
        connect(reply, &QNetworkReply::finished, [this, reply]{
            auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
            auto err = reply->error();
            //qDebug() << "Response code:" << code << reason << err;

            if (err != QNetworkReply::NoError) {
                qWarning() << "Error:" << err;
                emit error();
            } else if (code > 299 && code < 399) {
                qWarning() << "Redirection received but unsupported";
                emit error();
            } else if (code > 299) {
                qWarning() << "Unsupported response code:" << reply->error() << code << reason;
                emit error();
            } else {
                auto data = reply->readAll();
                if (data.isEmpty()) {
                    qWarning() << "No data received";
                    emit error();
                } else {
                    Utils::writeToCacheFile(m_filename.arg(m_year), data, true);
                    if (!parseData()) {
                        emit error();
                    }
                }
            }
            reply->deleteLater();
            m_refreshing = false;
            emit refreshingChanged();
            setBusy(false);
            updateModel();
        });
    } else {
        qWarning() << "Refreshing already active";
    }
}

QVariantList FosdemModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto event = dynamic_cast<FosdemItem*>(item);
        if (event->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(event->url()));
            map.insert("name", QVariant(event->name()));
            map.insert("icon", QVariant(
                           QUrl::fromLocalFile(IconProvider::pathToNoResId("icon-fosdem"))));
            map.insert("author", QVariant(QString("Fosdem %1").arg(m_year)));
            list << map;
        }
    }

    return list;
}

QList<ListItem*> FosdemModel::makeItems()
{
    QList<ListItem*> items;

    auto filter = getFilter();

    int l = filter.isEmpty() ? 1000 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute("id")) {
            auto id = entry.attribute("id");

            auto name = entry.elementsByTagName("title").at(0).toElement().text();
            auto track = entry.elementsByTagName("track").at(0).toElement().text();

            // getting video URL, mp4 is preferred
            QString url;
            auto urls = entry.elementsByTagName("link");
            int ul = urls.length();
            for (int ui = 0; ui < ul; ++ui) {
                auto href = urls.at(ui).toElement().attribute("href");
                if (href.endsWith(".mp4", Qt::CaseInsensitive)) {
                    url = href;
                    break;
                }
            }
            if (url.isEmpty()) {
                // fallback to webm
                for (int ui = 0; ui < ul; ++ui) {
                    auto href = urls.at(ui).toElement().attribute("href");
                    if (href.endsWith(".webm", Qt::CaseInsensitive)) {
                        url = href;
                        break;
                    }
                }
            }

            // getting persons
            QString person;
            auto persons = entry.elementsByTagName("person");
            int pl = persons.length();
            for (int pi = 0; pi < pl; ++pi) {
                auto p = persons.at(pi).toElement().text();
                if (!p.isEmpty()) {
                    person += p;
                    if (pi != pl - 1) {
                        person += " ";
                    }
                }
            }

            if (!url.isEmpty() && !name.isEmpty()) {
                if (name.contains(filter, Qt::CaseInsensitive) ||
                    track.contains(filter, Qt::CaseInsensitive) ||
                    person.contains(filter, Qt::CaseInsensitive)) {
                    items << new FosdemItem(
                                    id, // id
                                    name, // name
                                    track, // track
                                    QUrl(url) // url
                                );
                }
            }
        }

        if (items.length() > 999)
            break;
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<FosdemItem*>(a);
        auto bb = dynamic_cast<FosdemItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

void FosdemModel::refreshItem(const QString &id)
{
    auto item = dynamic_cast<FosdemItem*>(find(id));
    if (item)
        item->refresh();
}

FosdemItem::FosdemItem(const QString &id,
                   const QString &name,
                   const QString &track,
                   const QUrl &url,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_track(track),
    m_url(url)
{
}

QHash<int, QByteArray> FosdemItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[TrackRole] = "track";
    names[UrlRole] = "url";
    names[SelectedRole] = "selected";
    return names;
}

QVariant FosdemItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case TrackRole:
        return track();
    case UrlRole:
        return url();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}

void FosdemItem::refresh()
{
}
