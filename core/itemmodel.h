/* Copyright (C) 2018 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ITEMMODEL_H
#define ITEMMODEL_H

#include <QObject>
#include <QList>
#include <QThread>
#include <memory>

#include "listmodel.h"

class ItemModel;

class ItemWorker : public QThread
{
    Q_OBJECT
friend class ItemModel;
friend class SelectableItemModel;

public:
    explicit ItemWorker(ItemModel *model, const QString &data = QString());

private:
    QString data;
    ItemModel *model;
    QList<ListItem*> items;
    void run();
};

class ItemModel : public ListModel
{
    Q_OBJECT
    Q_PROPERTY (bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY (int count READ getCount NOTIFY countChanged)

friend class ItemWorker;

public:
    explicit ItemModel(ListItem *prototype, QObject *parent = nullptr);
    int getCount();

public slots:
    virtual void updateModel(const QString &data = QString());

signals:
    void busyChanged();
    void countChanged();

protected slots:
    virtual void workerDone();

protected:
    std::unique_ptr<ItemWorker> m_worker;
    virtual QList<ListItem*> makeItems() = 0;
    virtual void clear();

private slots:
    bool isBusy();

private:
    bool m_busy = true;
    void setBusy(bool busy);
};

class SelectableItem: public ListItem
{
    Q_OBJECT

public:
    SelectableItem(QObject* parent = 0) : ListItem(parent) {}
    inline bool selected() const { return m_selected; }
    void setSelected(bool value);
private:
    bool m_selected = false;
};

class SelectableItemModel : public ItemModel
{
    Q_OBJECT
    Q_PROPERTY (QString filter READ getFilter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY (int selectedCount READ selectedCount NOTIFY selectedCountChanged)

public:
    explicit SelectableItemModel(SelectableItem *prototype, QObject *parent = nullptr);
    void setFilter(const QString& filter);
    QString getFilter();
    int selectedCount();

    Q_INVOKABLE void setSelected(int index, bool value);
    Q_INVOKABLE void setAllSelected(bool value);
    Q_INVOKABLE virtual QVariantList selectedItems();

public slots:
    void updateModel(const QString &data = QString());

signals:
    void filterChanged();
    void selectedCountChanged();

private slots:
    void workerDone();

private:
    QString m_filter;
    int m_selectedCount = 0;
    void clear();
};

#endif // ITEMMODEL_H
