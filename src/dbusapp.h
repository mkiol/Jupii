/* Copyright (C) 2017-2021 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef DBUSAPP_H
#define DBUSAPP_H

#include <QObject>
#include <QString>
#include <QUrl>

#include "dbus_jupii_adaptor.h"

class DbusProxy :
        public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canControl READ canControl WRITE setCanControl NOTIFY canControlChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceNameChanged)

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit DbusProxy(QObject *parent = nullptr);
    ~DbusProxy();
    bool canControl();
    void setCanControl(bool value);
    bool playing();
    void setPlaying(bool value);
    QString deviceName();

signals:
    void focusRequested();
    void canControlChanged();
    void playingChanged();
    void deviceNameChanged();
    void enabledChanged();

    // internal
    void CanControlPropertyChanged(bool canControl);
    void PlayingPropertyChanged(bool playing);
    void DeviceNamePropertyChanged(const QString &deviceName);

public slots:
    void appendPath(const QString& path);
    void addPath(const QString& path, const QString& name);
    void addUrl(const QString& url, const QString& name);
    void openUrl(const QStringList& arguments);
    void addPathOnce(const QString& path, const QString& name);
    void addPathOnceAndPlay(const QString& path, const QString& name);
    void addUrlOnce(const QString& url, const QString& name);
    void addUrlOnceAndPlay(const QString& url, const QString& name);
    void add(const QString &url, const QString &origUrl, const QString &name,
             const QString &author, const QString &description,
             int type, const QString &app,
             const QString &icon, bool once, bool play);
    void pause();
    void play();
    void togglePlay();
    void focus();
    void clearPlaylist();
    void updatePlaying();
    void updateCanControl();
    void setEnabled(bool value);
    inline bool enabled() const { return m_enabled; }

private:
    PlayerAdaptor player;
    PlaylistAdaptor playlist;
    bool m_enabled = false;
    bool m_canControl = false;
    bool m_playing = false;
    QStringList m_pendingOpenUrlArguments;

    void openPendingUrl();
};

#endif // DBUSAPP_H
