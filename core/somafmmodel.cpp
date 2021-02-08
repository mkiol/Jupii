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
#include <QNetworkRequest>
#include <memory>

#include "somafmmodel.h"
#include "utils.h"
#include "iconprovider.h"

const QString SomafmModel::m_dirUrl = "https://somafm.com/channels.xml";
const QString SomafmModel::m_dirFilename = "somafm2.xml";
const QString SomafmModel::m_imageFilename = "somafm_image_";

SomafmModel::SomafmModel(QObject *parent) :
    SelectableItemModel(new SomafmItem, parent)
{
    if (Utils::cacheFileExists(m_dirFilename))
        parseData();
    else
        refresh();
}

bool SomafmModel::parseData()
{
    QByteArray data;
    if (Utils::readFromCacheFile(m_dirFilename, data)) {
        qDebug() << "Parsing Somafm channels";
        QDomDocument doc; QString error;
        if (doc.setContent(data, false, &error)) {
            m_entries = doc.elementsByTagName("channel");
            return true;
        } else {
            qWarning() << "Parse error:" << error;
            return false;
        }
    }

    qWarning() << "Cannot read somafm data";
    return false;
}

bool SomafmModel::isRefreshing()
{
    return m_refreshing;
}

QString SomafmModel::bestImage(const QDomElement& entry)
{
    auto image = entry.elementsByTagName("ximage").at(0).toElement().text();
    if (image.isEmpty()) {
        image = entry.elementsByTagName("largeimage").at(0).toElement().text();
        if (image.isEmpty()) {
            image = entry.elementsByTagName("image").at(0).toElement().text();
            if (image.isEmpty()) {
                qWarning() << "Cannot find image for somafm channel";
            }
        }
    }

    return image;
}

void SomafmModel::downloadImages()
{
    m_imagesToDownload.clear();

    Utils::removeFromCacheDir(QStringList() << (m_imageFilename + "*"));

    int l = m_entries.length();
    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute("id")) {
            auto id = entry.attribute("id");
            auto image = bestImage(entry);
            if (!image.isNull())
                m_imagesToDownload << QPair<QString,QString>(id, image);
        }
    }

    downloadImage();
}

void SomafmModel::handleIconDownloadFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (reply && m_replyToId.contains(reply)) {
        auto id = m_replyToId.value(reply);
        auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        auto reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
        auto err = reply->error();
        //qDebug() << "Image download response code:" << code << reason << err;

        if (err != QNetworkReply::NoError) {
            qWarning() << "Error:" << err;
        } else if (code > 299 && code < 399) {
            qWarning() << "Redirection received but unsupported";
        } else if (code > 299) {
            qWarning() << "Unsupported response code:" << reply->error() << code << reason;
        } else {
            auto data = reply->readAll();
            if (data.isEmpty()) {
                qWarning() << "No data received";
            } else {
                auto filename = m_imageFilename + id;
                auto ext = reply->url().fileName().split('.').last();
                if (!ext.isEmpty())
                    filename = filename + "." + ext;
                qDebug() << "Downloaded image:" << filename;
                Utils::writeToCacheFile(filename, data, true);
            }
        }

        m_replyToId.remove(reply);
        reply->deleteLater();

        downloadImage();
    }
}

void SomafmModel::downloadImage()
{
    if (m_imagesToDownload.isEmpty()) {
        updateModel();
    } else {
        auto image = m_imagesToDownload.takeFirst();  // <id, image URL>
        auto id = image.first;

        QNetworkRequest request;
        request.setUrl(QUrl(image.second));
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        auto reply = Utils::instance()->nam->get(request);
        QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);

        m_replyToId.insert(reply, id);
        connect(reply, &QNetworkReply::finished, this,
                &SomafmModel::handleIconDownloadFinished, Qt::QueuedConnection);
    }
}

void SomafmModel::handleDataDownloadFinished()
{
    auto reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
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
                if (!parseData()) {
                    emit error();
                } else {
                    downloadImages();
                }
            }
        }
        reply->deleteLater();
        m_refreshing = false;
        emit refreshingChanged();
        setBusy(false);
        updateModel();
    }
}

void SomafmModel::refresh()
{
    if (!m_refreshing) {
        setBusy(true);
        m_refreshing = true;
        emit refreshingChanged();

        QNetworkRequest request;
        request.setUrl(QUrl(m_dirUrl));
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        auto reply = Utils::instance()->nam->get(request);
        QTimer::singleShot(httpTimeout, reply, &QNetworkReply::abort);
        connect(reply, &QNetworkReply::finished, this,
                &SomafmModel::handleDataDownloadFinished, Qt::QueuedConnection);
    } else {
        qWarning() << "Refreshing already active";
    }
}

QVariantList SomafmModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto channel = qobject_cast<SomafmItem*>(item);
        if (channel->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(channel->url()));
            map.insert("name", QVariant(channel->name()));
            map.insert("icon", QVariant(channel->icon()));
            map.insert("author", QVariant("SomaFM"));
            map.insert("app", "somafm");
            list << map;
        }
    }

    return list;
}

QList<ListItem*> SomafmModel::makeItems()
{
    QList<ListItem*> items;

    auto filter = getFilter();

    int l = filter.isEmpty() ? 1000 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute("id")) {
            auto id = entry.attribute("id");

            auto name = entry.elementsByTagName("title").at(0).toElement().text();
            auto desc = entry.elementsByTagName("description").at(0).toElement().text();
            auto genre = entry.elementsByTagName("genre").at(0).toElement().text();
            auto dj = entry.elementsByTagName("dj").at(0).toElement().text();

            // getting low bitrate MP3 stream URL
            QString url;
            auto urls = entry.elementsByTagName("fastpls");
            int ul = urls.length();
            for (int ui = 0; ui < ul; ++ui) {
                auto ue = urls.at(ui).toElement();
                if (ue.attribute("format") == "mp3") {
                    url = ue.text();
                }
            }

            if (!url.isEmpty() && !name.isEmpty()) {
                if (name.contains(filter, Qt::CaseInsensitive) ||
                    desc.contains(filter, Qt::CaseInsensitive) ||
                    genre.contains(filter, Qt::CaseInsensitive) ||
                    dj.contains(filter, Qt::CaseInsensitive)) {

                    QUrl icon;
                    auto filename = m_imageFilename + id;
                    auto imgpath = Utils::pathToCacheFile(filename + ".jpg");
                    if (!QFileInfo::exists(imgpath)) {
                        imgpath = Utils::pathToCacheFile(filename + ".png");
                        if (!QFileInfo::exists(imgpath))
                            icon = IconProvider::urlToNoResId("icon-somafm"); // default icon
                    }
                    if (icon.isEmpty())
                        icon = QUrl::fromLocalFile(imgpath);

                    items << new SomafmItem(
                                    id, // id
                                    name, // name
                                    desc, // desc
                                    QUrl(url), // url
                                    icon
                                );
                }
            }
        }

        if (items.length() > 999)
            break;
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<SomafmItem*>(a);
        auto bb = dynamic_cast<SomafmItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

SomafmItem::SomafmItem(const QString &id,
                   const QString &name,
                   const QString &description,
                   const QUrl &url,
                   const QUrl &icon,
                   QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_name(name),
    m_description(description),
    m_url(url),
    m_icon(icon)
{
}

QHash<int, QByteArray> SomafmItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[DescriptionRole] = "description";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[SelectedRole] = "selected";
    return names;
}

QVariant SomafmItem::data(int role) const
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
    case IconRole:
        return icon();
    case SelectedRole:
        return selected();
    default:
        return QVariant();
    }
}
