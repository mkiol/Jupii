/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QFileInfo>
#include <QFile>
#include <QUrl>
#include <QStringList>

// TagLib
#include "fileref.h"
#include "tag.h"

#include "recmodel.h"
#include "settings.h"
#include "contentserver.h"

RecModel::RecModel(QObject *parent) :
    SelectableItemModel(new RecItem, parent),
    m_dir(Settings::instance()->getRecDir())
{
    auto s = Settings::instance();
    m_queryType = s->getRecQueryType();
}

QVariantList RecModel::selectedItems()
{
    QVariantList list;

    for (auto item : m_list) {
        auto rec = dynamic_cast<RecItem*>(item);
        if (rec->selected()) {
            QVariantMap map;
            map.insert("url", QVariant(QUrl::fromLocalFile(rec->path())));
            map.insert("name", QVariant(rec->title()));
            map.insert("author", QVariant(rec->author()));
            list << map;
        }
    }

    return list;
}

void RecModel::deleteSelected()
{
    bool removed = false;

    for (auto item : m_list) {
        auto rec = dynamic_cast<RecItem*>(item);
        if (rec->selected()) {
            if (QFile::remove(rec->path()))
                removed = true;
        }
    }

    if (removed)
        updateModel();
}

QList<ListItem*> RecModel::makeItems()
{
    QList<ListItem*> items;

    auto filter = getFilter();

    auto files = m_dir.entryInfoList(QStringList() << "*.jupii_rec.*", QDir::Files);
    for (const auto& file : files) {
        auto path = file.absoluteFilePath();

        QString title, author, album, comment, recStation, recUrl;
        QDateTime recDate;

        if (!ContentServer::readMetaUsingTaglib(path, title, author, album,
                               comment, recStation, recUrl, recDate)) {
            qWarning() << "Cannot read meta data from file";
        }

        // fallbacks
        if (title.isEmpty())
            title = file.baseName();
        if (author.isEmpty())
            author = tr("Unknown");
        if (album.isEmpty())
            album = tr("Unknown");
        if (recStation.isEmpty())
            recStation = author;
        if (recDate.isNull())
            recDate = file.created();

        if (title.contains(filter, Qt::CaseInsensitive) ||
            author.contains(filter, Qt::CaseInsensitive) ||
            recStation.contains(filter, Qt::CaseInsensitive)) {
            items << new RecItem(
                         path,
                         path,
                         title,
                         author,
                         recDate);
        }
    }

    // Sorting
    if (m_queryType == 0) { // by date
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = dynamic_cast<RecItem*>(a);
            auto bb = dynamic_cast<RecItem*>(b);
            return aa->date() > bb->date();
        });
    } else if (m_queryType == 1) { // by title
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = dynamic_cast<RecItem*>(a);
            auto bb = dynamic_cast<RecItem*>(b);
            return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
        });
    } else { // by author (station name)
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = dynamic_cast<RecItem*>(a);
            auto bb = dynamic_cast<RecItem*>(b);
            return aa->author().compare(bb->author(), Qt::CaseInsensitive) < 0;
        });
    }

    return items;
}

int RecModel::getQueryType()
{
    return m_queryType;
}

void RecModel::setQueryType(int value)
{
    if (value != m_queryType) {
        m_queryType = value;
        emit queryTypeChanged();
        auto s = Settings::instance();
        s->setRecQueryType(m_queryType);
        updateModel();
    }
}

RecItem::RecItem(const QString &id,
                 const QString &path,
                 const QString &title,
                 const QString &author,
                 const QDateTime &date,
                 QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_path(path),
    m_title(title),
    m_author(author),
    m_date(date)
{
}

QHash<int, QByteArray> RecItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[PathRole] = "path";
    names[TitleRole] = "title";
    names[AuthorRole] = "author";
    names[SelectedRole] = "selected";
    names[DateRole] = "date";
    return names;
}

QVariant RecItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case PathRole:
        return path();
    case TitleRole:
        return title();
    case AuthorRole:
        return author();
    case SelectedRole:
        return selected();
    case DateRole:
        return date();
    default:
        return QVariant();
    }
}
