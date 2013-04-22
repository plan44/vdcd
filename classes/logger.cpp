//
//  logger.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 11.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "logger.hpp"



Logger::Logger()
{
  pthread_mutex_init(&reportMutex, NULL);
  #ifdef DEBUG
  debugLevel = LOG_DEBUG;
  #else
  debugLevel = LOG_NOTICE;
  #endif
}

#define LOGBUFSIZ 4096


void Logger::log(int aErrlevel, const char *aFmt, ... )
{
  if (aErrlevel <= debugLevel) {
    char buf[LOGBUFSIZ];
    va_list ap;
    pthread_mutex_lock(&reportMutex);

    strncpy(buf, "p44bridged: ", LOGBUFSIZ);
    va_start(ap, aFmt);
    vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), aFmt, ap);
    va_end(ap);

    char buf2[LOGBUFSIZ], *sp, tsbuf[30];
    struct timeval t;
    gettimeofday(&t, NULL);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
    (void)snprintf(buf2, LOGBUFSIZ, "[%s.%03d] ", tsbuf, (int)t.tv_usec / 1000);
    for (sp = buf; *sp != '\0'; sp++)
//      if (isprint(*sp) || (isspace(*sp) && (sp[1]=='\0' || sp[2]=='\0')))
      if (isprint(*sp) || (*sp=='\n')) // printable and LFs are oj
        (void)snprintf(buf2+strlen(buf2), 2, "%c", *sp);
      else
        (void)snprintf(buf2+strlen(buf2), 6, "\\x%02x", (unsigned)*sp); // rest is shown C-escaped
    (void)fputs(buf2, stderr);

    pthread_mutex_unlock(&reportMutex);
  }
}


void Logger::logSysError(int aErrlevel, int aErrNum)
{
  if (aErrlevel <= debugLevel) {
    // obtain error number if none specified
    if (aErrNum==0)
      aErrNum = errno;
    // obtain error message
    char buf[LOGBUFSIZ];
    strerror_r(aErrNum, buf, LOGBUFSIZ);
    // show it
    log(aErrlevel, "System error message: %s", buf);
  }
}
