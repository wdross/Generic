/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include "CanPoller.h"
long int Fault=0;
long int OK=0;

CanRxType CanInBuffers[NUM_IN_BUFFERS];
CanTxType CanOutBuffers[NUM_OUT_BUFFERS];
MCP_CAN CAN(SPI_CS_PIN);    // Set CS pin

void CanPollerInit()
{
  // set up our arrays
  for (int j=0; j<NUM_OUT_BUFFERS; j++) {
    CanOutBuffers[j].Can.COBID = 0; // not used
  }
  for (int j=0; j<NUM_IN_BUFFERS; j++) {
    CanInBuffers[j].Can.COBID = 0; // not used
  }
}

// Set up &buf to receive len bytes of data from COBID seen on CAN-BUS
void CanPollSetRx(INT32U COBID, char len, INT8U *buf)
{
  // set up the next available CanInBuffers[] entry to be a receiver
  for (int j=0; j<NUM_IN_BUFFERS; j++) {
    if (CanInBuffers[j].Can.COBID == 0) {
      CanInBuffers[j].Can.Length = len;
      CanInBuffers[j].Can.pMessage = buf;
      CanInBuffers[j].Can.COBID = COBID;
      break; // we're done
    }
  }
}

// set up len bytes at &buf to be sent as a PDO with given COBID
void CanPollSetTx(INT32U COBID, char len, INT8U *buf)
{
  // set up the next available CanOutBuffers[] entry to be a sender
  int j=0;
  for (; j<NUM_OUT_BUFFERS; j++) {
    if (CanOutBuffers[j].Can.COBID == 0) {
      CanOutBuffers[j].Can.Length = len;
      CanOutBuffers[j].Can.pMessage = buf;
      CanOutBuffers[j].Can.COBID = COBID;
      break; // we're done, now set the Tx intervals
    }
  }
  if (j < NUM_OUT_BUFFERS) {
    for (int i=j; i>=0; i--) {
      CanOutBuffers[i].NextSendTime.SetTimer(CAN_TX_INTERVAL * (i+1) / (j+1));
    }
  }
  else
    Serial.println("Attempt to set up more TX buffers than allowed");
}

// Intended to be called from loop(), services both in and outbound messages
void CanPoller() // returns true if any messages were transmitted this call
{
  // let's receive all messages into their registered buffer
  while (CAN_MSGAVAIL == CAN.checkReceive()) { // while data present
    INT8U len;
    INT8U Msg[8];
    CAN.readMsgBuf(&len, Msg); // read data,  len: data length, buf: data buf
    INT32U COBID = CAN.getCanId(); // valid after readMsgBuf()
    for (int j=0; j<NUM_IN_BUFFERS && CanInBuffers[j].Can.COBID; j++) {
      if (COBID == CanInBuffers[j].Can.COBID) {
        // found match, record time and save the bytes
        CanInBuffers[j].LastRxTime.SetTimer(0); // record 'now'
        memcpy(CanInBuffers[j].Can.pMessage,Msg,CanInBuffers[j].Can.Length);
      }
    }
  }

  // see if it is time to send any of our messages at their interval
  int sent = 0;
  for (int j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
    if (CanOutBuffers[j].NextSendTime.IsTimeout()) {
      char ret = CAN.sendMsgBuf(CanOutBuffers[j].Can.COBID&COB_ID_MASK,CanOutBuffers[j].Can.COBID&IS_EXTENDED_COBID,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
*CanOutBuffers[j].Can.pMessage += 1; // increment first byte every TX
      CanOutBuffers[j].NextSendTime.IncrementTimer(CAN_TX_INTERVAL);
      sent++;
      if (ret == CAN_OK)
        OK++;
      else
        Fault++;
    }
  }
  return;
}

void CanPollDisplay(int io)
{
  CFwTimer now(0); // what 'now' is
  Serial.print("now()="); Serial.println(now.getStartTime());

  if (io & 1) {
    Serial.println("Output buffers defined:");
    Serial.println("Index\tCOBID\tLen\t&buffer\text?\tNextTime");
    for (int j=0; j<NUM_OUT_BUFFERS; j++) {
      if (CanOutBuffers[j].Can.COBID) {
        Serial.print("[");
        Serial.print(j);
        Serial.print("]\t");
        Serial.print(CanOutBuffers[j].Can.COBID&COB_ID_MASK,HEX);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.Length);
        Serial.print("\t");
        Serial.print((long)CanOutBuffers[j].Can.pMessage);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.COBID & IS_EXTENDED_COBID);
        Serial.print("\t");
        Serial.println(CanOutBuffers[j].NextSendTime.getStartTime());
      }
    }
  }

  if ((io & 1) && (io & 2))
    Serial.println();

  if (io & 2) {
    Serial.println("Input buffers defined:");
    Serial.println("Index\tCOBID\tLen\t&buffer\tLastRx");
    for (int j=0; j<NUM_IN_BUFFERS; j++) {
      if (CanInBuffers[j].Can.COBID) {
        Serial.print("[");
        Serial.print(j);
        Serial.print("]\t");
        Serial.print(CanInBuffers[j].Can.COBID,HEX);
        Serial.print("\t");
        Serial.print(CanInBuffers[j].Can.Length);
        Serial.print("\t");
        Serial.print((long)CanInBuffers[j].Can.pMessage);
        Serial.print("\t");
        Serial.println(CanInBuffers[j].LastRxTime.getStartTime());
      }
    }
  }
}

