/*
 * dalicomm.h
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */


#ifndef DALICOMM_H_
#define DALICOMM_H_

#include "p44bridged_common.hpp"

class DaliComm;
typedef boost::shared_ptr<DaliComm> DaliCommPtr;

typedef boost::function<void (DaliCommPtr aDaliComm, uint8_t aResp1, uint8_t aResp2)> DaliBridgeResultCB;


/// A class providing low level access to the DALI bus
class DaliComm
{
public:
  void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB);

  void allOn();
  void ackAllOn(DaliCommPtr aDaliComm, uint8_t aResp1, uint8_t aResp2);

};

#endif /* DALICOMM_H_ */


/*
 * Using bind with Boost.Function

class button
{
public:

    boost::function<void()> onClick;
};

class player
{
public:

    void play();
    void stop();
};

button playButton, stopButton;
player thePlayer;

void connect()
{
    playButton.onClick = boost::bind(&player::play, &thePlayer);
    stopButton.onClick = boost::bind(&player::stop, &thePlayer);
}

 *
 *
 */
