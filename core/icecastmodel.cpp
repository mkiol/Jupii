/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
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

#include "icecastmodel.h"
#include "utils.h"
#include "iconprovider.h"

const QString IcecastModel::m_dirUrl = "http://dir.xiph.org/yp.xml";
const QString IcecastModel::m_dirFilename = "icecast.xml";

IcecastModel::IcecastModel(QObject *parent) :
    SelectableItemModel(new IcecastItem, parent)
{
    if (Utils::cacheFileExists(m_dirFilename))
        parseData();
    else
        refresh();
}

bool IcecastModel::parseData()
{
    QByteArray data;
    if (Utils::readFromCacheFile(m_dirFilename, data)) {
        qDebug() << "Parsing Icecast directory";
        QDomDocument doc; QString error;
        if (doc.setContent(data, false, &error)) {
            m_entries = doc.elementsByTagName("entry");
            return true;
        } else {
            qWarning() << "Parse error:" << error;
            return false;
        }
    }

    qWarning() << "Cannot read icecast data";
    return false;
}

bool IcecastModel::isRefreshing()
{
    return m_refreshing;
}

void IcecastModel::refresh()
{
    if (!m_refreshing) {
        setBusy(true);
        m_refreshing = true;
        emit refreshingChanged();

        QNetworkRequest request;
        request.setUrl(m_dirUrl);
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
                    Utils::writeToCacheFile(m_dirFilename, data, true);
                }
            }

            reply->deleteLater();

            m_refreshing = false;
            emit refreshingChanged();
            setBusy(false);

            if (parseData())
                updateModel();
            else
                emit error();
        });
    } else {
        qWarning() << "Refreshing already active";
    }
}

QVariantList IcecastModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto station = dynamic_cast<IcecastItem*>(item);
        if (station->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(station->url()));
            map.insert("name", QVariant(station->name()));
            map.insert("icon", QVariant(
                           QUrl::fromLocalFile(IconProvider::pathToNoResId("icon-icecast"))));
            map.insert("author", QVariant("Icecast"));
            list << map;
        }
    }

    return list;
}

QList<ListItem*> IcecastModel::makeItems()
{
    QList<ListItem*> items;

    auto filter = getFilter();

    int l = filter.isEmpty() ? 500 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull()) {
            auto name = entry.elementsByTagName("server_name").at(0).toElement().text();
            auto genre = entry.elementsByTagName("genre").at(0).toElement().text();
            auto url = entry.elementsByTagName("listen_url").at(0).toElement().text();

            if (!url.isEmpty() && !name.isEmpty()) {
                if (name.contains(filter, Qt::CaseInsensitive) ||
                    genre.contains(filter, Qt::CaseInsensitive) ||
                    QUrl(url).path().contains(filter, Qt::CaseInsensitive)) {

                    auto mime = entry.elementsByTagName("server_type").at(0).toElement().text();
                    auto bitrate = entry.elementsByTagName("bitrate").at(0).toElement().text().toInt();
                    QString type = mime.contains("audio/mpeg", Qt::CaseInsensitive) ? "MP3" :
                                mime.contains("audio/aac", Qt::CaseInsensitive) ? "AAC" :
                                mime.contains("audio/aacp", Qt::CaseInsensitive) ? "AAC+" :
                                mime.contains("application/ogg", Qt::CaseInsensitive) ? "OGG" :
                                mime.contains("video/webm", Qt::CaseInsensitive) ? "WEBM" :
                                mime.contains("video/nsv", Qt::CaseInsensitive) ? "NSV" : "";

                    auto desc = (genre.isEmpty() ?
                                (type.isEmpty() ? "" : type + " ") : genre + " · " +
                                (type.isEmpty() ? "" : type + " ")) +
                                (bitrate > 0 ? QString::number(bitrate) + "kbps" : "");

                    items << new IcecastItem(
                                    url, // id
                                    name, // name
                                    desc, // desc
                                    QUrl(url), // url
                                    ContentServer::typeFromMime(mime) // type
                                );
                }
            }
        }

        if (items.length() > 49)
            break;
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<IcecastItem*>(a);
        auto bb = dynamic_cast<IcecastItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

IcecastItem::IcecastItem(const QString &id,
                   const QString &name,
                   const QString &description,
                   const QUrl &url,
                   ContentServer::Type type,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_description(description),
    m_url(url),
    m_type(type)
{
}

QHash<int, QByteArray> IcecastItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[DescriptionRole] = "description";
    names[UrlRole] = "url";
    names[TypeRole] = "type";
    names[SelectedRole] = "selected";
    return names;
}

QVariant IcecastItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case DescriptionRole:
        return description();
    case UrlRole:
        return url();
    case TypeRole:
        return type();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}
