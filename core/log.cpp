/* Copyright (C) 2019 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "log.h"

#include <QStandardPaths>
#include <QDir>
#include <QByteArray>
#include <cstdlib>
#ifdef FFMPEG
#include <libavutil/log.h>
#endif

FILE * logFile = nullptr;

void qtLog(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (!logFile) {
        QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        auto file = home.filePath(LOG_FILE).toLatin1();
        logFile = fopen(file.data(), "w");
    }

    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    switch (type) {
    /*case QtDebugMsg:
        fprintf(logFile, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        fprintf(logFile, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;*/
    case QtWarningMsg:
        fprintf(logFile, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(logFile, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(logFile, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    }
}

#ifdef FFMPEG
void ffmpegLog(void *ptr, int level, const char *fmt, va_list vargs)
{
    if (!logFile) {
        QDir home(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
        auto file = home.filePath("jupii.log").toLatin1();
        logFile = fopen(file.data(), "w");
    }

    fprintf(logFile, "FFMPEG %s: ",
            level == AV_LOG_TRACE ? "Trace" :
            level == AV_LOG_DEBUG ? "Debug" :
            level == AV_LOG_VERBOSE ? "Verbose" :
            level == AV_LOG_INFO ? "Info" :
            level == AV_LOG_WARNING ? "Warning" :
            level == AV_LOG_ERROR ? "Error" :
            level == AV_LOG_FATAL ? "Fatal" :
            level == AV_LOG_PANIC ? "Panic" : "Unknown");
    vfprintf(logFile, fmt, vargs);
}
#endif
