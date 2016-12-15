/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include <mcp_can.h>
#include <mcp_can_dfs.h>
#include <SPI.h>
#include <CFwTimer.h>

#define NUM_OUT_BUFFERS 8
#define NUM_IN_BUFFERS 8
#define SPI_CS_PIN 9
#define CAN_TX_INTERVAL 90 // ms

#define COB_ID_MASK (1l<<(29+1))-1 // creates 29-bit mask (0x1FFFFFFF)

typedef void (*voidFuncPtr)(void);

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
  voidFuncPtr RxFunction;
};


extern CanRxType CanInBuffers[NUM_IN_BUFFERS];
extern CanTxType CanOutBuffers[NUM_OUT_BUFFERS];
extern long int Fault;
extern long int OK;
extern MCP_CAN CAN;


void CanPollerInit();

void CanPoller();

void CanPollSetRx(INT32U COBID, char len, INT8U *buf, voidFuncPtr userFN);
void CanPollSetTx(INT32U COBID, char len, INT8U *buf);

// list the contents of the Can[TR]xType Can[In|Out]Buffers
// io bit value 1 is Outputs
// io bit value 2 if Inputs
void CanPollDisplay(int io);

