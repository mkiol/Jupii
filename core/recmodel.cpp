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
#include <algorithm>

#include "recmodel.h"
#include "settings.h"
#include "contentserver.h"
#include "utils.h"
#include "tracker.h"

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

    if (removed) {
        m_items.clear();
        updateModel();
    }
}

QList<ListItem*> RecModel::makeItems()
{
    if (m_items.isEmpty()) {
        auto files = m_dir.entryInfoList(QStringList() << "*.jupii_rec.*", QDir::Files);
        for (int i = 0; i < std::min(files.size(), 1000); ++i) {
            const auto& file = files.at(i);
            Item item;
            item.path = file.absoluteFilePath();

            if (!ContentServer::readMetaUsingTaglib(item.path,
                                item.title, item.author, item.album, item.comment,
                                item.recUrl, item.recDate, item.iconPath)) {
                qWarning() << "Cannot read meta with TagLib";
                continue;
            }

            // fallbacks
            if (item.iconPath.isEmpty() && !item.author.isEmpty() && !item.album.isEmpty())
                item.iconPath = Tracker::instance()->genAlbumArtFile(item.album, item.author);
            if (item.title.isEmpty())
                item.title = file.baseName();
            if (item.author.isEmpty())
                item.author = tr("Unknown");
            if (item.album.isEmpty())
                item.album = tr("Unknown");
            /*if (item.recDate.isNull())
                item.recDate = file.created();*/

            m_items << item;
        }
    }

    QList<ListItem*> items;
    auto filter = getFilter();

    for (const auto& item : m_items) {
        if (item.title.contains(filter, Qt::CaseInsensitive) ||
            item.author.contains(filter, Qt::CaseInsensitive)) {
            items << new RecItem(
                         item.path,
                         item.path,
                         item.title,
                         item.author,
                         item.recDate,
#ifdef SAILFISH
                         QUrl::fromLocalFile(item.iconPath));
#else
                         QIcon(item.iconPath));
#endif
        }
    }

    // Sorting
    if (m_queryType == 0) { // by date
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = dynamic_cast<RecItem*>(a);
            auto bb = dynamic_cast<RecItem*>(b);
            if (aa->date().isNull() && !bb->date().isNull())
                return false;
            if (!aa->date().isNull() && bb->date().isNull())
                return true;
            return aa->date() > bb->date();
        });
    } else if (m_queryType == 1) { // by title
        std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
            auto aa = dynamic_cast<RecItem*>(a);
            auto bb = dynamic_cast<RecItem*>(b);
            return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
        });
    } else { // by author
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
#ifdef SAILFISH
                 const QUrl &icon,
#else
                 const QIcon &icon,
#endif
                 QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_path(path),
    m_title(title),
    m_author(author),
    m_date(date),
    m_icon(icon)
{
}

QString RecItem::friendlyDate() const
{
    return Utils::friendlyDate(m_date);
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
    names[FriendlyDateRole] = "friendlyDate";
    names[IconRole] = "icon";
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
    case FriendlyDateRole:
        return friendlyDate();
    case IconRole:
        return icon();
    default:
        return QVariant();
    }
}
