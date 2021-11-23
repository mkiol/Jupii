/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp /home/mkiol/dev/Jupii/sailfish/../dbus/jupii.xml -a ../core/dbus_jupii_adaptor
 *
 * qdbusxml2cpp is Copyright (C) 2016 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * This file may have been hand-edited. Look for HAND-EDIT comments
 * before re-generating it.
 */

#ifndef DBUS_JUPII_ADAPTOR_H
#define DBUS_JUPII_ADAPTOR_H

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
QT_BEGIN_NAMESPACE
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;
QT_END_NAMESPACE

/*
 * Adaptor class for interface org.jupii.Player
 */
class PlayerAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.jupii.Player")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"org.jupii.Player\">\n"
"    <property access=\"read\" type=\"b\" name=\"canControl\"/>\n"
"    <property access=\"read\" type=\"s\" name=\"deviceName\"/>\n"
"    <property access=\"read\" type=\"b\" name=\"playing\"/>\n"
"    <signal name=\"CanControlPropertyChanged\">\n"
"      <arg direction=\"out\" type=\"b\" name=\"canControl\"/>\n"
"    </signal>\n"
"    <signal name=\"PlayingPropertyChanged\">\n"
"      <arg direction=\"out\" type=\"b\" name=\"playing\"/>\n"
"    </signal>\n"
"    <signal name=\"DeviceNamePropertyChanged\">\n"
"      <arg direction=\"out\" type=\"s\" name=\"deviceName\"/>\n"
"    </signal>\n"
"    <method name=\"appendPath\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"path\"/>\n"
"      <annotation value=\"true\" name=\"org.freedesktop.DBus.Deprecated\"/>\n"
"    </method>\n"
"    <method name=\"addPath\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"path\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"addPathOnce\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"path\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"addPathOnceAndPlay\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"path\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"addUrl\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"url\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"addUrlOnce\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"url\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"addUrlOnceAndPlay\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"url\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"    </method>\n"
"    <method name=\"add\">\n"
"      <arg direction=\"in\" type=\"s\" name=\"url\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"orig_url\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"author\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"description\"/>\n"
"      <arg direction=\"in\" type=\"i\" name=\"type\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"app\"/>\n"
"      <arg direction=\"in\" type=\"s\" name=\"icon\"/>\n"
"      <arg direction=\"in\" type=\"b\" name=\"once\"/>\n"
"      <arg direction=\"in\" type=\"b\" name=\"play\"/>\n"
"    </method>\n"
"    <method name=\"focus\"/>\n"
"    <method name=\"pause\"/>\n"
"    <method name=\"play\"/>\n"
"    <method name=\"togglePlay\"/>\n"
"    <method name=\"clearPlaylist\"/>\n"
"  </interface>\n"
        "")
public:
    PlayerAdaptor(QObject *parent);
    virtual ~PlayerAdaptor();

public: // PROPERTIES
    Q_PROPERTY(bool canControl READ canControl)
    bool canControl() const;

    Q_PROPERTY(QString deviceName READ deviceName)
    QString deviceName() const;

    Q_PROPERTY(bool playing READ playing)
    bool playing() const;

public Q_SLOTS: // METHODS
    void add(const QString &url, const QString &orig_url, const QString &name, const QString &author, const QString &description, int type, const QString &app, const QString &icon, bool once, bool play);
    void addPath(const QString &path, const QString &name);
    void addPathOnce(const QString &path, const QString &name);
    void addPathOnceAndPlay(const QString &path, const QString &name);
    void addUrl(const QString &url, const QString &name);
    void addUrlOnce(const QString &url, const QString &name);
    void addUrlOnceAndPlay(const QString &url, const QString &name);
    Q_DECL_DEPRECATED void appendPath(const QString &path);
    void clearPlaylist();
    void focus();
    void pause();
    void play();
    void togglePlay();
Q_SIGNALS: // SIGNALS
    void CanControlPropertyChanged(bool canControl);
    void DeviceNamePropertyChanged(const QString &deviceName);
    void PlayingPropertyChanged(bool playing);
};

/*
 * Adaptor class for interface org.mkiol.jupii.Playlist
 */
class PlaylistAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mkiol.jupii.Playlist")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"org.mkiol.jupii.Playlist\">\n"
"    <method name=\"openUrl\">\n"
"      <arg direction=\"in\" type=\"as\" name=\"arguments\"/>\n"
"    </method>\n"
"  </interface>\n"
        "")
public:
    PlaylistAdaptor(QObject *parent);
    virtual ~PlaylistAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    void openUrl(const QStringList &arguments);
Q_SIGNALS: // SIGNALS
};

#endif
