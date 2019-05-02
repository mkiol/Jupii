/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QFileInfo>
#include <QUrl>
#include <QStringList>

// TagLib
#include "fileref.h"
#include "tag.h"

#include "recmodel.h"
#include "settings.h"

RecModel::RecModel(QObject *parent) :
    SelectableItemModel(new RecItem, parent),
    m_dir(Settings::instance()->getRecDir())
{
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

QList<ListItem*> RecModel::makeItems()
{
    QList<ListItem*> items;

    auto filter = getFilter();

    auto files = m_dir.entryInfoList(QStringList() << "*.jupii_rec.*", QDir::Files);
    for (const auto& file : files) {
        QString path = file.absoluteFilePath();
        QString title, author;
        TagLib::FileRef f(path.toUtf8().constData());
        if(f.isNull() || !f.tag()) {
            qWarning() << "Cannot extract meta data with TagLib:" << path;
            title = file.baseName();
        } else {
            TagLib::Tag *tag = f.tag();
            title = QString::fromWCharArray(tag->title().toCWString());
            author = QString::fromWCharArray(tag->artist().toCWString());
        }

        if (title.contains(filter, Qt::CaseInsensitive) ||
            author.contains(filter, Qt::CaseInsensitive)) {
            items << new RecItem(
                         path,
                         path,
                         title,
                         author);
        }
    }

    // Sorting
    std::sort(items.begin(), items.end(), [](ListItem *a, ListItem *b) {
        auto aa = dynamic_cast<RecItem*>(a);
        auto bb = dynamic_cast<RecItem*>(b);
        return aa->title().compare(bb->title(), Qt::CaseInsensitive) < 0;
    });

    return items;
}

RecItem::RecItem(const QString &id,
                 const QString &path,
                 const QString &title,
                 const QString &author,
                 QObject *parent) :
    SelectableItem(parent),
    m_id(id),
    m_path(path),
    m_title(title),
    m_author(author)
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
    default:
        return QVariant();
    }
}
