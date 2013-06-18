//
//  dsparams.cpp
//  p44bridged
//
//  Created by Lukas Zeller on 31.05.13.
//  Copyright (c) 2013 plan44.ch. All rights reserved.
//

#include "dsparams.hpp"

using namespace p44;


static const paramEntry bank0params[] =
{
  { 0x0, 4, "DSMDSID" },
  { 0x4, 4, "LASTDSMDSID" },
  { 0x8, 1, "PROGEN" },
  { 0, 0, NULL },
};


static const paramEntry bank1params[] =
{
  { 0x0, 2, "VER" },
  { 0x2, 4, "DSID" },
  { 0x6, 2, "FID" },
  { 0x8, 2, "VID" },
  { 0xA, 2, "PID" },
  { 0xC, 2, "ADR" },
  { 0xE, 2, "VSK" },
  { 0x10, 8, "GRP" },
  { 0x18, 2, "RebootCnt" },
  { 0x1a, 2, "DatasetVersion" },
  { 0x1c, 2, "OEM_Serial" },
  { 0x1e, 1, "OEM_PartNo" },
  { 0x20, 2, "ST[]" },
  { 0x40, 3, "StateTable[]" },
  { 0, 0, NULL },
};


static const paramEntry bank2params[] =
{
  { 0x0, 2, "HWVer" },
  { 0x2, 2, "OverTempCnt" },
  { 0x4, 2, "OverCurrCnt" },
  { 0x6, 2, "OverLoadCnt" },
  { 0x8, 2, "dSCResetCnt" },
  { 0xa, 1, "FCHPOL" },
  { 0xb, 1, "FIXED_FREQ" },
  { 0, 0, NULL },
};


static const paramEntry bank3params[] =
{
  { 0x0, 1, "MODE" },
  { 0x1, 1, "LTNUMGRP0" },
  { 0x2, 1, "OFFVAL" },
  { 0x3, 1, "ONVAL" },
  { 0x4, 1, "MINDIM" },
  { 0x5, 1, "MAXDIM" },
  { 0x6, 1, "DIMTIME0_UP" },
  { 0x7, 1, "DIMTIME0_DOWN" },
  { 0x8, 1, "DIMTIME1_UP" },
  { 0x9, 1, "DIMTIME1_DOWN" },
  { 0xa, 1, "DIMTIME2_UP" },
  { 0xb, 1, "DIMTIME2_DOWN" },
  { 0xc, 1, "STEP0_UP" },
  { 0xd, 1, "STEP0_DOWN" },
  { 0xe, 1, "FOFFVAL0" },
  { 0xf, 1, "FONVAL0" },
  { 0x10, 1, "FOFFTIME0" },
  { 0x11, 1, "FONTIME0" },
  { 0x12, 1, "FCOUNT0" },
  { 0x13, 1, "FOFFVAL1" },
  { 0x14, 1, "FONVAL1" },
  { 0x15, 1, "FOFFTIME1" },
  { 0x16, 1, "FONTIME1" },
  { 0x17, 1, "FCOUNT1" },
  { 0x18, 1, "LEDCON0" },
  { 0x19, 1, "LEDCON1" },
  { 0x1a, 1, "LEDCON2" },
  { 0x1b, 1, "LEDRGB0" },
  { 0x1c, 1, "LEDRGB1" },
  { 0x1d, 1, "LTCON" },
  { 0x1e, 1, "LTMODE" },
  { 0x1f, 2, "DEFVAL" },
  { 0x21, 2, "LTMODE2TOUT" },
  { 0x23, 1, "LTSCENEON" },
  { 0x24, 1, "LTSCENEONCFG" },
  { 0x25, 1, "LTSCENEOFF" },
  { 0x26, 1, "LTSCENEOFFCFG" },
  { 0x28, 1, "LEDCONFLASH" },
  { 0x29, 1, "FMODE" },
  { 0x2a, 6, "OEM_EAN" },
//  { 0x30, 32, "FID_DEPENDENT_VAL[]" },
  { 0, 0, NULL },
};


static const paramEntry bank3params_M[] =
{
  // Klemme M/Schnurdimmer M/Tasterklemme M
  { 0x30, 17, "OUTTABLE" },
  { 0x41, 1, "TRP" },
  { 0x42, 1, "TRN" },
  { 0x43, 1, "SW_THR" },
  { 0x44, 1, "SW_RAMP_THR" },
  { 0, 0, NULL },
};


static const paramEntry bank3params_Lx[] =
{
  // GE/SW/ZWS Klemme Lx
  { 0x30, 2, "POS_TIMEUP" },
  { 0x32, 2, "POS_TIMEDOWN" },
  { 0x34, 2, "POS_STARTUP" },
  { 0x36, 2, "POS_STARTDOWN" },
  { 0x38, 2, "POS_DIMDEC" },
  { 0x3A, 2, "POS_DIMINC" },
  { 0x3C, 1, "SW_THR" },
  { 0x3d, 1, "SW_RAMP_THR" },
  { 0, 0, NULL },
};


static const paramEntry bank3params_L[] =
{
  // GR Klemme L
  { 0x30, 2, "PosTimeUp" },
  { 0x32, 2, "PosTimeDown" },
  { 0x34, 1, "DeadTime" },
  { 0x35, 1, "AngRestore" },
  { 0x36, 1, "MotorInv" },
  { 0x37, 1, "ReversingTime" },
  { 0x38, 1, "EndPosOverDrv" },
  { 0x39, 1, "IMotorThr" },
  { 0x3A, 1, "DimStepNum" },
  { 0x3b, 1, "DimStepLen" },
  { 0x3c, 1, "DimStepPause" },
  { 0x3d, 1, "CalibEn" },
  { 0x3e, 2, "LamellaTime" },
  { 0x40, 1, "PosCorrUp" },
  { 0x41, 1, "PosCorrDown" },
  { 0x42, 2, "ST_Delay" },
  { 0x44, 1, "ST_Len" },
  { 0x45, 1, "ST_ID" },
  { 0x46, 1, "ST_Cnt" },
  { 0x47, 1, "LamAngleLast" },
  { 0, 0, NULL },
};

static const paramEntry bank3params_9W[] =
{
  // 9-Wegetaster
  { 0x30, 2, "solltemp" },
  { 0x32, 1, "anmeldev_on" },
  { 0, 0, NULL },
};


static const paramEntry bank5params[] =
{
  { 0x0, 1, "PRIO" }, // 4 array elements
  { 0x4, 1, "TMR" }, // 4 array elements
  { 0x8, 6, "ST" }, // 32 array elements
  { 0, 0, NULL },
};


static const paramEntry bank6params[] =
{
  { 0x0, 6, "ET[16]" },
  { 0, 0, NULL },
};


static const paramEntry bank64params[] =
{
  { 0x0, 1, "VAL" },
  { 0x1, 1, "KEYSTATE" },
  { 0x2, 2, "Position" },
  { 0x4, 1, "LamAngle" },
  { 0x6, 2, "ActPos" },
  { 0, 0, NULL },
};


static const paramEntry bank128params[] =
{
  { 0x0, 1, "SCE" }, // 128 array elements
  { 0x80, 1, "SCECON" }, // 128 array elements
  { 0, 0, NULL },
};


static const paramEntry bank129params[] =
{
  { 0x0, 1, "SCE_LO" }, // 64 array elements (each byte holds two nibbles, so 64 elements are enough for 128 scene value extensions to 12bit
  { 0, 0, NULL },
};


static const paramEntry bank130params[] =
{
  { 0x0, 1, "ScnAngle" },
  { 0, 0, NULL },
};


static const bankEntry paramTable[] =
{
  { 0, 0, bank0params },
  { 1, 0, bank1params },
  { 2, 0, bank2params },
  { 3, 0, bank3params },
  { 3, 1, bank3params_M },
  { 3, 2, bank3params_Lx },
  { 3, 3, bank3params_L },
  { 3, 4, bank3params_9W },
  { 2, 0, bank5params },
  { 2, 0, bank6params },
  { 2, 0, bank64params },
  { 2, 0, bank128params },
  { 2, 0, bank129params },
  { 2, 0, bank130params },
  { 0, NULL }
};


const paramEntry *DsParams::paramEntryForOffset(const paramEntry *aBankTable, uint8_t aOffset)
{
  const paramEntry *foundEntry = NULL;
  const paramEntry *pp = aBankTable;
  // find first entry where next entry exceeds given offset
  while (pp->name!=NULL) {
    // not last entry
    if (pp[1].name==NULL || pp[1].offset>aOffset) {
      // this is my entry
      foundEntry = pp;
      break;
    }
    ++pp; // next param
  }
  return foundEntry;
}


const paramEntry *DsParams::paramEntryForBankOffset(uint8_t aBank, uint8_t aOffset, uint8_t aVariant)
{
  const paramEntry *foundEntry = NULL;
  const bankEntry *bp = paramTable;
  while (bp->bankParams!=NULL) {
    if (bp->bankNo==aBank) {
      // bank found, check value
      foundEntry = paramEntryForOffset(bp->bankParams, aOffset);
      // search specific variant, if any
      if (foundEntry==NULL && aVariant!=0) {
        bp++; // check following entries for bank variants
        while (bp->bankParams!=NULL && bp->bankNo==aBank) {
          if (bp->variant==aVariant) {
            // variant specific bank table found, check that as well
            foundEntry = paramEntryForOffset(bp->bankParams, aOffset);
            break;
          }
          bp++;
        }
      }
      break;
    } // found bank
    ++bp; // next bank
  }
  return foundEntry;
}


const paramEntry *DsParams::paramEntryForName(const char *aName, uint8_t aVariant)
{
  const bankEntry *bp = paramTable;
  while (bp->bankParams!=NULL) {
    // check table for name
    const paramEntry *pp = bp->bankParams;
    while (pp->name!=NULL) {
      if (strcasecmp(pp->name, aName)==0) {
        // found
        return pp;
      }
      ++pp;
    }
    ++bp;
  }
  return NULL; // none found
}


