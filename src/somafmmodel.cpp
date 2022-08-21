/* Copyright (C) 2018-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "somafmmodel.h"

#include <QDateTime>
#include <QDebug>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <QFileInfo>
#include <QList>
#include <QThread>
#include <memory>

#include "downloader.h"
#include "iconprovider.h"
#include "utils.h"

const QUrl SomafmModel::m_dirUrl{
    QStringLiteral("https://somafm.com/channels.xml")};
const QString SomafmModel::m_dirFilename{QStringLiteral("somafm2.xml")};
const QString SomafmModel::m_imageFilename{QStringLiteral("somafm_image_")};

SomafmModel::SomafmModel(QObject *parent)
    : SelectableItemModel(new SomafmItem, parent) {
    if (QFileInfo{Utils::pathToCacheFile(m_dirFilename)}.created().daysTo(
            QDateTime::currentDateTime()) > 30) {
        qDebug() << "cache expired";
        Utils::removeFromCacheDir({m_dirFilename});
    }
}

SomafmModel::~SomafmModel() { m_worker.reset(); }

bool SomafmModel::parseData(const QByteArray &data) {
    qDebug() << "parsing somafm channels";

    QDomDocument doc{};
    if (QString error; !doc.setContent(data, false, &error)) {
        qWarning() << "parse error:" << error;
        return false;
    }

    m_entries = doc.elementsByTagName(QStringLiteral("channel"));
    return true;
}

bool SomafmModel::parseData() {
    QByteArray data;

    if (!Utils::readFromCacheFile(m_dirFilename, data)) {
        qWarning() << "cannot read somafm data";
        return false;
    }

    return parseData(data);
}

QString SomafmModel::bestImage(const QDomElement &entry) {
    auto image = entry.elementsByTagName(QStringLiteral("ximage"))
                     .at(0)
                     .toElement()
                     .text();
    if (image.isEmpty()) {
        image = entry.elementsByTagName(QStringLiteral("largeimage"))
                    .at(0)
                    .toElement()
                    .text();
        if (image.isEmpty()) {
            image = entry.elementsByTagName(QStringLiteral("image"))
                        .at(0)
                        .toElement()
                        .text();
            if (image.isEmpty()) {
                qWarning() << "cannot find image for somafm channel";
            }
        }
    }

    return image;
}

bool SomafmModel::downloadImages(std::shared_ptr<QNetworkAccessManager> nam) {
    Utils::removeFromCacheDir({m_imageFilename + "*"});

    for (int i = 0; i < m_entries.length(); ++i) {
        if (QThread::currentThread()->isInterruptionRequested()) return false;

        auto entry = m_entries.at(i).toElement();

        if (!entry.isNull() && entry.hasAttribute(QStringLiteral("id"))) {
            auto id = entry.attribute(QStringLiteral("id"));
            auto url = QUrl{bestImage(entry)};
            if (!id.isEmpty() && !url.isEmpty()) {
                auto data = Downloader{nam}.downloadData(url);
                if (!data.bytes.isEmpty()) {
                    auto filename = m_imageFilename + id;
                    if (auto ext = url.fileName().split('.').last();
                        !ext.isEmpty())
                        filename = filename + "." + ext;
                    Utils::writeToCacheFile(filename, data.bytes, true);
                }
            }
        }

        emit progressChanged(i + 1, m_entries.length());
    }

    return true;
}

void SomafmModel::downloadDir() {
    if (!Utils::cacheFileExists(m_dirFilename)) {
        auto nam = std::make_shared<QNetworkAccessManager>();
        auto data = Downloader{nam}.downloadData(m_dirUrl);

        if (QThread::currentThread()->isInterruptionRequested()) return;

        if (data.bytes.isEmpty()) {
            qWarning() << "No data received";
            emit error();
            return;
        }

        if (!parseData(data.bytes)) {
            emit error();
            Utils::removeFromCacheDir({m_dirFilename});
            return;
        }

        if (downloadImages(std::move(nam))) {
            Utils::writeToCacheFile(m_dirFilename, data.bytes, true);
        }
    } else if (!parseData()) {
        emit error();
        Utils::removeFromCacheDir({m_dirFilename});
        return;
    }
}

void SomafmModel::refresh() {
    Utils::removeFromCacheDir({m_dirFilename});
    m_entries = {};
    updateModel();
}

QVariantList SomafmModel::selectedItems() {
    QVariantList list;

    std::transform(m_list.cbegin(), m_list.cend(), std::back_inserter(list),
                   [](const auto *item) {
                       auto channel = qobject_cast<const SomafmItem *>(item);
                       return QVariantMap{{"url", channel->url()},
                                          {"name", channel->name()},
                                          {"icon", channel->icon()},
                                          {"author", "SomaFM"},
                                          {"app", "somafm"}};
                   });

    return list;
}

QList<ListItem *> SomafmModel::makeItems() {
    if (m_entries.isEmpty()) downloadDir();

    QList<ListItem *> items;

    const auto &filter = getFilter();

    int l = filter.isEmpty() ? 1000 : m_entries.length();

    items.reserve(l);

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute(QStringLiteral("id"))) {
            auto name = entry.elementsByTagName(QStringLiteral("title"))
                            .at(0)
                            .toElement()
                            .text();

            auto url = [&] {  // getting low bitrate MP3 stream URL
                auto urls = entry.elementsByTagName(QStringLiteral("fastpls"));
                int ul = urls.length();
                for (int ui = 0; ui < ul; ++ui) {
                    auto ue = urls.at(ui).toElement();
                    if (ue.attribute(QStringLiteral("format")) ==
                        QStringLiteral("mp3")) {
                        return ue.text();
                    }
                }
                return QString{};
            }();

            if (!url.isEmpty() && !name.isEmpty()) {
                auto desc =
                    entry.elementsByTagName(QStringLiteral("description"))
                        .at(0)
                        .toElement()
                        .text();
                auto genre = entry.elementsByTagName(QStringLiteral("genre"))
                                 .at(0)
                                 .toElement()
                                 .text();
                auto dj = entry.elementsByTagName(QStringLiteral("dj"))
                              .at(0)
                              .toElement()
                              .text();

                if (name.contains(filter, Qt::CaseInsensitive) ||
                    desc.contains(filter, Qt::CaseInsensitive) ||
                    genre.contains(filter, Qt::CaseInsensitive) ||
                    dj.contains(filter, Qt::CaseInsensitive)) {
                    auto id = entry.attribute(QStringLiteral("id"));
                    auto icon = [&] {
                        auto filename = m_imageFilename + id;
                        auto imgpath =
                            Utils::pathToCacheFile(filename + ".jpg");
                        if (!QFileInfo::exists(imgpath)) {
                            imgpath = Utils::pathToCacheFile(filename + ".png");
                            if (!QFileInfo::exists(imgpath))
                                return IconProvider::urlToNoResId(
                                    QStringLiteral("icon-somafm"));
                        }
                        return QUrl::fromLocalFile(imgpath);
                    }();

                    items.push_back(new SomafmItem{
                        std::move(id), std::move(name), std::move(desc),
                        QUrl{url}, std::move(icon)});
                }
            }
        }
    }

    std::sort(items.begin(), items.end(), [](const auto *a, const auto *b) {
        auto *aa = qobject_cast<const SomafmItem *>(a);
        auto *bb = qobject_cast<const SomafmItem *>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

SomafmItem::SomafmItem(QString id, QString name, QString description, QUrl url,
                       QUrl icon, QObject *parent)
    : SelectableItem(parent), m_id{std::move(id)}, m_name{std::move(name)},
      m_description{std::move(description)}, m_url{std::move(url)},
      m_icon{std::move(icon)} {}

QHash<int, QByteArray> SomafmItem::roleNames() const {
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[DescriptionRole] = "description";
    names[UrlRole] = "url";
    names[IconRole] = "icon";
    names[SelectedRole] = "selected";
    return names;
}

QVariant SomafmItem::data(int role) const {
    switch (role) {
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
