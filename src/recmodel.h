#ifndef RECMODEL_H
#define RECMODEL_H

#include <QDir>
#include <QUrl>
#include <QString>
#include <QDateTime>
#include <QVariantList>
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
        SelectedRole,
        DateRole,
        FriendlyDateRole,
        IconRole
    };

    RecItem(QObject* parent = nullptr) : SelectableItem(parent) {}
    explicit RecItem(const QString &id,
                     const QString &path,
                     const QString &title,
                     const QString &author,
                     const QDateTime &date,
                     const QUrl &icon,
                     QObject *parent = nullptr);
    QVariant data(int role) const;
    QHash<int, QByteArray> roleNames() const;
    inline QString id() const { return m_id; }
    inline QString path() const { return m_path; }
    inline QString title() const { return m_title; }
    inline QString author() const { return m_author; }
    inline QDateTime date() const { return m_date; }
    QString friendlyDate() const;
    inline QUrl icon() const { return m_icon; }

private:
    QString m_id;
    QString m_path;
    QString m_title;
    QString m_author;
    QDateTime m_date;
    QUrl m_icon;
};

class RecModel : public SelectableItemModel
{
    Q_OBJECT
    Q_PROPERTY (int queryType READ getQueryType WRITE setQueryType NOTIFY queryTypeChanged)
public:
    explicit RecModel(QObject *parent = nullptr);
    Q_INVOKABLE QVariantList selectedItems() override;
    Q_INVOKABLE void deleteSelected();
    int getQueryType();
    void setQueryType(int value);

signals:
    void queryTypeChanged();

private:
    struct Item {
        QString path;
        QString title;
        QString author;
        QString album;
        QString comment;
        QString recUrl;
        QString iconPath;
        QDateTime recDate;
    };
    QList<Item> m_items;
    QDir m_dir;
    QList<ListItem*> makeItems() override;
    int m_queryType = 0;
};

#endif // RECMODEL_H
