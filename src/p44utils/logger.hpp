//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__logger__
#define __p44utils__logger__

#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <syslog.h>

#include "p44obj.hpp"

#if defined(DEBUG) || ALWAYS_DEBUG
#define DEBUGLOGGING 1
#define DBGLOGENABLED(lvl) globalLogger.logEnabled(lvl)
#define DBGLOG(lvl,...) { if (globalLogger.logEnabled(lvl)) globalLogger.log(lvl,##__VA_ARGS__); }
#define DBGFOCUSLOG FOCUSLOG
#define LOGGER_DEFAULT_LOGLEVEL LOG_DEBUG
#else
#define DEBUGLOGGING 0
#define DBGLOGENABLED(lvl) false
#define DBGLOG(lvl,...)
#define DBGFOCUSLOG(...)
#define LOGGER_DEFAULT_LOGLEVEL LOG_NOTICE
#endif

#if FOCUSLOGLEVEL
#define FOCUSLOG(...) LOG(FOCUSLOGLEVEL,##__VA_ARGS__)
#define FOCUSLOGGING 1
#define FOCUSLOGENABLED globalLogger.logEnabled(FOCUSLOGLEVEL)
#if !(defined(DEBUG) || ALWAYS_DEBUG || FOCUSLOGLEVEL>=7)
#warning "**** FOCUSLOGLEVEL<7 enabled in non-DEBUG build ****"
#endif
#else
#define FOCUSLOGGING 0
#define FOCUSLOG(...)
#define FOCUSLOGENABLED false
#endif

#define LOGENABLED(lvl) globalLogger.logEnabled(lvl)
#define LOG(lvl,...) { if (globalLogger.logEnabled(lvl)) globalLogger.log(lvl,##__VA_ARGS__); }

#define SETLOGLEVEL(lvl) globalLogger.setLogLevel(lvl)
#define SETERRLEVEL(lvl, dup) globalLogger.setErrLevel(lvl, dup)
#define LOGLEVEL (globalLogger.getLogLevel())


namespace p44 {

  class Logger : public P44Obj
  {
    pthread_mutex_t reportMutex;
    int logLevel;
    int stderrLevel;
    bool errToStdout;
  public:
    Logger();

    /// test if log is enabled at a given level
    /// @param aErrLevel level to check
    /// @return true if any logging (stderr or stdout) is enabled at the specified level
    bool logEnabled(int aErrLevel);

    /// test if log to std out is enabled at a given level
    /// @param aErrLevel level to check
    /// @return true if logging to stdout is enabled at this level.
    /// Note: logging might still occur on stderr, even if this function returns false
    bool stdoutLogEnabled(int aErrLevel);


    /// log a message
    /// @param aErrLevel error level of the message
    /// @param aFmt, ... printf style error message
    void log(int aErrLevel, const char *aFmt, ... );

    /// log a system error
    /// @param aErrLevel error level of the message
    /// @param aErrNum optional system error code (if none specified, errno global will be used)
    void logSysError(int aErrLevel, int aErrNum = 0);

    /// set log level
    /// @param aLogLevel the new log level
    /// @note even if aLogLevel is set to suppress messages, messages that qualify for going to stderr
    ///   (see setErrorLevel) will still be shown on stderr, but not on stdout.
    void setLogLevel(int aLogLevel);

    /// return current log level
    /// @return current log level
    int getLogLevel() { return logLevel; }

    /// set level required to send messages to stderr
    /// @param aStderrLevel any messages with this or a lower (=higher priority) level will be sent to stderr (default = LOG_ERR)
    /// @param aErrToStdout if set, messages that qualify for stderr will STILL be duplicated to stdout as well (default = true)
    void setErrLevel(int aStderrLevel, bool aErrToStdout);

  };

} // namespace p44


extern p44::Logger globalLogger;



#endif /* defined(__p44utils__logger__) */
