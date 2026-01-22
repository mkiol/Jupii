/* Copyright (C) 2017-2026 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "settings.h"

#include <unistd.h>

#ifdef USE_DESKTOP
#include <QQuickStyle>
#endif
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTime>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <libupnpp/control/description.hxx>
#include <limits>
#include <type_traits>

#include "config.h"
#include "directory.h"
#include "logger.hpp"
#include "module_tools.hpp"
#include "utils.h"

QDebug &operator<<(QDebug &dbg, Settings::RelayPolicy policy) {
    switch (policy) {
        case Settings::RelayPolicy::RelayPolicy_Auto:
            dbg << "auto";
            break;
        case Settings::RelayPolicy::RelayPolicy_AlwaysRelay:
            dbg << "always-relay";
            break;
        case Settings::RelayPolicy::RelayPolicy_DontRelayUpnp:
            dbg << "dont-relay-upnp";
            break;
    }

    return dbg;
}

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

void Settings::initLogger() const {
    if (getLogToFile() && !JupiiLogger::logToFileEnabled()) {
        JupiiLogger::init(JupiiLogger::LogType::Trace,
                          getLogToFile()
                              ? QDir{QStandardPaths::writableLocation(
                                         QStandardPaths::CacheLocation)}
                                    .filePath(QStringLiteral("jupii.log"))
                                    .toStdString()
                              : std::string{});
    }
}

Settings::Settings()
    : QSettings{settingsFilepath(), QSettings::NativeFormat},
#ifdef USE_SFOS
      hwName{readHwInfo()}
#else
      hwName{QSysInfo::machineHostName()}
#endif
{
    initLogger();

    initOpenUrlMode();

    getCacheDir();

    // remove qml cache
    QDir{QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
         "/qmlcache"}
        .removeRecursively();

    updateSandboxStatus();

    qDebug() << "HW name:" << hwName;
    qDebug() << "app:" << APP_ORG << APP_ID << APP_VERSION;
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

    discoverCasterSources();
}

#ifdef USE_SFOS
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
        qWarning() << "failed to open file:" << f.fileName() << f.errorString();
    }

    return {};
}
#endif

QString Settings::prettyName() const {
    return hwName.isEmpty() ? QStringLiteral(APP_NAME)
                            : QStringLiteral("%1 (%2)").arg(APP_NAME, hwName);
}

void Settings::setLogToFile(bool value) {
    if (getLogToFile() != value) {
        setValue(QStringLiteral("logtofile"), value);
        emit logToFileChanged();
        setRestartRequired(true);
    }
}

bool Settings::getLogToFile() const {
    return value(QStringLiteral("logtofile"), false).toBool();
}

#define X(name, getter, setter, key, type, dvalue, restart)                    \
    type Settings::getter() const {                                            \
        return [&](auto v) {                                                   \
            if constexpr (std::is_enum_v<decltype(v)>) {                       \
                return static_cast<type>(                                      \
                    value(QStringLiteral(#key), static_cast<int>(v)).toInt()); \
            } else {                                                           \
                return qvariant_cast<type>(value(QStringLiteral(#key), v));    \
            }                                                                  \
        }(dvalue);                                                             \
    }                                                                          \
    void Settings::setter(const type &value) {                                 \
        if (getter() != value) {                                               \
            [&](auto v) {                                                      \
                if constexpr (std::is_enum_v<decltype(v)>) {                   \
                    setValue(QStringLiteral(#key), static_cast<int>(v));       \
                } else {                                                       \
                    setValue(QStringLiteral(#key), v);                         \
                }                                                              \
                emit name##Changed();                                          \
                if (restart) setRestartRequired(true);                         \
            }(value);                                                          \
        }                                                                      \
    }
SETTINGS_PROPERTY_TABLE
#undef X

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
    auto path = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    if (path.isEmpty()) {
        qWarning() << "music location dir is empty";
        return path;
    }

    QString plsDir = QStringLiteral("playlists");

    QDir dir{path};
    if (!dir.exists(plsDir)) {
        if (!dir.mkdir(plsDir)) {
            qWarning() << "unable to create playlist dir";
            return {};
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

void Settings::setRecDir(const QString &value) {
    if (getRecDir() != value) {
        setValue("recdir", QDir(value).exists() ? value : "");
        emit recDirChanged();
    }
}

QString Settings::getRecDir() {
    auto defDir =
        QDir{QStandardPaths::writableLocation(QStandardPaths::MusicLocation)}
            .absoluteFilePath(QStringLiteral("JupiiRecordings"));
    auto recDir = value("recdir", defDir).toString();

    QDir d{recDir};

    if (recDir.isEmpty() || !d.exists()) {
        d.mkpath(defDir);
        setValue("recdir", defDir);
        emit recDirChanged();
        return defDir;
    }

    return recDir;
}

void Settings::setSlidesDir(const QString &value) {
    if (getRecDir() != value) {
        setValue("slidesdir", QDir(value).exists() ? value : "");
        emit recDirChanged();
    }
}

QString Settings::getSlidesDir() {
    auto defDir =
        QDir{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}
            .absoluteFilePath(QStringLiteral("slides"));
    auto slidesDir = value("slidesdir", defDir).toString();

    QDir d{slidesDir};

    if (slidesDir.isEmpty() || !d.exists()) {
        d.mkpath(defDir);
        setValue("slidesdir", defDir);
        emit slidesDirChanged();
        return defDir;
    }

    return slidesDir;
}

void Settings::setPrefNetInf(const QString &value) {
    if (getPrefNetInf() != value) {
        setValue("prefnetinf", value);
        emit prefNetInfChanged();
        setRestartRequired(true);
    }
}

QString Settings::getPrefNetInf() const {
#ifdef USE_SFOS
    return value("prefnetinf", "").toString();
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

void Settings::setUseHWVolume(bool value) {
#ifdef USE_SFOS
    if (getUseHWVolume() != value) {
        setValue("usehwvolume", value);
        emit useHWVolumeChanged();
    }
#else
    Q_UNUSED(value)
#endif
}

bool Settings::getUseHWVolume() const {
#ifdef USE_SFOS
    return value("usehwvolume", true).toBool();
#else
    return false;
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

    setRestartRequired(true);
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

QStringList Settings::radionetSearchHistory() const {
    return value(QStringLiteral("radionetsearchhistory"), {}).toStringList();
}

void Settings::addRadionetSearchHistory(const QString &value) {
    auto v = value.toLower();
    auto list = radionetSearchHistory();
    list.removeOne(v);
    list.push_front(v);
    if (list.size() > maxSearchHistory) list.removeLast();
    setValue(QStringLiteral("radionetsearchhistory"), list);
    emit radionetSearchHistoryChanged();
}

void Settings::removeRadionetSearchHistory(const QString &value) {
    auto list = radionetSearchHistory();
    if (list.removeOne(value.toLower())) {
        setValue(QStringLiteral("radionetsearchhistory"), list);
        emit radionetSearchHistoryChanged();
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
        APP_BINARY_ID)};
}

void Settings::initOpenUrlMode() {
    const QDir dbusPath{
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        "/dbus-1/services"};
    if (!dbusPath.exists()) dbusPath.mkpath(dbusPath.absolutePath());
    QFile dbusServiceFile{dbusPath.absolutePath() + "/" + APP_DBUS_APP_SERVICE +
                          ".service"};
    if (dbusServiceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream s{&dbusServiceFile};
        s.setCodec("UTF-8");
        s << "[D-BUS Service]\n";
        s << "Name=" << APP_DBUS_APP_SERVICE << "\n";
        s << "Exec=/usr/bin/invoker --type=silica-qt5 --single-instance "
          << APP_BINARY_ID << "\n";
        s.flush();
    }

    desktopFile().remove();
}

QUrl Settings::appIcon() const {
    return QUrl::fromLocalFile(
        QStringLiteral("/usr/share/icons/hicolor/172x172/apps/%1.png")
            .arg(APP_BINARY_ID));
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
        setRestartRequired(true);
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
        setRestartRequired(true);
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
#ifdef DEBUG
    return true;
#else
    return false;
#endif
}

bool Settings::isHarbour() const {
#ifdef USE_SFOS_HARBOUR
    return true;
#else
    return false;
#endif
}

bool Settings::isFlatpak() const {
#ifdef USE_FLATPAK
    return true;
#endif
    return false;
}

bool Settings::isPy() const {
#ifdef USE_PY
    return true;
#endif
    return false;
}

bool Settings::isWayland() const {
    return QGuiApplication::platformName() == "wayland";
}

bool Settings::isXcb() const {
    return QGuiApplication::platformName() == "xcb";
}

QString Settings::getCasterMic() const {
    auto v = value(QStringLiteral("caster_mic"), QString{}).toString();
    if (v.isEmpty() || !m_mics.contains(v))
        return m_mics.empty() ? QString{} : m_mics.front();
    return v;
}

void Settings::setCasterMic(const QString &name) {
    if (name != getCasterMic()) {
        setValue(QStringLiteral("caster_mic"), name);
        emit casterMicChanged();
    }
}

QString Settings::getCasterCam() const {
    auto v = value(QStringLiteral("caster_cam"), QString{}).toString();
    if (v.isEmpty() || !m_cams.contains(v))
        return m_cams.empty() ? QString{} : m_cams.front();
    return v;
}

void Settings::setCasterCam(const QString &name) {
    if (name != getCasterCam()) {
        setValue(QStringLiteral("caster_cam"), name);
        emit casterCamChanged();
    }
}

QString Settings::getCasterScreen() const {
    auto v = value(QStringLiteral("caster_screen"), QString{}).toString();
    if (v.isEmpty() || !m_screens.contains(v))
        return m_screens.empty() ? QString{} : m_screens.front();
    return v;
}

void Settings::setCasterScreen(const QString &name) {
    if (name != getCasterScreen()) {
        setValue(QStringLiteral("caster_screen"), name);
        emit casterScreenChanged();
    }
}

QString Settings::getCasterPlayback() const {
    auto v = value(QStringLiteral("caster_playback"), QString{}).toString();
    if (v.isEmpty() || !m_playbacks.contains(v))
        return m_playbacks.empty() ? QString{} : m_playbacks.front();
    return v;
}

void Settings::setCasterPlayback(const QString &name) {
    if (name != getCasterPlayback()) {
        setValue(QStringLiteral("caster_playback"), name);
        emit casterPlaybackChanged();
    }
}

QStringList Settings::getCasterCams() const { return m_cams_fn; }

QStringList Settings::getCasterScreens() const { return m_screens_fn; }

QStringList Settings::getCasterMics() const { return m_mics_fn; }

QStringList Settings::getCasterPlaybacks() const { return m_playbacks_fn; }

QString Settings::casterFriendlyName([[maybe_unused]] const QString &name,
                                     QString &&friendlyName) {
#ifdef USE_SFOS
    if (name == "mic") return tr("Built-in microphone");
    if (name == "playback") return tr("Audio capture");
    if (name == "screen" || name == "screen-rotate") return tr("Screen");
    if (name == "cam-raw-back" || name == "cam-raw-back-rotate")
        return tr("Back camera");
    if (name == "cam-raw-front" || name == "cam-raw-front-rotate")
        return tr("Front camera");
#endif
    return std::move(friendlyName);
}

void Settings::discoverCasterSources() {
    m_cams.clear();
    m_cams_fn.clear();
    m_screens.clear();
    m_screens_fn.clear();
    m_mics.clear();
    m_mics_fn.clear();
    m_playbacks.clear();
    m_playbacks_fn.clear();

    try {
        uint32_t videoFlags = Caster::OptionsFlags::V4l2VideoSources |
                              Caster::OptionsFlags::X11CaptureVideoSources |
                              Caster::OptionsFlags::DroidCamRawVideoSources |
                              Caster::OptionsFlags::LipstickCaptureVideoSources;
        uint32_t audioFlags = Caster::OptionsFlags::AllAudioSources;
#ifdef USE_DESKTOP
        // playback capture is broken in pipe-wire
        audioFlags &= ~Caster::OptionsFlags::PaPlaybackAudioSources;
#endif
        m_videoSources = Caster::videoSources(videoFlags);
        m_audioSources = Caster::audioSources(audioFlags);
    } catch (const std::runtime_error &err) {
        qWarning() << "caster error:" << err.what();
    }

    for (const auto &s : m_audioSources) {
        auto n = QString::fromStdString(s.name);
        if (n.startsWith(QStringLiteral("mic"))) {
            m_mics.push_back(n);
            m_mics_fn.push_back(
                casterFriendlyName(n, QString::fromStdString(s.friendlyName)));
        } else if (n.startsWith(QStringLiteral("playback")) ||
                   n.startsWith(QStringLiteral("monitor"))) {
            m_playbacks.push_back(n);
            m_playbacks_fn.push_back(
                casterFriendlyName(n, QString::fromStdString(s.friendlyName)));
        }
    }

    for (const auto &s : m_videoSources) {
        auto n = QString::fromStdString(s.name);
        if (n.startsWith(QStringLiteral("screen")) &&
            !n.endsWith(QStringLiteral("-rotate"))) {
            qDebug() << "screen:" << n;
            m_screens.push_back(n);
            m_screens_fn.push_back(
                casterFriendlyName(n, QString::fromStdString(s.friendlyName)));
        } else if (n.startsWith(QStringLiteral("cam")) &&
                   !n.endsWith(QStringLiteral("-rotate"))) {
            m_cams.push_back(n);
            m_cams_fn.push_back(
                casterFriendlyName(n, QString::fromStdString(s.friendlyName)));
        }
    }

    emit casterSourcesChanged();

    if (!casterVideoSourceExists(getCasterScreen()))
        setCasterScreen(m_screens.empty() ? QString{} : m_screens.front());
    if (!casterVideoSourceExists(getCasterCam()))
        setCasterCam(m_cams.empty() ? QString{} : m_cams.front());
    if (!casterAudioSourceExists(getCasterMic()))
        setCasterMic(m_mics.empty() ? QString{} : m_mics.front());
    if (!casterAudioSourceExists(getCasterPlayback()))
        setCasterPlayback(m_playbacks.empty() ? QString{}
                                              : m_playbacks.front());

    emit casterMicChanged();
    emit casterScreenChanged();
    emit casterCamChanged();
    emit casterPlaybackChanged();
}

int Settings::getCasterCamIdx() const {
    auto it = std::find(m_cams.cbegin(), m_cams.cend(), getCasterCam());
    if (it == m_cams.cend()) return 0;

    return static_cast<int>(std::distance(m_cams.cbegin(), it));
}

void Settings::setCasterCamIdx(int value) {
    auto idx = getCasterCamIdx();
    if (value == idx || value > m_cams.size() || value < 0) return;

    setCasterCam(m_cams.at(value));
}

int Settings::getCasterMicIdx() const {
    auto it = std::find(m_mics.cbegin(), m_mics.cend(), getCasterMic());
    if (it == m_mics.cend()) return 0;

    return static_cast<int>(std::distance(m_mics.cbegin(), it));
}

void Settings::setCasterMicIdx(int value) {
    auto idx = getCasterMicIdx();
    if (value == idx || value > m_mics.size() || value < 0) return;

    setCasterMic(m_mics.at(value));
}

int Settings::getCasterScreenIdx() const {
    auto it =
        std::find(m_screens.cbegin(), m_screens.cend(), getCasterScreen());
    if (it == m_screens.cend()) return 0;

    return static_cast<int>(std::distance(m_screens.cbegin(), it));
}

void Settings::setCasterScreenIdx(int value) {
    auto idx = getCasterScreenIdx();
    if (value == idx || value > m_screens.size() || value < 0) return;

    setCasterScreen(m_screens.at(value));
}

int Settings::getCasterPlaybackIdx() const {
    auto it = std::find(m_playbacks.cbegin(), m_playbacks.cend(),
                        getCasterPlayback());
    if (it == m_playbacks.cend()) return 0;

    return static_cast<int>(std::distance(m_playbacks.cbegin(), it));
}

void Settings::setCasterPlaybackIdx(int value) {
    auto idx = getCasterPlaybackIdx();
    if (value == idx || value > m_playbacks.size() || value < 0) return;

    setCasterPlayback(m_playbacks.at(value));
}

QString Settings::casterVideoSourceFriendlyName(const QString &name) const {
    auto it = std::find_if(m_videoSources.cbegin(), m_videoSources.cend(),
                           [name = name.toStdString()](const auto &props) {
                               return props.name == name;
                           });
    if (it == m_videoSources.cend()) return {};

    return casterFriendlyName(QString::fromStdString(it->name),
                              QString::fromStdString(it->friendlyName));
}

QString Settings::casterAudioSourceFriendlyName(const QString &name) const {
    auto it = std::find_if(m_audioSources.cbegin(), m_audioSources.cend(),
                           [name = name.toStdString()](const auto &props) {
                               return props.name == name;
                           });
    if (it == m_audioSources.cend()) return {};

    return casterFriendlyName(QString::fromStdString(it->name),
                              QString::fromStdString(it->friendlyName));
}

bool Settings::casterVideoSourceExists(const QString &name) const {
    auto it = std::find_if(m_videoSources.cbegin(), m_videoSources.cend(),
                           [name = name.toStdString()](const auto &props) {
                               return props.name == name;
                           });
    return it != m_videoSources.cend();
}

bool Settings::casterAudioSourceExists(const QString &name) const {
    auto it = std::find_if(m_audioSources.cbegin(), m_audioSources.cend(),
                           [name = name.toStdString()](const auto &props) {
                               return props.name == name;
                           });
    return it != m_audioSources.cend();
}

void Settings::setRestartRequired(bool value) {
    if (m_restartRequired != value) {
        m_restartRequired = value;
        emit restartRequiredChanged();
    }
}

QString Settings::videoOrientationStrStatic(
    CasterVideoOrientation orientation) {
    switch (orientation) {
        case CasterVideoOrientation::CasterVideoOrientation_Auto:
            return tr("Auto");
        case CasterVideoOrientation::CasterVideoOrientation_Portrait:
            return tr("Portrait");
        case CasterVideoOrientation::CasterVideoOrientation_InvertedPortrait:
            return tr("Inverted portrait");
        case CasterVideoOrientation::CasterVideoOrientation_Landscape:
            return tr("Landscape");
        case CasterVideoOrientation::CasterVideoOrientation_InvertedLandscape:
            return tr("Inverted landscape");
    }

    return {};
}

QString Settings::videoOrientationStr(
    CasterVideoOrientation orientation) const {
    return videoOrientationStrStatic(orientation);
}

QString Settings::pyPath() const {
    return value(QStringLiteral("py_path"), {}).toString();
}

void Settings::setPyPath(const QString &value) {
    if (pyPath() != value) {
        setValue(QStringLiteral("py_path"), value);
        emit pyPathChanged();
        setRestartRequired(true);
    }
}

QString Settings::moduleChecksum(const QString &name) const {
    return value(QStringLiteral("service/module_%1_checksum").arg(name))
        .toString();
}

void Settings::setModuleChecksum(const QString &name, const QString &value) {
    if (value != moduleChecksum(name)) {
        setValue(QStringLiteral("service/module_%1_checksum").arg(name), value);
    }
}

QString Settings::prevAppVer() const {
    return value(QStringLiteral("prev_app_ver")).toString();
}

void Settings::setPrevAppVer(const QString &value) {
    if (prevAppVer() != value) {
        setValue(QStringLiteral("prev_app_ver"), value);
        emit prevAppVerChanged();
    }
}

Settings::AvNextUriPolicy Settings::getAvNextUriPolicy() const {
    return static_cast<AvNextUriPolicy>(
        value(QStringLiteral("av_next_uri_policy"),
              static_cast<int>(AvNextUriPolicy::AvNextUriPolicy_Auto))
            .toInt());
}

void Settings::setAvNextUriPolicy(AvNextUriPolicy value) {
    if (getAvNextUriPolicy() != value) {
        setValue(QStringLiteral("av_next_uri_policy"), static_cast<int>(value));
        emit avNextUriPolicyChanged();
    }
}

bool Settings::qtStyleAuto() const {
    return value(QStringLiteral("qt_style_auto"), true).toBool();
}

void Settings::setQtStyleAuto(bool value) {
    if (qtStyleAuto() != value) {
        setValue(QStringLiteral("qt_style_auto"), value);
        emit qtStyleChanged();
        setRestartRequired(true);
    }
}

QStringList Settings::qtStyles() const {
#ifdef USE_DESKTOP
    auto styles = QQuickStyle::availableStyles();
    styles.append(tr("Don't force any style"));
    return styles;
#else
    return {};
#endif
}

int Settings::qtStyleIdx() const {
#ifdef USE_DESKTOP
    auto name = qtStyleName();

    auto styles = QQuickStyle::availableStyles();

    if (name.isEmpty()) return styles.size();

    return styles.indexOf(name);
#endif
    return -1;
}

void Settings::setQtStyleIdx([[maybe_unused]] int value) {
#ifdef USE_DESKTOP
    auto styles = QQuickStyle::availableStyles();

    if (value < 0 || value >= styles.size()) {
        setQtStyleName({});
        return;
    }

    setQtStyleName(styles.at(value));
#endif
}

QString Settings::qtStyleName() const {
#ifdef USE_DESKTOP
    auto name =
        value(QStringLiteral("qt_style_name"), defaultQtStyle).toString();

    if (!QQuickStyle::availableStyles().contains(name)) return {};

    return name;
#else
    return {};
#endif
}

void Settings::setQtStyleName([[maybe_unused]] QString name) {
#ifdef USE_DESKTOP
    if (!QQuickStyle::availableStyles().contains(name)) name.clear();

    if (qtStyleName() != name) {
        setValue(QStringLiteral("qt_style_name"), name);
        emit qtStyleChanged();
        setRestartRequired(true);
    }
#endif
}

#ifdef USE_DESKTOP
static bool useDefaultQtStyle() {
    const auto *desk_name_str = getenv("XDG_CURRENT_DESKTOP");
    if (!desk_name_str) {
        qDebug() << "no XDG_CURRENT_DESKTOP";
        return false;
    }

    qDebug() << "XDG_CURRENT_DESKTOP:" << desk_name_str;

    QString desk_name{desk_name_str};

    return desk_name.contains("KDE") || desk_name.contains("XFCE");
}

void Settings::updateQtStyle(QQmlApplicationEngine *engine) {
    if (auto prefix = module_tools::path_to_dir_for_path(
            QStringLiteral("lib"), QStringLiteral("plugins"));
        !prefix.isEmpty()) {
        QCoreApplication::addLibraryPath(
            QStringLiteral("%1/plugins").arg(prefix));
    }

    if (auto prefix = module_tools::path_to_dir_for_path(QStringLiteral("lib"),
                                                         QStringLiteral("qml"));
        !prefix.isEmpty()) {
        engine->addImportPath(QStringLiteral("%1/qml").arg(prefix));
    }

    if (auto prefix = module_tools::path_to_dir_for_path(
            QStringLiteral("lib"), QStringLiteral("qml/QtQuick/Controls.2"));
        !prefix.isEmpty()) {
        QQuickStyle::addStylePath(
            QStringLiteral("%1/qml/QtQuick/Controls.2").arg(prefix));
    }

    auto styles = QQuickStyle::availableStyles();

    qDebug() << "available styles:" << styles;
    qDebug() << "style paths:" << QQuickStyle::stylePathList();
    qDebug() << "import paths:" << engine->importPathList();
    qDebug() << "library paths:" << QCoreApplication::libraryPaths();

    QString style;

    if (qtStyleAuto()) {
        qDebug() << "using auto qt style";

        if (styles.contains(defaultQtStyleFallback)) {
            style = useDefaultQtStyle() && styles.contains(defaultQtStyle)
                        ? defaultQtStyle
                        : defaultQtStyleFallback;
        } else if (styles.contains(defaultQtStyle)) {
            style = defaultQtStyle;
        } else {
            qWarning() << "default qt style not found";
        }
    } else {
        auto idx = qtStyleIdx();

        if (idx >= 0 && idx < styles.size()) style = styles.at(idx);

        if (!styles.contains(style)) {
            qWarning() << "qt style not found:" << style;
            style.clear();
        }
    }

    if (style.isEmpty()) {
        qDebug() << "don't forcing any qt style";
        m_native_style = true;
    } else {
        qDebug() << "switching to style:" << style;

        QQuickStyle::setStyle(style);

        m_native_style = style == defaultQtStyle;
    }
}
#endif
