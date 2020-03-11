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

#include "settings.h"
#include "directory.h"
#include "utils.h"
#include "info.h"
#include "log.h"

#include <libupnpp/control/description.hxx>

Settings* Settings::inst = nullptr;

Settings::Settings(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent, 1),
    settings(parent)
{
    if (getLogToFile())
        qInstallMessageHandler(qtLog);

#ifdef SAILFISH
    hwName = readHwInfo();
#else
    hwName = QSysInfo::machineHostName();
#endif
    qDebug() << "HW name:" << hwName;
}

bool Settings::isDebug()
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
    QFile f(HW_RELEASE_FILE);
    if (f.open(QIODevice::ReadOnly)) {
        auto d = f.readAll();
        f.close();
        auto data = QString::fromUtf8(d).split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
        QRegExp rx("^NAME=\"?([^\"]*)\"?$", Qt::CaseInsensitive);
        for (const auto& line: data) {
            if (rx.indexIn(line) != -1)
                return rx.cap(1);
        }
    } else {
        qWarning() << "Cannot open file" << f.fileName() <<
                      "for reading (" + f.errorString() + ")";
    }

    return QString();
}
#endif

Settings* Settings::instance()
{
    if (Settings::inst == nullptr) {
        Settings::inst = new Settings();
    }

    return Settings::inst;
}

QString Settings::prettyName()
{
    return hwName.isEmpty() ? QString(Jupii::APP_NAME) : QString("%1 (%2)")
                              .arg(Jupii::APP_NAME, hwName);
}

void Settings::setLogToFile(bool value)
{
#ifdef SAILFISH
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
#else
    Q_UNUSED(value)
#endif
}

bool Settings::getLogToFile()
{
#ifdef SAILFISH
    return settings.value("logtofile", false).toBool();
#else
    return false;
#endif
}

void Settings::setPort(int value)
{
    if (getPort() != value) {
        settings.setValue("port", value);
        emit portChanged();
    }
}

int Settings::getPort()
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

int Settings::getVolStep()
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

int Settings::getForwardTime()
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

QHash<QString,QVariant> Settings::getLastDevices()
{
    return settings.value("lastdevices",QHash<QString,QVariant>()).toHash();
}

QString Settings::getCacheDir()
{
   return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString Settings::getPlaylistDir()
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

QHash<QString,QVariant> Settings::getFavDevices()
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

QString Settings::getLastDir()
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

QString Settings::getPrefNetInf()
{
    return settings.value("prefnetinf", "").toString();
}

void Settings::setLastPlaylist(const QStringList& value)
{
    if (getLastPlaylist() != value) {
        settings.setValue("lastplaylist", value);
        emit lastPlaylistChanged();
    }
}

QStringList Settings::getLastPlaylist()
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

bool Settings::getShowAllDevices()
{
    return settings.value("showalldevices", false).toBool();
}

void Settings::setRec(bool value)
{
#ifdef SAILFISH
    if (getRec() != value) {
        settings.setValue("rec", value);
        emit recChanged();
    }
#else
    Q_UNUSED(value)
#endif
}

bool Settings::getRec()
{
#ifdef SAILFISH
    return settings.value("rec", false).toBool();
#else
    return true;
#endif
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

bool Settings::getScreenSupported()
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
    return settings.value("screenaudio", false).toBool();
}

void Settings::setScreenFramerate(int value)
{
    if (getScreenFramerate() != value) {
        settings.setValue("screenframerate", value);
        emit screenFramerateChanged();
    }
}

int Settings::getScreenFramerate()
{
#ifdef SAILFISH
    // default 5 fps
    return settings.value("screenframerate", 5).toInt();
#else
    // default 15 fps
    return settings.value("screenframerate", 15).toInt();
#endif
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

bool Settings::getUseHWVolume()
{
#ifdef SAILFISH
    return settings.value("usehwvolume", true).toBool();
#else
    return false;
#endif
}

void Settings::setMicVolume(float value)
{
    value = value < 0 ? 0 : value > 100 ? 100 : value;

    if (getMicVolume() > value || getMicVolume() < value) {
        settings.setValue("micvolume", value);
        emit micVolumeChanged();
    }
}

float Settings::getMicVolume()
{
#ifdef SAILFISH
    return settings.value("micvolume", 50.0).toFloat();
#else
    return settings.value("micvolume", 1.0).toFloat();
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

int Settings::getRemoteContentMode()
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

int Settings::getAlbumQueryType()
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

int Settings::getRecQueryType()
{
    // 0 - by rec date
    // 1 - by title
    // 2 - by station name
    return settings.value("recquerytype", 0).toInt();
}

void Settings::setCDirQueryType(int value)
{
    if (getCDirQueryType() != value) {
        settings.setValue("cdirquerytype", value);
        emit cdirQueryTypeChanged();
    }
}

int Settings::getCDirQueryType()
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

int Settings::getPlayMode()
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

bool Settings::getContentDirSupported()
{
    return settings.value("contentdirsupported", true).toBool();
}

void Settings::setSkipFrames(int value)
{
    if (getSkipFrames() != value) {
        settings.setValue("skipframes", value);
        emit skipFramesChanged();
    }
}

int Settings::getSkipFrames()
{
    return settings.value("skipframes", 5).toInt();
}
