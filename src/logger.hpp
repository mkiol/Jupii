/* Copyright (C) 2022 Michal Kosciesza <michal@mkiol.net>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef JUPII_LOGGER_H
#define JUPII_LOGGER_H

#include <fstream>
#include <optional>
#include <sstream>

#ifdef USE_TRACE_LOGS
#define LOGT(msg)                                                              \
    JupiiLogger::Message(logger::LogType::Trace, __FILE__, __func__, __LINE__) \
        << msg
#else
#define LOGT(msg)
#endif
#define LOGD(msg)                                                         \
    JupiiLogger::Message(JupiiLogger::LogType::Debug, __FILE__, __func__, \
                         __LINE__)                                        \
        << msg
#define LOGI(msg)                                                        \
    JupiiLogger::Message(JupiiLogger::LogType::Info, __FILE__, __func__, \
                         __LINE__)                                       \
        << msg
#define LOGW(msg)                                                           \
    JupiiLogger::Message(JupiiLogger::LogType::Warning, __FILE__, __func__, \
                         __LINE__)                                          \
        << msg
#define LOGE(msg)                                                         \
    JupiiLogger::Message(JupiiLogger::LogType::Error, __FILE__, __func__, \
                         __LINE__)                                        \
        << msg
#define LOGF(msg)                                                         \
    JupiiLogger::Message(JupiiLogger::LogType::Error, __FILE__, __func__, \
                         __LINE__)                                        \
        << msg;                                                           \
    std::ostringstream ss;                                                \
    ss << __func__ << ':' << __LINE__ << " - " << msg;                    \
    throw std::runtime_error { ss.str() }

class JupiiLogger {
   public:
    enum class LogType {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warning = 3,
        Error = 4,
        Quiet = 5
    };
    friend std::ostream &operator<<(std::ostream &os, LogType type);

    class Message {
        std::ostringstream m_os;
        LogType m_type;
        const char *m_file;
        const char *m_fun;
        int m_line;

       public:
        Message(LogType type, const char *file, const char *function, int line);
        ~Message();

        template <typename T>
        Message &operator<<(const T &t) {
            m_os << std::boolalpha << t;
            return *this;
        }
    };

    static void init(LogType level, const std::string &file = {});
    static void setLevel(LogType level);
    static LogType level();
    static void setFile(const std::string &file);
    static inline bool logToFileEnabled() { return m_file.has_value(); }
    static bool match(LogType type);
    JupiiLogger() = delete;

   private:
    inline static const char *m_emptyStr = "()";
    static LogType m_level;
    static std::optional<std::ofstream> m_file;
};

#endif  // JUPII_LOGGER_H
