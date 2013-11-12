//
//  demodevice.hpp
//  vdcd
//
//  Created by Lukas Zeller on 2013-11-11
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __vdcd__demodevice__
#define __vdcd__demodevice__

#include "device.hpp"


using namespace std;

namespace p44 {

  class DemoDeviceContainer;
  class DemoDevice;

  typedef boost::intrusive_ptr<DemoDevice> DemoDevicePtr;
  class DemoDevice : public Device
  {
    typedef Device inherited;

  public:
    DemoDevice(DemoDeviceContainer *aClassContainerP,
               const std::string location, const std::string uuid);
    
    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();


    /// @name interaction with subclasses, actually representing physical I/O
    /// @{

    /// set new output value on device
    /// @param aOutputBehaviour the output behaviour which has a new output value to be sent to the hardware output
    /// @note depending on how the actual device communication works, the implementation might need to consult all
    ///   output behaviours to collect data for an outgoing message.
    virtual void updateOutputValue(OutputBehaviour &aOutputBehaviour);

    /// @}


    /// @name identification of the addressable entity
    /// @{

    /// @return human readable model name/short description
    virtual string modelName();

    /// @}

    string getDeviceDescriptionURL() const;

  protected:

    void deriveDsUid();
    std::string m_locationURL;
    std::string m_uuid;
  };

} // namespace p44

#endif /* defined(__vdcd__demodevice__) */
