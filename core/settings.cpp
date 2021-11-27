/* Copyright (C) 2017 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QDebug>
#include <QByteArray>
#include <QStandardPaths>
#include <QTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QSysInfo>
#include <QFile>
#include <QSysInfo>
#include <QProcess>
#include <QTimer>
#include <limits>

#include "settings.h"
#include "directory.h"
#include "utils.h"
#include "info.h"
#include "log.h"

#include <libupnpp/control/description.hxx>

Settings* Settings::inst = nullptr;

Settings::Settings(QObject *parent) :
    QObject{parent},
    TaskExecutor{parent},
    settings{parent},
#ifdef SAILFISH
    hwName{readHwInfo()}
#else
    hwName{QSysInfo::machineHostName()}
#endif
{
    removeLogFiles();
    if (getLogToFile()) qInstallMessageHandler(qtLog);

#ifdef SAILFISH
    initOpenUrlMode();
#endif

    qDebug() << "HW name:" << hwName;
}

bool Settings::isDebug() const
{
#ifdef QT_DEBUG
    return true;
#else
    return false;
#endif
}

#ifdef SAILFISH
QString Settings::readHwInfo()
{
    QFile f{HW_RELEASE_FILE};
    if (f.open(QIODevice::ReadOnly)) {
        const auto d = f.readAll();
        f.close();
        const auto data = QString::fromUtf8(d).split(QRegExp{"[\r\n]"}, QString::SkipEmptyParts);
        QRegExp rx("^NAME=\"?([^\"]*)\"?$", Qt::CaseInsensitive);
        for (const auto &line : data) if (rx.indexIn(line) != -1) return rx.cap(1);
    } else {
        qWarning() << "Cannot open file:" << f.fileName() << f.errorString();
    }

    return {};
}
#endif

Settings* Settings::instance()
{
    if (Settings::inst == nullptr) {
        Settings::inst = new Settings();
    }

    return Settings::inst;
}

QString Settings::prettyName() const
{
    return hwName.isEmpty() ? QString{Jupii::APP_NAME} :
                            QString{"%1 (%2)"}.arg(Jupii::APP_NAME, hwName);
}

void Settings::setLogToFile(bool value)
{
    if (getLogToFile() != value) {
        settings.setValue("logtofile", value);
        if (value) {
            qDebug() << "Logging to file enabled";
            qInstallMessageHandler(qtLog);
            qDebug() << "Logging to file enabled";
        } else {
            qDebug() << "Logging to file disabled";
            qInstallMessageHandler(nullptr);
            qDebug() << "Logging to file disabled";
        }
        emit logToFileChanged();
    }
}

bool Settings::getLogToFile() const
{
    return settings.value("logtofile", false).toBool();
}

void Settings::setPort(int value)
{
    if (getPort() != value) {
        settings.setValue("port", value);
        emit portChanged();
    }
}

int Settings::getPort() const
{
    return settings.value("port", 9092).toInt();
}

void Settings::setVolStep(int value)
{
    if (getVolStep() != value) {
        settings.setValue("volstep", value);
        emit volStepChanged();
    }
}

int Settings::getVolStep() const
{
    return settings.value("volstep", 5).toInt();
}

void Settings::setForwardTime(int value)
{
    // disabled option
    /*if (value < 1 || value > 60)
        return; // incorrect value

    if (getForwardTime() != value) {
        settings.setValue("forwardtime", value);
        emit forwardTimeChanged();
    }*/
    Q_UNUSED(value)
}

int Settings::getForwardTime() const
{
    // disabled option
    //return settings.value("forwardtime", 10).toInt();
    return 10;
}

void Settings::setFavDevices(const QHash<QString,QVariant> &devs)
{
    settings.setValue("favdevices", devs);
    emit favDevicesChanged();
}

void Settings::setLastDevices(const QHash<QString,QVariant> &devs)
{
    settings.setValue("lastdevices", devs);
}

QHash<QString,QVariant> Settings::getLastDevices() const
{
    return settings.value("lastdevices",QHash<QString,QVariant>()).toHash();
}

QString Settings::getCacheDir() const
{
   return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString Settings::getPlaylistDir() const
{
   const auto path = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

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

void Settings::asyncAddFavDevice(const QString &id)
{
    startTask([this, &id](){
        addFavDevice(id);
    });
}

void Settings::asyncRemoveFavDevice(const QString &id)
{
    startTask([this, &id](){
        removeFavDevice(id);
    });
}

void Settings::addFavDevice(const QString &id)
{
    auto list = getFavDevices();
    if (list.contains(id))
        return;

    QString url;
    if (writeDeviceXML(id, url)) {
        list.insert(id, url);
        setFavDevices(list);
        emit favDeviceChanged(id);
    }
}

void Settings::addLastDevice(const QString &id)
{
    auto list = getLastDevices();
    if (list.contains(id))
        return;

    QString url;
    if (writeDeviceXML(id, url)) {
        list.insert(id, url);
        setLastDevices(list);
    }
}

void Settings::addLastDevices(const QStringList &ids)
{
    QHash<QString,QVariant> list;

    for (const auto& id : ids) {
        QString url;
        if (writeDeviceXML(id, url))
            list.insert(id, url);
    }

    setLastDevices(list);
}

void Settings::removeFavDevice(const QString &id)
{
    auto list = getFavDevices();
    if (list.contains(id)) {
        list.remove(id);
        setFavDevices(list);
        emit favDeviceChanged(id);
    }
}

bool Settings::writeDeviceXML(const QString &id, QString& url)
{
    UPnPClient::UPnPDeviceDesc ddesc;

    if (Directory::instance()->getDeviceDesc(id, ddesc)) {

        QString _id(id);
        QString filename = "device-" + _id.replace(':','-') + ".xml";

        if (Utils::writeToCacheFile(filename, QByteArray::fromStdString(ddesc.XMLText), true)) {
            url = QString::fromStdString(ddesc.URLBase);
            return true;
        } else {
            qWarning() << "Cannot write description file for device" << id;
        }

    } else {
        qWarning() << "Cannot find device description for" << id;
    }

    return false;
}

bool Settings::readDeviceXML(const QString &id, QByteArray& xml)
{
    QString _id(id);
    QString filename = "device-" + _id.replace(':','-') + ".xml";

    if (Utils::readFromCacheFile(filename, xml)) {
        return true;
    } else {
        qWarning() << "Cannot read description file for device" << id;
    }

    return false;
}

QHash<QString,QVariant> Settings::getFavDevices() const
{
    return settings.value("favdevices",QHash<QString,QVariant>()).toHash();
}

void Settings::setLastDir(const QString& value)
{
    if (getLastDir() != value) {
        settings.setValue("lastdir", value);
        emit lastDirChanged();
    }
}

QString Settings::getLastDir() const
{
    return settings.value("lastdir", "").toString();
}

void Settings::setRecDir(const QString& value)
{
    if (getRecDir() != value) {
        settings.setValue("recdir", QDir(value).exists() ? value : "");
        emit recDirChanged();
    }
}

QString Settings::getRecDir()
{
    auto defDir = QStandardPaths::writableLocation(
                           QStandardPaths::MusicLocation) + "/JupiiRecordings";
    auto recDir = settings.value("recdir", defDir).toString();
    QDir d(recDir);
    if (recDir.isEmpty()) {
        d.mkpath(defDir);
        settings.setValue("recdir", defDir);
        emit recDirChanged();
        return defDir;
    } else {
        if (!d.exists()) {
            qWarning() << "Recording dir doesn't exist";
            if (!d.mkpath(recDir)) {
                qWarning() << "Cannot create recording dir";
                d.mkpath(defDir);
                settings.setValue("recdir", defDir);
                emit recDirChanged();
                return defDir;
            }
        }
    }
    return recDir;
}

void Settings::setPrefNetInf(const QString& value)
{
    if (getPrefNetInf() != value) {
        settings.setValue("prefnetinf", value);
        emit prefNetInfChanged();
    }
}

QString Settings::getPrefNetInf() const
{
#ifdef SAILFISH
    if (isDebug())
        return settings.value("prefnetinf", "").toString();
    else
        return {};
#else
    return settings.value("prefnetinf", "").toString();
#endif
}

void Settings::setLastPlaylist(const QStringList& value)
{
    if (getLastPlaylist() != value) {
        settings.setValue("lastplaylist", value);
        emit lastPlaylistChanged();
    }
}

QStringList Settings::getLastPlaylist() const
{
    return settings.value("lastplaylist").toStringList();
}

void Settings::setShowAllDevices(bool value)
{
    if (getShowAllDevices() != value) {
        settings.setValue("showalldevices", value);
        emit showAllDevicesChanged();
    }
}

bool Settings::getShowAllDevices() const
{
    return settings.value("showalldevices", false).toBool();
}

void Settings::setRec(bool value)
{
    if (getRec() != value) {
        settings.setValue("rec", value);
        emit recChanged();
    }
}

bool Settings::getRec() const
{
    return settings.value("rec", true).toBool();
}

void Settings::setScreenSupported(bool value)
{
#ifdef SAILFISH
    if (getScreenSupported() != value) {
        settings.setValue("screensupported", value);
        emit screenSupportedChanged();
    }
#else
    Q_UNUSED(value)
#endif
}

bool Settings::getScreenSupported() const
{
#ifdef SAILFISH
    return settings.value("screensupported", false).toBool();
#else
    return true;
#endif
}

void Settings::setScreenCropTo169(int value)
{
    // 0 - disabled
    // 1 - scale
    // 2 - crop
    if (getScreenCropTo169() != value) {
        settings.setValue("screencrop169", value);
        emit screenCropTo169Changed();
    }
}

int Settings::getScreenCropTo169()
{
    // 0 - disabled
    // 1 - scale
    // 2 - crop
#ifdef SAILFISH
    return settings.value("screencrop169", 1).toInt();
#else
    return settings.value("screencrop169", 2).toInt();
#endif
}

void Settings::setScreenAudio(bool value)
{
    if (getScreenAudio() != value) {
        settings.setValue("screenaudio", value);
        emit screenAudioChanged();
    }
}

bool Settings::getScreenAudio()
{
#ifdef SAILFISH
    return settings.value("screenaudio", false).toBool();
#else
    return settings.value("screenaudio", true).toBool();
#endif
}

void Settings::setScreenEncoder(const QString &value)
{
    if (getScreenEncoder() != value) {
        settings.setValue("screenencoder", value);
        emit screenEncoderChanged();
    }
}

QString Settings::getScreenEncoder() const
{
#ifdef SAILFISH
    if (isDebug()) {
        // valid encoders: libx264, libx264rgb, h264_omx
        auto enc_name = settings.value("screenencoder").toString();
        if (enc_name == "libx264" ||
                enc_name == "libx264rgb" ||
                enc_name == "h264_omx")
            return enc_name;
    }

    return {};
#else
    // valid encoders: libx264, libx264rgb, h264_nvenc
    auto enc_name = settings.value("screenencoder").toString();
    if (enc_name == "libx264" ||
            enc_name == "libx264rgb" ||
            enc_name == "h264_nvenc" ||
            enc_name == "h264_omx")
        return enc_name;

    return {};
#endif
}

void Settings::setScreenFramerate(int value)
{
    if (getScreenFramerate() != value) {
        settings.setValue("screenframerate", value);
        emit screenFramerateChanged();
    }
}

int Settings::getScreenFramerate() const
{
#ifdef SAILFISH
    // default 5 fps
    if (isDebug())
        return settings.value("screenframerate", 5).toInt();
    else
        return 5;
#else
    // default 15 fps
    return settings.value("screenframerate", 15).toInt();
#endif
}

void Settings::setScreenQuality(int value)
{
    if (getScreenQuality() != value) {
        settings.setValue("screenquality", value);
        emit screenQualityChanged();
    }
}

int Settings::getScreenQuality() const
{
    return settings.value("screenquality", 3).toInt();
}

void Settings::setUseHWVolume(bool value)
{
#ifdef SAILFISH
    if (getUseHWVolume() != value) {
        settings.setValue("usehwvolume", value);
        emit useHWVolumeChanged();
    }
#else
    Q_UNUSED(value)
#endif
}

bool Settings::getUseHWVolume() const
{
#ifdef SAILFISH
    return settings.value("usehwvolume", true).toBool();
#else
    return false;
#endif
}

void Settings::setMicVolume(float value)
{
    value = value < 1 ? 1 : value > 100 ? 100 : value;

    if (getMicVolume() > value || getMicVolume() < value) {
        settings.setValue("micvolume", value);
        emit micVolumeChanged();
    }
}

float Settings::getMicVolume() const
{
#ifdef SAILFISH
    return settings.value("micvolume", 50.0).toFloat();
#else
    return settings.value("micvolume", 1.0).toFloat();
#endif
}

void Settings::setAudioBoost(float value)
{
#ifdef SAILFISH
    Q_UNUSED(value)
    return;
#else
    value = value < 1 ? 1 : value > 10 ? 10 : value;

    if (getAudioBoost() > value || getAudioBoost() < value) {
        settings.setValue("audioboost", value);
        emit audioBoostChanged();
    }
#endif
}

float Settings::getAudioBoost() const
{
#ifdef SAILFISH
    //return settings.value("micvolume", 2.3f).toFloat();
    return 2.3f;
#else
    return settings.value("audioboost", 1.0f).toFloat();
#endif
}

QByteArray Settings::resetKey()
{
    QByteArray key;

    // Seed init, needed for key generation
    qsrand(static_cast<uint>(QTime::currentTime().msec()));

    // key is 10 random chars
    for (uint i = 0; i < 10; ++i)
        key.append(static_cast<char>(qrand()));

    settings.setValue("key", key);

    return key;
}

QByteArray Settings::getKey()
{
    QByteArray key = settings.value("key").toByteArray();

    if (key.isEmpty()) {
        key = resetKey();
    }

    return key;
}

void Settings::setRemoteContentMode(int value)
{
    if (getRemoteContentMode() != value) {
        settings.setValue("remotecontentmode2", value);
        emit remoteContentModeChanged();
    }
}

int Settings::getRemoteContentMode() const
{
    // 0 - proxy for all
    // 1 - redirection for all
    // 2 - none for all
    // 3 - proxy for shoutcast, redirection for others
    // 4 - proxy for shoutcast, none for others
    return settings.value("remotecontentmode2", 0).toInt();
}

void Settings::setAlbumQueryType(int value)
{
    if (getAlbumQueryType() != value) {
        settings.setValue("albumquerytype", value);
        emit albumQueryTypeChanged();
    }
}

int Settings::getAlbumQueryType() const
{
    // 0 - by album title
    // 1 - by artist
    return settings.value("albumquerytype", 0).toInt();
}

void Settings::setRecQueryType(int value)
{
    if (getRecQueryType() != value) {
        settings.setValue("recquerytype", value);
        emit recQueryTypeChanged();
    }
}

int Settings::getRecQueryType() const
{
    // 0 - by rec date
    // 1 - by title
    // 2 - by author
    return settings.value("recquerytype", 0).toInt();
}

void Settings::setCDirQueryType(int value)
{
    if (getCDirQueryType() != value) {
        settings.setValue("cdirquerytype", value);
        emit cdirQueryTypeChanged();
    }
}

int Settings::getCDirQueryType() const
{
    // 0 - by title
    // 1 - by album
    // 2 - by artist
    // 3 - by track number
    // 4 - by date
    return settings.value("cdirquerytype", 0).toInt();
}

void Settings::setPlayMode(int value)
{
    if (getPlayMode() != value) {
        settings.setValue("playmode", value);
        emit playModeChanged();
    }
}

int Settings::getPlayMode() const
{
    /*PM_Normal = 0,
    PM_RepeatAll = 1,
    PM_RepeatOne = 2*/
    int pm = settings.value("playmode", 1).toInt();
    return pm == 2 ? pm : 1;
}

QString Settings::mediaServerDevUuid()
{
    auto defaultUUID = QUuid::createUuid().toString().mid(1,36);
    if (!settings.contains("mediaserverdevuuid"))
        settings.setValue("mediaserverdevuuid", defaultUUID);
    return settings.value("mediaserverdevuuid", defaultUUID).toString();
}

void Settings::setContentDirSupported(bool value)
{
    if (getContentDirSupported() != value) {
        settings.setValue("contentdirsupported", value);
        emit contentDirSupportedChanged();
    }
}

bool Settings::getContentDirSupported() const
{
    return settings.value("contentdirsupported", true).toBool();
}

bool Settings::hintEnabled(HintType hint) const
{
    int hints = settings.value("hints", std::numeric_limits<int>::max()).toInt();
    return hints & static_cast<int>(hint);
}

void Settings::disableHint(HintType hint)
{
    int hints = settings.value("hints", std::numeric_limits<int>::max()).toInt();
    hints &= ~static_cast<int>(hint);
    settings.setValue("hints", hints);
}

void Settings::resetHints()
{
    settings.setValue("hints", std::numeric_limits<int>::max());
}

void Settings::setColorScheme(int value)
{
    if (m_colorScheme != value) {
        m_colorScheme = value;
        emit colorSchemeChanged();
    }
}

int Settings::getColorScheme() const
{
    return m_colorScheme;
}

void Settings::reset()
{
    qDebug() << "Resetting settings";

    if (!QFile::remove(settings.fileName())) {
        qWarning() << "Cannot remove file:" << settings.fileName();
    }
}

void Settings::setFsapiPin(const QString &value)
{
    if (fsapiPin() != value) {
        settings.setValue("fsapipin", value);
        emit fsapiPinChanged();
    }
}

QString Settings::fsapiPin() const
{
    return settings.value("fsapipin", "1234").toString();
}

void Settings::setGlobalYtdl(bool value)
{
    if (globalYtdl() != value) {
        settings.setValue("globalytdl", value);
        emit globalYtdlChanged();
    }
}

bool Settings::globalYtdl() const
{
    return settings.value("globalytdl", true).toBool();
}

Settings::OpenUrlModeType Settings::openUrlMode() const
{
    return static_cast<OpenUrlModeType>(settings.value("openurlmode",
                       static_cast<int>(OpenUrlModeType::OpenUrlMode_All)).toInt());
}

void Settings::setOpenUrlMode(OpenUrlModeType value)
{
    if (openUrlMode() != value) {
        settings.setValue("openurlmode", static_cast<int>(value));
        emit openUrlModeChanged();
        initOpenUrlMode();
    }
}

std::pair<int,int> Settings::sysVer()
{
    const auto ver = QSysInfo::productVersion().split(".");
    if (ver.size() > 1) return {ver.value(0).toInt(), ver.value(1).toInt()};
    return {0, 0}; // unknown version
}

static inline void updateDesktopDb()
{
    QProcess::startDetached("update-desktop-database " +
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation));
}
static inline QFile desktopFile()
{
    return QFile{QString{"%1/%2-open-url.desktop"}.arg(
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation), Jupii::APP_ID)};
}

void Settings::initOpenUrlMode()
{
    // code mostly borrowed from https://github.com/Wunderfitz/harbour-piepmatz project

    const QDir dbusPath{QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/dbus-1/services"};
    if (!dbusPath.exists()) dbusPath.mkpath(dbusPath.absolutePath());
    if (QFile dbusServiceFile{dbusPath.absolutePath() + "/" + Jupii::DBUS_SERVICE + ".service"};
        !dbusServiceFile.exists()) {
        if (dbusServiceFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream s{&dbusServiceFile};
            s.setCodec("UTF-8");
            s << "[D-BUS Service]\n";
            s << "Name=" << Jupii::DBUS_SERVICE << "\n";
            s << "Exec=/usr/bin/invoker --type=silica-qt5 --id=" << Jupii::APP_ID << "--single-instance " << Jupii::APP_ID << "\n";
            s.flush();
        }
    }

    desktopFile().remove();
    updateDesktopDb();

    QTimer::singleShot(3000, this, [this] {
        qDebug() << "Open URL mode:" << static_cast<int>(openUrlMode());
            if (auto df{desktopFile()}; df.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream s{&df};
                s.setCodec("UTF-8");
                s << "[Desktop Entry]\n";
                s << "Type=Application\n";
                s << "Name=" << Jupii::APP_NAME << "\n";
                s << "Icon=" << Jupii::APP_ID << "\n";
                s << "NoDisplay=true\n";
                s << "MimeType=" << fileMimesForOpenWith.join(";");
                if (openUrlMode() == OpenUrlModeType::OpenUrlMode_All) {
                    s << ";" << urlMimesForOpenWith.join(";");
                }
                s << ";\n";
                s << "X-Maemo-Service=" << Jupii::DBUS_SERVICE << "\n";
                s << "X-Maemo-Method=" << Jupii::DBUS_SERVICE << ".Playlist.openUrl\n";
                s.flush();
                df.close();
                updateDesktopDb();
            }
    });
}
