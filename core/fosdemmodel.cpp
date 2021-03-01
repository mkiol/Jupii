/* Copyright (C) 2020-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "fosdemmodel.h"

#include <QDebug>
#include <QDate>
#include <QList>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomText>
#include <memory>

#include "utils.h"
#include "iconprovider.h"
#include "downloader.h"

const QString FosdemModel::m_url{"https://fosdem.org/%1/schedule/xml"};
const QString FosdemModel::m_url_archive{"https://archive.fosdem.org/%1/schedule/xml"};
const QString FosdemModel::m_filename{"fosdem%1.xml"};

FosdemModel::FosdemModel(QObject *parent) :
    SelectableItemModel(new FosdemItem, parent),
    m_year(0)
{
}

FosdemModel::~FosdemModel()
{
    m_worker.reset();
}

int FosdemModel::year() const
{
    return m_year;
}

void FosdemModel::setYear(int value)
{
    if (value != m_year) {
        m_year = value;
        emit yearChanged();
        updateModel();
    }
}

QUrl FosdemModel::makeUrl() const
{
    if (QDate::currentDate().year() == m_year)
        return QUrl{m_url.arg(m_year)};
    return QUrl{m_url_archive.arg(m_year)};
}

bool FosdemModel::parseData()
{
    if (QByteArray data; Utils::readFromCacheFile(m_filename.arg(m_year), data)) {
        QString error;
        if (QDomDocument doc; doc.setContent(data, false, &error)) {
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

void FosdemModel::downloadDir()
{
    auto dirFilename = m_filename.arg(m_year);

    if (!Utils::cacheFileExists(dirFilename)) {
        auto nam = std::make_shared<QNetworkAccessManager>();
        auto data = Downloader{nam}.downloadData(makeUrl());

        if (QThread::currentThread()->isInterruptionRequested())
            return;

        if (data.isEmpty()) {
            qWarning() << "No data received";
            emit error();
            return;
        }

        Utils::writeToCacheFile(dirFilename, data, true);
    }

    if (!parseData()) {
        emit error();
        Utils::removeFromCacheDir({dirFilename});
    }
}

void FosdemModel::refresh()
{
    Utils::removeFromCacheDir({m_filename.arg(m_year)});
    m_entries = {};
    updateModel();
}

QVariantList FosdemModel::selectedItems()
{
    QVariantList list;

    foreach (const auto item, m_list) {
        const auto event = qobject_cast<FosdemItem*>(item);
        if (event->selected()) {
            list << QVariantMap{
            {"url", event->url()},
            {"name", event->name()},
            {"icon", IconProvider::urlToNoResId("icon-fosdem")},
            {"author", QString("FOSDEM %1").arg(m_year)},
            {"app", "fosdem"}};
        }
    }

    return list;
}

QList<ListItem*> FosdemModel::makeItems()
{
    if (m_entries.isEmpty())
        downloadDir();

    QList<ListItem*> items;

    const auto& filter = getFilter();

    const int l = filter.isEmpty() ? 1000 : m_entries.length();

    for (int i = 0; i < l; ++i) {
        auto entry = m_entries.at(i).toElement();
        if (!entry.isNull() && entry.hasAttribute("id")) {
            const auto id = entry.attribute("id");

            const auto name = entry.elementsByTagName("title").at(0).toElement().text().trimmed();
            const auto track = entry.elementsByTagName("track").at(0).toElement().text().trimmed();

            // getting video URL, mp4 is preferred
            QString url;
            auto urls = entry.elementsByTagName("link");
            int ul = urls.length();
            for (int ui = 0; ui < ul; ++ui) {
                const auto href = urls.at(ui).toElement().attribute("href");
                if (href.contains("fosdem.org", Qt::CaseInsensitive) && href.endsWith(".mp4", Qt::CaseInsensitive)) {
                    url = href;
                    break;
                }
            }
            if (url.isEmpty()) {
                // fallback to webm
                for (int ui = 0; ui < ul; ++ui) {
                    const auto href = urls.at(ui).toElement().attribute("href");
                    if (href.contains("fosdem.org", Qt::CaseInsensitive) && href.endsWith(".webm", Qt::CaseInsensitive)) {
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
                const auto p = persons.at(pi).toElement().text().trimmed();
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
                    items << new FosdemItem{
                                    id, // id
                                    name, // name
                                    track, // track
                                    QUrl(url) // url
                                 };
                }
            }
        }

        if (items.length() > 999)
            break;
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        const auto aa = qobject_cast<FosdemItem*>(a);
        const auto bb = qobject_cast<FosdemItem*>(b);
        return aa->name().compare(bb->name(), Qt::CaseInsensitive) < 0;
    });

    return items;
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
        return {};
    }
}
