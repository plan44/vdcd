//
//  huedevicecontainer.cpp
//  vdcd
//
//  Created by Lukas Zeller on 02.09.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "huedevicecontainer.hpp"

#include "huedevice.hpp"

using namespace p44;


HueDeviceContainer::HueDeviceContainer(int aInstanceNumber) :
  inherited(aInstanceNumber)
{
}

const char *HueDeviceContainer::deviceClassIdentifier() const
{
  return "hue_Lights_Container";
}


void HueDeviceContainer::collectDevices(CompletedCB aCompletedCB, bool aExhaustive)
{
  brigdeSearcher = SsdpSearchPtr(new SsdpSearch(SyncIOMainLoop::currentMainLoop()));
  brigdeSearcher->startSearch("upnp:rootdevice", boost::bind(&HueDeviceContainer::bridgeDiscoveryHandler, this, _1, _2));

  #warning // TODO: actually implement
  aCompletedCB(ErrorPtr()); // %%% just call it completed for now
}


void HueDeviceContainer::bridgeDiscoveryHandler(SsdpSearch *aSsdpSearchP, ErrorPtr aError)
{
  
}
