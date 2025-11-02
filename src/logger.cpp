/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "logger.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>
#include <threads.h>

#include <chrono>
#include <cstdio>

JupiiLogger::LogType JupiiLogger::m_level = JupiiLogger::LogType::Error;
std::optional<std::ofstream> JupiiLogger::m_file = std::nullopt;

std::ostream &operator<<(std::ostream &os, JupiiLogger::LogType type) {
    switch (type) {
        case JupiiLogger::LogType::Trace:
            os << "trace";
            break;
        case JupiiLogger::LogType::Debug:
            os << "debug";
            break;
        case JupiiLogger::LogType::Info:
            os << "info";
            break;
        case JupiiLogger::LogType::Warning:
            os << "warning";
            break;
        case JupiiLogger::LogType::Error:
            os << "error";
            break;
        case JupiiLogger::LogType::Quiet:
            os << "quiet";
            break;
    }
    return os;
}

void JupiiLogger::init(LogType level, const std::string &file) {
    m_level = level;

    setFile(file);
}

void JupiiLogger::setLevel(LogType level) {
    if (m_level != level) {
        auto old = m_level;
        m_level = level;
        LOGD("logging level changed: " << old << " => " << m_level);
    }
}

void JupiiLogger::setFile(const std::string &file) {
    if (file.empty()) {
        m_file.reset();
        LOGI("logging to stderr enabled");
    } else {
        m_file.emplace(file, std::ios::app);
        if (!m_file->good()) {
            m_file.reset();
            LOGW("failed to create log file: " << file);
        } else {
            LOGI("logging to file enabled: " << file);
        }
    }
}

JupiiLogger::LogType JupiiLogger::level() { return m_level; }

bool JupiiLogger::match(LogType type) {
    return static_cast<int>(type) >= static_cast<int>(m_level);
}

JupiiLogger::Message::Message(LogType type, const char *file,
                              const char *function, int line)
    : m_type{type}, m_file{file}, m_fun{function}, m_line{line} {}

inline static auto typeToChar(JupiiLogger::LogType type) {
    switch (type) {
        case JupiiLogger::LogType::Trace:
            return 'T';
        case JupiiLogger::LogType::Debug:
            return 'D';
        case JupiiLogger::LogType::Info:
            return 'I';
        case JupiiLogger::LogType::Warning:
            return 'W';
        case JupiiLogger::LogType::Error:
            return 'E';
        case JupiiLogger::LogType::Quiet:
            return 'Q';
    }
    return '-';
}

JupiiLogger::Message::~Message() {
    if (!match(m_type)) return;

    auto now = std::chrono::system_clock::now();
    auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now.time_since_epoch())
                     .count() %
                 1000;

    const auto str = m_os.str();
    if (str.empty()) return;

    if (m_fun == nullptr || m_fun[0] == '\0') m_fun = m_emptyStr;

    auto fmt =
        fmt::format("[{{0}}] {{1:%H:%M:%S}}.{{2}} {{3:#10x}} {{4}}{}- {{5}}{}",
                    m_line > 0 ? ":{6} " : " ", str.back() == '\n' ? "" : "\n");
    try {
        auto line = fmt::format(fmt, typeToChar(m_type), now, msecs,
                                thrd_current(), m_fun, str, m_line);
        if (JupiiLogger::m_file) {
            *JupiiLogger::m_file << line;
            JupiiLogger::m_file->flush();
        } else {
            fmt::print(stderr, "{}", line);
            fflush(stderr);
        }
    } catch (const std::runtime_error &e) {
        fmt::print(stderr, "logger error: {}\n", e.what());
        fmt::print(stderr, "{}\n", str);
    }
}
