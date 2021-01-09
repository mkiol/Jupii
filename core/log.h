/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef LOGF_H
#define LOGF_H

#include <QMessageLogContext>
#include <QString>
#include <cstdio>

#define LOG_FILE "jupii.log"
#define FFMPEG_LOG_FILE "jupii_ffmpeg.log"
#define UPNPP_LOG_FILE "jupii_upnpp.log"

extern FILE * logFile;
extern FILE * ffmpegLogFile;

void qtLog(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void ffmpegLog(void *ptr, int level, const char *fmt, va_list vargs);
void removeLogFiles();

#endif // LOGF_H
