#if !defined(IODEFS_INCLUDED)
#define IODEFS_INCLUDED

#include "CanOpen.h"
#include "BitObject.h"
#include "AnalogObject.h"

// This will define the I/O points for the Merillat boathouse door project
// Intended to be multiply included, and once per project will include with
// #define DEFINE_BITOBJECTS already declared, which will allocate the
// required data elements to link against.

// NodeIDs of CAN devices in the different boxes:
// (New) South Door control box:
#define ESD_SOUTH_DOOR_DIO_OUTPUTS 0x11
#define ESD_SOUTH_DOOR_DIO_INPUTS 0x12
#define ESD_SOUTH_DOOR_ANALOG 0x13
#define IMTRA_PPC800 0x08FF0000L|IS_EXTENDED_COBID
// Existing South Hydraulic control box:
#define ESD_SOUTH_HYDRAULIC 0x21
// (New) North Door control box:
#define ESD_NORTH_DOOR_DIO 0x31
#define ESD_NORTH_DOOR_ANALOG 0x32
// Existing North Hydraulic control box:
#define ESD_NORTH_HYDRAULIC 0x41

#define RX_PGN 0x11000100L // turning on these bits max the RX PGN

// Make up the COBIDs we'll need to go with the Inputs below
#define SOUTHDOORDIO_RX_COBID    MK_COBID(ESD_SOUTH_DOOR_DIO_INPUTS,TXPDO1) // 0x192
#define SOUTHDOORANALOG_RX_COBID MK_COBID(ESD_SOUTH_DOOR_ANALOG,TXPDO2)     // 0x293
#define THRUSTER_RX_COBID       (IMTRA_PPC800|RX_PGN) // Extended!        0x19FF0100
#define NORTHDOORDIO_RX_COBID    MK_COBID(ESD_NORTH_DOOR_DIO,TXPDO1)        // 0x1B1
#define NORTHDOORANALOG_RX_COBID MK_COBID(ESD_NORTH_DOOR_ANALOG,TXPDO2)     // 0x2B2
#define NORTHHYDRAULIC_RX_COBID  MK_COBID(ESD_NORTH_HYDRAULIC,TXPDO1)       // 0x1C1
// Outputs
#define SOUTHDOORDIO_TX_COBID   MK_COBID(ESD_SOUTH_DOOR_DIO_OUTPUTS,RXPDO1)
#define THRUSTER_TX_COBID       (IMTRA_PPC800)  // Extended!
#define SOUTHHYDRAULIC_TX_COBID MK_COBID(ESD_SOUTH_HYDRAULIC,RXPDO1)
#define NORTHDOORDIO_TX_COBID   MK_COBID(ESD_NORTH_DOOR_DIO,RXPDO1)
#define NORTHHYDRAULIC_TX_COBID MK_COBID(ESD_NORTH_HYDRAULIC,RXPDO1)


#define NO_ACTION 0 // (unused) Retract field of PGN65280CanFrame

// Direction field of PGN65280CanFrame
#define DIRECTION_NONE 0
#define DIRECTION_STARBOARD 1
#define DIRECTION_PORT 2
#define DIRECTION_CLOSE DIRECTION_STARBOARD
#define DIRECTION_OPEN  DIRECTION_PORT

struct FC_SN {
  unsigned int SN:3; // Sequence Number
  unsigned int FC:5; // Frame Counter
};

struct PGN130817 {
  // Pages 15..16 of "S-link Gateway 01_0_confidential_2016_LR.pdf"
  CFwTimer LastRxTime;

  struct FC_SN fc_sn;

  INT8U        NDB; // Number of Data Bytes to be sent/received

  INT16U       ManufacturerCode:11; //      #1
  INT16U       ReservedA:2; // 0x3          #2
  INT16U       IndustryGroup:3; //          #3

  INT8U        SleipnerDeviceType; //       #4

  INT16U       SleipnerDeviceInstance:4; // #5
  INT16U       ReservedB:4; // 0xf          #6

  INT16U       Status; //                   #7

  INT8U        ThrusterMotorTemperature; // #8
  INT8U        ThrusterPowerTemperature; // #9

  INT8U        ReservedC; //               #10

  INT16U       MotorVoltage; // 0.01v      #11

  INT16U       MotorRPM; // N/A, 1/4RPM,   #12

  INT16U       MotorCurrent; //            #13

  INT8U        OutputThrust; //            #14
}; // PGN130817 (multi-part message received from each PPC800)

struct PGN130817CanFrames {
  struct FC_SN fc_sn;

  union {
    struct {
      unsigned int NBD:8;                      // LSBits
      unsigned int ManufactureCode:11;
      unsigned int Reserved:2;
      unsigned int IndustryGroup:3;
      unsigned int SleipnerDeviceType:8;
      unsigned int SleipnerDeviceInstance:4;
      unsigned int ReservedB:4; // 0xf
      unsigned int Status:16;
    };
    struct {
      unsigned int ThrusterMotorTemperature:8; // LSBits
      unsigned int ThrusterPowerTemperature:8;
      unsigned int ReservedC:8;
      unsigned int MotorVoltage:16; // 0.01v
               int MotorRPM:16; // Not implemented, 1/4RPM
    };
    struct {
      unsigned int MotorCurrent:16;            // LSBits
      unsigned int OutputThrust:8;
      unsigned long AllFs:32;
    };
  };
}; // union of 3 sequential frames to Receive

// ThrusterInstance field of PGN65280CanFrame and index into array
enum {INPUT_INSTANCE, NORTH_INSTANCE, SOUTH_INSTANCE, NUM_INSTANCES};

struct PGN65280CanFrame {
  INT16U ManufactureCode:11; // LSbit
  INT16U Reserved:2;
  INT16U IndustryGroup:3;    // MSbit

  INT8U  ThrusterInstance:4; // LSbit
  INT8U  Direction:2;
  INT8U  Retract:2;          // MSbit

  INT16U Thrust:10;
  INT16U ReservedB:6;

  INT8U  ReservedC;
  INT16U ReservedD;
}; // message to TX to each Thruster

struct InputType {
  INT8U SouthDoorDIO_Rx;
  INT16S SouthDoorAnalog_Rx[2];

  // Will receive PGN130817 every 100ms
  union {
    INT8U Thruster_Rx[8];
    struct PGN130817CanFrames ThrusterRx;
  };

  // South Hydraulic doesn't have Inputs

  INT8U NorthDoorDIO_Rx;
  INT16S NorthDoorAnalog_Rx[2];

  INT8U NorthHydraulic_Rx;

  PGN130817 Thrusters[NUM_INSTANCES]; // AssembleBuffer, North, South (Instance ID of Sleipner frame)
};

enum LatchRequestType {lr_No_Request, lr_Unlatch_Request, lr_Latch_Request};

struct OutputType {
  INT8U SouthDoorDIO_Tx; // ID11: 5 Out
#define SOUTHDOOR_OUTPUT_MASK 0x9f

  INT8U SouthHydraulic_Tx;
#define SOUTHHYDRAULIC_OUTPUT_MASK 0x83

  INT8U NorthDoorDIO_Tx;
#define NORTHDOOR_OUTPUT_MASK 0xc3

  INT8U NorthHydraulic_Tx;
#define NORTHHYDRAULIC_OUTPUT_MASK 0x83

  // PPC needs a heartbeat of PGN65280CanFrame every 100ms (300ms timeout)
  struct PGN65280CanFrame Thrusters[NUM_INSTANCES]; // Unused, North, South (Instance ID of Sleipner frame)
};

enum DoorEnum {deWinterSouthDoor, deWinterNorthDoor, deUpperSouthDoor, deUpperNorthDoor, deNUM_DOORS};
// Elements that should be stored in EEPROM over power downs:
struct myEEPayloadType {
  char Signature[5];
  INT8U  TotalSize;
  struct DoorOpenCloseInfo Doors[deNUM_DOORS]; // 4
};
// wrap the above structure into a struct including it's checksum
struct myEEType {
  myEEPayloadType Data;
  INT32U CheckSum; // 32-bit CRC
};

struct DoorInfoStruct {
 char *Name;
 AnalogObject *Position;
 INT32U COBID;
 int Y;
};

extern myEEType myEE;
extern OutputType Outputs;
extern InputType  Inputs;

#endif // IODEFS_INCLUDED

// Depending upon desire to create instances of BitObjects, we'll be doing extern or not


#ifndef DEFINE_BITOBJECTS
extern
#endif
       DoorInfoStruct DoorInfo[deNUM_DOORS]
#ifdef DEFINE_BITOBJECTS
                              = {{"Winter South Door",&Winter_South_Door_Position,SOUTHDOORANALOG_RX_COBID},
                                 {"Winter North Door",&Winter_North_Door_Position,NORTHDOORANALOG_RX_COBID},
                                 {"Upper South Door", &Upper_South_Door_Position, SOUTHDOORANALOG_RX_COBID},
                                 {"Upper North Door", &Upper_North_Door_Position, NORTHDOORANALOG_RX_COBID}}
#endif
                                                                      ;
// South door control box
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject South_Winter_Lock_Open_IsLatched
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,0)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject South_Winter_Lock_Open_IsUnlatched
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,1)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Winter_Lock_Closed_IsLatched
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,2)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Winter_Lock_Closed_IsUnlatched
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,3)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject System_Enable
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,7)
#endif
                                                                      ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       AnalogObject Winter_South_Door_Position
#ifdef DEFINE_BITOBJECTS
                                              (&Inputs.SouthDoorAnalog_Rx[0],&myEE.Data.Doors[deWinterSouthDoor],false,false)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       AnalogObject Upper_South_Door_Position
#ifdef DEFINE_BITOBJECTS
                                             (&Inputs.SouthDoorAnalog_Rx[1],&myEE.Data.Doors[deUpperSouthDoor],false,true)
#endif
                                                                      ;
// North door control box
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject North_Winter_Lock_Open_IsLatched
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.NorthDoorDIO_Rx,2)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject North_Winter_Lock_Open_IsUnlatched
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.NorthDoorDIO_Rx,3)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       AnalogObject Winter_North_Door_Position
#ifdef DEFINE_BITOBJECTS
                                              (&Inputs.NorthDoorAnalog_Rx[0],&myEE.Data.Doors[deWinterNorthDoor],true,false)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       AnalogObject Upper_North_Door_Position
#ifdef DEFINE_BITOBJECTS
                                              (&Inputs.NorthDoorAnalog_Rx[1],&myEE.Data.Doors[deUpperNorthDoor],true,true)
#endif
                                                                      ;
// North hydraulic
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Remote_IsRequestingOpen
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.NorthHydraulic_Rx,2)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Remote_IsRequestingClose
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.NorthHydraulic_Rx,3)
#endif
                                                                      ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Center_Winter_Latch
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.SouthDoorDIO_Tx,2,2)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject South_Winter_Latch
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.SouthDoorDIO_Tx,0,2)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Inflate_South
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.SouthDoorDIO_Tx,4)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Activity_Panel1
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.SouthDoorDIO_Tx,7)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Upper_South_Door
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.SouthHydraulic_Tx,0,2)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Activity_Panel2
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.SouthHydraulic_Tx,7)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject North_Winter_Latch
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.NorthDoorDIO_Tx,0,2)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Inflate_North
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.NorthDoorDIO_Tx,6)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Activity_Panel3
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.NorthDoorDIO_Tx,7)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Upper_North_Door
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.NorthHydraulic_Tx,0,2)
#endif
                                                                       ;

#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Activity_Panel4
#ifdef DEFINE_BITOBJECTS
                                         (&Outputs.NorthHydraulic_Tx,7)
#endif
                                                                       ;

