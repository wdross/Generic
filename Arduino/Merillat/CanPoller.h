/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include <SPI.h>
#include <CFwTimer.h>
#include "IODefs.h"

#define NUM_OUT_BUFFERS 8
#define NUM_IN_BUFFERS 8
#define SPI_CS_PIN 9
#define CAN_TX_INTERVAL 90 // ms

#define COB_ID_MASK 0x1FFFFFFFL // 29-bit mask

#undef DO_LOGGING
#ifdef DO_LOGGING
#define NUM_BUFFS 10
struct CanRXType {
  INT32U COBID;
  INT8U Length;
  INT8U Message[8];
  INT32U Time;
};
extern volatile CanRXType CanRXBuff[NUM_BUFFS];
void print_hex(long int v, int num_places);
void AddToDisplayBuffer(INT32U id, INT8U len, INT8U *buf);
extern volatile int Head; // updated in ISR
extern int Tail;
#endif

struct CanEntryType {
  INT32U COBID;
  INT8U Length;
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

void CanPoller();

void CanPollSetRx(INT32U COBID, char len, INT8U *buf);
void CanPollSetTx(INT32U COBID, char len, INT8U *buf);

// Returns the amount of ms elapsed since the last reception
long CanPollElapsedFromLastRxByCOBID(INT32U COBID);

// list the contents of the Can[TR]xType Can[In|Out]Buffers
// io bit value 1 is Outputs
// io bit value 2 if Inputs
void CanPollDisplay(int io);

