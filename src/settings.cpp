/* Copyright (C) 2017-2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "settings.h"

#include <unistd.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTime>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <libupnpp/control/description.hxx>
#include <limits>

#include "directory.h"
#include "info.h"
#include "log.h"
#include "utils.h"

QString Settings::settingsFilepath() {
    QDir confDir{
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)};
    confDir.mkpath(QCoreApplication::organizationName() + QDir::separator() +
                   QCoreApplication::applicationName());
    return confDir.absolutePath() + QDir::separator() +
           QCoreApplication::organizationName() + QDir::separator() +
           QCoreApplication::applicationName() + QDir::separator() +
           settingsFilename;
}

Settings::Settings()
    : QSettings{settingsFilepath(), QSettings::NativeFormat},
#ifdef SAILFISH
      hwName {
    readHwInfo()
}
#else
      hwName {
    QSysInfo::machineHostName()
}
#endif
{
    initOpenUrlMode();
    getCacheDir();
    removeLogFiles();
    qInstallMessageHandler(qtLog);
    ::configureLogToFile(getLogToFile());
    updateSandboxStatus();

    qDebug() << "HW name:" << hwName;
    qDebug() << "app:" << Jupii::ORG << Jupii::APP_ID << Jupii::APP_VERSION;
    qDebug() << "config location:"
             << QStandardPaths::writableLocation(
                    QStandardPaths::ConfigLocation);
    qDebug() << "data location:"
             << QStandardPaths::writableLocation(
                    QStandardPaths::AppDataLocation);
    qDebug() << "cache location:"
             << QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    qDebug() << "settings file:" << fileName();
    qDebug() << "sandboxed:" << m_sandboxed;
}

#ifdef SAILFISH
QString Settings::readHwInfo() {
    QFile f{HW_RELEASE_FILE};
    if (f.open(QIODevice::ReadOnly)) {
        const auto d = f.readAll();
        f.close();
        const auto data = QString::fromUtf8(d).split(QRegExp{"[\r\n]"},
                                                     QString::SkipEmptyParts);
        QRegExp rx("^NAME=\"?([^\"]*)\"?$", Qt::CaseInsensitive);
        for (const auto &line : data)
            if (rx.indexIn(line) != -1) return rx.cap(1);
    } else {
        qWarning() << "Cannot open file:" << f.fileName() << f.errorString();
    }

    return {};
}
#endif

QString Settings::prettyName() const {
    return hwName.isEmpty() ? QString{Jupii::APP_NAME}
                            : QString{"%1 (%2)"}.arg(Jupii::APP_NAME, hwName);
}

void Settings::setLogToFile(bool value) {
    if (getLogToFile() != value) {
        setValue("logtofile", value);
        if (value) {
            qDebug() << "Logging to file enabled";
            ::configureLogToFile(true);
        } else {
            qDebug() << "Logging to file disabled";
            ::configureLogToFile(false);
        }
        emit logToFileChanged();
    }
}

bool Settings::getLogToFile() const {
    return value("logtofile", false).toBool();
}

void Settings::setPort(int value) {
    if (getPort() != value) {
        setValue("port", value);
        emit portChanged();
    }
}

int Settings::getPort() const { return value("port", 9092).toInt(); }

void Settings::setVolStep(int value) {
    if (getVolStep() != value) {
        setValue("volstep", value);
        emit volStepChanged();
    }
}

int Settings::getVolStep() const { return value("volstep", 5).toInt(); }

void Settings::setForwardTime([[maybe_unused]] int value) {
    // disabled option
    /*if (value < 1 || value > 60)
        return; // incorrect value

    if (getForwardTime() != value) {
        setValue("forwardtime", value);
        emit forwardTimeChanged();
    }*/
}

int Settings::getForwardTime() const {
    // disabled option
    // return value("forwardtime", 10).toInt();
    return 10;
}

void Settings::setFavDevices(const QHash<QString, QVariant> &devs) {
    setValue("favdevices", devs);
    emit favDevicesChanged();
}

void Settings::setLastDevices(const QHash<QString, QVariant> &devs) {
    setValue("lastdevices", devs);
}

QHash<QString, QVariant> Settings::getLastDevices() const {
    return value("lastdevices", QHash<QString, QVariant>()).toHash();
}

QString Settings::getCacheDir() const {
    auto path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!QFile::exists(path)) QDir::root().mkpath(path);
    return path;
}

QString Settings::getPlaylistDir() const {
    const auto path =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    if (path.isEmpty()) {
        qWarning() << "MusicLocation dir is empty";
        return path;
    }

    const QString plsDir = "playlists";

    QDir dir(path);
    if (!dir.exists(plsDir)) {
        if (!dir.mkdir(plsDir)) {
            qWarning() << "Unable to create playlist dir";
            return QString();
        }
    }

    return path + "/" + plsDir;
}

void Settings::asyncAddFavDevice(const QString &id) {
    startTask([this, &id]() { addFavDevice(id); });
}

void Settings::asyncRemoveFavDevice(const QString &id) {
    startTask([this, &id]() { removeFavDevice(id); });
}

void Settings::addFavDevice(const QString &id) {
    auto list = getFavDevices();
    if (list.contains(id)) return;

    QString url;
    if (writeDeviceXML(id, url)) {
        list.insert(id, url);
        setFavDevices(list);
        emit favDeviceChanged(id);
    }
}

void Settings::addLastDevice(const QString &id) {
    auto list = getLastDevices();
    if (list.contains(id)) return;

    QString url;
    if (writeDeviceXML(id, url)) {
        list.insert(id, url);
        setLastDevices(list);
    }
}

void Settings::addLastDevices(const QStringList &ids) {
    QHash<QString, QVariant> list;

    for (const auto &id : ids) {
        QString url;
        if (writeDeviceXML(id, url)) list.insert(id, url);
    }

    setLastDevices(list);
}

void Settings::removeFavDevice(const QString &id) {
    auto list = getFavDevices();
    if (list.contains(id)) {
        list.remove(id);
        setFavDevices(list);
        emit favDeviceChanged(id);
    }
}

bool Settings::writeDeviceXML(const QString &id, QString &url) {
    UPnPClient::UPnPDeviceDesc ddesc;

    if (Directory::instance()->getDeviceDesc(id, ddesc)) {
        QString _id(id);
        QString filename = "device-" + _id.replace(':', '-') + ".xml";

        if (Utils::writeToCacheFile(
                filename, QByteArray::fromStdString(ddesc.XMLText), true)) {
            url = QString::fromStdString(ddesc.URLBase);
            return true;
        }
        qWarning() << "Cannot write description file for device" << id;
    } else {
        qWarning() << "Cannot find device description for" << id;
    }

    return false;
}

bool Settings::readDeviceXML(const QString &id, QByteArray &xml) {
    QString _id(id);
    QString filename = "device-" + _id.replace(':', '-') + ".xml";

    if (Utils::readFromCacheFile(filename, xml)) {
        return true;
    }
    qWarning() << "Cannot read description file for device" << id;
    return false;
}

QHash<QString, QVariant> Settings::getFavDevices() const {
    return value("favdevices", QHash<QString, QVariant>()).toHash();
}

void Settings::setLastDir(const QString &value) {
    if (getLastDir() != value) {
        setValue("lastdir", value);
        emit lastDirChanged();
    }
}

QString Settings::getLastDir() const { return value("lastdir", "").toString(); }

void Settings::setRecDir(const QString &value) {
    if (getRecDir() != value) {
        setValue("recdir", QDir(value).exists() ? value : "");
        emit recDirChanged();
    }
}

QString Settings::getRecDir() {
    auto defDir =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation) +
        "/JupiiRecordings";
    auto recDir = value("recdir", defDir).toString();

    QDir d{recDir};

    if (recDir.isEmpty() || !d.exists()) {
        qWarning() << "recording dir doesn't exist:" << recDir;
        d.mkpath(defDir);
        setValue("recdir", defDir);
        emit recDirChanged();
        return defDir;
    }

    return recDir;
}

void Settings::setPrefNetInf(const QString &value) {
    if (getPrefNetInf() != value) {
        setValue("prefnetinf", value);
        emit prefNetInfChanged();
    }
}

QString Settings::getPrefNetInf() const {
#ifdef SAILFISH
    if (isDebug()) return value("prefnetinf", "").toString();
    return {};
#else
    return value("prefnetinf", "").toString();
#endif
}

void Settings::setLastPlaylist(const QStringList &value) {
    if (getLastPlaylist() != value) {
        setValue("lastplaylist", value);
        emit lastPlaylistChanged();
    }
}

QStringList Settings::getLastPlaylist() const {
    return value("lastplaylist").toStringList();
}

void Settings::setShowAllDevices(bool value) {
    if (getShowAllDevices() != value) {
        setValue("showalldevices", value);
        emit showAllDevicesChanged();
    }
}

bool Settings::getShowAllDevices() const {
    return value("showalldevices", false).toBool();
}

void Settings::setScreenSupported([[maybe_unused]] bool value) {
#ifdef SAILFISH
    if (getScreenSupported() != value) {
        setValue("screensupported", value);
        emit screenSupportedChanged();
    }
#endif
}

bool Settings::getScreenSupported() const {
#if defined(SAILFISH) && !defined(QT_DEBUG)
    // Sreen Capture does not work with sandboxing
    return sandboxed() ? false : value("screensupported", false).toBool();
#else
    return true;
#endif
}

void Settings::setScreenCropTo169(int value) {
    // 0 - disabled
    // 1 - scale
    // 2 - crop
    if (getScreenCropTo169() != value) {
        setValue("screencrop169", value);
        emit screenCropTo169Changed();
    }
}

int Settings::getScreenCropTo169() {
    // 0 - disabled
    // 1 - scale
    // 2 - crop
#ifdef SAILFISH
    return value("screencrop169", 1).toInt();
#else
    return value("screencrop169", 2).toInt();
#endif
}

void Settings::setScreenAudio(bool value) {
    if (getScreenAudio() != value) {
        setValue("screenaudio", value);
        emit screenAudioChanged();
    }
}

bool Settings::getScreenAudio() {
#ifdef SAILFISH
    return value("screenaudio", false).toBool();
#else
    return value("screenaudio", true).toBool();
#endif
}

void Settings::setScreenEncoder(const QString &value) {
    if (getScreenEncoder() != value) {
        setValue("screenencoder", value);
        emit screenEncoderChanged();
    }
}

QString Settings::getScreenEncoder() const {
#ifdef SAILFISH
    if (isDebug()) {
        // valid encoders: libx264, libx264rgb, h264_omx
        auto enc_name = value("screenencoder").toString();
        if (enc_name == "libx264" || enc_name == "libx264rgb" ||
            enc_name == "h264_omx")
            return enc_name;
    }

    return {};
#else
    // valid encoders: libx264, libx264rgb, h264_nvenc
    auto enc_name = value("screenencoder").toString();
    if (enc_name == "libx264" || enc_name == "libx264rgb" ||
        enc_name == "h264_nvenc" || enc_name == "h264_omx")
        return enc_name;

    return {};
#endif
}

void Settings::setScreenFramerate(int value) {
    if (getScreenFramerate() != value) {
        setValue("screenframerate", value);
        emit screenFramerateChanged();
    }
}

int Settings::getScreenFramerate() const {
#ifdef SAILFISH
    // default 5 fps
    if (isDebug()) return value("screenframerate", 5).toInt();
    return 5;
#else
    // default 15 fps
    return value("screenframerate", 15).toInt();
#endif
}

void Settings::setScreenQuality(int value) {
    if (getScreenQuality() != value) {
        setValue("screenquality", value);
        emit screenQualityChanged();
    }
}

int Settings::getScreenQuality() const {
    return value("screenquality", 3).toInt();
}

void Settings::setUseHWVolume(bool value) {
#ifdef SAILFISH
    if (getUseHWVolume() != value) {
        setValue("usehwvolume", value);
        emit useHWVolumeChanged();
    }
#else
    Q_UNUSED(value)
#endif
}

bool Settings::getUseHWVolume() const {
#ifdef SAILFISH
    return value("usehwvolume", true).toBool();
#else
    return false;
#endif
}

void Settings::setMicVolume(float value) {
    value = value < 1 ? 1 : value > 100 ? 100 : value;

    if (getMicVolume() > value || getMicVolume() < value) {
        setValue("micvolume", value);
        emit micVolumeChanged();
    }
}

float Settings::getMicVolume() const {
#ifdef SAILFISH
    return value("micvolume", 50.0).toFloat();
#else
    return value("micvolume", 1.0).toFloat();
#endif
}

void Settings::setAudioBoost([[maybe_unused]] float value) {
#ifdef SAILFISH
#else
    value = value < 1 ? 1 : value > 10 ? 10 : value;

    if (getAudioBoost() > value || getAudioBoost() < value) {
        setValue("audioboost", value);
        emit audioBoostChanged();
    }
#endif
}

float Settings::getAudioBoost() const {
#ifdef SAILFISH
    // return value("micvolume", 2.3f).toFloat();
    return 2.3f;
#else
    return value("audioboost", 1.0f).toFloat();
#endif
}

QByteArray Settings::resetKey() {
    QByteArray key;

    // Seed init, needed for key generation
    qsrand(static_cast<uint>(QTime::currentTime().msec()));

    // key is 10 random chars
    for (uint i = 0; i < 10; ++i) key.append(static_cast<char>(qrand()));

    setValue("key", key);

    return key;
}

QByteArray Settings::getKey() {
    QByteArray key = value("key").toByteArray();

    if (key.isEmpty()) {
        key = resetKey();
    }

    return key;
}

void Settings::setRemoteContentMode(RemoteContentMode value) {
    if (!isDebug()) return;

    if (getRemoteContentMode() != value) {
        setValue("remotecontentmode2", static_cast<int>(value));
        emit remoteContentModeChanged();
    }
}

Settings::RemoteContentMode Settings::getRemoteContentMode() const {
    if (!isDebug()) return RemoteContentMode::RemoteContentMode_Proxy_All;

    return static_cast<RemoteContentMode>(
        value("remotecontentmode2",
              static_cast<int>(RemoteContentMode::RemoteContentMode_Proxy_All))
            .toInt());
}

void Settings::setAlbumQueryType(int value) {
    if (getAlbumQueryType() != value) {
        setValue("albumquerytype", value);
        emit albumQueryTypeChanged();
    }
}

int Settings::getAlbumQueryType() const {
    // 0 - by album title
    // 1 - by artist
    return value("albumquerytype", 0).toInt();
}

void Settings::setRecQueryType(int value) {
    if (getRecQueryType() != value) {
        setValue("recquerytype", value);
        emit recQueryTypeChanged();
    }
}

int Settings::getRecQueryType() const {
    // 0 - by rec date
    // 1 - by title
    // 2 - by author
    return value("recquerytype", 0).toInt();
}

void Settings::setCDirQueryType(int value) {
    if (getCDirQueryType() != value) {
        setValue("cdirquerytype", value);
        emit cdirQueryTypeChanged();
    }
}

int Settings::getCDirQueryType() const {
    // 0 - by title
    // 1 - by album
    // 2 - by artist
    // 3 - by track number
    // 4 - by date
    return value("cdirquerytype", 0).toInt();
}

void Settings::setPlayMode(int value) {
    if (getPlayMode() != value) {
        setValue("playmode", value);
        emit playModeChanged();
    }
}

int Settings::getPlayMode() const {
    /*PM_Normal = 0,
    PM_RepeatAll = 1,
    PM_RepeatOne = 2*/
    int pm = value("playmode", 1).toInt();
    return pm == 2 ? pm : 1;
}

QString Settings::mediaServerDevUuid() {
    auto defaultUUID = QUuid::createUuid().toString().mid(1, 36);
    if (!contains("mediaserverdevuuid"))
        setValue("mediaserverdevuuid", defaultUUID);
    return value("mediaserverdevuuid", defaultUUID).toString();
}

void Settings::setContentDirSupported(bool value) {
    if (getContentDirSupported() != value) {
        setValue("contentdirsupported", value);
        emit contentDirSupportedChanged();
    }
}

bool Settings::getContentDirSupported() const {
    return value("contentdirsupported", true).toBool();
}

bool Settings::hintEnabled(HintType hint) const {
    int hints = value("hints", std::numeric_limits<int>::max()).toInt();
    return hints & static_cast<int>(hint);
}

void Settings::disableHint(HintType hint) {
    int hints = value("hints", std::numeric_limits<int>::max()).toInt();
    hints &= ~static_cast<int>(hint);
    setValue("hints", hints);
}

void Settings::resetHints() {
    setValue("hints", std::numeric_limits<int>::max());
}

void Settings::setColorScheme(int value) {
    if (m_colorScheme != value) {
        m_colorScheme = value;
        emit colorSchemeChanged();
    }
}

int Settings::getColorScheme() const { return m_colorScheme; }

void Settings::reset() {
    qDebug() << "Resetting settings";

    if (!QFile::remove(fileName())) {
        qWarning() << "Cannot remove file:" << fileName();
    }
}

void Settings::setFsapiPin(const QString &value) {
    if (fsapiPin() != value) {
        setValue("fsapipin", value);
        emit fsapiPinChanged();
    }
}

QString Settings::fsapiPin() const {
    return value("fsapipin", "1234").toString();
}

std::pair<int, int> Settings::sysVer() {
    const auto ver = QSysInfo::productVersion().split(".");
    if (ver.size() > 1) return {ver.value(0).toInt(), ver.value(1).toInt()};
    return {0, 0};  // unknown version
}

QStringList Settings::bcSearchHistory() const {
    return value("bcsearchhistory", {}).toStringList();
}

void Settings::addBcSearchHistory(const QString &value) {
    auto v = value.toLower();
    auto list = bcSearchHistory();
    list.removeOne(v);
    list.push_front(v);
    if (list.size() > maxSearchHistory) list.removeLast();
    setValue("bcsearchhistory", list);
    emit bcSearchHistoryChanged();
}

void Settings::removeBcSearchHistory(const QString &value) {
    auto list = bcSearchHistory();
    if (list.removeOne(value.toLower())) {
        setValue("bcsearchhistory", list);
        emit bcSearchHistoryChanged();
    }
}

QStringList Settings::soundcloudSearchHistory() const {
    return value("soundcloudsearchhistory", {}).toStringList();
}

void Settings::addSoundcloudSearchHistory(const QString &value) {
    auto v = value.toLower();
    auto list = soundcloudSearchHistory();
    list.removeOne(v);
    list.push_front(v);
    if (list.size() > maxSearchHistory) list.removeLast();
    setValue("soundcloudsearchhistory", list);
    emit soundcloudSearchHistoryChanged();
}

void Settings::removeSoundcloudSearchHistory(const QString &value) {
    auto list = soundcloudSearchHistory();
    if (list.removeOne(value.toLower())) {
        setValue("soundcloudsearchhistory", list);
        emit soundcloudSearchHistoryChanged();
    }
}

QStringList Settings::icecastSearchHistory() const {
    return value("icecastsearchhistory", {}).toStringList();
}

void Settings::addIcecastSearchHistory(const QString &value) {
    auto v = value.toLower();
    auto list = icecastSearchHistory();
    list.removeOne(v);
    list.push_front(v);
    if (list.size() > maxSearchHistory) list.removeLast();
    setValue("icecastsearchhistory", list);
    emit icecastSearchHistoryChanged();
}

void Settings::removeIcecastSearchHistory(const QString &value) {
    auto list = icecastSearchHistory();
    if (list.removeOne(value.toLower())) {
        setValue("icecastsearchhistory", list);
        emit icecastSearchHistoryChanged();
    }
}

QStringList Settings::tuneinSearchHistory() const {
    return value("tuneinsearchhistory", {}).toStringList();
}

void Settings::addTuneinSearchHistory(const QString &value) {
    auto v = value.toLower();
    auto list = tuneinSearchHistory();
    list.removeOne(v);
    list.push_front(v);
    if (list.size() > maxSearchHistory) list.removeLast();
    setValue("tuneinsearchhistory", list);
    emit tuneinSearchHistoryChanged();
}

void Settings::removeTuneinSearchHistory(const QString &value) {
    auto list = tuneinSearchHistory();
    if (list.removeOne(value.toLower())) {
        setValue("tuneinsearchhistory", list);
        emit tuneinSearchHistoryChanged();
    }
}

QStringList Settings::ytSearchHistory() const {
    return value("ytsearchhistory", {}).toStringList();
}

void Settings::addYtSearchHistory(const QString &value) {
    auto v = value.toLower();
    auto list = ytSearchHistory();
    list.removeOne(v);
    list.push_front(v);
    if (list.size() > maxSearchHistory) list.removeLast();
    setValue("ytsearchhistory", list);
    emit ytSearchHistoryChanged();
}

void Settings::removeYtSearchHistory(const QString &value) {
    auto list = ytSearchHistory();
    if (list.removeOne(value.toLower())) {
        setValue("ytsearchhistory", list);
        emit ytSearchHistoryChanged();
    }
}

static inline QFile desktopFile() {
    return QFile{QString{"%1/%2-open-url.desktop"}.arg(
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
        Jupii::APP_BINARY_ID)};
}

void Settings::initOpenUrlMode() {
    const QDir dbusPath{
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        "/dbus-1/services"};
    if (!dbusPath.exists()) dbusPath.mkpath(dbusPath.absolutePath());
    QFile dbusServiceFile{dbusPath.absolutePath() + "/" + Jupii::DBUS_SERVICE +
                          ".service"};
    if (dbusServiceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream s{&dbusServiceFile};
        s.setCodec("UTF-8");
        s << "[D-BUS Service]\n";
        s << "Name=" << Jupii::DBUS_SERVICE << "\n";
        s << "Exec=/usr/bin/invoker --type=silica-qt5 --single-instance "
          << Jupii::APP_BINARY_ID << "\n";
        s.flush();
    }

    desktopFile().remove();
}

QUrl Settings::appIcon() const {
    return QUrl::fromLocalFile(
        QStringLiteral("/usr/share/icons/hicolor/172x172/apps/%1.png")
            .arg(Jupii::APP_BINARY_ID));
}

void Settings::setPythonChecksum(const QString &value) {
    if (value != pythonChecksum()) {
        setValue("pythonchecksum", value);
    }
}

QString Settings::pythonChecksum() const {
    return value("pythonchecksum").toString();
}

void Settings::setCacheType(CacheType value) {
    if (!isDebug()) return;

    if (value != cacheType()) {
        setValue("cachetype", static_cast<int>(value));
        emit cacheTypeChanged();
    }
}

Settings::CacheType Settings::cacheType() const {
    if (!isDebug()) return CacheType::Cache_Auto;

    return static_cast<CacheType>(
        value("cachetype", static_cast<int>(CacheType::Cache_Auto)).toInt());
}

void Settings::setCacheCleaningType(CacheCleaningType value) {
    if (!isDebug()) return;

    if (value != cacheCleaningType()) {
        setValue("cachecleaningtype", static_cast<int>(value));
        emit cacheCleaningTypeChanged();
    }
}

Settings::CacheCleaningType Settings::cacheCleaningType() const {
    if (!isDebug()) return CacheCleaningType::CacheCleaning_Auto;

    return static_cast<CacheCleaningType>(
        value("cachecleaningtype",
              static_cast<int>(CacheCleaningType::CacheCleaning_Auto))
            .toInt());
}

void Settings::setYtPreferredType(YtPreferredType value) {
    if (value != ytPreferredType()) {
        setValue("ytpreferredtype", static_cast<int>(value));
        emit ytPreferredTypeChanged();
    }
}

Settings::YtPreferredType Settings::ytPreferredType() const {
    return static_cast<YtPreferredType>(
        value("ytpreferredtype",
              static_cast<int>(YtPreferredType::YtPreferredType_Audio))
            .toInt());
}

void Settings::updateSandboxStatus() {
    auto path = QStringLiteral("/proc/%1/stat").arg(getppid());

    QFile pf{path};
    if (!pf.open(QIODevice::ReadOnly)) {
        qWarning() << "cannot open:" << path;
        return;
    }

    m_sandboxed = pf.readAll().contains("firejail");
}

void Settings::setAllowNotIsomMp4(bool value) {
    if (value != allowNotIsomMp4()) {
        setValue("allownotisommp4", value);
        emit allowNotIsomMp4Changed();
    }
}

bool Settings::allowNotIsomMp4() const {
    return value("allownotisommp4", true).toBool();
}

bool Settings::isDebug() const {
#ifdef QT_DEBUG
    return true;
#else
    return false;
#endif
}

bool Settings::isHarbour() const {
#ifdef HARBOUR
    return true;
#else
    return false;
#endif
}
