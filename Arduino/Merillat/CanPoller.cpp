/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include "CanPoller.h"
#include "CanOpen.h"
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
      CanInBuffers[j].LastRxTime.SetTimer(INFINITE); // nothing yet
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

long CanPollElapsedFromLastRxByIndex(byte i)
{
  if (CanInBuffers[i].LastRxTime.GetExpiredBy() < 0) { // wrapped
    CanInBuffers[i].LastRxTime.SetTimer(INFINITE);
    return INFINITE; // been a *very* long time, make sure it can never become valid
  }
  else
    return CanInBuffers[i].LastRxTime.GetExpiredBy();
} // CanPollElapsedFromLastRxByIndex

long CanPollElapsedFromLastRxByCOBID(INT32U COBID)
{
  for (int j=0; j<NUM_IN_BUFFERS; j++) {
    if (CanInBuffers[j].Can.COBID == COBID)
      return(CanPollElapsedFromLastRxByIndex(j));
  }
  return INFINITE; // not found
}

#define NOT_TALKING_TIMEOUT 200 // ms without talking, we'll report !HaveCome
CFwTimer GrossTimeoutChecker;
bool HaveComm = false;
// Intended to be called from loop(), services both in and outbound messages
void CanPoller()
{
  int j;
  // let's receive all messages into their registered buffer
  while (CAN_MSGAVAIL == CAN.checkReceive()) { // while data present
    INT8U len;
    INT8U Msg[8];
    CAN.readMsgBuf(&len, Msg); // read data,  len: data length, buf: data buf
    INT32U COBID = CAN.getCanId(); // valid after readMsgBuf()
    for (j=0; j<NUM_IN_BUFFERS && CanInBuffers[j].Can.COBID; j++) {
      if (COBID == CanInBuffers[j].Can.COBID) {
        // found match, record time and save the bytes
        CanInBuffers[j].LastRxTime.SetTimer(0); // record 'now'
        memcpy(CanInBuffers[j].Can.pMessage,Msg,CanInBuffers[j].Can.Length);
        break; // skip out of the loop
      }
    }
  }

  if (GrossTimeoutChecker.IsTimeout()) {
    bool AnyExpired = false;
    for (j=0; j<NUM_IN_BUFFERS && CanInBuffers[j].Can.COBID; j++) {
      if (CanPollElapsedFromLastRxByIndex(j) > NOT_TALKING_TIMEOUT)
        AnyExpired = true;
    }
    if (AnyExpired && HaveComm) {
      HaveComm = false;
      Serial.println("Just lost a unit");
    }
    if (!AnyExpired && !HaveComm) {
      HaveComm = true;
      Serial.println("All talking now");
    }
    GrossTimeoutChecker.IncrementTimer(NOT_TALKING_TIMEOUT/2);
  }

#define NO_COMM_INTERVAL 50 // send some 'get us going' message at this interval
  static CFwTimer NoCommTimer;
  if (!HaveComm) {
    // in this state, we've not gotten all our messages, so we need to keep
    // trying to get them to respond to us.
    // It appears each card needs to have it's Transmit PDO parameter's Transmission Type
    // (0x180n.2) set to a 1 so it will respond to a SYNC.
    // The cards also need a NMT (directed or global) to respond to SYNCs
    static int CheckRX = 0;
    if (NoCommTimer.IsTimeout()) {
      // let's try to get something talking to us
      while (CheckRX < NUM_IN_BUFFERS && CanInBuffers[CheckRX].Can.COBID) {
        if (!(CanInBuffers[CheckRX].Can.COBID&IS_EXTENDED_COBID) &&
            CanPollElapsedFromLastRxByIndex(CheckRX) > NOT_TALKING_TIMEOUT) {
          // haven't heard from this guy, set his Comm parameter
          SDOwrite(GET_NID(CanInBuffers[CheckRX].Can.COBID),0x1801,2,1,1);
          CheckRX++;
          NoCommTimer.SetTimer(NO_COMM_INTERVAL);
          Serial.println("SDO write");
          return;
        }
        CheckRX++;
      }
      // we've looped once thru all the units and set their PDO Comm param to
      // answer us on a SYNC.  First time after the individual messages, send a
      // NMT to get all units Operational
      if (CheckRX <= NUM_IN_BUFFERS) {
        NMTsend();
        CheckRX = NUM_IN_BUFFERS+1; // forces 'next state'
        NoCommTimer.SetTimer(NO_COMM_INTERVAL);
        Serial.println("NMT");
        return;
      }
      SYNCsend();
      Serial.println("SYNC");
      NoCommTimer.SetTimer(NO_COMM_INTERVAL);
      CheckRX = 0; // go ahead and wrap around, in case something still isn't answering
      return;
    }
  }
  else {
    // see if it is time to send any of our messages at their interval
    for (j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
      if (CanOutBuffers[j].NextSendTime.IsTimeout()) {
        char ret = CAN.sendMsgBuf((INT32U)(CanOutBuffers[j].Can.COBID&COB_ID_MASK),CanOutBuffers[j].Can.COBID&IS_EXTENDED_COBID?1:0,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
        CanOutBuffers[j].NextSendTime.IncrementTimer(CAN_TX_INTERVAL);
        if (ret == CAN_OK)
          OK++;
        else
          Fault++;
      }
    }
    NoCommTimer.SetTimer(0); // keep it expired
  } // Comm appears to be OK
}

void CanPollDisplay(int io)
{
  CFwTimer now(0); // what 'now' is
  Serial.print("now()="); Serial.println(now.getEndTime());

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
        Serial.print(CanOutBuffers[j].Can.COBID & IS_EXTENDED_COBID?1:0);
        Serial.print("\t");
        Serial.println(CanOutBuffers[j].NextSendTime.getEndTime());
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
        Serial.println(CanInBuffers[j].LastRxTime.getEndTime());
      }
    }
  }
}

