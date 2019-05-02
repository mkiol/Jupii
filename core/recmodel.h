#ifndef RECMODEL_H
#define RECMODEL_H

#include <QDir>
#include "itemmodel.h"

class RecItem: public SelectableItem
{
    Q_OBJECT
public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        IdRole = Qt::UserRole,
        PathRole,
        AuthorRole,
        SelectedRole
    };

    RecItem(QObject* parent = nullptr) : SelectableItem(parent) {}
    explicit RecItem(const QString &id,
                     const QString &path,
                     const QString &title,
                     const QString &author,
                     QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString path() const { return m_path; }
    inline QString title() const { return m_title; }
    inline QString author() const { return m_author; }

private:
    QString m_id;
    QString m_path;
    QString m_title;
    QString m_author;
};

class RecModel : public SelectableItemModel
{
    Q_OBJECT
public:
    explicit RecModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems();

private:
    QDir m_dir;
    QList<ListItem*> makeItems();
};

#endif // RECMODEL_H
