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
#include <QStringList>
#include <utility>

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
    Q_PROPERTY (OpenUrlModeType openUrlMode READ openUrlMode WRITE setOpenUrlMode NOTIFY openUrlModeChanged)
    Q_PROPERTY (QStringList bcSearchHistory READ bcSearchHistory NOTIFY bcSearchHistoryChanged)
    Q_PROPERTY (QStringList soundcloudSearchHistory READ soundcloudSearchHistory NOTIFY soundcloudSearchHistoryChanged)
    Q_PROPERTY (QStringList icecastSearchHistory READ icecastSearchHistory NOTIFY icecastSearchHistoryChanged)
    Q_PROPERTY (QStringList tuneinSearchHistory READ tuneinSearchHistory NOTIFY tuneinSearchHistoryChanged)

public:
    enum class HintType {
        Hint_DeviceSwipeLeft =      1 << 0,
        Hint_NotConnectedTip =      1 << 1,
        Hint_ExpandPlayerPanelTip = 1 << 2,
        Hint_MediaInfoSwipeLeft =   1 << 3
    };
    Q_ENUM(HintType)

    enum class OpenUrlModeType {
        OpenUrlMode_None = 0,
        OpenUrlMode_All = 1
    };
    Q_ENUM(OpenUrlModeType)

#ifdef SAILFISH
    static constexpr const char* HW_RELEASE_FILE = "/etc/hw-release";
#endif
    static Settings* instance();

    void setPort(int value);
    int getPort() const;
    void setForwardTime(int value);
    int getForwardTime() const;
    void setVolStep(int value);
    int getVolStep() const;
    void setShowAllDevices(bool value);
    bool getShowAllDevices() const;
    void setRec(bool value);
    bool getRec() const;
    void setScreenSupported(bool value);
    bool getScreenSupported() const;
    void setScreenFramerate(int value);
    int getScreenFramerate() const;
    void setScreenQuality(int value);
    int getScreenQuality() const;
    void setScreenCropTo169(int value);
    int getScreenCropTo169();
    void setScreenAudio(bool value);
    bool getScreenAudio();
    void setScreenEncoder(const QString &value);
    QString getScreenEncoder() const;
    void setUseHWVolume(bool value);
    bool getUseHWVolume() const;
    void setLogToFile(bool value);
    bool getLogToFile() const;
    void setGlobalYtdl(bool value);
    bool globalYtdl() const;
    void setAlbumQueryType(int value);
    int getAlbumQueryType() const;
    void setRecQueryType(int value);
    int getRecQueryType() const;
    void setCDirQueryType(int value);
    int getCDirQueryType() const;
    void setPlayMode(int value);
    int getPlayMode() const;
    void setFsapiPin(const QString &value);
    QString fsapiPin() const;
    void setFavDevices(const QHash<QString,QVariant> &devs);
    QHash<QString, QVariant> getFavDevices() const;
    void setLastDevices(const QHash<QString,QVariant> &devs);
    QHash<QString, QVariant> getLastDevices() const;
    Q_INVOKABLE void addFavDevice(const QString &id);
    void addLastDevice(const QString &id);
    void addLastDevices(const QStringList &ids);
    Q_INVOKABLE void removeFavDevice(const QString &id);
    Q_INVOKABLE void asyncAddFavDevice(const QString &id);
    Q_INVOKABLE void asyncRemoveFavDevice(const QString &id);
    static bool readDeviceXML(const QString& id, QByteArray& xml);
    static bool writeDeviceXML(const QString& id, QString &url);
    QString getLastDir() const;
    void setLastDir(const QString &value);
    QString getRecDir();
    void setRecDir(const QString &value);
    void setMicVolume(float value);
    float getMicVolume() const;
    void setAudioBoost(float value);
    float getAudioBoost() const;
    QStringList getLastPlaylist() const;
    void setLastPlaylist(const QStringList &value);
    QByteArray getKey();
    QByteArray resetKey();
    QString getCacheDir() const;
    QString getPlaylistDir() const;
    QString getPrefNetInf() const;
    void setPrefNetInf(const QString &value);
    void setRemoteContentMode(int value);
    int getRemoteContentMode() const;
    void setContentDirSupported(bool value);
    bool getContentDirSupported() const;
    void setColorScheme(int value);
    int getColorScheme() const;
    QString mediaServerDevUuid();
    QString prettyName() const;
    Q_INVOKABLE bool isDebug() const;
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool hintEnabled(HintType hint) const;
    Q_INVOKABLE void disableHint(HintType hint);
    Q_INVOKABLE void resetHints();
    OpenUrlModeType openUrlMode() const;
    void setOpenUrlMode(OpenUrlModeType value);
    QStringList bcSearchHistory() const;
    Q_INVOKABLE void addBcSearchHistory(const QString &value);
    Q_INVOKABLE void removeBcSearchHistory(const QString &value);
    QStringList soundcloudSearchHistory() const;
    Q_INVOKABLE void addSoundcloudSearchHistory(const QString &value);
    Q_INVOKABLE void removeSoundcloudSearchHistory(const QString &value);
    QStringList icecastSearchHistory() const;
    Q_INVOKABLE void addIcecastSearchHistory(const QString &value);
    Q_INVOKABLE void removeIcecastSearchHistory(const QString &value);
    QStringList tuneinSearchHistory() const;
    Q_INVOKABLE void addTuneinSearchHistory(const QString &value);
    Q_INVOKABLE void removeTuneinSearchHistory(const QString &value);

signals:
    void portChanged();
    void favDevicesChanged();
    void favDeviceChanged(const QString &id);
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
    void openUrlModeChanged();
    void bcSearchHistoryChanged();
    void soundcloudSearchHistoryChanged();
    void icecastSearchHistoryChanged();
    void tuneinSearchHistoryChanged();

private:
    inline static const QStringList urlMimesForOpenWith = {
        "x-scheme-handler/http",
        "x-scheme-handler/https"
    };
    inline static const QStringList fileMimesForOpenWith = {
        "audio/*",
        "video/*"
    };
    static const int maxSearchHistory = 3;
    QSettings settings;
    static Settings* inst;
    const QString hwName;
    int m_colorScheme = 0;

    explicit Settings(QObject* parent = nullptr);
#ifdef SAILFISH
    static QString readHwInfo();
#endif
    static std::pair<int,int> sysVer();
    void initOpenUrlMode();
};

#endif // SETTINGS_H
