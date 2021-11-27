/* Copyright (C) 2014-2019 J.F.Dockes
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _LOG_H_X_INCLUDED_
#define _LOG_H_X_INCLUDED_

#include <string.h>
#include <fstream> 
#include <iostream>
#include <string>

#ifndef LOGGER_THREADSAFE
#define LOGGER_THREADSAFE 1
#endif

#if LOGGER_THREADSAFE
#include <mutex>
#endif

// Can't use the symbolic Logger::LLXX names in preproc. 6 is LLDEB1
// STATICVERBOSITY is the level above which logging statements are
// preproc'ed out (can't be dynamically turned on).
#ifndef LOGGER_STATICVERBOSITY
#define LOGGER_STATICVERBOSITY 5
#endif

#define LOGGER_DATESIZE 100

/** @brief This is a singleton class. The logger pointer is obtained
 * when needed by calls to @ref getTheLog(), only the first of which
 * actually creates the object and initializes the output. */
class Logger {
public:
    /** Initialize logging to file name. Use "stderr" for stderr
     * output. Creates the singleton logger object. Only the first
     * call changes the state, further ones just return the Logger
     * pointer. */
    static Logger *getTheLog(const std::string& fn = std::string());

    /** Close and reopen the output file. For rotating the log: rename
     * then reopen. */
    bool reopen(const std::string& fn);

    /** Retrieve the output stream in case you need to write directly
     * to it. In a multithreaded program, you probably also need to obtain
     * the mutex with @ref getmutex, and lock it. */
    std::ostream& getstream() {
        return m_tocerr ? std::cerr : m_stream;
    }

    /** @brief Log level values. Messages at level above the current will
     * not be printed. Messages at a level above
     * LOGGER_STATICVERBOSITY will not even be compiled in. */
    enum LogLevel {LLNON=0, LLFAT=1, LLERR=2, LLINF=3, LLDEB=4,
                   LLDEB0=5, LLDEB1=6, LLDEB2=7};

    /** @brief Set the log dynamic verbosity level */
    void setLogLevel(LogLevel level) {
        m_loglevel = level;
    }
    /** @brief Set the log dynamic verbosity level */
    void setloglevel(LogLevel level) {
        m_loglevel = level;
    }

    /** @brief Retrieve the current log level */
    int getloglevel() const {
        return m_loglevel;
    }
    /** @brief Retrieve current log file name */
    const std::string& getlogfilename() const {
        return m_fn;
    }
    /** @brief Logging to stderr ? */
    bool logisstderr() const {
        return m_tocerr;
    }

    /** @brief turn date logging on or off (default is off) */
    void logthedate(bool onoff) {
        m_logdate = onoff;
    }

    bool loggingdate() const {
        return m_logdate;
    }
    
    /** @brief Set the date format, as an strftime() format string. 
     * Default: "%Y%m%d-%H%M%S" . */
    void setdateformat(const std::string fmt) {
        m_datefmt = fmt;
    }

    /** Call with log locked */
    const char *datestring();
    
#if LOGGER_THREADSAFE
    std::recursive_mutex& getmutex() {
        return m_mutex;
    }
#endif
    
private:
    bool m_tocerr{false};
    bool m_logdate{false};
    int m_loglevel{LLERR};
    std::string m_datefmt{"%Y%m%d-%H%M%S"};
    std::string m_fn;
    std::ofstream m_stream;
#if LOGGER_THREADSAFE
    std::recursive_mutex m_mutex;
#endif
    char m_datebuf[LOGGER_DATESIZE];
    Logger(const std::string& fn);
    Logger(const Logger &);
    Logger& operator=(const Logger &);
};

#define LOGGER_PRT (Logger::getTheLog()->getstream())

#if LOGGER_THREADSAFE
#define LOGGER_LOCK                                                     \
    std::unique_lock<std::recursive_mutex> lock(Logger::getTheLog()->getmutex())
#else
#define LOGGER_LOCK
#endif

#ifndef LOGGER_LOCAL_LOGINC
#define LOGGER_LOCAL_LOGINC 0
#endif

#define LOGGER_LEVEL (Logger::getTheLog()->getloglevel() +    \
                      LOGGER_LOCAL_LOGINC)

#define LOGGER_DATE (Logger::getTheLog()->loggingdate() ? \
                     Logger::getTheLog()->datestring() : "")

#define LOGGER_DOLOG(L,X) LOGGER_PRT << LOGGER_DATE << ":" << L << ":" << \
                             __FILE__ << ":" << __LINE__ << "::" << X \
    << std::flush


#if LOGGER_STATICVERBOSITY >= 7
#define LOGDEB2(X) {                            \
        if (LOGGER_LEVEL >= Logger::LLDEB2) {   \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLDEB2, X);    \
        }                                       \
    }
#else
#define LOGDEB2(X)
#endif

#if LOGGER_STATICVERBOSITY >= 6
#define LOGDEB1(X) {                            \
        if (LOGGER_LEVEL >= Logger::LLDEB1) {   \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLDEB1, X);    \
        }                                       \
    }
#else
#define LOGDEB1(X)
#endif

#if LOGGER_STATICVERBOSITY >= 5
#define LOGDEB0(X) {                            \
        if (LOGGER_LEVEL >= Logger::LLDEB0) {   \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLDEB0, X);    \
        }                                       \
    }
#else
#define LOGDEB0(X)
#endif

#if LOGGER_STATICVERBOSITY >= 4
#define LOGDEB(X) {                             \
        if (LOGGER_LEVEL >= Logger::LLDEB) {    \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLDEB, X);     \
        }                                       \
    }
#else
#define LOGDEB(X)
#endif

#if LOGGER_STATICVERBOSITY >= 3
/** Log a message at level INFO. Other macros exist for other levels (LOGFAT,
 * LOGERR, LOGINF, LOGDEB, LOGDEB0... Use as: 
 * LOGINF("some text" << other stuff << ... << "\n");
 */
#define LOGINF(X) {                             \
        if (LOGGER_LEVEL >= Logger::LLINF) {    \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLINF, X);     \
        }                                       \
    }
#else
#define LOGINF(X)
#endif
#define LOGINFO LOGINF

#if LOGGER_STATICVERBOSITY >= 2
#define LOGERR(X) {                             \
        if (LOGGER_LEVEL >= Logger::LLERR) {    \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLERR, X);     \
        }                                       \
    }
#else
#define LOGERR(X)
#endif

#if LOGGER_STATICVERBOSITY >= 1
#define LOGFAT(X) {                             \
        if (LOGGER_LEVEL >= Logger::LLFAT) {    \
            LOGGER_LOCK;                        \
            LOGGER_DOLOG(Logger::LLFAT, X);     \
        }                                       \
    }
#else
#define LOGFAT(X)
#endif
#define LOGFATAL LOGFAT

#if defined(sun) || defined(_WIN32)
#define LOGSYSERR(who, what, arg) {                                     \
        LOGERR(who << ": " << what << "("  << arg << "): errno " << errno << \
               ": " << strerror(errno) << std::endl);                   \
    }
#else // not WINDOWS or sun

inline char *_log_check_strerror_r(int, char *errbuf) {return errbuf;}
inline char *_log_check_strerror_r(char *cp, char *){return cp;}

#define LOGSYSERR(who, what, arg) {                                     \
        char buf[200]; buf[0] = 0;                                      \
        LOGERR(who << ": " << what << "("  << arg << "): errno " << errno << \
               ": " << _log_check_strerror_r(                           \
                   strerror_r(errno, buf, 200), buf) << std::endl);     \
    }

#endif // not windows

#endif /* _LOG_H_X_INCLUDED_ */
