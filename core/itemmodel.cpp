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
    data(data),
    model(model)
{
}

void ItemWorker::run()
{
    items = model->makeItems();
    model->postMakeItems(items);
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

void ItemModel::postMakeItems(const QList<ListItem*> &items)
{
    Q_UNUSED(items)
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
    auto worker = dynamic_cast<ItemWorker*>(sender());
    if (worker) {
        int old_l = m_list.length();

        if (m_list.length() != 0)
            removeRows(0,rowCount());

        if (!worker->items.isEmpty())
            appendRows(worker->items);
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

int SelectableItemModel::selectableCount()
{
    return m_selectableCount;
}

void SelectableItemModel::clear()
{
    ItemModel::clear();

    m_selectedCount = 0;
    emit selectedCountChanged();
}

void SelectableItemModel::postMakeItems(const QList<ListItem*> &items)
{
    int count = 0;
    for (const auto &item : items) {
        auto sitem = dynamic_cast<SelectableItem*>(item);
        if (sitem->selectable())
            count++;
    }

    if (count != m_selectableCount) {
        m_selectableCount = count;
        emit selectableCountChanged();
    }
}

void SelectableItemModel::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();

        updateModel();
    }
}

void SelectableItemModel::setFilterNoUpdate(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();
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

    if (item->selectable()) {
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
}

void SelectableItemModel::setAllSelected(bool value)
{
    if (m_list.isEmpty())
        return;

    int c = m_selectedCount;

    foreach (auto li, m_list) {
        auto item = qobject_cast<SelectableItem*>(li);
        if (item->selectable()) {
            bool cvalue = item->selected();

            if (cvalue != value) {
                item->setSelected(value);

                if (value)
                    m_selectedCount++;
                else
                    m_selectedCount--;
            }
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
    setAllSelected(false);
    ItemModel::updateModel(m_filter);
}
