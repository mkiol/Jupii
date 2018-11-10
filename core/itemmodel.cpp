/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>

#include "itemmodel.h"

ItemWorker::ItemWorker(ItemModel *model, const QString &data) :
    QThread(model),
    model(model),
    data(data)
{
}

void ItemWorker::run()
{
    items = model->makeItems();
}

ItemModel::ItemModel(ListItem *prototype, QObject *parent) :
    ListModel(prototype, parent)
{
}

void ItemModel::updateModel(const QString &data)
{
    if (m_worker && m_worker->isRunning()) {
        //qWarning() << "Previous worker is running";
        return;
    }

    setBusy(true);
    m_worker = std::unique_ptr<ItemWorker>(new ItemWorker(this, data));
    connect(m_worker.get(), &QThread::finished, this, &ItemModel::workerDone);
    m_worker->start(QThread::IdlePriority);
}

void ItemModel::clear()
{
    if (m_list.length() == 0)
        return;

    removeRows(0,rowCount());
    emit countChanged();
}

void ItemModel::workerDone()
{
    if (m_worker && m_worker->isFinished()) {
        int old_l = m_list.length();

        if (m_list.length() != 0)
            removeRows(0,rowCount());

        if (!m_worker->items.isEmpty())
            appendRows(m_worker->items);
        else
            qWarning() << "No items";

        if (old_l != m_list.length())
            emit countChanged();

        m_worker.reset(nullptr);
    }

    setBusy(false);
}

int  ItemModel::getCount()
{
    return m_list.length();
}

bool ItemModel::isBusy()
{
    return m_busy;
}

void ItemModel::setBusy(bool busy)
{
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void SelectableItem::setSelected(bool value)
{
    if (m_selected != value) {
        m_selected = value;
        emit dataChanged();
    }
}

SelectableItemModel::SelectableItemModel(SelectableItem *prototype, QObject *parent) :
    ItemModel(prototype, parent)
{
}

int SelectableItemModel::selectedCount()
{
    return m_selectedCount;
}

void SelectableItemModel::clear()
{
    ItemModel::clear();

    m_selectedCount = 0;
    emit selectedCountChanged();
}

void SelectableItemModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();

        updateModel();
    }
}

QString SelectableItemModel::getFilter()
{
    return m_filter;
}

void SelectableItemModel::setSelected(int index, bool value)
{
    int l = m_list.length();

    if (index >= l) {
        qWarning() << "Index is invalid";
        return;
    }

    auto item = dynamic_cast<SelectableItem*>(m_list.at(index));

    bool cvalue = item->selected();

    if (cvalue != value) {
        item->setSelected(value);

        if (value)
            m_selectedCount++;
        else
            m_selectedCount--;

        emit selectedCountChanged();
    }
}

void SelectableItemModel::setAllSelected(bool value)
{
    if (m_list.isEmpty())
        return;

    int c = m_selectedCount;

    for (auto li : m_list) {
        auto item = dynamic_cast<SelectableItem*>(li);

        bool cvalue = item->selected();

        if (cvalue != value) {
            item->setSelected(value);

            if (value)
                m_selectedCount++;
            else
                m_selectedCount--;
        }
    }

    if (c != m_selectedCount)
         emit selectedCountChanged();
}

QVariantList SelectableItemModel::selectedItems()
{
    return QVariantList();
}

void SelectableItemModel::workerDone()
{
    if (m_worker && m_worker->data != m_filter) {
        //qDebug() << "Filter has changed, so updating model";
        updateModel(m_filter);
    } else {
        ItemModel::workerDone();
    }
}

void SelectableItemModel::updateModel(const QString &data)
{
    Q_UNUSED(data)
    ItemModel::updateModel(m_filter);
}
