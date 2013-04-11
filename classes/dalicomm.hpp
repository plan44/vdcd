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

typedef boost::function<void (DaliComm *aDaliCommP, uint8_t aResp1, uint8_t aResp2)> DaliBridgeResultCB;


/// A class providing low level access to the DALI bus
class DaliComm
{
public:
  void sendBridgeCommand(uint8_t aCmd, uint8_t aDali1, uint8_t aDali2, DaliBridgeResultCB aResultCB);

  void allOn();
  void ackAllOn(DaliComm *aDaliComm, uint8_t aResp1, uint8_t aResp2);

};

#endif /* DALICOMM_H_ */
