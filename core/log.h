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

extern FILE * logFile;
void qtLog(QtMsgType type, const QMessageLogContext &context, const QString &msg);
#ifdef FFMPEG
void ffmpegLog(void *ptr, int level, const char *fmt, va_list vargs);
#endif

#endif // LOGF_H
