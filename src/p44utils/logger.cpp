//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "logger.hpp"

#include "utils.hpp"

using namespace p44;

p44::Logger globalLogger;

Logger::Logger()
{
  pthread_mutex_init(&reportMutex, NULL);
  logLevel = LOGGER_DEFAULT_LOGLEVEL;
  stderrLevel = LOG_ERR;
  errToStdout = true;
}

#define LOGBUFSIZ 8192


bool Logger::logEnabled(int aErrLevel)
{
  return (aErrLevel<=logLevel);
}


void Logger::log(int aErrLevel, const char *aFmt, ... )
{
  if (logEnabled(aErrLevel) || aErrLevel<=stderrLevel) {
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
    char tsbuf[40];
    char *p = tsbuf;
    struct timeval t;
    gettimeofday(&t, NULL);
    p += strftime(p, sizeof(tsbuf), "[%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
    p += sprintf(p, ".%03d]", t.tv_usec/1000);
    // output
    if (aErrLevel<=stderrLevel) {
      // must go to stderr anyway
      fputs(tsbuf, stderr);
      if (isMultiline)
        fputs("\n", stderr);
      else
        fputs(" ", stderr);
      fputs(message.c_str(), stderr);
      fflush(stderr);
    }
    if (logEnabled(aErrLevel) && (aErrLevel>stderrLevel || errToStdout)) {
      // must go to stdout as well
      fputs(tsbuf, stdout);
      if (isMultiline)
        fputs("\n", stdout);
      else
        fputs(" ", stdout);
      fputs(message.c_str(), stdout);
      fflush(stdout);
    }
    pthread_mutex_unlock(&reportMutex);
  }
}


void Logger::logSysError(int aErrLevel, int aErrNum)
{
  if (logEnabled(aErrLevel)) {
    // obtain error number if none specified
    if (aErrNum==0)
      aErrNum = errno;
    // obtain error message
    char buf[LOGBUFSIZ];
    strerror_r(aErrNum, buf, LOGBUFSIZ);
    // show it
    log(aErrLevel, "System error message: %s\n", buf);
  }
}


void Logger::setLogLevel(int aLogLevel)
{
  if (aLogLevel<LOG_EMERG || aLogLevel>LOG_DEBUG) return;
  logLevel = aLogLevel;
}


void Logger::setErrLevel(int aStderrLevel, bool aErrToStdout)
{
  if (aStderrLevel<LOG_EMERG || aStderrLevel>LOG_DEBUG) return;
  stderrLevel = aStderrLevel;
  errToStdout = aErrToStdout;
}
