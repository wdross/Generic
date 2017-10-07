/*
  Create an array of messages that can be received into (at any time)
  or sent from at a given interval.
  
*/
#include "CanPoller.h"
#include "IODefs.h"
#include "CanOpen.h"

int sizeofRawArray = 0;
uint32_t* pRawArray;

long int Fault=0;
long int OK=0;
long int OutputSetTimer=INFINITE;

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
      CanInBuffers[j].Can.COBID = COBID & COB_ID_MASK; // make sure any Extended indicator bit is cleared off
      CanInBuffers[j].LastRxTime.SetTimer(INFINITE); // nothing yet
      CanInBuffers[j].RxFunction = userFN;
      break; // we're done
    }
  }
} // CanPollSetRx

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
      CanOutBuffers[j].NextSendTime.SetTimer(INFINITE);
      break; // we're done, now set the Tx intervals
    }
  }
  if (j < NUM_OUT_BUFFERS) {
  }
  else
    Serial.println("Attempt to set up more TX buffers than space allows");
} // CanPollSetTx

// This is being called upon a hardware timer every 0.4ms thru use of the FlexiTimer2 class.
// The time value was selected based upon some sample data taken from 7 ESD modules connected
// and responding in our needed fashion, and seeing the shortest aparent message was a single
// 0.26ms but is more typically 0.44-0.52ms per shortest arrival message.
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
      if (COBID == CanInBuffers[j].Can.COBID) {
        // found match, record time and save the bytes
        CanInBuffers[j].LastRxTime.SetTimer(0); // record 'now'
        memcpy(CanInBuffers[j].Can.pMessage,Msg,CanInBuffers[j].Can.Length);


        // check if the pushbutton is pressed.
        // if it is, the buttonState is HIGH, so we want to do our simulation:
        if (//digitalRead(DOOR_SIMULATE_PIN) == LOW && // jumper/switch says to SIMULATE AND
            CanInBuffers[j].RxFunction) {             // there is a function that will emulate against this message
          CanInBuffers[j].RxFunction(); // invoke function
        }
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

  // see if it is time to send any of our messages at their interval
  for (int j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
    if (CanOutBuffers[j].NextSendTime.IsTimeout()) {
      INT32U COBID = CanOutBuffers[j].Can.COBID&COB_ID_MASK;
      char ret;
      if (COBID != CanOutBuffers[j].Can.COBID) {
        INT8U local[8];
        for (int k=0; k<sizeof(local); k++)
          local[k] = pgm_read_byte_near(&CanOutBuffers[j].Can.pMessage[k]);
        CanOutBuffers[j].Can.pMessage += sizeof(local); // advance 8 bytes
        if (CanOutBuffers[j].Can.pMessage - (byte*)pRawArray >= sizeofRawArray)
          CanOutBuffers[j].Can.pMessage = (byte*)pRawArray; // wrap
        ret = CAN.sendMsgBuf(COBID,CanOutBuffers[j].Can.COBID&IS_EXTENDED_COBID?1:0,CanOutBuffers[j].Can.Length,(INT8U*)&local);
#ifdef DO_LOGGING
        AddToDisplayBuffer(COBID,CanOutBuffers[j].Can.Length,(INT8U*)&local);
#endif
        CanOutBuffers[j].NextSendTime.IncrementTimer(OutputSetTimer); // controlled by a changeable timer
      }
      else {
        ret = CAN.sendMsgBuf(COBID,CanOutBuffers[j].Can.COBID&IS_EXTENDED_COBID?1:0,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
#ifdef DO_LOGGING
        AddToDisplayBuffer(COBID,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
#endif
        CanOutBuffers[j].NextSendTime.SetTimer(INFINITE); // causes to wait for a SYNC to be re-triggered
      }
      if (ret == CAN_OK)
        OK++;
      else
        Fault++;
      break; // skip out of for loop
    }
  }
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
        Serial.print("\t");
        Serial.print((long)CanOutBuffers[j].Can.pMessage,HEX);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].Can.COBID & IS_EXTENDED_COBID?1:0);
        Serial.print("\t");
        Serial.print(CanOutBuffers[j].NextSendTime.getEndTime());
        if (CanOutBuffers[j].Can.Length) {
          Serial.print("\t[");
          for (int k=0; k<CanOutBuffers[j].Can.Length; k++) {
            if ((CanOutBuffers[j].Can.COBID&COB_ID_MASK) != CanOutBuffers[j].Can.COBID)
              Serial.print(pgm_read_byte_near(&CanOutBuffers[j].Can.pMessage[k]),HEX);
            else
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
        Serial.print("\t");
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

