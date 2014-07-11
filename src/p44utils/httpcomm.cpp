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

#include "httpcomm.hpp"


using namespace p44;

HttpComm::HttpComm(SyncIOMainLoop &aMainLoop) :
  mainLoop(aMainLoop),
  requestInProgress(false),
  mgConn(NULL),
  responseDataFd(-1)
{
}


HttpComm::~HttpComm()
{
  responseCallback.clear(); // prevent calling back now
  cancelRequest(); // make sure subthread is cancelled
}


//  I used mg_download to establish the http connection and then mg_read to get the http response.
//  You will have to create your HTTP header in the mg_download line:
//
//  connection = mg_download (
//                 host,
//                 port,
//                 0,
//                 ebuf,
//                 sizeof(ebuf),
//                 "GET /pgrest/%s HTTP/1.1\r\nHost: %s\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n",
//                 queryUrl,
//                 host
//              );
//
//  Then you can use the connection returned by mg_download in the mg_read().
//  Others have used this to enable streaming also.



void HttpComm::requestThread(ChildThreadWrapper &aThread)
{
  string protocol, hostSpec, host, doc;
  uint16_t port;

  requestError.reset();
  response.clear();
  splitURL(requestURL.c_str(), &protocol, &hostSpec, &doc, NULL, NULL);
  bool useSSL = false;
  if (protocol=="http") {
    port = 80;
    useSSL = false;
  }
  else if (protocol=="https") {
    port = 443;
    useSSL = true;
  }
  else {
    requestError = ErrorPtr(new HttpCommError(HttpCommError_invalidParameters, "invalid protocol"));
  }
  splitHost(hostSpec.c_str(), &host, &port);
  if (Error::isOK(requestError)) {
    // now issue request
    const size_t ebufSz = 100;
    char ebuf[ebufSz];
    if (requestBody.length()>0) {
      // is a request which sends data in the HTTP message body (e.g. POST)
      mgConn = mg_download(
        host.c_str(),
        port,
        useSSL,
        ebuf, ebufSz,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: %s; charset=UTF-8\r\n"
        "Content-Length: %ld\r\n"
        "\r\n"
        "%s",
        method.c_str(),
        doc.c_str(),
        host.c_str(),
        contentType.c_str(),
        requestBody.length(),
        requestBody.c_str()
      );
    }
    else {
      // no request body (e.g. GET, DELETE)
      mgConn = mg_download(
        host.c_str(),
        port,
        useSSL,
        ebuf, ebufSz,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
//        "Content-Type: %s; charset=UTF-8\r\n"
        "\r\n",
        method.c_str(),
        doc.c_str(),
        host.c_str()
//        ,contentType.c_str()
      );
    }
    if (!mgConn) {
      requestError = ErrorPtr(new HttpCommError(HttpCommError_mongooseError, ebuf));
    }
    else {
      // successfully initiated connection
      // - get headers if requested
      if (responseHeaders) {
        struct mg_request_info *requestInfo = mg_get_request_info(mgConn);
        if (requestInfo) {
          for (int i=0; i<requestInfo->num_headers; i++) {
            (*responseHeaders)[requestInfo->http_headers[i].name] = requestInfo->http_headers[i].value;
          }
        }
      }
      // - read data
      const size_t bufferSz = 2048;
      uint8_t *bufferP = new uint8_t[bufferSz];
      while (true) {
        ssize_t res = mg_read(mgConn, bufferP, bufferSz);
        if (res==0) {
          // connection has closed, all bytes read
          break;
        }
        else if (res<0) {
          // read error
          requestError = ErrorPtr(new HttpCommError(HttpCommError_read));
          break;
        }
        else {
          // data read
          if (responseDataFd>=0) {
            // write to fd
            write(responseDataFd, bufferP, res);
          }
          else {
            // collect in string
            response.append((const char *)bufferP, (size_t)res);
          }
        }
      }
      delete[] bufferP;
      mg_close_connection(mgConn);
    }
  }
  // ending the thread function will call the requestThreadSignal on the main thread
}



void HttpComm::requestThreadSignal(ChildThreadWrapper &aChildThread, ThreadSignals aSignalCode)
{
  DBGLOG(LOG_DEBUG,"HttpComm: Received signal from child thread: %d\n", aSignalCode);
  if (aSignalCode==threadSignalCompleted) {
    DBGLOG(LOG_DEBUG,"- HTTP subthread exited - request completed\n");
    requestInProgress = false; // thread completed
    // call back with result of request
    // Note: as this callback might initiate another request already and overwrite the callback, copy it here
    HttpCommCB cb = responseCallback;
    string resp = response;
    ErrorPtr reqErr = requestError;
    // release child thread object now
    responseCallback.clear();
    childThread.reset();
    // now execute callback
    if (cb) {
      cb(resp, reqErr);
    }
  }
}



bool HttpComm::httpRequest(
  const char *aURL,
  HttpCommCB aResponseCallback,
  const char *aMethod,
  const char* aRequestBody,
  const char* aContentType,
  int aResponseDataFd,
  bool aSaveHeaders
)
{
  if (requestInProgress || !aURL)
    return false; // blocked or no URL
  responseDataFd = aResponseDataFd;
  responseHeaders.reset();
  if (aSaveHeaders)
    responseHeaders = HttpHeaderMapPtr(new HttpHeaderMap);
  requestURL = aURL;
  responseCallback = aResponseCallback;
  method = aMethod;
  requestBody = nonNullCStr(aRequestBody);
  if (aContentType)
    contentType = aContentType; // use specified content type
  else
    contentType = defaultContentType(); // use default for the class
  // now let subthread handle this
  requestInProgress = true;
  childThread = SyncIOMainLoop::currentMainLoop().executeInThread(
    boost::bind(&HttpComm::requestThread, this, _1),
    boost::bind(&HttpComm::requestThreadSignal, this, _1, _2)
  );
  return true; // could be initiated (even if immediately ended due to error, but callback was called)
}


void HttpComm::cancelRequest()
{
  if (requestInProgress && childThread) {
    childThread->cancel();
    requestInProgress = false; // prevent cancelling multiple times
  }
}

#pragma mark - Utilities


//  8.2.1. The form-urlencoded Media Type
//
//     The default encoding for all forms is `application/x-www-form-
//     urlencoded'. A form data set is represented in this media type as
//     follows:
//
//          1. The form field names and values are escaped: space
//          characters are replaced by `+', and then reserved characters
//          are escaped as per [URL]; that is, non-alphanumeric
//          characters are replaced by `%HH', a percent sign and two
//          hexadecimal digits representing the ASCII code of the
//          character. Line breaks, as in multi-line text field values,
//          are represented as CR LF pairs, i.e. `%0D%0A'.
//
//          2. The fields are listed in the order they appear in the
//          document with the name separated from the value by `=' and
//          the pairs separated from each other by `&'. Fields with null
//          values may be omitted. In particular, unselected radio
//          buttons and checkboxes should not appear in the encoded
//          data, but hidden fields with VALUE attributes present
//          should.
//
//              NOTE - The URI from a query form submission can be
//              used in a normal anchor style hyperlink.
//              Unfortunately, the use of the `&' character to
//              separate form fields interacts with its use in SGML
//              attribute values as an entity reference delimiter.
//              For example, the URI `http://host/?x=1&y=2' must be
//              written `<a href="http://host/?x=1&#38;y=2"' or `<a
//              href="http://host/?x=1&amp;y=2">'.
//
//              HTTP server implementors, and in particular, CGI
//              implementors are encouraged to support the use of
//              `;' in place of `&' to save users the trouble of
//              escaping `&' characters this way.

string HttpComm::urlEncode(const string &aString, bool aFormURLEncoded)
{
  string result;
  const char *p = aString.c_str();
  while (char c = *p++) {
    if (aFormURLEncoded && c==' ')
      result += '+'; // replace spaces by pluses
    else if (c=='&' || c=='%' || c=='=' || (aFormURLEncoded && c=='+') || c<=0x20 || c>0x7E)
      string_format_append(result, "%%%02X", c);
    else
      result += c; // just append
  }
  return result;
}


void HttpComm::appendFormValue(string &aDataString, const string &aFieldname, const string &aValue)
{
  if (aDataString.size()>0) aDataString += '&';
  aDataString += urlEncode(aFieldname, true);
  aDataString += '=';
  aDataString += urlEncode(aValue, true);
}


