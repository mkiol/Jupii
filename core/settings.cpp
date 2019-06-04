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

#include "settings.h"
#include "directory.h"
#include "utils.h"

#include <libupnpp/control/description.hxx>

Settings* Settings::inst = nullptr;

Settings::Settings(QObject *parent) :
    QObject(parent),
    TaskExecutor(parent, 1),
    settings(parent)
{
}

Settings* Settings::instance()
{
    if (Settings::inst == nullptr) {
        Settings::inst = new Settings();
    }

    return Settings::inst;
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
    if (value < 1 || value > 60)
        return; // incorrect value

    if (getForwardTime() != value) {
        settings.setValue("forwardtime", value);
        emit forwardTimeChanged();
    }
}

int Settings::getForwardTime()
{
    // Default value is 10 s
    return settings.value("forwardtime", 10).toInt();
}

void Settings::setFavDevices(const QHash<QString,QVariant> &devs)
{
    settings.setValue("favdevices", devs);
    emit favDevicesChanged();
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
    }
}

void Settings::removeFavDevice(const QString &id)
{
    auto list = getFavDevices();
    if (list.contains(id)) {
        list.remove(id);
        setFavDevices(list);
    }
}

bool Settings::writeDeviceXML(const QString &id, QString& url)
{
    UPnPClient::UPnPDeviceDesc ddesc;

    if (Directory::instance()->getDeviceDesc(id, ddesc)) {

        QString _id(id);
        QString filename = "device-" + _id.replace(':','-') + ".xml";

        if (Utils::writeToCacheFile(filename, QByteArray::fromStdString(ddesc.XMLText))) {
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
    if (getRec() != value) {
        settings.setValue("rec", value);
        emit recChanged();
    }
}

bool Settings::getRec()
{
    return settings.value("rec", false).toBool();
}

void Settings::setImageSupported(bool value)
{
    if (getImageSupported() != value) {
        settings.setValue("imagesupported", value);
        emit imageSupportedChanged();
    }
}

bool Settings::getImageSupported()
{
    return settings.value("imagesupported", false).toBool();
}

void Settings::setScreenCropTo169(bool value)
{
    if (getScreenCropTo169() != value) {
        settings.setValue("screencorop169", value);
        emit screenCropTo169Changed();
    }
}

bool Settings::getScreenCropTo169()
{
    return settings.value("screencorop169", false).toBool();
}

void Settings::setPulseMode(int value)
{
    if (getPulseMode() != value) {
        settings.setValue("pulsemode", value);
        emit pulseModeChanged();
    }
}

int Settings::getPulseMode()
{
    // modes:
    // 0 - MP3 16-bit 44100 Hz stereo 128 kbps (default)
    // 1 - MP3 16-bit 44100 Hz stereo 96 kbps
    // 2 - LPCM 16-bit 44100 Hz stereo 1411 kbps
    // 3 - LPCM 16-bit 22050 Hz stereo 706 kbps
    return settings.value("pulsemode", 0).toInt();
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
    // deafult 15 fps
    return settings.value("screenframerate", 15).toInt();
}

void Settings::setUseHWVolume(bool value)
{
    if (getUseHWVolume() != value) {
        settings.setValue("usehwvolume", value);
        emit useHWVolumeChanged();
    }
}

bool Settings::getUseHWVolume()
{
    return settings.value("usehwvolume", true).toBool();
}

void Settings::setMicVolume(float value)
{
    value = value < 0 ? 0 : value > 100 ? 100 : value;

    if (getMicVolume() != value) {
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

void Settings::setRememberPlaylist(bool value)
{
    if (getRememberPlaylist() != value) {
        settings.setValue("rememberplaylist", value);
        emit rememberPlaylistChanged();
    }
}

bool Settings::getRememberPlaylist()
{
    return settings.value("rememberplaylist", true).toBool();
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
