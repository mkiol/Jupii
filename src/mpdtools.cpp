/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "mpdtools.hpp"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <cstring>
#include <optional>

namespace mpdtools {

static const auto* const mpdConfing =
    "db_file                 \"~/.config/mpd/database\"\n"
    "log_file                \"~/.config/mpd/log\"\n"
    "state_file              \"~/.config/mpd/state\"\n"
    "sticker_file            \"~/.config/mpd/sticker.sql\"\n"
    "bind_to_address         \"127.0.0.1\"\n"
    "port                    \"6600\"\n"
    "auto_update             \"yes\"\n"
    "audio_output {\n"
    "    type        \"pulse\"\n"
    "    name        \"PulseAudio\"\n"
    "}\n";

static std::optional<QString> execSystemctl(const QString& arg1,
                                            const QString& arg2) {
    QProcess process;
    process.start(QStringLiteral("/usr/bin/invoker"),
                  QStringList() << QStringLiteral("--type=generic")
                                << QStringLiteral("/usr/bin/systemctl")
                                << QStringLiteral("--user") << arg1 << arg2);

    process.waitForFinished(-1);

    if (process.exitCode() != 0) return std::nullopt;

    return process.readAllStandardOutput();
}

static bool servicesExist(const QString& name1, const QString& name2) {
    auto output =
        execSystemctl(QStringLiteral("list-units"), QStringLiteral("--all"));
    if (!output) return false;

    return output->contains(name1 + ".service") &&
           output->contains(name2 + ".service");
}

static void stopService(const QString& name) {
    if (execSystemctl(QStringLiteral("is-active"), name)) {
        qDebug() << "stopping:" << name;
        execSystemctl(QStringLiteral("stop"), name);
        qDebug() << "stopped:" << name;
    } else {
        qDebug() << "service is not running:" << name;
    }
}

static void startService(const QString& name) {
    if (!execSystemctl(QStringLiteral("is-active"), name)) {
        qDebug() << "starting:" << name;
        execSystemctl(QStringLiteral("start"), name);
        qDebug() << "started:" << name;
    } else {
        qDebug() << "service already started:" << name;
    }
}

static void makeMpdConfig() {
#ifdef USE_SFOS
    QFile config{QStringLiteral("%1/mpd/mpd.conf")
                     .arg(QStandardPaths::writableLocation(
                         QStandardPaths::ConfigLocation))};

    if (config.exists()) return;

    if (!config.open(QIODevice::WriteOnly)) {
        qWarning() << "failed to write mpd config";
        return;
    }

    config.write(mpdConfing, std::strlen(mpdConfing));
#endif
}

void start() {
    if (!servicesExist(QStringLiteral("mpd"), QStringLiteral("upmpdcli"))) {
        qDebug() << "mpd or upmpdcli not installed";
        return;
    }

    makeMpdConfig();

    startService(QStringLiteral("mpd"));
    startService(QStringLiteral("upmpdcli"));
}

void stop() {
    stopService(QStringLiteral("upmpdcli"));
    stopService(QStringLiteral("mpd"));
}

}  // namespace mpdtools
