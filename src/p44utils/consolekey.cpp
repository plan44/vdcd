//
//  Copyright (c) 2013-2015 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "consolekey.hpp"


#include <sys/select.h>
#include <termios.h>
#include <sys/ioctl.h>

using namespace p44;


#pragma mark - console key

ConsoleKey::ConsoleKey(char aKeyCode, const char *aDescription, bool aInitialState) :
  keyHandlerTicket(0)
{
  // check type of key
  initialState = aInitialState;
  canToggle = false;
  char c = tolower(aKeyCode);
  if (c>='a' && c<='z') {
    aKeyCode = c; // use lowercase only
    canToggle = true;
  }
  // store params
  keyCode = aKeyCode;
  description = aDescription;
  // display usage
  if (canToggle)
    printf("- Console input '%s' - Press '%c' to pulse, '%c' to toggle state\n", description.c_str(), keyCode, toupper(keyCode));
  else
    printf("- Console input '%s' - Press '%c' to pulse\n", description.c_str(), keyCode);
  // initial state
  state = aInitialState;
  if (state) {
    printf("  Initial state is active: 1\n");
  }
}


ConsoleKey::~ConsoleKey()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(keyHandlerTicket);
}


bool ConsoleKey::isSet()
{
  return state;
}


void ConsoleKey::setConsoleKeyHandler(ConsoleKeyHandlerCB aHandler)
{
  keyHandler = aHandler;
}


void ConsoleKey::setState(bool aState)
{
  MainLoop::currentMainLoop().cancelExecutionTicket(keyHandlerTicket);
  state = aState;
  printf("- Console input '%s' - changed to %d\n", description.c_str(), state);
  reportState();
}


void ConsoleKey::toggle()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(keyHandlerTicket);
  state = !state;
  printf("- Console input '%s' - toggled to %d\n", description.c_str(), state);
  reportState();
}


void ConsoleKey::pulse()
{
  MainLoop::currentMainLoop().cancelExecutionTicket(keyHandlerTicket);
  keyHandlerTicket = MainLoop::currentMainLoop().executeOnce(boost::bind(&ConsoleKey::pulseEnd, this), 200*MilliSecond);
  if (state==initialState) {
    state = !initialState;
    reportState();
  }
  printf("- Console input '%s' - pulsed to %d for 200mS\n", description.c_str(), state);
}


void ConsoleKey::pulseEnd()
{
  if (state!=initialState) {
    state = initialState;
    reportState();
  }
}


void ConsoleKey::reportState()
{
  if (keyHandler) {
    keyHandler(state, MainLoop::now());
  }
}



#pragma mark - console key manager singleton

static ConsoleKeyManager *consoleKeyManager = NULL;


ConsoleKeyManager *ConsoleKeyManager::sharedKeyManager()
{
  if (consoleKeyManager==NULL) {
    consoleKeyManager = new ConsoleKeyManager();
  }
  return consoleKeyManager;
}


ConsoleKeyManager::ConsoleKeyManager() :
  termInitialized(false)
{
  // install polling
  MainLoop::currentMainLoop().registerIdleHandler(this, boost::bind(&ConsoleKeyManager::consoleKeyPoll, this));
}


ConsoleKeyManager::~ConsoleKeyManager()
{
  MainLoop::currentMainLoop().unregisterIdleHandlers(this);
}


ConsoleKeyPtr ConsoleKeyManager::newConsoleKey(char aKeyCode, const char *aDescription,  bool aInitialState)
{
  ConsoleKeyPtr newKey = ConsoleKeyPtr(new ConsoleKey(aKeyCode, aDescription, aInitialState));
  // store in map
  keyMap[newKey->keyCode] = newKey;
  // return
  return newKey;
}


void ConsoleKeyManager::setKeyPressHandler(ConsoleKeyPressCB aHandler)
{
  keyPressHandler = aHandler;
}






int ConsoleKeyManager::kbHit()
{
  static const int STDIN = 0;

  if (!termInitialized) {
    // Use termios to turn off line buffering
    termios term;
    tcgetattr(STDIN, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(STDIN, TCSANOW, &term);
    setbuf(stdin, NULL);
    termInitialized = true;
  }
  // return number of bytes waiting
  int bytesWaiting;
  ioctl(STDIN, FIONREAD, &bytesWaiting);
  return bytesWaiting;
}



bool ConsoleKeyManager::consoleKeyPoll()
{
  // process all pending console input
  while (kbHit()>0) {
    char  c = getchar();
    bool handled = false;
    if (keyPressHandler) {
      // call custom keypress handler
      handled = keyPressHandler(c);
    }
    if (!handled) {
      bool toggle = false;
      // A..Z are special "sticky" toggle variants of a..z
      if (c>='A' && c<='Z') {
        // make lowercase
        c = tolower(c);
        // toggle
        toggle = true;
      }
      // search key in map
      ConsoleKeyMap::iterator keypos = keyMap.find(c);
      if (keypos!=keyMap.end()) {
        // key is mapped
        if (toggle) {
          // toggle state
          keypos->second->toggle();
        }
        else {
          // pulse
          keypos->second->pulse();
        }
      }
    }
  }
  return true; // done for this cycle
}





