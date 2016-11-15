#include <EEPROM.h>

#include "CanPoller.h"
#include "IODefs.h"


/* Merillat Boathouse Controller:
 *  Designing to run on:
 *  - Arduino MEGA module (from SainSmart)
 *  - CAN-BUS Shield v1.2 from Seeed Studio
 *  - 3.2" 240x320 touch screen (SainSmart)
 *
 *  The CAN-BUS Shield sits on the MEGA, but 2 pins on each side make
 *  no connection to the MEGA module.  This is OK.
 *
 *  The touch screen connection is a set of parallel pins on a dual-row header
 *  that needs to be extended (two rows of 2x18 pass-thru header required, 5/8" tall).
 *  Also need 2 nylon stand offs (1/8" dia screw x 7/16" body) with 2 nylon nuts
 */

void setup()
{
  Serial.begin(115200);
  Serial.println("Merillat boathouse controller");
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

  // Data we want to collect from the bus
  //           COBID,                   NumberOfBytesToReceive,           AddressOfDataStorage
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx);
  CanPollSetRx(SOUTHDOORANALOG_RX_COBID,sizeof(Inputs.SouthDoorAnalog_Rx),(INT8U*)&Inputs.SouthDoorAnalog_Rx);
  CanPollSetRx(SOUTHTHRUSTER_RX_COBID,  sizeof(Inputs.SouthThruster_Rx),  (INT8U*)&Inputs.SouthThruster_Rx);
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx);
  CanPollSetRx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx);
  CanPollSetRx(NORTHTHRUSTER_RX_COBID,  sizeof(Inputs.NorthThruster_Rx),  (INT8U*)&Inputs.NorthThruster_Rx);
  CanPollSetRx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx);

  // Data we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  //           COBID,                  NumberOfBytesToTransmit,          29-bit?,AddressOfDataToTransmit
  CanPollSetTx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  false, (INT8U*)&Outputs.SouthDoorDIO_Tx);
  CanPollSetTx(SOUTHTHRUSTER_TX_COBID, sizeof(Outputs.SouthThruster_Tx), true,  (INT8U*)&Outputs.SouthThruster_Tx);
  CanPollSetTx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),false, (INT8U*)&Outputs.SouthHydraulic_Tx);
  CanPollSetTx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  false, (INT8U*)&Outputs.NorthDoorDIO_Tx);
  CanPollSetTx(NORTHTHRUSTER_TX_COBID, sizeof(Outputs.NorthThruster_Tx), true,  (INT8U*)&Outputs.NorthThruster_Tx);
  CanPollSetTx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),false, (INT8U*)&Outputs.NorthHydraulic_Tx);
}

int maxCycle = 0;
float tCycle = 0;
int minCycle = 0x7fff;
int cycle = 0;
int counter = 0;

void dump() {
  CFwTimer now(0); // what 'now' is
  Serial.print("now()="); Serial.println(now.getStartTime());
  for (int j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
    Serial.print("["); Serial.print(j); Serial.print("]="); Serial.println(CanOutBuffers[j].NextSendTime.getStartTime());
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

