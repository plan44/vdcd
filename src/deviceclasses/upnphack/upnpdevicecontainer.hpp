//
//  upnpdevicecontainer.hpp
//  vdcd
//

#ifndef __vdcd__upnpdevicecontainer__
#define __vdcd__upnpdevicecontainer__

#include "vdcd_common.hpp"
#include "ssdpsearch.hpp"
#include "deviceclasscontainer.hpp"

using namespace std;

namespace p44 {

  class UpnpDeviceContainer;
  typedef boost::intrusive_ptr<UpnpDeviceContainer> UpnpDeviceContainerPtr;
  class UpnpDeviceContainer : public DeviceClassContainer
  {
    typedef DeviceClassContainer inherited;

  public:
    UpnpDeviceContainer(int aInstanceNumber);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// @return human readable model name/short description
    virtual string modelName() { return "UPnP vDC"; }

  private:
    SsdpSearch m_dmr_search;

    void collectHandler(CompletedCB aCompletedCB, SsdpSearch *aSsdpSearchP, ErrorPtr aError);
  };

} // namespace p44


#endif /* defined(__vdcd__upnpdevicecontainer__) */
