//
//  device.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 18.04.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__device__
#define __p44bridged__device__

#include "deviceclasscontainer.hpp"

#include "dsid.hpp"

using namespace std;

namespace p44 {

  class Device;
  class DSBehaviour;


  /// a DSBehaviour represents and implements a device behaviour according to dS specs
  /// (for example: the dS Light state machine). The interface of a DSBehaviour is generic
  /// such that it can be used by different physical implementations (e.g. both DALI devices
  /// and hue devices will make use of the dS light state machine behaviour.
  class DSBehaviour
  {
  public:

    /// @name functional identification for digitalSTROM system
    /// @{

    virtual uint16_t functionId() = 0;
    virtual uint16_t productId() = 0;
    virtual uint16_t groupMemberShip() = 0;
    virtual uint8_t ltMode() = 0;
    virtual uint8_t outputMode() = 0;
    virtual uint8_t buttonIdGroup() = 0;

    /// @}


  };


  typedef boost::shared_ptr<Device> DevicePtr;
  /// base class representing a virtual digitalSTROM device
  /// for each type of subsystem (enOcean, DALI, ...) this class is subclassed to implement
  /// the device class' specifics.
  class Device
  {
    bool registered; ///< set when registered by dS system
    DSBehaviour *behaviourP; ///< private, owned instance of the behaviour.
  protected:
    DeviceClassContainer *classContainerP;
  public:
    Device(DeviceClassContainer *aClassContainerP);
    virtual ~Device();

    /// get pointer to the behaviour
    /// @return the behaviour. If NULL, the device ist not yet set up and cannot be operated
    DSBehaviour *getDSBehaviour() { return behaviourP; };

    /// description of object, mainly for debug and logging
    /// @return textual description of object
    virtual string description();

    /// the digitalstrom ID
    dSID dsid;
  };


} // namespace p44


#endif /* defined(__p44bridged__device__) */
