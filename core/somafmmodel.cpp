/* Copyright (C) 2018-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QFileInfo>
#include <QList>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QThread>
#include <memory>

#include "somafmmodel.h"
#include "utils.h"
#include "iconprovider.h"
#include "downloader.h"

const QUrl SomafmModel::m_dirUrl{"https://somafm.com/channels.xml"};
const QString SomafmModel::m_dirFilename{"somafm2.xml"};
const QString SomafmModel::m_imageFilename{"somafm_image_"};

SomafmModel::SomafmModel(QObject *parent) :
    SelectableItemModel(new SomafmItem, parent)
{
}

SomafmModel::~SomafmModel()
{
    m_worker.reset();
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

void SomafmModel::downloadImages(std::shared_ptr<QNetworkAccessManager> nam)
{
    Utils::removeFromCacheDir({m_imageFilename + "*"});

    for (int i = 0; i < m_entries.length(); ++i) {
        if (QThread::currentThread()->isInterruptionRequested())
            break;

        const auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute("id")) {
            const auto id = entry.attribute("id");
            const auto url = QUrl{bestImage(entry)};
            if (!id.isEmpty() && !url.isEmpty()) {
                auto data = Downloader{nam}.downloadData(url);
                if (!data.isEmpty()) {
                    auto filename = m_imageFilename + id;
                    if (auto ext = url.fileName().split('.').last(); !ext.isEmpty())
                        filename = filename + "." + ext;
                    qDebug() << "Downloaded image:" << filename;
                    Utils::writeToCacheFile(filename, data, true);
                }
            }
        }
    }
}

void SomafmModel::downloadDir()
{
    if (!Utils::cacheFileExists(m_dirFilename)) {
        auto nam = std::make_shared<QNetworkAccessManager>();
        auto data = Downloader{nam}.downloadData(m_dirUrl);

        if (QThread::currentThread()->isInterruptionRequested())
            return;

        if (data.isEmpty()) {
            qWarning() << "No data received";
            emit error();
            return;
        }

        Utils::writeToCacheFile(m_dirFilename, data, true);

        if (!parseData()) {
            emit error();
            Utils::removeFromCacheDir({m_dirFilename});
            return;
        }

        downloadImages(nam);
    } else if (!parseData()) {
        emit error();
        Utils::removeFromCacheDir({m_dirFilename});
        return;
    }
}

void SomafmModel::refresh()
{
    Utils::removeFromCacheDir({m_dirFilename});
    m_entries = {};
    updateModel();
}

QVariantList SomafmModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto channel = qobject_cast<SomafmItem*>(item);
        if (channel->selected()) {
            list << QVariantMap{
                {"url", channel->url()},
                {"name", channel->name()},
                {"icon", channel->icon()},
                {"author", "SomaFM"},
                {"app", "somafm"}};
        }
    }

    return list;
}

QList<ListItem*> SomafmModel::makeItems()
{
    if (m_entries.isEmpty())
        downloadDir();

    QList<ListItem*> items;

    const auto& filter = getFilter();

    const int l = filter.isEmpty() ? 1000 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute("id")) {
            const auto name = entry.elementsByTagName("title").at(0).toElement().text();

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
                const auto desc = entry.elementsByTagName("description").at(0).toElement().text();
                const auto genre = entry.elementsByTagName("genre").at(0).toElement().text();
                const auto dj = entry.elementsByTagName("dj").at(0).toElement().text();

                if (name.contains(filter, Qt::CaseInsensitive) ||
                    desc.contains(filter, Qt::CaseInsensitive) ||
                    genre.contains(filter, Qt::CaseInsensitive) ||
                    dj.contains(filter, Qt::CaseInsensitive)) {

                    QUrl icon;
                    const auto id = entry.attribute("id");
                    const auto filename = m_imageFilename + id;
                    auto imgpath = Utils::pathToCacheFile(filename + ".jpg");
                    if (!QFileInfo::exists(imgpath)) {
                        imgpath = Utils::pathToCacheFile(filename + ".png");
                        if (!QFileInfo::exists(imgpath))
                            icon = IconProvider::urlToNoResId("icon-somafm"); // default icon
                    }
                    if (icon.isEmpty())
                        icon = QUrl::fromLocalFile(imgpath);

                    items << new SomafmItem{
                                    id, // id
                                    name, // name
                                    desc, // desc
                                    QUrl{url}, // url
                                    icon};
                }
            }
        }

        if (items.length() > 999)
            break;
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        const auto aa = qobject_cast<SomafmItem*>(a);
        const auto bb = qobject_cast<SomafmItem*>(b);
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
        return {};
    }
}
