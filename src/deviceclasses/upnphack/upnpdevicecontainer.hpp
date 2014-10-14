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
    UpnpDeviceContainer(int aInstanceNumber, DeviceContainer *aDeviceContainerP, int aTag);

    virtual const char *deviceClassIdentifier() const;

    virtual void collectDevices(CompletedCB aCompletedCB, bool aIncremental, bool aExhaustive);

    /// @return human readable, language independent suffix to explain vdc functionality.
    ///   Will be appended to product name to create modelName() for vdcs
    virtual string vdcModelSuffix() { return "UPnP"; }

  private:
    SsdpSearchPtr m_dmr_search;

    void collectHandler(CompletedCB aCompletedCB, SsdpSearchPtr aSsdpSearch, ErrorPtr aError);
  };

} // namespace p44


#endif /* defined(__vdcd__upnpdevicecontainer__) */
