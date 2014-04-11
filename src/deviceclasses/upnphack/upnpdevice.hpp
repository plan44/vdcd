//
//  upnpdevice.hpp
//  vdcd
//

#ifndef __vdcd__upnpdevice__
#define __vdcd__upnpdevice__

#include "device.hpp"
#include "ssdpsearch.hpp"


using namespace std;

namespace p44 {

  class UpnpDeviceContainer;
  class UpnpDevice;

  typedef boost::intrusive_ptr<UpnpDevice> UpnpDevicePtr;
  class UpnpDevice : public Device
  {
    typedef Device inherited;

    string descriptionURL;
    string upnpDeviceUUID;
    long presenceTicket;

  public:
    UpnpDevice(UpnpDeviceContainer *aClassContainerP, string aLocation, string aUuid);

    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// check presence of UPnP device
    /// @param aPresenceResultHandler will be called to report presence status
    virtual void checkPresence(PresenceCB aPresenceResultHandler);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @return hardware GUID in URN format to identify hardware as uniquely as possible
    /// @note when grouping vdSDs which belong to the same hardware device using numDevicesInHW() and deviceIndexInHW()
    ///   hardwareGUID() must return the same unique ID for the containing hardware device for all contained dSDs
    virtual string hardwareGUID();

    /// @}

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

  protected:

    // property access implementation
    virtual int numProps(int aDomain);
    virtual const PropertyDescriptor *getPropertyDescriptor(int aPropIndex, int aDomain);
    virtual bool accessField(bool aForWrite, ApiValuePtr aPropValue, const PropertyDescriptor &aPropertyDescriptor, int aIndex);

  private:

    void deriveDsUid();
    void presenceHandler(PresenceCB aPresenceResultHandler, SsdpSearch *aSsdpSearchP, ErrorPtr aError);
    void timeoutHandler(PresenceCB aPresenceResultHandler, SsdpSearchPtr aSrch);

  };

} // namespace p44

#endif /* defined(__vdcd__upnpdevice__) */
