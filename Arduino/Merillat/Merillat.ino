#include <EEPROM.h>

// demo: CAN-BUS Shield, send data
#include "CanPoller.h"

// NodeIDs of CAN devices in the different boxes:
// (New) South Door control box:
#define ESD_SOUTH_DOOR_DIO_OUTPUTS 7
#define ESD_SOUTH_DOOR_DIO_INPUTS 8
#define ESD_SOUTH_DOOR_ANALOG 20
#define IMTRA_PPC800_SOUTH 120 // Extended
// Existing South Hydraulic control box:
#define ESD_SOUTH_HYDRAULIC 10
// (New) North Door control box:
#define ESD_NORTH_DOOR_DIO 11
#define ESD_NORTH_DOOR_ANALOG 22
#define IMTRA_PPC800_NORTH 140 // Extended
// Existing North Hydraulic control box:
#define ESD_NORTH_HYDRAULIC 33

// Enough CANOPEN stuff to define COBIDs
#define MK_COBID(ID,FN) (ID&0x7f | (FN&7<<11)) // TODO: FIX
#define TXPDO1 0 // TODO: FIX
#define RXPDO1 0 // TODO: FIX

// Make up the COBIDs we'll need to go with the Inputs below
#define SOUTHDOORDIO_RX_COBID    MK_COBID(ESD_SOUTH_DOOR_DIO_INPUTS,TXPDO1)
#define SOUTHDOORANALOG_RX_COBID MK_COBID(ESD_SOUTH_DOOR_ANALOG,TXPDO1)
#define SOUTHTHRUSTER_RX_COBID   MK_COBID(IMTRA_PPC800_SOUTH,TXPDO1) // Extended!
#define NORTHDOORDIO_RX_COBID    MK_COBID(ESD_NORTH_DOOR_DIO,TXPDO1)
#define NORTHDOORANALOG_RX_COBID MK_COBID(ESD_NORTH_DOOR_ANALOG,TXPDO1)
#define NORTHTHRUSTER_RX_COBID   MK_COBID(IMTRA_PPC800_NORTH,TXPDO1) // Extended!
#define NORTHHYDRAULIC_RX_COBID  MK_COBID(ESD_NORTH_HYDRAULIC,TXPDO1)
// Outputs
#define SOUTHDOORDIO_TX_COBID   MK_COBID(ESD_SOUTH_DOOR_DIO_OUTPUTS,RXPDO1)
#define SOUTHTHRUSTER_TX_COBID  MK_COBID(IMTRA_PPC800_SOUTH,RXPDO1)  // Extended!
#define SOUTHHYDRAULIC_TX_COBID MK_COBID(ESD_SOUTH_HYDRAULIC,RXPDO1)
#define NORTHDOORDIO_TX_COBID   MK_COBID(ESD_NORTH_DOOR_DIO,RXPDO1)
#define NORTHTHRUSTER_TX_COBID  MK_COBID(IMTRA_PPC800_NORTH,RXPDO1)  // Extended!
#define NORTHHYDRAULIC_TX_COBID MK_COBID(ESD_NORTH_HYDRAULIC,RXPDO1)


struct {
  INT8U SouthDoorDIO_Rx;
  INT8U SouthDoorAnalog_Rx[2];
  INT8U SouthThruster_Rx[8];

  // South Hydraulic doesn't have Inputs

  INT8U NorthDoorDIO_Rx;
  INT8U NorthDoorAnalog_Rx[2];
  INT8U NorthThruster_Rx[8];

  INT8U NorthHydraulic_Rx;
} Inputs;

struct {
  INT8U SouthDoorDIO_Tx;
  INT8U SouthThruster_Tx[8];

  INT8U SouthHydraulic_Tx;
  
  INT8U NorthDoorDIO_Tx;
  INT8U NorthThruster_Tx[8];

  INT8U NorthHydraulic_Tx;
} Outputs;


void setup()
{
  Serial.begin(115200);
  CanPollerInit();

  while (CAN_OK != CAN.begin(CAN_500KBPS))              // init can bus : baudrate = 500k
  {
      Serial.println("CAN BUS Shield init fail");
      Serial.println(" Init CAN BUS Shield again");
      delay(100);
  }
  Serial.println("CAN BUS Shield init ok!");
  Serial.print("EEPROM size = ");
  Serial.println(EEPROM.length());

  // list of COBIDs we want to collect from the bus
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx);
  CanPollSetRx(SOUTHDOORANALOG_RX_COBID,sizeof(Inputs.SouthDoorAnalog_Rx),(INT8U*)&Inputs.SouthDoorAnalog_Rx);
  CanPollSetRx(SOUTHTHRUSTER_RX_COBID,  sizeof(Inputs.SouthThruster_Rx),  (INT8U*)&Inputs.SouthThruster_Rx);
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx);
  CanPollSetRx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx);
  CanPollSetRx(NORTHTHRUSTER_RX_COBID,  sizeof(Inputs.NorthThruster_Rx),  (INT8U*)&Inputs.NorthThruster_Rx);
  CanPollSetRx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx);

  // list of COBIDs we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  CanPollSetTx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  false, (INT8U*)&Outputs.SouthDoorDIO_Tx);
  CanPollSetTx(SOUTHTHRUSTER_TX_COBID, sizeof(Outputs.SouthThruster_Tx), true,  (INT8U*)&Outputs.SouthThruster_Tx);
  CanPollSetTx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),false, (INT8U*)&Outputs.SouthHydraulic_Tx);
  CanPollSetTx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  false, (INT8U*)&Outputs.NorthDoorDIO_Tx);
  CanPollSetTx(NORTHTHRUSTER_TX_COBID, sizeof(Outputs.NorthThruster_Tx), true,  (INT8U*)&Outputs.NorthThruster_Tx);
  CanPollSetTx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),false, (INT8U*)&Outputs.NorthHydraulic_Tx);

  Serial.print("millis() at end of init = ");
  Serial.println(millis());
}

int maxCycle = 0;
float tCycle = 0;
int minCycle = 0x7fff;
int cycle = 0;
int counter = 0;

void dump() {
  unsigned long now = millis();
  Serial.print("millis()="); Serial.println(now);
  for (int j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
    Serial.print("["); Serial.print(j); Serial.print("]="); Serial.print(CanOutBuffers[j].NextSendTime); 
    unsigned long subtract = CanOutBuffers[j].NextSendTime-now;
    Serial.print(" ("); Serial.print(subtract); Serial.println(")");
  }
  Serial.println();
}

void loop()
{
  // service Tx routine and see if it did anything this go
  if (CanPoller()) {
    // see how our cycle counters show our CPU loading is
    if (cycle > maxCycle)
      maxCycle = cycle;
    if (cycle < minCycle)
      minCycle = cycle;
    tCycle += cycle;
    counter++;
    if (cycle < 100) {
      // wrapped, print all our Nexts
      dump();
    }
    cycle=0; // restart
  }

  if (Serial.available()) {
    if (Serial.read() == 'd')
      dump();
    else {
      Serial.println();
      Serial.print("OK=");
      Serial.print(OK);
      Serial.print(", Fault=");
      Serial.println(Fault);
  
      Serial.print("min=");
      Serial.print(minCycle);
      Serial.print(", max=");
      Serial.print(maxCycle);
      Serial.print(", Avg=");
      Serial.print(tCycle/counter);
      Serial.print(" in ");
      Serial.print(millis());
      Serial.println("ms of running");
    }
  }
  cycle++;
}

