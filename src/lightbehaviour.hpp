//
//  lightbehaviour.hpp
//  p44bridged
//
//  Created by Lukas Zeller on 19.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#ifndef __p44bridged__lightbehaviour__
#define __p44bridged__lightbehaviour__

#include "device.hpp"

using namespace std;

namespace p44 {

  typedef uint8_t Brightness;
  typedef uint8_t SceneNo;


  class PersistentParams
  {
  public:
    PersistentParams();
    bool dirty; ///< if set, means that values need to be saved 
  };



  class LightScene : public PersistentParams
  {
  public:
    LightScene(SceneNo aSceneNo); ///< constructor, sets values according to dS specs' default values
    SceneNo sceneNo; ///< scene number
    Brightness sceneValue; ///< output value for this scene
    bool dontCare; ///< if set, applying this scene does not change the output value
    bool ignoreLocalPriority; ///< if set, local priority is ignored when calling this scene
    bool specialBehaviour; ///< special behaviour active
    bool flashing; ///< flashing active for this scene
    bool slowTransition; ///< set if transition must be slow
  };
  typedef boost::shared_ptr<LightScene> LightScenePtr;
  typedef map<SceneNo, LightScenePtr> LightSceneMap;



  /// the persistent parameters of a device with light behaviour
  class LightSettings : public PersistentParams
  {
  public:
    bool isDimmable; ///< if set, device can be dimmed
    Brightness onThreshold; ///< if !isDimmable, output will be on when output value is >= the threshold
    Brightness minDim; ///< minimal dimming value, dimming down will not go below this
    Brightness maxDim; ///< maximum dimming value, dimming up will not go above this
  private:
    LightSceneMap scenes; ///< the user defined light scenes (default scenes will be created on the fly)
  public:
    LightScenePtr getScene(SceneNo aSceneNo);
  };


  class LightBehaviour : public DSBehaviour
  {
    bool localPriority; ///< if set, device is in local priority, i.e. ignores scene calls
    bool isLocigallyOn; ///< if set, device is logically ON (but may be below threshold to enable the output)
    Brightness outputValue; ///< current internal output value. For non-dimmables, output is on only if outputValue>onThreshold

  public:
    /// @return current brightness value, 0..255, linear brightness as perceived by humans (half value = half brightness)
    Brightness getBrightness();

  };

}

#endif /* defined(__p44bridged__lightbehaviour__) */
