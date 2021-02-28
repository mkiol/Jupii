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
    Q_PROPERTY (QStringList lastPlaylist READ getLastPlaylist WRITE setLastPlaylist NOTIFY lastPlaylistChanged)
    Q_PROPERTY (bool useHWVolume READ getUseHWVolume WRITE setUseHWVolume NOTIFY useHWVolumeChanged)
    Q_PROPERTY (QString prefNetInf READ getPrefNetInf WRITE setPrefNetInf NOTIFY prefNetInfChanged)
    Q_PROPERTY (float micVolume READ getMicVolume WRITE setMicVolume NOTIFY micVolumeChanged)
    Q_PROPERTY (float audioBoost READ getAudioBoost WRITE setAudioBoost NOTIFY audioBoostChanged)
    Q_PROPERTY (QString recDir READ getRecDir WRITE setRecDir NOTIFY recDirChanged)
    Q_PROPERTY (bool rec READ getRec WRITE setRec NOTIFY recChanged)
    Q_PROPERTY (int volStep READ getVolStep WRITE setVolStep NOTIFY volStepChanged)
    Q_PROPERTY (int screenFramerate READ getScreenFramerate WRITE setScreenFramerate NOTIFY screenFramerateChanged)
    Q_PROPERTY (int screenCropTo169 READ getScreenCropTo169 WRITE setScreenCropTo169 NOTIFY screenCropTo169Changed)
    Q_PROPERTY (bool screenAudio READ getScreenAudio WRITE setScreenAudio NOTIFY screenAudioChanged)
    Q_PROPERTY (bool screenSupported READ getScreenSupported WRITE setScreenSupported NOTIFY screenSupportedChanged)
    Q_PROPERTY (int screenQuality READ getScreenQuality WRITE setScreenQuality NOTIFY screenQualityChanged)
    Q_PROPERTY (QString screenEncoder READ getScreenEncoder WRITE setScreenEncoder NOTIFY screenEncoderChanged)
    Q_PROPERTY (int remoteContentMode READ getRemoteContentMode WRITE setRemoteContentMode NOTIFY remoteContentModeChanged)
    Q_PROPERTY (int albumQueryType READ getAlbumQueryType WRITE setAlbumQueryType NOTIFY albumQueryTypeChanged)
    Q_PROPERTY (int albumRecQueryType READ getRecQueryType WRITE setRecQueryType NOTIFY recQueryTypeChanged)
    Q_PROPERTY (int playMode READ getPlayMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY (bool contentDirSupported READ getContentDirSupported WRITE setContentDirSupported NOTIFY contentDirSupportedChanged)
    Q_PROPERTY (bool logToFile READ getLogToFile WRITE setLogToFile NOTIFY logToFileChanged)
    Q_PROPERTY (int colorScheme READ getColorScheme WRITE setColorScheme NOTIFY colorSchemeChanged)
    Q_PROPERTY (QString fsapiPin READ fsapiPin WRITE setFsapiPin NOTIFY fsapiPinChanged)
    Q_PROPERTY (bool globalYtdl READ globalYtdl WRITE setGlobalYtdl NOTIFY globalYtdlChanged)

public:   
    enum Hint {
        Hint_DeviceSwipeLeft =      1 << 0,
        Hint_NotConnectedTip =      1 << 1,
        Hint_ExpandPlayerPanelTip = 1 << 2,
        Hint_MediaInfoSwipeLeft =   1 << 3
    };
    Q_ENUM(Hint)

#ifdef SAILFISH
    static constexpr const char* HW_RELEASE_FILE = "/etc/hw-release";
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

    void setScreenSupported(bool value);
    bool getScreenSupported();

    void setScreenFramerate(int value);
    int getScreenFramerate();

    void setScreenQuality(int value);
    int getScreenQuality();

    void setScreenCropTo169(int value);
    int getScreenCropTo169();

    void setScreenAudio(bool value);
    bool getScreenAudio();

    void setScreenEncoder(const QString &value);
    QString getScreenEncoder();

    void setUseHWVolume(bool value);
    bool getUseHWVolume();

    void setLogToFile(bool value);
    bool getLogToFile();

    void setGlobalYtdl(bool value);
    bool globalYtdl() const;

    void setAlbumQueryType(int value);
    int getAlbumQueryType();
    void setRecQueryType(int value);
    int getRecQueryType();
    void setCDirQueryType(int value);
    int getCDirQueryType();

    void setPlayMode(int value);
    int getPlayMode();

    void setFsapiPin(const QString &value);
    QString fsapiPin() const;

    void setFavDevices(const QHash<QString,QVariant> &devs);
    QHash<QString, QVariant> getFavDevices();
    void setLastDevices(const QHash<QString,QVariant> &devs);
    QHash<QString, QVariant> getLastDevices();
    Q_INVOKABLE void addFavDevice(const QString &id);
    void addLastDevice(const QString &id);
    void addLastDevices(const QStringList &ids);
    Q_INVOKABLE void removeFavDevice(const QString &id);
    Q_INVOKABLE void asyncAddFavDevice(const QString &id);
    Q_INVOKABLE void asyncRemoveFavDevice(const QString &id);
    static bool readDeviceXML(const QString& id, QByteArray& xml);
    static bool writeDeviceXML(const QString& id, QString &url);

    QString getLastDir();
    void setLastDir(const QString& value);

    QString getRecDir();
    void setRecDir(const QString& value);

    void setMicVolume(float value);
    float getMicVolume();

    void setAudioBoost(float value);
    float getAudioBoost();

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

    void setColorScheme(int value);
    int getColorScheme();

    QString mediaServerDevUuid();
    QString prettyName();
    Q_INVOKABLE bool isDebug();
    Q_INVOKABLE void reset();

    Q_INVOKABLE bool hintEnabled(Settings::Hint hint);
    Q_INVOKABLE void disableHint(Settings::Hint hint);
    Q_INVOKABLE void resetHints();

signals:
    void portChanged();
    void favDevicesChanged();
    void favDeviceChanged(const QString& id);
    void lastDirChanged();
    void recDirChanged();
    void lastPlaylistChanged();
    void showAllDevicesChanged();
    void forwardTimeChanged();
    void imageSupportedChanged();
    void pulseSupportedChanged();
    void useHWVolumeChanged();
    void prefNetInfChanged();
    void micVolumeChanged();
    void recChanged();
    void volStepChanged();
    void screenFramerateChanged();
    void screenCropTo169Changed();
    void screenAudioChanged();
    void screenSupportedChanged();
    void screenQualityChanged();
    void screenEncoderChanged();
    void remoteContentModeChanged();
    void albumQueryTypeChanged();
    void recQueryTypeChanged();
    void cdirQueryTypeChanged();
    void playModeChanged();
    void contentDirSupportedChanged();
    void logToFileChanged();
    void colorSchemeChanged();
    void audioBoostChanged();
    void fsapiPinChanged();
    void globalYtdlChanged();

private:
    QSettings settings;
    static Settings* inst;
    QString hwName;
    int m_colorScheme = 0;

    explicit Settings(QObject* parent = nullptr);
#ifdef SAILFISH
    QString readHwInfo();
#endif
};

#endif // SETTINGS_H
