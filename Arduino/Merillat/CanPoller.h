/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include <SPI.h>
#include "CFwTimer.h"

#define NUM_OUT_BUFFERS 10
#define NUM_IN_BUFFERS 10
#define SPI_CS_PIN 9
#define CAN_TX_INTERVAL 90 // ms



struct CanEntryType {
  INT32U COBID;
  char Length;
  char Extended;
  INT8U *pMessage;
};
struct CanTxType {
  CFwTimer NextSendTime;
  CanEntryType Can;
};
struct CanRxType {
  CFwTimer LastRxTime;
  CanEntryType Can;
};


extern CanRxType CanInBuffers[NUM_IN_BUFFERS];
extern CanTxType CanOutBuffers[NUM_OUT_BUFFERS];
extern long int Fault;
extern long int OK;
extern MCP_CAN CAN;


void CanPollerInit();

bool CanPoller();

void CanPollSetRx(INT32U COBID, char len, INT8U *buf);
void CanPollSetTx(INT32U COBID, char len, bool extended, INT8U *buf);

