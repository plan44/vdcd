//
//  demodevice.cpp
//  vdcd
//
//  Created by Lukas Zeller on 2013-11-11
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "demodevice.hpp"

#include "fnv.hpp"

#include "buttonbehaviour.hpp"
#include "lightbehaviour.hpp"

using namespace p44;


DemoDevice::DemoDevice(DemoDeviceContainer *aClassContainerP, std::string location, std::string uuid) :
  Device((DeviceClassContainer *)aClassContainerP),
  m_locationURL(location),
  m_uuid(uuid)
{
  // a demo device is a light which shows its dimming value as a string of 0..50 hashes on the console
  // - is a light device
  primaryGroup = group_cyan_audio;
  /*
  // - use light settings, which include a fully functional scene table
  deviceSettings = DeviceSettingsPtr(new LightDeviceSettings(*this));
  // - create one output with light behaviour
  LightBehaviourPtr l = LightBehaviourPtr(new LightBehaviour(*this));
  // - set default config to act as dimmer with variable ramps
  l->setHardwareOutputConfig(outputFunction_dimmer, usage_undefined, true, -1);
  addBehaviour(l);
  // - hardware is defined, now derive dSUID
  */
	deriveDsUid();
}


void DemoDevice::updateOutputValue(OutputBehaviour &aOutputBehaviour)
{
  // as this demo device has only one output
  if (aOutputBehaviour.getIndex()==0) {
    // This would be the place to implement sending the output value to the hardware
    // For the demo device, we show the output as a bar of 0..50 '#' chars
    // - read the output value from the behaviour
    int hwValue = aOutputBehaviour.valueForHardware();
    // - display as a bar of hash chars
    string bar;
    while (hwValue>0) {
      // one hash character per 4 output value steps (0..255 = 0..64 hashes)
      bar += '#';
      hwValue -= 4;
    }
    printf("Demo Device Output: %s\n", bar.c_str());
  }
  else
    return inherited::updateOutputValue(aOutputBehaviour); // let superclass handle this
}



void DemoDevice::deriveDsUid()
{
  Fnv64 hash;

  if (getDeviceContainer().usingDsUids()) {
    // vDC implementation specific UUID:
    //   UUIDv5 with name = classcontainerinstanceid::SingularDemoDevice
    DsUid vdcNamespace(DSUID_P44VDC_NAMESPACE_UUID);
    string s = classContainerP->deviceClassContainerInstanceIdentifier();
    s += "::SingularDemoDevice";
    dSUID.setNameInSpace(s, vdcNamespace);
  }
  else {
    // we have no unqiquely defining device information, construct something as reproducible as possible
    // - use class container's ID
    string s = classContainerP->deviceClassContainerInstanceIdentifier();
    hash.addBytes(s.size(), (uint8_t *)s.c_str());
    // - add-in the Demo name
    hash.addCStr("SingularDemoDevice");
    dSUID.setObjectClass(DSID_OBJECTCLASS_DSDEVICE);
    dSUID.setDsSerialNo(hash.getHash28()<<4); // leave lower 4 bits for input number
  }
}


string DemoDevice::modelName()
{
  return "Demo UPnP";
}


string DemoDevice::description()
{
  string s = inherited::description();
  string_format_append(s, "- Demo output to console\n");
  return s;
}

string DemoDevice::getDeviceDescriptionURL() const
{
    return m_locationURL;
}
