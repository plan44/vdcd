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
#define DBGLOG(lvl,...) globalLogger.log(lvl,__VA_ARGS__)
#define DBGLOGERRNO(lvl) globalLogger.logSysError(lvl)
#define LOGGER_DEFAULT_LOGLEVEL LOG_DEBUG
#else
#define DBGLOG(lvl,...)
#define DBGLOGERRNO(lvl)
#define LOGGER_DEFAULT_LOGLEVEL LOG_NOTICE
#endif

#define LOG(lvl,...) globalLogger.log(lvl,__VA_ARGS__)
#define LOGERR(lvl,err) globalLogger.logSysError(lvl,err)
#define LOGERRNO(lvl) globalLogger.logSysError(lvl)


class Logger {
  pthread_mutex_t reportMutex;
  int debugLevel;
public:
  Logger();
  void log(int aErrlevel, const char *aFmt, ... );
  void logSysError(int aErrlevel, int aErrNum = 0);
};


static Logger globalLogger;


#endif /* defined(__p44bridged__logger__) */
