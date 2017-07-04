
#include <SPI.h>
#include <mcp_can.h>
#include <FlexiTimer2.h>
#include "IODefs.h"
#include "CanPoller.h"
#include "CanOpen.h"

// get rid of "deprecated conversion from string constant to 'char*'" messages
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


bool cycle = false;
CFwTimer SYNCTimer;

void RXSouthDoor() {
  // called when we get a SouthDoor message
//  Serial.println(South_Winter_Door_Position);
}


void setup()
{
  // initialize the pushbutton pin as an input:
  pinMode(DOOR_SIMULATE_PIN, INPUT);

  Serial.flush();
  delay(100);
  Serial.begin(115200);
  delay(100);

  CurrentCANBaud = CAN_250KBPS;
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
  CanPollSetRx(SOUTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx,   NULL);
  CanPollSetRx(SOUTHDOORANALOG_RX_COBID,sizeof(Inputs.SouthDoorAnalog_Rx),(INT8U*)&Inputs.SouthDoorAnalog_Rx,RXSouthDoor);
  CanPollSetRx(SOUTHTHRUSTER_RX_COBID,  sizeof(Inputs.SouthThruster_Rx),  (INT8U*)&Inputs.SouthThruster_Rx,  NULL);
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx,   NULL);
  CanPollSetRx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx,NULL);
  CanPollSetRx(NORTHTHRUSTER_RX_COBID,  sizeof(Inputs.NorthThruster_Rx),  (INT8U*)&Inputs.NorthThruster_Rx,  NULL);
  CanPollSetRx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx, NULL);

  // Data we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  //           COBID,                  NumberOfBytesToTransmit,          AddressOfDataToTransmit
  CanPollSetTx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  (INT8U*)&Outputs.SouthDoorDIO_Tx);
  CanPollSetTx(SOUTHTHRUSTER_TX_COBID, sizeof(Outputs.SouthThruster_Tx), (INT8U*)&Outputs.SouthThruster_Tx);
  CanPollSetTx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),(INT8U*)&Outputs.SouthHydraulic_Tx);
  CanPollSetTx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  (INT8U*)&Outputs.NorthDoorDIO_Tx);
  CanPollSetTx(NORTHTHRUSTER_TX_COBID, sizeof(Outputs.NorthThruster_Tx), (INT8U*)&Outputs.NorthThruster_Tx);
  CanPollSetTx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),(INT8U*)&Outputs.NorthHydraulic_Tx);

  FlexiTimer2::set(1, 0.0004, CanPoller); // Every 0.4 ms (400us)
  FlexiTimer2::start();

  startUpText();
  Serial.flush();
}

void startUpText()
{
  Serial.println("--------  CAN BUS Logger  ---------");
  Serial.println();
  Serial.println("To PAUSE: send a 'P' ");
  Serial.println();
  Serial.println("To RESUME: send a 'R' ");
  Serial.println();
  Serial.println("To change the BAUD rate: send a 'Bnnn', like B500 for 500kBaud");
  Serial.println();
  Serial.println("-----------------------------------------------------");
  Serial.println();
  Serial.println(" Init CAN BUS Shield OKAY!");
  Serial.println();
}



void loop()
{
  char serialRead;

  serialRead = Serial.read();
  if(serialRead == 'P')    // Type p in the Serial monitor bar p=pause
  {
    while(Serial.read() != 'R' ){}
  }
  else if (serialRead == 'S' || serialRead == 's') // SYNC transmit
  {
    SYNCsend();
  }
  else if (serialRead == 'n' || serialRead == 'N') // NMT 'start all nodes'
  {
    NMTsend();
  }
  else if (serialRead == 'd' || serialRead == 'D') // sDo
  {
    SDOread(ESD_SOUTH_DOOR_ANALOG,0x1a01,0);
  }
  else if (serialRead == 'w' || serialRead == 'W') // Write sdo
  {
    SDOwrite(ESD_SOUTH_DOOR_ANALOG,0x1801,2,1,1);
  }
  else if (serialRead == 'c' || serialRead == 'C') // Cycle
  {
    cycle = !cycle;
    if (cycle) {
      SYNCTimer.SetTimer(100);
    }
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

  if (cycle && SYNCTimer.IsTimeout()) {
    SYNCsend();
    SYNCTimer.IncrementTimer(100);
  }

  if (Head != Tail) {
    // not caught up, print out the next message we have
    Tail = (Tail + 1) % NUM_BUFFS;
    if (Overflow) {
      Serial.print("Overflow ");
      Serial.println(Overflow);
      Overflow = 0;
    }

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
    Serial.println((0.001)*CanRXBuff[Tail].Time, 3);
  }
}

