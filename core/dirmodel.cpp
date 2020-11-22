/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QStandardPaths>
#include <QFileInfo>
#include "dirmodel.h"

DirModel::DirModel(QObject *parent) :
    ItemModel(new DirItem, parent),
    m_dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
{
}

QList<ListItem*> DirModel::makeItems()
{
    QList<ListItem*> items;

    auto dirs = m_dir.entryInfoList(QDir::Dirs|QDir::NoDot);
    for (auto& dir : dirs) {
        if (!m_dir.isRoot() || dir.fileName() != "..") {
            items << new DirItem(
                         dir.absoluteFilePath(),
                         dir.fileName(),
                         dir.absoluteFilePath());
        }
    }

    return items;
}

bool DirModel::isCurrentWritable()
{
    return QFileInfo(m_dir.canonicalPath()).isWritable();
}

void DirModel::changeToRemovable()
{
    setCurrentPath("/run/media/" + qgetenv("USER"));
}

void DirModel::changeToHome()
{
    setCurrentPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
}

void DirModel::setCurrentPath(const QString &path)
{
    auto currentPath = m_dir.canonicalPath();
    auto newPath = QDir::cleanPath(path);
    if (currentPath != newPath) {
        m_dir.setPath(newPath);
        emit currentDirChanged();
        updateModel();
    }
}

QString DirModel::getCurrentPath()
{
    return m_dir.canonicalPath();
}

QString DirModel::getCurrentName()
{
    return m_dir.dirName();
}

DirItem::DirItem(const QString &id,
                 const QString &name,
                 const QString &path,
                 QObject *parent) :
    ListItem(parent),
    m_id(id),
    m_name(name),
    m_path(path)
{
}

QHash<int, QByteArray> DirItem::roleNames() const
{
    QHash<int, QByteArray> names;
    names[IdRole] = "id";
    names[NameRole] = "name";
    names[PathRole] = "path";
    return names;
}

QVariant DirItem::data(int role) const
{
    switch(role) {
    case IdRole:
        return id();
    case NameRole:
        return name();
    case PathRole:
        return path();
    default:
        return QVariant();
    }
}
