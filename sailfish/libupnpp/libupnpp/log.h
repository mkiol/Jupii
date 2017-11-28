/* Copyright (C) 2014 J.F.Dockes
 *	 This program is free software; you can redistribute it and/or modify
 *	 it under the terms of the GNU General Public License as published by
 *	 the Free Software Foundation; either version 2 of the License, or
 *	 (at your option) any later version.
 *
 *	 This program is distributed in the hope that it will be useful,
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	 GNU General Public License for more details.
 *
 *	 You should have received a copy of the GNU General Public License
 *	 along with this program; if not, write to the
 *	 Free Software Foundation, Inc.,
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
/* Copyright (C) 2006-2016 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#ifndef _LOG_H_X_INCLUDED_
#define _LOG_H_X_INCLUDED_

#include <fstream> 
#include <iostream>
#include <string>
#include <mutex>

#ifndef LOGGER_THREADSAFE
#define LOGGER_THREADSAFE 1
#endif

#if LOGGER_THREADSAFE
#include <mutex>
#endif

// Can't use the symbolic Logger::LLXX names in preproc. 6 is LLDEB1
#ifndef LOGGER_STATICVERBOSITY
#define LOGGER_STATICVERBOSITY 5
#endif

class Logger {
public:
    /** Initialize logging to file name. Use "stderr" for stderr
       output. Creates the singleton logger object */
    static Logger *getTheLog(const std::string& fn);

    bool reopen(const std::string& fn);
    
    std::ostream& getstream() {
        return m_tocerr ? std::cerr : m_stream;
    }
    enum LogLevel {LLNON=0, LLFAT=1, LLERR=2, LLINF=3, LLDEB=4,
                   LLDEB0=5, LLDEB1=6, LLDEB2=7};
    void setLogLevel(LogLevel level) {
        m_loglevel = level;
    }
    int getloglevel() {
        return m_loglevel;
    }

#if LOGGER_THREADSAFE
    std::recursive_mutex& getmutex() {
        return m_mutex;
    }
#endif
    
private:
    bool m_tocerr;
    int m_loglevel;
    std::string m_fn;
    std::ofstream m_stream;
#if LOGGER_THREADSAFE
    std::recursive_mutex m_mutex;
#endif

    Logger(const std::string& fn);
    Logger(const Logger &);
    Logger& operator=(const Logger &);
};

#define LOGGER_PRT (Logger::getTheLog("")->getstream())

#if LOGGER_THREADSAFE
#define LOGGER_LOCK \
    std::unique_lock<std::recursive_mutex> lock(Logger::getTheLog("")->getmutex())
#else
#define LOGGER_LOCK
#endif

#ifndef LOGGER_LOCAL_LOGINC
#define LOGGER_LOCAL_LOGINC 0
#endif

#define LOGGER_LEVEL (Logger::getTheLog("")->getloglevel() + \
                      LOGGER_LOCAL_LOGINC)

#define LOGGER_DOLOG(L,X) LOGGER_PRT << ":" << L << ":" <<            \
                                  __FILE__ << ":" << __LINE__ << "::" << X \
    << std::flush

#if LOGGER_STATICVERBOSITY >= 7
#define LOGDEB2(X) {                                                    \
        if (LOGGER_LEVEL >= Logger::LLDEB2) {                           \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLDEB2, X);                            \
        }                                                               \
    }
#else
#define LOGDEB2(X)
#endif

#if LOGGER_STATICVERBOSITY >= 6
#define LOGDEB1(X) {                                                    \
        if (LOGGER_LEVEL >= Logger::LLDEB1) {                           \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLDEB1, X);                            \
        }                                                               \
    }
#else
#define LOGDEB1(X)
#endif

#if LOGGER_STATICVERBOSITY >= 5
#define LOGDEB0(X) {                                                    \
        if (LOGGER_LEVEL >= Logger::LLDEB0) {                           \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLDEB0, X);                            \
        }                                                               \
    }
#else
#define LOGDEB0(X)
#endif

#if LOGGER_STATICVERBOSITY >= 4
#define LOGDEB(X) {                                                     \
        if (LOGGER_LEVEL >= Logger::LLDEB) {                            \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLDEB, X);                             \
        }                                                               \
    }
#else
#define LOGDEB(X)
#endif

#if LOGGER_STATICVERBOSITY >= 3
#define LOGINF(X) {                                                     \
        if (LOGGER_LEVEL >= Logger::LLINF) {                            \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLINF, X);                             \
        }                                                               \
    }
#else
#define LOGINF(X)
#endif
#define LOGINFO LOGINF

#if LOGGER_STATICVERBOSITY >= 2
#define LOGERR(X) {                                                     \
        if (LOGGER_LEVEL >= Logger::LLERR) {                            \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLERR, X);                             \
        }                                                               \
    }
#else
#define LOGERR(X)
#endif

#if LOGGER_STATICVERBOSITY >= 1
#define LOGFAT(X) {                                                     \
        if (LOGGER_LEVEL >= Logger::LLFAT) {                            \
            LOGGER_LOCK;                                                \
            LOGGER_DOLOG(Logger::LLFAT, X);                             \
        }                                                               \
    }
#else
#define LOGFAT(X)
#endif
#define LOGFATAL LOGFAT

#endif /* _LOG_H_X_INCLUDED_ */
