/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include "CanPoller.h"
#include "CanOpen.h"
long int Fault=0;
long int OK=0;
long CanPollerNominalOutputInterval = INFINITE; // nothing to output until configured so

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
} // CanPollerInit

// Set up &buf to receive len bytes of data from COBID seen on CAN-BUS
void CanPollSetRx(INT32U COBID, char len, INT8U *buf, voidFuncPtr userFN)
{
  // set up the next available CanInBuffers[] entry to be a receiver
  for (int j=0; j<NUM_IN_BUFFERS; j++) {
    if (CanInBuffers[j].Can.COBID == 0) {
      CanInBuffers[j].Can.Length = len;
      CanInBuffers[j].Can.pMessage = buf;
      CanInBuffers[j].Can.COBID = COBID;
      CanInBuffers[j].LastRxTime.SetTimer(INFINITE); // nothing yet
      CanInBuffers[j].RxFunction = userFN;
      break; // we're done
    }
  }
} // CanPollSetRx

// set up len bytes at &buf to be sent as a PDO with given COBID
void CanPollSetTx(INT32U COBID, char len, INT8U *buf, INT8U mask)
{
  // set up the next available CanOutBuffers[] entry to be a sender
  int j=0;
  for (; j<NUM_OUT_BUFFERS; j++) {
    if (CanOutBuffers[j].Can.COBID == 0) {
      CanOutBuffers[j].Can.Length = len;
      CanOutBuffers[j].Can.pMessage = buf;
      CanOutBuffers[j].Can.COBID = COBID;
      CanOutBuffers[j].OutputMask = mask;
      break; // we're done, now set the Tx intervals
    }
  }
  CanPollerNominalOutputInterval = CAN_TX_INTERVAL / (j+1); // ms
  if (j < NUM_OUT_BUFFERS) {
    for (int i=j; i>=0; i--) {
      CanOutBuffers[i].NextSendTime.SetTimer(CAN_TX_INTERVAL * (i+1) / (j+1));
    }
  }
  else
    Serial.println("Attempt to set up more TX buffers than space allows");
} // CanPollSetTx

long CanPollElapsedFromLastRxByIndex(byte i)
{
  long expBy = CanInBuffers[i].LastRxTime.GetExpiredBy();
  if (expBy < -(INFINITE/2)) { // wrapped
    CanInBuffers[i].LastRxTime.SetTimer(INFINITE);
    return INFINITE; // been a *very* long time, make sure it can never become valid
  }
  else
    return expBy;
} // CanPollElapsedFromLastRxByIndex

long CanPollElapsedFromLastRxByCOBID(INT32U COBID)
{
  for (int j=0; j<NUM_IN_BUFFERS; j++) {
    if (CanInBuffers[j].Can.COBID == COBID)
      return(CanPollElapsedFromLastRxByIndex(j));
  }
  return INFINITE; // not found
} // CanPollElapsedFromLastRxByCOBID


int MapFnToCommParam(INT32U COBID)
{
  int fn = GET_FN(COBID);
  switch (fn) {
    case TXPDO1:return 0x1800;
    case TXPDO2:return 0x1801;
  }
  return 0;
}

CFwTimer GrossTimeoutChecker;
CFwTimer NominalTimer;
bool HaveComm = false;
// This is being called upon a hardware timer every 0.35ms (350us) thru use of the FlexiTimer2 class.
// The time value was selected based upon some sample data taken from 7 ESD modules connected
// and responding in our needed fashion, and seeing the shortest aparent message was a single
// 0.26ms but is more typically 0.44-0.52ms per shortest arrival message.
// During some testing, 0.4ms was not short enough, so lowered to 0.35ms (2857 Hz)
// It services both in and outbound messages, so MUST BE EFFICIENT!!
void CanPoller()
{
  // did an experiment which indicates there is no reentry risk here --
  // it appears the interrupts stack and ends up with back-to-back calls
  // should things run behind.
  static INT16U EntryCount=0;
  EntryCount++;

  int j;
  // let's receive all messages into their registered buffer
  while (CAN_MSGAVAIL == CAN.checkReceive()) { // while data present
    INT8U len;
    INT8U Msg[8];
    CAN.readMsgBuf(&len, Msg); // read data,  len: data length, buf: data buf
    INT32U COBID = CAN.getCanId(); // valid after readMsgBuf()
#ifdef DO_LOGGING
    AddToDisplayBuffer(COBID,len,Msg);
#endif
    for (j=0; j<NUM_IN_BUFFERS && CanInBuffers[j].Can.COBID; j++) {
      if (COBID == (CanInBuffers[j].Can.COBID&COB_ID_MASK)) {
        // found match, record time and save the bytes
        CanInBuffers[j].LastRxTime.SetTimer(0); // record 'now'
        memcpy(CanInBuffers[j].Can.pMessage,Msg,CanInBuffers[j].Can.Length);
        if (CanInBuffers[j].RxFunction)
          CanInBuffers[j].RxFunction();
        break; // skip out of the loop
      }
    }
  }

  // The rest of this routine doesn't need to run at nearly the same interval
  // as checking for message arrivals, so we'll scale back how often the time-out and
  // transmit checks are made.  If we keep at the same 0.4ms rate, and if we only run
  // every few calls, then we'd check at 3.2ms intervals -- close enough!
  if (EntryCount % 8 != 0)
    return;


  if (GrossTimeoutChecker.IsTimeout()) {
    bool AnyExpired = false;
    for (j=0; j<NUM_IN_BUFFERS && CanInBuffers[j].Can.COBID; j++) {
      long Since = CanPollElapsedFromLastRxByIndex(j);
      if (Since > NOT_TALKING_TIMEOUT) {
        AnyExpired = true;
        Serial.print("Rx[");
        Serial.print(j);
        Serial.print("]=");
        Serial.println(Since);
      }
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
      while (CheckRX < NUM_IN_BUFFERS) {
        int NID = GET_NID(CanInBuffers[CheckRX].Can.COBID);
        if (NID &&
            !(CanInBuffers[CheckRX].Can.COBID&IS_EXTENDED_COBID) &&
            CanPollElapsedFromLastRxByIndex(CheckRX) > NOT_TALKING_TIMEOUT) {
          // haven't heard from this guy, set his Comm parameter
          // The PDO1/PDO2 parameter means 0x1800/0x1801
          int Index = MapFnToCommParam(CanInBuffers[CheckRX].Can.COBID);
          int SubIndex = 2;
          int Value = 1;
          SDOwrite(NID,Index,SubIndex,Value,1);
          CheckRX++;
          NoCommTimer.SetTimer(NO_COMM_INTERVAL);
#if !defined(DO_LOGGING)
          Serial.print("SDO to NID 0x");
          Serial.print(NID,HEX);
          Serial.print(" Rx[");
          Serial.print(CheckRX);
          Serial.print("]: write 0x");
          Serial.print(Index,HEX);
          Serial.print(".");
          Serial.print(SubIndex);
          Serial.print(" = ");
          Serial.println(Value);
#endif
          return;
        }
        CheckRX++;
      }
      // Don't forget to check the outputs too.  The DIO modules have to be placed
      // into output mode, each output needs a "1" bit in 0x2250.1 that is to be an output
      while (CheckRX < NUM_IN_BUFFERS+NUM_OUT_BUFFERS*2) {
        int subscript = CheckRX-NUM_IN_BUFFERS;
        bool hb = (subscript % 2); // false, then true
        subscript = subscript / 2; // 0..NUM_OUT_BUFFERS
        int NID = GET_NID(CanOutBuffers[subscript].Can.COBID);
        if (!(IS_EXTENDED_COBID & CanOutBuffers[subscript].Can.COBID) && // not any of the extended ones AND
            NID) {                                                       // not SYNC
          // defined module that should be configured to define all bits as outpus
          int Index = 0x2250;
          int SubIndex = 1;
          INT32U Value = CanOutBuffers[subscript].OutputMask;
          byte nbytes = 1;
          if (hb) {
            Index = 0x1016;
            SubIndex = 1;
            Value = (0x01LL << 16) | 300; // they should look for 'our' ID of #1 within 300ms; we'll make a HeartBeat in our list of tx messages
            nbytes = 4;
          }
          SDOwrite(NID,Index,SubIndex,Value,nbytes);
          CheckRX++;
          NoCommTimer.SetTimer(NO_COMM_INTERVAL);
#if !defined(DO_LOGGING)
          Serial.print("SDO to NID 0x");
          Serial.print(NID,HEX);
          Serial.print(" Tx[");
          Serial.print(subscript);
          Serial.print("]: write 0x");
          Serial.print(Index,HEX);
          Serial.print(".");
          Serial.print(SubIndex,HEX);
          Serial.print(" = ");
          Serial.println(Value);
#endif
          return;
        }
        CheckRX++;
      }
      // we've looped once thru all the units and set their PDO Comm param to
      // answer us on a SYNC.  First time after the individual messages, send a
      // NMT to get all units Operational
      if (CheckRX <= NUM_IN_BUFFERS+NUM_OUT_BUFFERS*2) {
        NMTsend();
        CheckRX = NUM_IN_BUFFERS+NUM_OUT_BUFFERS*2+1; // forces 'next state'
        NoCommTimer.SetTimer(NO_COMM_INTERVAL);
#if !defined(DO_LOGGING)
        Serial.println("NMT");
#endif
        return;
      }
      SYNCsend();
#if !defined(DO_LOGGING)
      Serial.println("SYNC");
#endif
      NoCommTimer.SetTimer(NO_COMM_INTERVAL);
      CheckRX = 0; // go ahead and wrap around, in case something still isn't answering
      return;
    }
  }
  else {
    if (NominalTimer.IsTimeout()) {
      // see if it is time to send any of our messages at their interval
      for (j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
        if (CanOutBuffers[j].NextSendTime.IsTimeout()) {
          INT32U COBID = CanOutBuffers[j].Can.COBID&COB_ID_MASK;
          char ret = CAN.sendMsgBuf(COBID,CanOutBuffers[j].Can.COBID&IS_EXTENDED_COBID?1:0,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
#ifdef DO_LOGGING
          AddToDisplayBuffer(COBID,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
#endif
          CanOutBuffers[j].NextSendTime.SetTimer(CAN_TX_INTERVAL);
          NominalTimer.IncrementTimer(CanPollerNominalOutputInterval); // we can only put one out this often
          NoCommTimer.SetTimer(0); // keep it expired
          if (ret == CAN_OK)
            OK++;
          else
            Fault++;
          break; // skip out of for loop
        }
      }
    }
  } // Comm appears to be OK
} // CanPoller

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
        if (CanOutBuffers[j].Can.COBID & 0xfff00000l)
          Serial.print("] 0x");
        else
          Serial.print("]\t0x");
        Serial.print(CanOutBuffers[j].Can.COBID&COB_ID_MASK,HEX);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.Length);
        Serial.print("\t0x");
        Serial.print((long)CanOutBuffers[j].Can.pMessage,HEX);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.COBID & IS_EXTENDED_COBID?1:0);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].NextSendTime.getEndTime());
        if (CanOutBuffers[j].Can.Length) {
          Serial.print("\t[");
          for (int k=0; k<CanOutBuffers[j].Can.Length; k++) {
            Serial.print(CanOutBuffers[j].Can.pMessage[k],HEX);
            if (k<CanOutBuffers[j].Can.Length-1)
              Serial.print(" ");
          }
          Serial.print("]");
        }
        Serial.println();
      }
    }
    Serial.println();
  }

  if (io & 2) {
    Serial.println("Input buffers defined:");
    Serial.println("Index\tCOBID\tLen\t&buffer\tLastRx");
    for (int j=0; j<NUM_IN_BUFFERS; j++) {
      if (CanInBuffers[j].Can.COBID) {
        Serial.print("[");
        Serial.print(j);
        if (CanInBuffers[j].Can.COBID & 0xfff00000l)
          Serial.print("] 0x");
        else
          Serial.print("]\t0x");
        Serial.print(CanInBuffers[j].Can.COBID&COB_ID_MASK,HEX);
        Serial.print("\t");
        Serial.print(CanInBuffers[j].Can.Length);
        Serial.print("\t0x");
        Serial.print((long)CanInBuffers[j].Can.pMessage,HEX);
        Serial.print("\t");
        Serial.print(CanInBuffers[j].LastRxTime.getEndTime());
        if (CanInBuffers[j].Can.Length) {
          Serial.print("\t[");
          for (int k=0; k<CanInBuffers[j].Can.Length; k++) {
            Serial.print(CanInBuffers[j].Can.pMessage[k],HEX);
            if (k<CanInBuffers[j].Can.Length-1)
              Serial.print(" ");
          }
          Serial.print("]");
        }
        Serial.println();
      }
    }
    Serial.println();
  }
} // CanPollDisplay

