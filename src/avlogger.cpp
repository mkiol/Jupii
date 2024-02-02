/* Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "avlogger.hpp"

#include <fmt/printf.h>

#include <sstream>

extern "C" {
#include <libavutil/log.h>
}

#include "logger.hpp"

[[maybe_unused]] static void avLog(void *avcl, int level, const char *fmt,
                                   va_list vl) {
    if (level > av_log_get_level()) return;

    const auto tag = [=]() {
        std::ostringstream os;
        os << "av::";
        auto *avc = avcl ? *static_cast<AVClass **>(avcl) : nullptr;
        if (avc != nullptr) {
            if (avc->parent_log_context_offset) {
                auto **parent = *reinterpret_cast<AVClass ***>(
                    (static_cast<uint8_t *>(avcl)) +
                    avc->parent_log_context_offset);
                if (parent && *parent)
                    os << (*parent)->item_name(parent) << " / ";
            }
            os << avc->item_name(avcl);
        }
        return os.str();
    }();

    JupiiLogger::Message msg{[=] {
                                 switch (level) {
                                     case AV_LOG_QUIET:
                                         return JupiiLogger::LogType::Quiet;
                                     case AV_LOG_DEBUG:
                                     case AV_LOG_VERBOSE:
                                         return JupiiLogger::LogType::Debug;
                                     case AV_LOG_TRACE:
                                         return JupiiLogger::LogType::Trace;
                                     case AV_LOG_INFO:
                                         return JupiiLogger::LogType::Info;
                                     case AV_LOG_WARNING:
                                         return JupiiLogger::LogType::Warning;
                                     case AV_LOG_ERROR:
                                     case AV_LOG_FATAL:
                                     case AV_LOG_PANIC:
                                         return JupiiLogger::LogType::Error;
                                 }
                                 return JupiiLogger::LogType::Debug;
                             }(),
                             "", tag.c_str(), 0};

    char buf[1024];
    vsnprintf(buf, 1024, fmt, vl);

    msg << buf;
}

void initAvLogger() {
#ifdef DEBUG
    av_log_set_level(AV_LOG_TRACE);
#else
    av_log_set_level(JupiiLogger::match(JupiiLogger::LogType::Debug)
                         ? AV_LOG_ERROR
                         : AV_LOG_QUIET);
    av_log_set_callback(avLog);
#endif
}
