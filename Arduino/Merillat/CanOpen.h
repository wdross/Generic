#if !defined(CANOPEN_INCLUDED)
#define CANOPEN_INCLUDED

#include <mcp_can.h>
#include <mcp_can_dfs.h>

// Enough CANOPEN stuff to define COBIDs
#define MK_COBID(ID,FN) ((ID)&0x07f | (((FN)&0xf)<<7) | (IS_EXTENDED_COBID&(ID)))
#define GET_NID(COBID) ((COBID)&0x7f)
#define EMERGENCY 1
#define TXPDO1 3
#define TXPDO2 5
#define TXPDO3 7
#define TXPDO4 9
#define RXPDO1 4
#define RXPDO2 6
#define RXPDO3 8
#define RXPDO4 10
#define TXSDO 11
#define RXSDO 12
#define NODE_GUARD 14

#define IS_EXTENDED_COBID 0x80000000L // we'll set bit 31 (of a 29-bit COBID) to mean 'Extended'

extern MCP_CAN CAN; // Defined by CanPoller.h

void SDOread(int NID, int index, INT8U subIndex);
void SDOwrite(int NID, int index, INT8U subIndex, INT32U value, INT8U valuesize);
void SYNCsend();
void NMTsend();

#endif // CANOPEN_INCLUDED
