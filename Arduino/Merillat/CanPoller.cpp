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


#ifdef DO_LOGGING
volatile int Head = 0; // updated in ISR
int Tail = 0;
volatile CanRXType CanRXBuff[NUM_BUFFS];
void AddToDisplayBuffer(INT32U id, INT8U len, INT8U *buf)
{
  int Next = (Head + 1) % NUM_BUFFS;
  if (Next != Tail) {
    CanRXBuff[Next].COBID = id; // MSBit set if isExtendedFrame()
    CanRXBuff[Next].Length = len;
    CanRXBuff[Next].Time = millis();
    for (int i=0; i<len; i++)
      CanRXBuff[Next].Message[i] = buf[i];
    Head = Next;
  }
  else
    Serial.println("Overflow");
} // AddToDisplayBuffer

// v: value to print
// num_places: number of bits we want to display
void print_hex(long int v, int num_places)
{
  int mask=0, n, num_nibbles, digit;

  for (n=1; n<=num_places; n++)
  {
    mask = (mask << 1) | 0x0001;
  }
  v = v & mask; // truncate v to specified number of places

  num_nibbles = (num_places+3) / 4;
  if ((num_places % 4) != 0)
  {
    ++num_nibbles;
  }

  do
  {
    digit = ((v >> (num_nibbles-1) * 4)) & 0x0f;
    Serial.print(digit, HEX);
  } while(--num_nibbles);
} // print_hex
#endif // DO_LOGGING

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
// This is being call upon a hardware timer every 1ms thru use of the FlexiTimer2 class.
// It services both in and outbound messages, so MUST BE EFFICIENT!!
void CanPoller()
{
  static bool ReEntry = false;
  if (ReEntry)
    return;
  ReEntry = true;

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
          ReEntry = false;
          return;
        }
        CheckRX++;
      }
      // Don't forget to check the outputs too.  The DIO modules have to be placed
      // into output mode, each output needs a "1" bit in 0x2250.1 that is to be an output
      while (CheckRX < NUM_IN_BUFFERS+NUM_OUT_BUFFERS) {
        int subscript = CheckRX-NUM_IN_BUFFERS;
        int NID = GET_NID(CanOutBuffers[CheckRX-NUM_IN_BUFFERS].Can.COBID);
        if (!(IS_EXTENDED_COBID & CanOutBuffers[CheckRX-NUM_IN_BUFFERS].Can.COBID) && // not any of the extended ones AND
            NID) {                                                                    // not SYNC
          // defined module that should be configured to define all bits as outpus
          int Index = 0x2250;
          int SubIndex = 1;
          int Value = CanOutBuffers[CheckRX-NUM_IN_BUFFERS].OutputMask;
          SDOwrite(NID,Index,SubIndex,Value,1);
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
          ReEntry = false;
          return;
        }
        CheckRX++;
      }
      // we've looped once thru all the units and set their PDO Comm param to
      // answer us on a SYNC.  First time after the individual messages, send a
      // NMT to get all units Operational
      if (CheckRX <= NUM_IN_BUFFERS+NUM_OUT_BUFFERS) {
        NMTsend();
        CheckRX = NUM_IN_BUFFERS+NUM_OUT_BUFFERS+1; // forces 'next state'
        NoCommTimer.SetTimer(NO_COMM_INTERVAL);
#if !defined(DO_LOGGING)
        Serial.println("NMT");
#endif
        ReEntry = false;
        return;
      }
      SYNCsend();
#if !defined(DO_LOGGING)
      Serial.println("SYNC");
#endif
      NoCommTimer.SetTimer(NO_COMM_INTERVAL);
      CheckRX = 0; // go ahead and wrap around, in case something still isn't answering
      ReEntry = false;
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
          NominalTimer.SetTimer(CanPollerNominalOutputInterval); // we can only put one out this often
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
  ReEntry = false;
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
        Serial.print("]\t0x");
        Serial.print(CanOutBuffers[j].Can.COBID&COB_ID_MASK,HEX);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.Length);
        Serial.print("\t");
        Serial.print((long)CanOutBuffers[j].Can.pMessage);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.COBID & IS_EXTENDED_COBID?1:0);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].NextSendTime.getEndTime());
        if (CanOutBuffers[j].Can.Length) {
          Serial.print("\t[");
          Serial.print(CanOutBuffers[j].Can.pMessage[0],HEX);
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
        Serial.print("]\t0x");
        Serial.print(CanInBuffers[j].Can.COBID&COB_ID_MASK,HEX);
        Serial.print("\t");
        Serial.print(CanInBuffers[j].Can.Length);
        Serial.print("\t");
        Serial.print((long)CanInBuffers[j].Can.pMessage);
        Serial.print("\t");
        Serial.print(CanInBuffers[j].LastRxTime.getEndTime());
        if (CanInBuffers[j].Can.Length) {
          Serial.print("\t[");
          for (int k=0; k<CanInBuffers[j].Can.Length; k++) {
            Serial.print(CanInBuffers[j].Can.pMessage[0],HEX);
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

