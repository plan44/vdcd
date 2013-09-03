//
//  ssdpsearch.hpp
//  p44utils
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__ssdpsearch__
#define __vdcd__ssdpsearch__

#include "socketcomm.hpp"

using namespace std;

namespace p44 {

  // Errors
  typedef enum {

  } SsdpErrors;

  class SsdpError : public Error
  {
  public:
    static const char *domain() { return "Ssdp"; }
    virtual const char *getErrorDomain() const { return SsdpError::domain(); };
    SsdpError(SsdpErrors aError) : Error(ErrorCode(aError)) {};
    SsdpError(SsdpErrors aError, std::string aErrorMessage) : Error(ErrorCode(aError), aErrorMessage) {};
  };


  class SsdpSearch;

  /// callback for delivering a Ssdp result
  typedef boost::function<void (SsdpSearch *aSsdpSearchP, ErrorPtr aError)> SsdpSearchCB;

  typedef boost::intrusive_ptr<SsdpSearch> SsdpSearchPtr;
  /// A class providing basic Ssdp service discovery
  class SsdpSearch : public SocketComm
  {
    typedef SocketComm inherited;

    SsdpSearchCB searchResultHandler;
    string searchTarget;

  public:

    SsdpSearch(SyncIOMainLoop *aMainLoopP);
    virtual ~SsdpSearch();

    /// start a SSDP search
    /// @param aSearchTarget search target string, like "ssdp:all" or "upnp:rootdevice"
    /// @param aSearchResultHandler will be called when a Ssdp search result has been received, or a search times out
    void startSearch(const string &aSearchTarget, SsdpSearchCB aSearchResultHandler);



  private:
    void gotData(ErrorPtr aError);
    void socketStatusHandler(ErrorPtr aError);
    
  };
  
} // namespace p44


#endif /* defined(__vdcd__ssdpsearch__) */
