/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp dbus/org.jupii.xml -a src/app_adaptor -p src/app_interface
 *
 * qdbusxml2cpp is Copyright (C) 2016 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef APP_INTERFACE_H
#define APP_INTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface org.jupii.Player
 */
class OrgJupiiPlayerInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.jupii.Player"; }

public:
    OrgJupiiPlayerInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~OrgJupiiPlayerInterface();

    Q_PROPERTY(bool canControl READ canControl)
    inline bool canControl() const
    { return qvariant_cast< bool >(property("canControl")); }

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<> appendPath(const QString &path)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(path);
        return asyncCallWithArgumentList(QStringLiteral("appendPath"), argumentList);
    }

    inline QDBusPendingReply<> clearPlaylist()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("clearPlaylist"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void CanControlPropertyChanged(bool canControl);
};

namespace org {
  namespace jupii {
    typedef ::OrgJupiiPlayerInterface Player;
  }
}
#endif
