
#include <SPI.h>
#include <mcp_can.h>
#include "IODefs.h"
#include "CanPoller.h"

#pragma GCC diagnostic ignored "-Wwrite-strings"

const int DOOR_SIMULATE_PIN = 12;

int CurrentCANBaud = 0;

struct CAN_Entry {
  int kBaud;
  int CAN_Konstant;
  char *rate;
} CANBaudLookup[] = {
                      {0,0,"placeholder"},
                      {5,CAN_5KBPS,"5,000"},
                      {10,CAN_10KBPS,"10,000"},
                      {20,CAN_20KBPS,"20,000"},
                      {25,CAN_25KBPS,"25,000"},
                      {31,CAN_31K25BPS,"31,000"},
                      {33,CAN_33KBPS,"33,000"},
                      {40,CAN_40KBPS,"40,000"},
                      {50,CAN_50KBPS,"50,000"},
                      {80,CAN_80KBPS,"80,000"},
                      {83,CAN_83K3BPS,"83,000"},
                      {95,CAN_95KBPS,"95,000"},
                      {100,CAN_100KBPS,"100,000"},
                      {125,CAN_125KBPS,"125,000"},
                      {200,CAN_200KBPS,"200,000"},
                      {250,CAN_250KBPS,"250,000"},
                      {500,CAN_500KBPS,"500,000"},
                      {666,CAN_666KBPS,"666,000"},
                      {1000,CAN_1000KBPS,"1,000,000"}
};


volatile int Head = 0; // updated in ISR
int Tail = 0;
#define NUM_BUFFS 10
struct CanRXType {
  INT32U COBID;
  INT8U Length;
  INT8U Message[8];
  INT32U Time;
} volatile CanRXBuff[NUM_BUFFS];


void RXSouthDoor() {
  // called when we get a SouthDoor message
  Serial.println("South door !!!");
}


void CanPoller()
{
  // let's receive all messages into their registered buffer
  while (CAN_MSGAVAIL == CAN.checkReceive()) { // while data present
    INT8U len;
    INT8U Msg[8];
    CAN.readMsgBuf(&len, Msg); // read data,  len: data length, buf: data buf
    INT32U COBID = CAN.getCanId() | (CAN.isExtendedFrame()?IS_EXTENDED_COBID:0); // valid after readMsgBuf()

    int Next = (Head + 1) % NUM_BUFFS;
    if (Next != Tail) {
      CanRXBuff[Next].COBID = COBID; // MSBit set if isExtendedFrame()
      CanRXBuff[Next].Length = len;
      CanRXBuff[Next].Time = millis();
      for (int i=0; i<len; i++)
        CanRXBuff[Next].Message[i] = Msg[i];
      Head = Next;
    }

    for (int j=0; j<NUM_IN_BUFFERS && CanInBuffers[j].Can.COBID; j++) {
      if (COBID == CanInBuffers[j].Can.COBID) { // also checks that we wanted ExtendedFrame() or not
        // found match, record time and save the bytes
        CanInBuffers[j].LastRxTime.SetTimer(0); // record 'now'
        memcpy(CanInBuffers[j].Can.pMessage,Msg,CanInBuffers[j].Can.Length);


        // check if the pushbutton is pressed.
        // if it is, the buttonState is HIGH, so we want to do our simulation:
        if (digitalRead(DOOR_SIMULATE_PIN) == LOW && // jumper/switch says to SIMULATE AND
            CanInBuffers[j].RxFunction) {             // there is a function that will emulate against this message
          CanInBuffers[j].RxFunction(); // invoke function
        }
      }
    }
  }

  // see if it is time to send any of our messages at their interval
  for (int j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
    if (CanOutBuffers[j].NextSendTime.IsTimeout()) {
      char ret = CAN.sendMsgBuf(CanOutBuffers[j].Can.COBID&COB_ID_MASK,CanOutBuffers[j].Can.COBID&IS_EXTENDED_COBID,CanOutBuffers[j].Can.Length,CanOutBuffers[j].Can.pMessage);
      CanOutBuffers[j].NextSendTime.SetTimer(INFINITE);
      if (ret == CAN_OK)
        OK++;
      else
        Fault++;
    }
  }
  return;
}


void setup()
{
  // initialize the pushbutton pin as an input:
  pinMode(DOOR_SIMULATE_PIN, INPUT);

  Serial.flush();
  delay(100);
  Serial.begin(115200);
  delay(100);

  CurrentCANBaud = CAN_500KBPS;
  while (CAN_OK != CAN.begin(CurrentCANBaud))              // init can bus
  {
    Serial.println("CAN BUS Shield init fail");
    Serial.println(" Init CAN BUS Shield again");
    delay(100);
  }
  Serial.print("Setup complete for CAN baudrate of ");
  Serial.println(CANBaudLookup[CurrentCANBaud].rate);
  Serial.println();

  CanPollerInit();
  // always set up CAN buffers, in case the input comes on later
  // These sections of Tx/Rx are opposite of the definitions in Merillat.ino
  // Data we want to collect from the bus
  //           COBID,                   NumberOfBytesToReceive,           AddressOfDataStorage
  CanPollSetTx(SOUTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx);
  CanPollSetTx(SOUTHDOORANALOG_RX_COBID,sizeof(Inputs.SouthDoorAnalog_Rx),(INT8U*)&Inputs.SouthDoorAnalog_Rx);
  CanPollSetTx(SOUTHTHRUSTER_RX_COBID,  sizeof(Inputs.SouthThruster_Rx),  (INT8U*)&Inputs.SouthThruster_Rx);
  CanPollSetTx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx);
  CanPollSetTx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx);
  CanPollSetTx(NORTHTHRUSTER_RX_COBID,  sizeof(Inputs.NorthThruster_Rx),  (INT8U*)&Inputs.NorthThruster_Rx);
  CanPollSetTx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx);

  // Data we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  //           COBID,                  NumberOfBytesToTransmit,          AddressOfDataToTransmit
  CanPollSetRx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  (INT8U*)&Outputs.SouthDoorDIO_Tx,  RXSouthDoor);
  CanPollSetRx(SOUTHTHRUSTER_TX_COBID, sizeof(Outputs.SouthThruster_Tx), (INT8U*)&Outputs.SouthThruster_Tx, NULL);
  CanPollSetRx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),(INT8U*)&Outputs.SouthHydraulic_Tx,NULL);
  CanPollSetRx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  (INT8U*)&Outputs.NorthDoorDIO_Tx,  NULL);
  CanPollSetRx(NORTHTHRUSTER_TX_COBID, sizeof(Outputs.NorthThruster_Tx), (INT8U*)&Outputs.NorthThruster_Tx, NULL);
  CanPollSetRx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),(INT8U*)&Outputs.NorthHydraulic_Tx,NULL);

  attachInterrupt(digitalPinToInterrupt(2), CanPoller, FALLING); // start interrupt for pin 2 (Interrupt 0)

  startUpText();
  Serial.flush();
}

void startUpText()
{
  Serial.println("--------  Arduino CAN BUS Monitor Program  ---------");
  Serial.println();
  Serial.println("To PAUSE the data: send a 'P' ");
  Serial.println();
  Serial.println("To RESUME the data: send a 'R' ");
  Serial.println();
  Serial.println("To END the data log: send a 'X' ");
  Serial.println();
  Serial.println("To change the BAUD rate: send a 'Bnnn', like B500 for 500kBaud");
  Serial.println();
  Serial.println("Paste data into the CAN_Messages_Tool.xlsm for parsing");
  Serial.println();
  Serial.println("-----------------------------------------------------");
  Serial.println();
  Serial.println(" Init CAN BUS Shield OKAY!");
  Serial.println();
/*  
  Serial.println(" Hit ENTER Key to go on the BUS.");
  Serial.println();

  while(Serial.read() != '\n' ){}
  */
}


void print_hex(int v, int num_places)
{
  int mask=0, n, num_nibbles, digit;

  for (n=1; n<=num_places; n++)
  {
    mask = (mask << 1) | 0x0001;
  }
  v = v & mask; // truncate v to specified number of places

  num_nibbles = num_places / 4;
  if ((num_places % 4) != 0)
  {
    ++num_nibbles;
  }

  do
  {
    digit = ((v >> (num_nibbles-1) * 4)) & 0x0f;
    Serial.print(digit, HEX);
  } while(--num_nibbles);
}


void loop()
{
  char serialRead;

  serialRead = Serial.read();
  if(serialRead == 'P')    // Type p in the Serial monitor bar p=pause
  {
    while(Serial.read() != 'R' ){}
  }
  else if(serialRead == 'X')    // Type p in the Serial monitor bar p=pause
  {
    Serial.end();
  }
  else if(serialRead == 'B') // Baud rate request
  {
    int b = 0;
    serialRead = Serial.read();
    while (serialRead >= '0' && serialRead <= '9') {
      b = b*10 + (serialRead - '0');
      serialRead = Serial.read();
    }
    int nB = CurrentCANBaud;
    for (int i=1; i<sizeof(CANBaudLookup)/sizeof(CANBaudLookup[0]); i++) {
      if (CANBaudLookup[i].kBaud == b) {
        nB = CANBaudLookup[i].CAN_Konstant;
        break;
      }
    }
    if (nB == CurrentCANBaud) {
      Serial.print("Leaving CAN Baud rate unchanged at ");
      Serial.println(CANBaudLookup[nB].rate);
    }
    else {
      Serial.print("Changing CAN Baud rate to ");
      Serial.println(CANBaudLookup[nB].rate);
      CurrentCANBaud = nB;

      while (CAN_OK != CAN.begin(CurrentCANBaud))              // init can bus
      {
        Serial.println("CAN BUS Shield init fail");
        Serial.println(" Init CAN BUS Shield again");
        delay(100);
      }
      Serial.println("OK!");
    }
  }

  if (Head != Tail) {
    // not caught up, print out the next message we have

    Serial.print(" 0    ");
    print_hex(CanRXBuff[Tail].COBID, 32);
    Serial.print("         ");
    Serial.print(CanRXBuff[Tail].Length);
    Serial.print("  ");
    for(int i = 0; i<8; i++)    // print the data
    {
      if(i >= CanRXBuff[Tail].Length)
        Serial.print("  ");   // Print 8-bytes no matter what
      else
        print_hex(CanRXBuff[Tail].Message[i],8);
      Serial.print("  ");
    }
    Serial.print("     ");
    Serial.println((0.000001)*CanRXBuff[Tail].Time, 6);
    Tail = (Tail + 1) % NUM_BUFFS;
  }
}

