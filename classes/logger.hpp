//
//  logger.h
//  p44bridged
//
//  Created by Lukas Zeller on 11.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__logger__
#define __p44bridged__logger__

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

#define ALWAYS_DEBUG 1

#if defined(DEBUG) || ALWAYS_DEBUG
#define DBGLOGENABLED(lvl) globalLogger.logEnabled(lvl)
#define DBGLOG(lvl,...) globalLogger.log(lvl,__VA_ARGS__)
#define DBGLOGERRNO(lvl) globalLogger.logSysError(lvl)
#define LOGGER_DEFAULT_LOGLEVEL LOG_DEBUG
#else
#define DBGLOGENABLED(lvl) false
#define DBGLOG(lvl,...)
#define DBGLOGERRNO(lvl)
#define LOGGER_DEFAULT_LOGLEVEL LOG_NOTICE
#endif

#define LOGENABLED(lvl) globalLogger.logEnabled(lvl)
#define LOG(lvl,...) globalLogger.log(lvl,__VA_ARGS__)
#define LOGERR(lvl,err) globalLogger.logSysError(lvl,err)
#define LOGERRNO(lvl) globalLogger.logSysError(lvl)

#define SETLOGLEVEL(lvl) globalLogger.setLogLevel(lvl)

namespace p44 {

  class Logger {
    pthread_mutex_t reportMutex;
    int logLevel;
  public:
    Logger();
    bool logEnabled(int aErrlevel);
    void log(int aErrlevel, const char *aFmt, ... );
    void logSysError(int aErrlevel, int aErrNum = 0);
    void setLogLevel(int aErrlevel);
  };

} // namespace p44


extern p44::Logger globalLogger;



#endif /* defined(__p44bridged__logger__) */
