/* Copyright (C) 2017-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#ifdef USE_DESKTOP
#include <QQmlApplicationEngine>
#endif
#include <utility>

#include "caster.hpp"
#include "singleton.h"
#include "taskexecutor.h"

// clang-format off
//    property-name                   getter                            setter                            setting-key                        type     default-value   restart-required
#define SETTINGS_PROPERTY_TABLE                                                                                                                                                       \
    X(port,                           getPort,                          setPort,                          port,                              int,     9092,           false         ) \
    X(volStep,                        getVolStep,                       setVolStep,                       volstep,                           int,     5,              false         ) \
    X(lastDir,                        getLastDir,                       setLastDir,                       lastdir,                           QString, "",             false         ) \
    X(showAllDevices,                 getShowAllDevices,                setShowAllDevices,                showalldevices,                    bool,    false,          false         ) \
    X(showPlaylistItemIcon,           getShowPlaylistItemIcon,          setShowPlaylistItemIcon,          show_playlist_item_icon,           bool,    true,           false         ) \
    X(imageAsVideo,                   getImageAsVideo,                  setImageAsVideo,                  image_as_video,                    bool,    false,          false         ) \
    X(imageDuration,                  getImageDuration,                 setImageDuration,                 image_duration,                    int,     30,             false         ) \
    X(imagePaused,                    getImagePaused,                   setImagePaused,                   image_paused,                      bool,    false,          false         ) \
    X(imageFps,                       getImageFps,                      setImageFps,                      image_fps,                         int,     2,              false         ) \
    X(imageScale,                     getImageScale,                    setImageScale,                    image_scale,                       Settings::ImageScale, Settings::ImageScale::ImageScale_1080, false) \
    X(imageRotate,                    getImageRotate,                   setImageRotate,                   image_rotate,                      Settings::ImageRotate, Settings::ImageRotate::ImageRotate_None, false) \
    X(slidesLoop,                     getSlidesLoop,                    setSlidesLoop,                    slides_loop,                       bool,    false,          false         ) \
    X(slidesShowCountInd,             getSlidesShowCountInd,            setSlidesShowCountInd,            slides_show_count_ind,             bool,    true,           false         ) \
    X(slidesShowProgrInd,             getSlidesShowProgrInd,            setSlidesShowProgrInd,            slides_show_progr_ind,             bool,    true,           false         ) \
    X(slidesShowDateInd,              getSlidesShowDateInd,             setSlidesShowDateInd,             slides_show_date_ind,              bool,    true,           false         ) \
    X(slidesShowCameraInd,            getSlidesShowCameraInd,           setSlidesShowCameraInd,           slides_show_camera_ind,            bool,    false,          false         ) \
    X(casterMicVolume,                getCasterMicVolume,               setCasterMicVolume,               caster_mic_volume,                 int,     0,              false         ) \
    X(casterPlaybackVolume,           getCasterPlaybackVolume,          setCasterPlaybackVolume,          caster_playback_volume,            int,     0,              false         ) \
    X(casterPlaybackMuted,            getCasterPlaybackMuted,           setCasterPlaybackMuted,           caster_playback_muted,             bool,    false,          false         ) \
    X(casterScreenRotate,             getCasterScreenRotate,            setCasterScreenRotate,            caster_screen_rotate,              bool,    false,          false         ) \
    X(casterScreenAudio,              getCasterScreenAudio,             setCasterScreenAudio,             caster_screen_audio,               bool,    false,          false         ) \
    X(casterCamAudio,                 getCasterCamAudio,                setCasterCamAudio,                caster_cam_audio,                  bool,    false,          false         ) \
    X(casterVideoOrientation,         getCasterVideoOrientation,        setCasterVideoOrientation,        caster_video_orientation,          Settings::CasterVideoOrientation, Settings::CasterVideoOrientation::CasterVideoOrientation_Auto, false) \
    X(casterVideoEncoder,             getCasterVideoEncoder,            setCasterVideoEncoder,            caster_video_encoder,              Settings::CasterVideoEncoder, Settings::CasterVideoEncoder::CasterVideoEncoder_Auto, false) \
    X(casterVideoStreamFormat,        getCasterVideoStreamFormat,       setCasterVideoStreamFormat,       caster_video_stream_format,        Settings::CasterStreamFormat, Settings::CasterStreamFormat::CasterStreamFormat_MpegTs, true) \
    X(casterVideoStreamFormatSlides,  getCasterVideoStreamFormatSlides, setCasterVideoStreamFormatSlides, caster_video_stream_format_slides, Settings::CasterStreamFormat, Settings::CasterStreamFormat::CasterStreamFormat_Avi, true) \
    X(casterAudioStreamFormat,        getCasterAudioStreamFormat,       setCasterAudioStreamFormat,       caster_audio_stream_format,        Settings::CasterStreamFormat, Settings::CasterStreamFormat::CasterStreamFormat_Mp3, true) \
    X(lipstickFps,                    getLipstickFps,                   setLipstickFps,                   lisptick_fps,                      int,     20,              false         ) \
    X(mjpegQuality,                   getMjpegQuality,                  setMjpegQuality,                  mjpeg_quality,                     int,     95,              false         ) \
    X(x264Quality,                    getX264Quality,                   setX264Quality,                   x264_quality,                      int,     60,              false         ) \
    X(mpdPolicy,                      getMpdPolicy,                     setMpdPolicy,                     mpd_policy,                        Settings::MpdPolicy, Settings::MpdPolicy::MpdPolicy_None, false) \
    // clang-format on

class Settings : public QSettings,
                 public TaskExecutor,
                 public Singleton<Settings> {
    Q_OBJECT

#define X(name, getter, setter, key, type, dvalue, restart) \
    Q_PROPERTY(type name READ getter WRITE setter NOTIFY name##Changed)
    SETTINGS_PROPERTY_TABLE
#undef X

    Q_PROPERTY(bool restartRequired READ getRestartRequired NOTIFY
                   restartRequiredChanged)
    Q_PROPERTY(int forwardTime READ getForwardTime WRITE setForwardTime NOTIFY
                   forwardTimeChanged)
    Q_PROPERTY(QStringList lastPlaylist READ getLastPlaylist WRITE
                   setLastPlaylist NOTIFY lastPlaylistChanged)
    Q_PROPERTY(bool useHWVolume READ getUseHWVolume WRITE setUseHWVolume NOTIFY
                   useHWVolumeChanged)
    Q_PROPERTY(QString prefNetInf READ getPrefNetInf WRITE setPrefNetInf NOTIFY
                   prefNetInfChanged)
    Q_PROPERTY(
        QString recDir READ getRecDir WRITE setRecDir NOTIFY recDirChanged)
    Q_PROPERTY(int albumQueryType READ getAlbumQueryType WRITE setAlbumQueryType
                   NOTIFY albumQueryTypeChanged)
    Q_PROPERTY(int albumRecQueryType READ getRecQueryType WRITE setRecQueryType
                   NOTIFY recQueryTypeChanged)
    Q_PROPERTY(
        int playMode READ getPlayMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY(bool contentDirSupported READ getContentDirSupported WRITE
                   setContentDirSupported NOTIFY contentDirSupportedChanged)
    Q_PROPERTY(bool logToFile READ getLogToFile WRITE setLogToFile NOTIFY
                   logToFileChanged)
    Q_PROPERTY(int colorScheme READ getColorScheme WRITE setColorScheme NOTIFY
                   colorSchemeChanged)
    Q_PROPERTY(
        QString fsapiPin READ fsapiPin WRITE setFsapiPin NOTIFY fsapiPinChanged)
    Q_PROPERTY(QStringList bcSearchHistory READ bcSearchHistory NOTIFY
                   bcSearchHistoryChanged)
    Q_PROPERTY(QStringList soundcloudSearchHistory READ soundcloudSearchHistory
                   NOTIFY soundcloudSearchHistoryChanged)
    Q_PROPERTY(QStringList icecastSearchHistory READ icecastSearchHistory NOTIFY
                   icecastSearchHistoryChanged)
    Q_PROPERTY(QStringList tuneinSearchHistory READ tuneinSearchHistory NOTIFY
                   tuneinSearchHistoryChanged)
    Q_PROPERTY(QStringList ytSearchHistory READ ytSearchHistory NOTIFY
                   ytSearchHistoryChanged)
    Q_PROPERTY(QStringList radionetSearchHistory READ radionetSearchHistory
                   NOTIFY radionetSearchHistoryChanged)
    Q_PROPERTY(CacheType cacheType READ cacheType WRITE setCacheType NOTIFY
                   cacheTypeChanged)
    Q_PROPERTY(CacheCleaningType cacheCleaningType READ cacheCleaningType WRITE
                   setCacheCleaningType NOTIFY cacheCleaningTypeChanged)
    Q_PROPERTY(bool allowNotIsomMp4 READ allowNotIsomMp4 WRITE
                   setAllowNotIsomMp4 NOTIFY allowNotIsomMp4Changed)
    Q_PROPERTY(YtPreferredType ytPreferredType READ ytPreferredType WRITE
                   setYtPreferredType NOTIFY ytPreferredTypeChanged)
    Q_PROPERTY(QString pyPath READ pyPath WRITE setPyPath NOTIFY pyPathChanged)
    Q_PROPERTY(QString prevAppVer READ prevAppVer WRITE setPrevAppVer NOTIFY
                   prevAppVerChanged)
    Q_PROPERTY(int qtStyleIdx READ qtStyleIdx WRITE setQtStyleIdx NOTIFY
                   qtStyleChanged)
    Q_PROPERTY(QString qtStyleName READ qtStyleName WRITE setQtStyleName NOTIFY
                   qtStyleChanged)
    Q_PROPERTY(bool qtStyleAuto READ qtStyleAuto WRITE setQtStyleAuto NOTIFY
                   qtStyleChanged)
    Q_PROPERTY(AvNextUriPolicy avNextUriPolicy READ getAvNextUriPolicy WRITE
                   setAvNextUriPolicy NOTIFY avNextUriPolicyChanged)

    // caster
    Q_PROPERTY(QStringList casterScreens READ getCasterScreens NOTIFY
                   casterSourcesChanged)
    Q_PROPERTY(
        QStringList casterCams READ getCasterCams NOTIFY casterSourcesChanged)
    Q_PROPERTY(
        QStringList casterMics READ getCasterMics NOTIFY casterSourcesChanged)
    Q_PROPERTY(QStringList casterPlaybacks READ getCasterPlaybacks NOTIFY
                   casterSourcesChanged)
    Q_PROPERTY(int casterScreenIdx READ getCasterScreenIdx WRITE
                   setCasterScreenIdx NOTIFY casterScreenChanged)
    Q_PROPERTY(int casterCamIdx READ getCasterCamIdx WRITE setCasterCamIdx
                   NOTIFY casterCamChanged)
    Q_PROPERTY(int casterMicIdx READ getCasterMicIdx WRITE setCasterMicIdx
                   NOTIFY casterMicChanged)
    Q_PROPERTY(int casterPlaybackIdx READ getCasterPlaybackIdx WRITE
                   setCasterPlaybackIdx NOTIFY casterPlaybackChanged)

   public:
    enum class CasterStreamFormat {
        CasterStreamFormat_Mp4 = 0,
        CasterStreamFormat_MpegTs = 1,
        CasterStreamFormat_Mp3 = 2,
        CasterStreamFormat_Avi = 3
    };
    Q_ENUM(CasterStreamFormat)

    enum class CasterVideoOrientation {
        CasterVideoOrientation_Auto = 0,
        CasterVideoOrientation_Portrait = 1,
        CasterVideoOrientation_InvertedPortrait = 2,
        CasterVideoOrientation_Landscape = 3,
        CasterVideoOrientation_InvertedLandscape = 4
    };
    Q_ENUM(CasterVideoOrientation)

    enum class CasterVideoEncoder {
        CasterVideoEncoder_Auto = 0,
        CasterVideoEncoder_X264 = 1,
        CasterVideoEncoder_Nvenc = 2,
        CasterVideoEncoder_V4l2 = 3,
        CasterVideoEncoder_Mjpeg = 4,
    };
    Q_ENUM(CasterVideoEncoder)

    enum class HintType {
        Hint_DeviceSwipeLeft = 1 << 0,
        Hint_NotConnectedTip = 1 << 1,
        Hint_ExpandPlayerPanelTip = 1 << 2,
        Hint_MediaInfoSwipeLeft = 1 << 3
    };
    Q_ENUM(HintType)

    enum class CacheType { Cache_Auto = 0, Cache_Always = 1, Cache_Never = 2 };
    Q_ENUM(CacheType)

    enum class CacheCleaningType {
        CacheCleaning_Auto = 0,
        CacheCleaning_Always = 1,
        CacheCleaning_Never = 2
    };
    Q_ENUM(CacheCleaningType)

    enum class YtPreferredType { YtPreferredType_Video, YtPreferredType_Audio };
    Q_ENUM(YtPreferredType)

    enum class AvNextUriPolicy {
        AvNextUriPolicy_Auto = 0,
        AvNextUriPolicy_DisableOnlyIfNotSupported = 1,
        AvNextUriPolicy_NeverDisable = 2,
        AvNextUriPolicy_AlwaysDisable = 3
    };
    Q_ENUM(AvNextUriPolicy)

    enum class ImageScale {
        ImageScale_None = 0,
        ImageScale_2160 = 1,
        ImageScale_1080 = 2,
        ImageScale_720 = 3
    };
    Q_ENUM(ImageScale)

    enum class ImageRotate {
        ImageRotate_None = 0,
        ImageRotate_Rot90 = 1,
        ImageRotate_Rot180 = 2,
        ImageRotate_Rot270 = 3
    };
    Q_ENUM(ImageRotate)

    enum class MpdPolicy {
        MpdPolicy_None = 0,
        MpdPolicy_Start = 1,
        MpdPolicy_StartStop = 2
    };
    Q_ENUM(MpdPolicy)

#ifdef USE_SFOS
    static constexpr const char *HW_RELEASE_FILE = "/etc/hw-release";
#endif
    Settings();
#define X(name, getter, setter, key, type, dvalue, restart) \
    type getter() const;                                    \
    void setter(const type &value);
    SETTINGS_PROPERTY_TABLE
#undef X
    QString moduleChecksum(const QString &name) const;
    void setModuleChecksum(const QString &name, const QString &value);
    void setRestartRequired(bool value);
    bool getRestartRequired() const { return m_restartRequired; }
    void setForwardTime(int value);
    int getForwardTime() const;
    void setUseHWVolume(bool value);
    bool getUseHWVolume() const;
    void setLogToFile(bool value);
    bool getLogToFile() const;
    void setAlbumQueryType(int value);
    int getAlbumQueryType() const;
    void setRecQueryType(int value);
    int getRecQueryType() const;
    void setCDirQueryType(int value);
    int getCDirQueryType() const;
    void setPlayMode(int value);
    int getPlayMode() const;
    void setFsapiPin(const QString &value);
    QString prevAppVer() const;
    void setPrevAppVer(const QString &value);
    QString fsapiPin() const;
    void setFavDevices(const QHash<QString, QVariant> &devs);
    QHash<QString, QVariant> getFavDevices() const;
    void setLastDevices(const QHash<QString, QVariant> &devs);
    QHash<QString, QVariant> getLastDevices() const;
    Q_INVOKABLE void addFavDevice(const QString &id);
    void addLastDevice(const QString &id);
    void addLastDevices(const QStringList &ids);
    Q_INVOKABLE void removeFavDevice(const QString &id);
    Q_INVOKABLE void asyncAddFavDevice(const QString &id);
    Q_INVOKABLE void asyncRemoveFavDevice(const QString &id);
    static bool readDeviceXML(const QString &id, QByteArray &xml);
    static bool writeDeviceXML(const QString &id, QString &url);
    QString getRecDir();
    void setRecDir(const QString &value);
    QString getSlidesDir();
    void setSlidesDir(const QString &value);
    QStringList getLastPlaylist() const;
    void setLastPlaylist(const QStringList &value);
    QByteArray getKey();
    QByteArray resetKey();
    Q_INVOKABLE QString getCacheDir() const;
    QString getPlaylistDir() const;
    QString getPrefNetInf() const;
    void setPrefNetInf(const QString &value);
    void setContentDirSupported(bool value);
    bool getContentDirSupported() const;
    void setColorScheme(int value);
    int getColorScheme() const;
    QString mediaServerDevUuid();
    QString prettyName() const;
    Q_INVOKABLE bool isDebug() const;
    Q_INVOKABLE bool isHarbour() const;
    Q_INVOKABLE bool isWayland() const;
    Q_INVOKABLE bool isXcb() const;
    Q_INVOKABLE bool isFlatpak() const;
    Q_INVOKABLE bool isPy() const;
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool hintEnabled(HintType hint) const;
    Q_INVOKABLE void disableHint(HintType hint);
    Q_INVOKABLE void resetHints();
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
    QStringList ytSearchHistory() const;
    Q_INVOKABLE void addYtSearchHistory(const QString &value);
    Q_INVOKABLE void removeYtSearchHistory(const QString &value);
    QStringList radionetSearchHistory() const;
    Q_INVOKABLE void addRadionetSearchHistory(const QString &value);
    Q_INVOKABLE void removeRadionetSearchHistory(const QString &value);
    Q_INVOKABLE QUrl appIcon() const;
    Q_INVOKABLE QStringList qtStyles() const;
    QString pythonChecksum() const;
    void setPythonChecksum(const QString &value);
    CacheType cacheType() const;
    void setCacheType(CacheType value);
    CacheCleaningType cacheCleaningType() const;
    void setCacheCleaningType(CacheCleaningType value);
    YtPreferredType ytPreferredType() const;
    void setYtPreferredType(YtPreferredType value);
    Q_INVOKABLE inline bool sandboxed() const { return m_sandboxed; }
    bool allowNotIsomMp4() const;
    void setAllowNotIsomMp4(bool value);
    static QString videoOrientationStrStatic(
        CasterVideoOrientation orientation);
    Q_INVOKABLE QString
    videoOrientationStr(CasterVideoOrientation orientation) const;
    QString pyPath() const;
    void setPyPath(const QString &value);
#ifdef USE_DESKTOP
    void updateQtStyle(QQmlApplicationEngine *engine);
#endif
    int qtStyleIdx() const;
    void setQtStyleIdx(int value);
    QString qtStyleName() const;
    void setQtStyleName(QString value);
    void setQtStyleAuto(bool value);
    bool qtStyleAuto() const;
    AvNextUriPolicy getAvNextUriPolicy() const;
    void setAvNextUriPolicy(AvNextUriPolicy value);

    // caster

    Q_INVOKABLE void discoverCasterSources();
    QStringList getCasterCams() const;
    QStringList getCasterScreens() const;
    QStringList getCasterMics() const;
    QStringList getCasterPlaybacks() const;
    int getCasterCamIdx() const;
    void setCasterCamIdx(int value);
    int getCasterMicIdx() const;
    void setCasterMicIdx(int value);
    int getCasterScreenIdx() const;
    void setCasterScreenIdx(int value);
    int getCasterPlaybackIdx() const;
    void setCasterPlaybackIdx(int value);
    QString getCasterMic() const;
    void setCasterMic(const QString &name);
    QString getCasterCam() const;
    void setCasterCam(const QString &name);
    QString getCasterScreen() const;
    void setCasterScreen(const QString &name);
    QString getCasterPlayback() const;
    void setCasterPlayback(const QString &name);
    QString casterVideoSourceFriendlyName(const QString &name) const;
    QString casterAudioSourceFriendlyName(const QString &name) const;
    bool casterVideoSourceExists(const QString &name) const;
    bool casterAudioSourceExists(const QString &name) const;

   signals:
#define X(name, getter, setter, key, type, dvalue, restart) \
    void name##Changed();
    SETTINGS_PROPERTY_TABLE
#undef X
    void favDevicesChanged();
    void favDeviceChanged(const QString &id);
    void recDirChanged();
    void slidesDirChanged();
    void lastPlaylistChanged();
    void forwardTimeChanged();
    void imageSupportedChanged();
    void useHWVolumeChanged();
    void prefNetInfChanged();
    void albumQueryTypeChanged();
    void recQueryTypeChanged();
    void cdirQueryTypeChanged();
    void playModeChanged();
    void contentDirSupportedChanged();
    void logToFileChanged();
    void colorSchemeChanged();
    void fsapiPinChanged();
    void bcSearchHistoryChanged();
    void soundcloudSearchHistoryChanged();
    void icecastSearchHistoryChanged();
    void tuneinSearchHistoryChanged();
    void ytSearchHistoryChanged();
    void radionetSearchHistoryChanged();
    void cacheTypeChanged();
    void cacheCleaningTypeChanged();
    void ytPreferredTypeChanged();
    void allowNotIsomMp4Changed();
    void pyPathChanged();
    void prevAppVerChanged();
    void qtStyleChanged();
    void avNextUriPolicyChanged();

    // caster
    void casterSourcesChanged();
    void casterMicChanged();
    void casterScreenChanged();
    void casterCamChanged();
    void casterPlaybackChanged();

    void restartRequiredChanged();

   private:
    inline static const QString settingsFilename =
        QStringLiteral("settings.config");
    inline static const QString defaultQtStyle =
        QStringLiteral("org.kde.desktop");
    inline static const QString defaultQtStyleFallback =
        QStringLiteral("org.kde.breeze");
    static const int maxSearchHistory = 3;

    QString hwName;
    int m_colorScheme = 0;
    bool m_sandboxed = false;
    bool m_restartRequired;
    std::vector<Caster::VideoSourceProps> m_videoSources;
    std::vector<Caster::AudioSourceProps> m_audioSources;
    QStringList m_cams_fn;
    QStringList m_mics_fn;
    QStringList m_screens_fn;
    QStringList m_playbacks_fn;
    QStringList m_cams;
    QStringList m_mics;
    QStringList m_screens;
    QStringList m_playbacks;
    bool m_native_style = false;

#ifdef USE_SFOS
    static QString readHwInfo();
#endif
    static std::pair<int, int> sysVer();
    static QString settingsFilepath();
    static void initOpenUrlMode();
    void updateSandboxStatus();
    static QString casterFriendlyName(const QString &name,
                                      QString &&friendlyName);

    void initLogger() const;
};

#endif  // SETTINGS_H
