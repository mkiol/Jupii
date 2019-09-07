/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QVariant>
#include <QSettings>
#include <QByteArray>

#include "taskexecutor.h"

class Settings:
        public QObject,
        public TaskExecutor
{
    Q_OBJECT
    Q_PROPERTY (int port READ getPort WRITE setPort NOTIFY portChanged)
    Q_PROPERTY (QString lastDir READ getLastDir WRITE setLastDir NOTIFY lastDirChanged)
    Q_PROPERTY (bool showAllDevices READ getShowAllDevices WRITE setShowAllDevices NOTIFY showAllDevicesChanged)
    Q_PROPERTY (int forwardTime READ getForwardTime WRITE setForwardTime NOTIFY forwardTimeChanged)
    Q_PROPERTY (bool imageSupported READ getImageSupported WRITE setImageSupported NOTIFY imageSupportedChanged)
    Q_PROPERTY (bool rememberPlaylist READ getRememberPlaylist WRITE setRememberPlaylist NOTIFY rememberPlaylistChanged)
    Q_PROPERTY (QStringList lastPlaylist READ getLastPlaylist WRITE setLastPlaylist NOTIFY lastPlaylistChanged)
    Q_PROPERTY (bool useHWVolume READ getUseHWVolume WRITE setUseHWVolume NOTIFY useHWVolumeChanged)
    Q_PROPERTY (QString prefNetInf READ getPrefNetInf WRITE setPrefNetInf NOTIFY prefNetInfChanged)
    Q_PROPERTY (float micVolume READ getMicVolume WRITE setMicVolume NOTIFY micVolumeChanged)
    Q_PROPERTY (int audioCaptureMode READ getAudioCaptureMode WRITE setAudioCaptureMode NOTIFY audioCaptureModeChanged)
    Q_PROPERTY (QString recDir READ getRecDir WRITE setRecDir NOTIFY recDirChanged)
    Q_PROPERTY (bool rec READ getRec WRITE setRec NOTIFY recChanged)
    Q_PROPERTY (int volStep READ getVolStep WRITE setVolStep NOTIFY volStepChanged)
    Q_PROPERTY (int screenFramerate READ getScreenFramerate WRITE setScreenFramerate NOTIFY screenFramerateChanged)
    Q_PROPERTY (bool screenCropTo169 READ getScreenCropTo169 WRITE setScreenCropTo169 NOTIFY screenCropTo169Changed)
    Q_PROPERTY (bool screenAudio READ getScreenAudio WRITE setScreenAudio NOTIFY screenAudioChanged)
    Q_PROPERTY (bool screenSupported READ getScreenSupported WRITE setScreenSupported NOTIFY screenSupportedChanged)
    Q_PROPERTY (int remoteContentMode READ getRemoteContentMode WRITE setRemoteContentMode NOTIFY remoteContentModeChanged)
    Q_PROPERTY (int albumQueryType READ getAlbumQueryType WRITE setAlbumQueryType NOTIFY albumQueryTypeChanged)
    Q_PROPERTY (int albumRecType READ getRecQueryType WRITE setRecQueryType NOTIFY recQueryTypeChanged)
    Q_PROPERTY (int playMode READ getPlayMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY (bool contentDirSupported READ getContentDirSupported WRITE setContentDirSupported NOTIFY contentDirSupportedChanged)

public:
#ifdef SAILFISH
    static constexpr char* HW_RELEASE_FILE = "/etc/hw-release";
#endif
    static Settings* instance();

    void setPort(int value);
    int getPort();

    void setForwardTime(int value);
    int getForwardTime();

    void setVolStep(int value);
    int getVolStep();

    void setShowAllDevices(bool value);
    bool getShowAllDevices();

    void setRec(bool value);
    bool getRec();

    void setImageSupported(bool value);
    bool getImageSupported();

    void setScreenSupported(bool value);
    bool getScreenSupported();

    void setAudioCaptureMode(int value);
    int getAudioCaptureMode();

    void setScreenFramerate(int value);
    int getScreenFramerate();

    void setScreenCropTo169(bool value);
    bool getScreenCropTo169();

    void setScreenAudio(bool value);
    bool getScreenAudio();

    void setRememberPlaylist(bool value);
    bool getRememberPlaylist();

    void setUseHWVolume(bool value);
    bool getUseHWVolume();

    void setAlbumQueryType(int value);
    int getAlbumQueryType();
    void setRecQueryType(int value);
    int getRecQueryType();

    void setPlayMode(int value);
    int getPlayMode();

    void setFavDevices(const QHash<QString,QVariant> &devs);
    void addFavDevice(const QString &id);
    void removeFavDevice(const QString &id);
    Q_INVOKABLE void asyncAddFavDevice(const QString &id);
    Q_INVOKABLE void asyncRemoveFavDevice(const QString &id);
    bool readDeviceXML(const QString& id, QByteArray& xml);
    QHash<QString, QVariant> getFavDevices();

    QString getLastDir();
    void setLastDir(const QString& value);

    QString getRecDir();
    void setRecDir(const QString& value);

    void setMicVolume(float value);
    float getMicVolume();

    QStringList getLastPlaylist();
    void setLastPlaylist(const QStringList& value);

    QByteArray getKey();
    QByteArray resetKey();

    QString getCacheDir();
    QString getPlaylistDir();

    QString getPrefNetInf();
    void setPrefNetInf(const QString& value);

    void setRemoteContentMode(int value);
    int getRemoteContentMode();

    void setContentDirSupported(bool value);
    bool getContentDirSupported();

    QString mediaServerDevUuid();
    QString prettyName();
    Q_INVOKABLE bool isDebug();

signals:
    void portChanged();
    void favDevicesChanged();
    void lastDirChanged();
    void recDirChanged();
    void lastPlaylistChanged();
    void showAllDevicesChanged();
    void forwardTimeChanged();
    void imageSupportedChanged();
    void pulseSupportedChanged();
    void audioCaptureModeChanged();
    void rememberPlaylistChanged();
    void useHWVolumeChanged();
    void prefNetInfChanged();
    void micVolumeChanged();
    void recChanged();
    void volStepChanged();
    void screenFramerateChanged();
    void screenCropTo169Changed();
    void screenAudioChanged();
    void screenSupportedChanged();
    void remoteContentModeChanged();
    void albumQueryTypeChanged();
    void recQueryTypeChanged();
    void playModeChanged();
    void contentDirSupportedChanged();

private:
    QSettings settings;
    static Settings* inst;
    QString hwName;

    explicit Settings(QObject* parent = nullptr);
    bool writeDeviceXML(const QString& id, QString &url);
#ifdef SAILFISH
    QString readHwInfo();
#endif
};

#endif // SETTINGS_H
