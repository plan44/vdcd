//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of vdcd.
//
//  vdcd is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  vdcd is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with vdcd. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __vdcd__dsbehaviour__
#define __vdcd__dsbehaviour__

#include "deviceclasscontainer.hpp"

#include "dsuid.hpp"
#include "dsdefs.h"

#include "dsscene.hpp"

using namespace std;

namespace p44 {

  // offset to differentiate containers and property keys for descriptions, settings and states
  enum {
    descriptions_key_offset = 1000,
    settings_key_offset = 2000,
    states_key_offset = 3000
  };


  typedef enum {
    behaviour_undefined,
    behaviour_button,
    behaviour_binaryinput,
    behaviour_output,
    behaviour_sensor
  } BehaviourType;


  class Device;

  class DsBehaviour;

  class DsScene;

  class ButtonBehaviour;
  class OutputBehaviour;
  class BinaryInputBehaviour;
  class SensorBehaviour;


  /// a DsBehaviour represents and implements a device behaviour according to dS specs
  /// (for example: the dS Light state machine). The interface of a DsBehaviour is generic
  /// such that it can be used by different physical implementations (e.g. both DALI devices
  /// and hue devices will make use of the dS light state machine behaviour.
  class DsBehaviour : public PropertyContainer, public PersistentParams
  {
    typedef PropertyContainer inheritedProps;
    typedef PersistentParams inheritedParams;

    friend class Device;
    friend class DsScene;

  protected:

    /// the device this behaviour belongs to
    Device &device;

    /// the index of this behaviour in the device's vector
    size_t index;

  protected:

    /// @name behaviour description, constants or variables
    ///   set by device implementations when adding a Behaviour.
    /// @{
    string hardwareName; ///< name that identifies this behaviour among others for the human user (terminal label text etc)
    /// @}

    /// @name persistent settings
    /// @{
    /// @}

    /// @name internal volatile state
    /// @{
    DsHardwareError hardwareError; ///< hardware error
    MLMicroSeconds hardwareErrorUpdated; ///< when was hardware error last updated
    /// @}


  public:
    DsBehaviour(Device &aDevice);
    virtual ~DsBehaviour();

    /// device type identifier
    /// @return constant identifier for this type of behaviour
    /// @note default is the basic behaviour type name. Subclasses need to override this method to get separate identification!
    virtual const char *behaviourTypeIdentifier() { return getTypeName(); }

    /// initialisation of hardware-specific constants for this button input
    /// @param aHardwareName name to identify this functionality in hardware (like input terminal label, button label or kind etc.)
    /// @note this must be called once before the device gets added to the device container.
    void setHardwareName(const string &aHardwareName) { hardwareName = aHardwareName; };

    /// @return hardware name
    string getHardwareName() { return hardwareName; };

    /// update of hardware status
    void setHardwareError(DsHardwareError aHardwareError);

    /// set group
    virtual void setGroup(DsGroup aGroup) { /* NOP in base class */ };

    /// push state
    /// @return true if API was connected and push could be sent
    bool pushBehaviourState();


    /// @name persistent settings management
    /// @{

    /// load behaviour parameters from persistent DB
    ErrorPtr load();

    /// save unsaved behaviour parameters to persistent DB
    ErrorPtr save();

    /// forget any parameters stored in persistent DB
    ErrorPtr forget();

    /// @}

    /// get the index value
    /// @return index of this behaviour in one of the owning device's behaviour lists
    size_t getIndex() { return index; };

    /// textual representation of getType()
    /// @return type string, which is the string used to prefix the xxxDescriptions, xxxSettings and xxxStates properties
    /// @note this only identifies the basic behaviour type. Subclassed behaviours can only be identified using behaviourTypeIdentifier()
    const char *getTypeName();

    /// description of object, mainly for debug and logging
    /// @return textual description of object, may contain LFs
    virtual string description();

    /// short (text without LFs!) description of object, mainly for referencing it in log messages
    /// @return textual description of object
    virtual string shortDesc();

  protected:

    /// type of behaviour
    virtual BehaviourType getType() = 0;

    /// @name property access implementation for descriptor/settings/states
    /// @{

    /// @return number of description (readonly) properties
    virtual int numDescProps() { return 0; };

    /// @param aPropIndex the description property index
    /// @return description (readonly) property descriptor
    virtual const PropertyDescriptorPtr getDescDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor) { return NULL; };

    /// @return number of settings (read/write) properties
    virtual int numSettingsProps() { return 0; };

    /// @param aPropIndex the settings property index
    /// @return settings (read/write) property descriptor
    virtual const PropertyDescriptorPtr getSettingsDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor) { return NULL; };

    /// @return number of states (read/write) properties
    virtual int numStateProps() { return 0; };

    /// @param aPropIndex the states property index
    /// @return states (read/write) property descriptor
    virtual const PropertyDescriptorPtr getStateDescriptorByIndex(int aPropIndex, PropertyDescriptorPtr aParentDescriptor) { return NULL; };


    /// access single field in this behaviour
    /// @param aMode access mode (see PropertyAccessMode: read, write or write preload)
    /// @param aPropertyDescriptor decriptor for a single value field/array in this behaviour.
    /// @param aPropValue JsonObject with a single value
    /// @param aIndex in case of array, the index of the element to access
    /// @return false if value could not be accessed
    virtual bool accessField(PropertyAccessMode aMode, ApiValuePtr aPropValue, PropertyDescriptorPtr aPropertyDescriptor);

    /// @}

    /// only for deeper levels
    virtual int numProps(int aDomain, PropertyDescriptorPtr aParentDescriptor);

  private:

    // key for saving this behaviour in the DB
    string getDbKey();

    // property access basic dispatcher implementation
    virtual PropertyDescriptorPtr getDescriptorByIndex(int aPropIndex, int aDomain, PropertyDescriptorPtr aParentDescriptor);
    int numLocalProps(PropertyDescriptorPtr aParentDescriptor);

  };
  typedef boost::intrusive_ptr<DsBehaviour> DsBehaviourPtr;

} // namespace p44


#endif /* defined(__vdcd__dsbehaviour__) */
