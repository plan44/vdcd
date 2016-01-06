//
//  Copyright (c) 2015-2016 plan44.ch / Lukas Zeller, Zurich, Switzerland
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

#include "ws281xcomm.hpp"

#if !DISABLE_LEDCHAIN

using namespace p44;


#if defined(RASPBERRYPI)

#define TARGET_FREQ WS2811_TARGET_FREQ // in Hz, default is 800kHz
#define GPIO_PIN 18 // P1 Pin 12, GPIO 18 (PCM_CLK)
#define GPIO_INVERT 0 // set to 1 if there is an inverting driver between GPIO 18 and the WS281x LEDs
#define DMA 5 // don't change unless you know why
#define MAX_BRIGHTNESS 255 // full brightness range
#endif


WS281xComm::WS281xComm(uint16_t aNumLeds, uint16_t aLedsPerRow, bool aXReversed, bool aAlternating, bool aSwapXY) :
  initialized(false)
{
  numLeds = aNumLeds;
  if (aLedsPerRow==0) {
    ledsPerRow = aNumLeds; // single row
    numRows = 1;
  }
  else {
    ledsPerRow = aLedsPerRow; // set row size
    numRows = (numLeds-1)/ledsPerRow+1; // calculate number of (full or partial) rows
  }
  xReversed = aXReversed;
  alternating = aAlternating;
  swapXY = aSwapXY;
  // prepare hardware related stuff
  #if defined(RASPBERRYPI)
  // initialize the led string structure
  ledstring.freq = TARGET_FREQ;
  ledstring.dmanum = DMA;
  ledstring.device = NULL; // private data pointer for library
  // channel 0
  ledstring.channel[0].gpionum = GPIO_PIN;
  ledstring.channel[0].count = numLeds;
  ledstring.channel[0].invert = GPIO_INVERT;
  ledstring.channel[0].brightness = MAX_BRIGHTNESS;
  ledstring.channel[0].leds = NULL; // will be allocated by the library
  // channel 1 - unused
  ledstring.channel[1].gpionum = 0;
  ledstring.channel[1].count = 0;
  ledstring.channel[1].invert = 0;
  ledstring.channel[1].brightness = MAX_BRIGHTNESS;
  ledstring.channel[1].leds = NULL; // will be allocated by the library
  #endif
  // make sure operation ends when mainloop terminates
  MainLoop::currentMainLoop().registerCleanupHandler(boost::bind(&WS281xComm::end, this));
}

WS281xComm::~WS281xComm()
{
  // end the operation when object gets deleted
  end();
}


uint16_t WS281xComm::getNumLeds()
{
  return numLeds;
}


uint16_t WS281xComm::getSizeX()
{
  return swapXY ? numRows : ledsPerRow;
}


uint16_t WS281xComm::getSizeY()
{
  return swapXY ? ledsPerRow : numRows;
}



bool WS281xComm::begin()
{
  if (!initialized) {
    #if defined(RASPBERRYPI)
    // initialize library
    initialized = ws2811_init(&ledstring)==0;
    #else
    initialized = true; // dummy
    #endif
  }
  return initialized;
}


void WS281xComm::end()
{
  if (initialized) {
    #if defined(RASPBERRYPI)
    // deinitialize library
    ws2811_fini(&ledstring);
    #endif
  }
  initialized = false;
}


void WS281xComm::show()
{
  if (!initialized) return;
  #if defined(RASPBERRYPI)
  ws2811_render(&ledstring);
  #endif
}


// brightness to PWM value conversion
static const uint8_t pwmtable[256] = {
  0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,
  6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11,
  11, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17,
  17, 18, 18, 18, 19, 19, 20, 20, 20, 21, 21, 22, 22, 22, 23, 23, 24, 24, 25, 25,
  26, 26, 26, 27, 27, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 34, 34, 35, 35, 36,
  37, 37, 38, 39, 39, 40, 41, 42, 42, 43, 44, 44, 45, 46, 47, 48, 49, 49, 50, 51,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 72,
  73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89, 90, 92, 93, 95, 97, 98, 100,
  101, 103, 105, 107, 108, 110, 112, 114, 116, 118, 120, 121, 123, 126, 128, 130,
  132, 134, 136, 138, 141, 143, 145, 148, 150, 152, 155, 157, 160, 163, 165, 168,
  171, 174, 176, 179, 182, 185, 188, 191, 194, 197, 201, 204, 207, 210, 214, 217,
  221, 224, 228, 232, 235, 239, 243, 247, 251, 255
};

const uint8_t brightnesstable[256] = {
  0, 7, 18, 27, 36, 43, 49, 55, 61, 66, 70, 75, 79, 83, 86, 90, 93, 96, 99, 102, 104,
  107, 109, 112, 114, 116, 118, 121, 123, 124, 126, 128, 130, 132, 133, 135, 137, 138,
  140, 141, 143, 144, 145, 147, 148, 150, 151, 152, 153, 154, 156, 157, 158, 159, 160,
  161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177,
  177, 178, 179, 180, 181, 181, 182, 183, 184, 184, 185, 186, 187, 187, 188, 189, 190,
  190, 191, 192, 192, 193, 194, 194, 195, 195, 196, 197, 197, 198, 199, 199, 200, 200,
  201, 201, 202, 203, 203, 204, 204, 205, 205, 206, 206, 207, 207, 208, 208, 209, 210,
  210, 211, 211, 211, 212, 212, 213, 213, 214, 214, 215, 215, 216, 216, 217, 217, 218,
  218, 218, 219, 219, 220, 220, 221, 221, 221, 222, 222, 223, 223, 224, 224, 224, 225,
  225, 226, 226, 226, 227, 227, 227, 228, 228, 229, 229, 229, 230, 230, 230, 231, 231,
  231, 232, 232, 233, 233, 233, 234, 234, 234, 235, 235, 235, 236, 236, 236, 237, 237,
  237, 238, 238, 238, 239, 239, 239, 240, 240, 240, 240, 241, 241, 241, 242, 242, 242,
  243, 243, 243, 244, 244, 244, 244, 245, 245, 245, 246, 246, 246, 246, 247, 247, 247,
  248, 248, 248, 248, 249, 249, 249, 249, 250, 250, 250, 251, 251, 251, 251, 252, 252,
  252, 252, 253, 253, 253, 253, 254, 254, 254, 254, 255, 255, 255, 255
};


uint16_t WS281xComm::ledIndexFromXY(uint16_t aX, uint16_t aY)
{
  if (swapXY) { uint16_t tmp=aY; aY=aX; aX=tmp; }
  uint16_t ledindex = aY*ledsPerRow;
  bool reversed = xReversed;
  if (alternating) {
    if (aY & 0x1) reversed = !reversed;
  }
  if (reversed) {
    ledindex += (ledsPerRow-1-aX);
  }
  else {
    ledindex += aX;
  }
  return ledindex;
}


void WS281xComm::setColorXY(uint16_t aX, uint16_t aY, uint8_t aRed, uint8_t aGreen, uint8_t aBlue)
{
  uint16_t ledindex = ledIndexFromXY(aX,aY);
  if (ledindex>=numLeds) return;
  #if defined(RASPBERRYPI)
  ws2811_led_t pixel =
  (pwmtable[aRed] << 16) |
  (pwmtable[aGreen] << 8) |
  (pwmtable[aBlue]);
  ledstring.channel[0].leds[ledindex] = pixel;
  #endif
}


void WS281xComm::setColor(uint16_t aLedNumber, uint8_t aRed, uint8_t aGreen, uint8_t aBlue)
{
  int y = aLedNumber / getSizeX();
  int x = aLedNumber % getSizeX();
  setColorXY(x, y, aRed, aGreen, aBlue);
}


void WS281xComm::setColorDimmedXY(uint16_t aX, uint16_t aY, uint8_t aRed, uint8_t aGreen, uint8_t aBlue, uint8_t aBrightness)
{
  setColorXY(aX, aY, (aRed*aBrightness)>>8, (aGreen*aBrightness)>>8, (aBlue*aBrightness)>>8);
}


void WS281xComm::setColorDimmed(uint16_t aLedNumber, uint8_t aRed, uint8_t aGreen, uint8_t aBlue, uint8_t aBrightness)
{
  int y = aLedNumber / getSizeX();
  int x = aLedNumber % getSizeX();
  setColorDimmedXY(x, y, aRed, aGreen, aBlue, aBrightness);
}


void WS281xComm::getColor(uint16_t aLedNumber, uint8_t &aRed, uint8_t &aGreen, uint8_t &aBlue)
{
  int y = aLedNumber / getSizeX();
  int x = aLedNumber % getSizeX();
  getColorXY(x, y, aRed, aGreen, aBlue);
}


void WS281xComm::getColorXY(uint16_t aX, uint16_t aY, uint8_t &aRed, uint8_t &aGreen, uint8_t &aBlue)
{
  uint16_t ledindex = ledIndexFromXY(aX,aY);
  if (ledindex>=numLeds) return;
  #if defined(RASPBERRYPI)
  ws2811_led_t pixel = ledstring.channel[0].leds[ledindex];
  aRed = brightnesstable[(pixel>>16) & 0xFF];
  aGreen = brightnesstable[(pixel>>8) & 0xFF];
  aBlue = brightnesstable[pixel & 0xFF];
  #endif
}

#endif // !DISABLE_LEDCHAIN

