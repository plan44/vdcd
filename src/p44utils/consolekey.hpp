//
//  Copyright (c) 2013-2014 plan44.ch / Lukas Zeller, Zurich, Switzerland
//
//  Author: Lukas Zeller <luz@plan44.ch>
//
//  This file is part of p44utils.
//
//  p44utils is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  p44utils is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with p44utils. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __p44utils__consolekey__
#define __p44utils__consolekey__

#include "p44_common.hpp"

using namespace std;

namespace p44 {

  class ConsoleKeyManager;

  /// Wrapper to use single keys on the console as simulated buttons
  class ConsoleKey : public P44Obj
  {
    friend class ConsoleKeyManager;

  public:
    /// button event handler
    /// @param aConsoleKeyP the consolekey object
    /// @param aNewState the current state of the key
    /// @param aTimestamp the main loop timestamp of the key action
    typedef boost::function<void (bool aNewState, MLMicroSeconds aTimestamp)> ConsoleKeyHandlerCB;

  private:
    char keyCode;
    string description;
    bool state;
    bool initialState;
    bool canToggle;
    ConsoleKeyHandlerCB keyHandler;

    ConsoleKey(char aKeyCode, const char *aDescription, bool aInitialState=false);

  public:

    virtual ~ConsoleKey();

    /// get state of input
    /// @return true if key is pressed
    bool isSet();

    /// set handler for when key state changes
    /// @param handler to call when input state changes
    void setConsoleKeyHandler(ConsoleKeyHandlerCB aHandler);

  private:
    void setState(bool aState);
    void toggle();
    void pulse();
    void pulseEnd();
    void reportState();

  };
	typedef boost::intrusive_ptr<ConsoleKey> ConsoleKeyPtr;


  typedef std::map<char, ConsoleKeyPtr> ConsoleKeyMap;



  /// manager of console keys
  class ConsoleKeyManager
  {
    friend class ConsoleKey;
  public:
    /// button event handler
    /// @param aConsoleKeyManagerP the console key manager
    /// @param aKeyPress key pressed
    /// @param aTimestamp the main loop timestamp of the button action
    /// @return true if fully handled already
    typedef boost::function<bool (char aKeyPress)> ConsoleKeyPressCB;

  private:

    ConsoleKeyMap keyMap;
    bool termInitialized;
    ConsoleKeyPressCB keyPressHandler;

    ConsoleKeyManager();
    virtual ~ConsoleKeyManager();

  public:
    static ConsoleKeyManager *sharedKeyManager();

    /// install a callback for when a key is pressed
    /// @param handler to call when a key is pressed on the console
    void setKeyPressHandler(ConsoleKeyPressCB aHandler);

    /// create a new console key
    /// @param aKeyCode ASCII-code of the key. A-Z are special, as
    ///   for these typing the lowercase (a-z) means pulsing the input state,
    ///   while typing the uppercase (A-Z) means toggling the input state.
    /// @param description description shown on console at initialisation and when key
    ///   is operated.
    ConsoleKeyPtr newConsoleKey(char aKeyCode, const char *aDescription, bool aInitialState=false);

  private:
    int kbHit();
    bool consoleKeyPoll();

  };

} // namespace


#endif /* defined(__p44utils__consolekey__) */
