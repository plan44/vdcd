//
//  logger.cpp
//  p44utils
//
//  Created by Lukas Zeller on 11.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "logger.hpp"

#include "utils.hpp"

using namespace p44;

p44::Logger globalLogger;

Logger::Logger()
{
  pthread_mutex_init(&reportMutex, NULL);
  logLevel = LOGGER_DEFAULT_LOGLEVEL;
}

#define LOGBUFSIZ 8192


bool Logger::logEnabled(int aErrlevel)
{
  return (aErrlevel <= logLevel);
}


void Logger::log(int aErrlevel, const char *aFmt, ... )
{
  if (logEnabled(aErrlevel)) {
    pthread_mutex_lock(&reportMutex);
    va_list args;
    va_start(args, aFmt);
    // format the message
    string message;
    string_format_v(message, false, aFmt, args);
    va_end(args);
    // escape non-printables and detect multiline
    bool isMultiline = false;
    string::size_type i=0;
    while (i<message.length()) {
      char c = message[i];
      if (c=='\n') {
        if (i!=message.length()-1)
          isMultiline = true; // not just trailing LF
      }
      else if (!isprint(c) && (uint8_t)c<0x80) {
        // ASCII control character, but not bit 7 set (UTF8 component char)
        message.replace(i, 1, string_format("\\x%02x", (unsigned)(c & 0xFF)));
      }
      i++;
    }
    // create date
    char tsbuf[30];
    struct timeval t;
    gettimeofday(&t, NULL);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
    // output
    fputs(tsbuf, stderr);
    if (isMultiline)
      fputs(":\n", stderr);
    else
      fputs(": ", stderr);
    fputs(message.c_str(), stderr);
    pthread_mutex_unlock(&reportMutex);
  }
}


void Logger::logSysError(int aErrlevel, int aErrNum)
{
  if (logEnabled(aErrlevel)) {
    // obtain error number if none specified
    if (aErrNum==0)
      aErrNum = errno;
    // obtain error message
    char buf[LOGBUFSIZ];
    strerror_r(aErrNum, buf, LOGBUFSIZ);
    // show it
    log(aErrlevel, "System error message: %s\n", buf);
  }
}


void Logger::setLogLevel(int aLogLevel)
{
  if (aLogLevel<LOG_EMERG || aLogLevel>LOG_DEBUG) return;
  logLevel = aLogLevel;
}
