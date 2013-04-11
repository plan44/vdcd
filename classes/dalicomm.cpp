/*
 * dalicomm.cpp
 *
 *  Created on: Apr 10, 2013
 *      Author: Lukas Zeller / luz@plan44.ch
 *   Copyright: 2012-2013 by plan44.ch/luz
 */

#include "dalicomm.hpp"

void DaliComm::sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB)
{
  aResultCB(this,0x41,0x42);
}


void DaliComm::ackAllOn(DaliCommPtr aDaliComm, uint8_t aResp1, uint8_t aResp2)
{
  printf("Resp1 = 0x%02X, Resp2 = 0x%02X\n", aResp1, aResp2);
}


void DaliComm::allOn()
{
  // %%% how to pass myself as instance for calling ackAllOn on?
  sendBridgeCommand(0x10, 0xFE, 0xFE, boost::bind(&DaliComm::ackAllOn, this));
}


