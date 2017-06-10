#include "CanOpen.h"
#include "CanPoller.h"

INT8U sdo[] = {0x40,0x01,0x18,0x02,0x00,0x00,0x00,0x00};
void SDOread(int NID, int index, INT8U subIndex)
{
  // build and send message to read a remote object
  INT8U len = sizeof(sdo);
  INT32U COBID = MK_COBID(NID,RXSDO);
  sdo[0] = 0x40; // READ
  sdo[1] = 0xff & index;        // LSBits
  sdo[2] = 0xff & (index >> 8); // MSBits
  sdo[3] = subIndex;
  sdo[4] = sdo[5] = sdo[6] = sdo[7] = 0;
  CAN.sendMsgBuf(COBID,0,len,sdo);
#ifdef DO_LOGGING
  AddToDisplayBuffer(COBID,len,sdo);
#endif
}

void SDOwrite(int NID, int index, INT8U subIndex, INT32U value, INT8U valuesize)
{
  // build and send message to change a remote object
  INT8U len = sizeof(sdo);
  INT32U COBID = MK_COBID(NID,RXSDO);
  switch (valuesize) {
    case 1: sdo[0] = 0x2f; break;
    case 2: sdo[0] = 0x2b; break;
    case 3: sdo[0] = 0x27; break;
    default: sdo[0] = 0x23; break;
  }
  sdo[1] = 0xff & index;        // LSBits
  sdo[2] = 0xff & (index >> 8); // MSBits
  sdo[3] = subIndex;
  sdo[4] = value;
  sdo[5] = value >> 8;
  sdo[6] = value >> 16;
  sdo[7] = value >> 24;
  CAN.sendMsgBuf(COBID,0,len,sdo);
#ifdef DO_LOGGING
  AddToDisplayBuffer(COBID,len,sdo);
#endif
}

void SYNCsend()
{
  INT8U len = 0;
  INT32U COBID = 0x80;
  char ret = CAN.sendMsgBuf(COBID,0,len,0);
#ifdef DO_LOGGING
  AddToDisplayBuffer(COBID,len,0);
#endif
}

void NMTsend()
{
  sdo[0] = 0x01;
  sdo[1] = 0x00;
  INT8U len = 2;
  INT32U COBID = 0x00;
  CAN.sendMsgBuf(COBID,0,len,sdo);
#ifdef DO_LOGGING
  AddToDisplayBuffer(COBID,len,sdo);
#endif
}
