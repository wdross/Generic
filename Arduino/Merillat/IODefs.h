#if !defined(IODEFS_INCLUDED)
#define IODEFS_INCLUDED

#include "CanOpen.h"
#include "BitObject.h"

// This will define the I/O points for the Merillat boathouse door project

// NodeIDs of CAN devices in the different boxes:
// (New) South Door control box:
#define ESD_SOUTH_DOOR_DIO_OUTPUTS 0x11
#define ESD_SOUTH_DOOR_DIO_INPUTS 0x12
#define ESD_SOUTH_DOOR_ANALOG 0x13
#define IMTRA_PPC800_SOUTH 0x08FF0000L|IS_EXTENDED_COBID|0x120
// Existing South Hydraulic control box:
#define ESD_SOUTH_HYDRAULIC 0x21
// (New) North Door control box:
#define ESD_NORTH_DOOR_DIO 0x31
#define ESD_NORTH_DOOR_ANALOG 0x32
#define IMTRA_PPC800_NORTH 0x08FF0000L|IS_EXTENDED_COBID|0x140
// Existing North Hydraulic control box:
#define ESD_NORTH_HYDRAULIC 0x41

#define RX_PGN 0x11000100L // turning on these bits max the RX PGN

// Make up the COBIDs we'll need to go with the Inputs below
#define SOUTHDOORDIO_RX_COBID    MK_COBID(ESD_SOUTH_DOOR_DIO_INPUTS,TXPDO1)
#define SOUTHDOORANALOG_RX_COBID MK_COBID(ESD_SOUTH_DOOR_ANALOG,TXPDO2)
#define SOUTHTHRUSTER_RX_COBID   (IMTRA_PPC800_SOUTH|RX_PGN) // Extended!
#define NORTHDOORDIO_RX_COBID    MK_COBID(ESD_NORTH_DOOR_DIO,TXPDO1)
#define NORTHDOORANALOG_RX_COBID MK_COBID(ESD_NORTH_DOOR_ANALOG,TXPDO2)
#define NORTHTHRUSTER_RX_COBID   (IMTRA_PPC800_NORTH|RX_PGN) // Extended!
#define NORTHHYDRAULIC_RX_COBID  MK_COBID(ESD_NORTH_HYDRAULIC,TXPDO1)
// Outputs
#define SOUTHDOORDIO_TX_COBID   MK_COBID(ESD_SOUTH_DOOR_DIO_OUTPUTS,RXPDO1)
#define SOUTHTHRUSTER_TX_COBID  (IMTRA_PPC800_SOUTH)  // Extended!
#define SOUTHHYDRAULIC_TX_COBID MK_COBID(ESD_SOUTH_HYDRAULIC,RXPDO1)
#define NORTHDOORDIO_TX_COBID   MK_COBID(ESD_NORTH_DOOR_DIO,RXPDO1)
#define NORTHTHRUSTER_TX_COBID  (IMTRA_PPC800_NORTH)  // Extended!
#define NORTHHYDRAULIC_TX_COBID MK_COBID(ESD_NORTH_HYDRAULIC,RXPDO1)


// ThrusterInstance field of PGN65280CanFrame
#define SOUTH_THRUSTER_INSTANCE 0
#define NORTH_THRUSTER_INSTANCE 0

#define NO_ACTION 0 // (unused) Retract field of PGN65280CanFrame

// Direction field of PGN65280CanFrame
#define NO_DIRECTION 0
#define STARBOARD 1
#define PORT 2


struct PGN130817 {
  unsigned int ManufacturerCode:11;
  unsigned int ReservedA:2; // 0x3
  unsigned int IndustryGroup:3;

  unsigned int SleipnerDeviceType:8;
  unsigned int SleipnerDeviceInstance:4;
  unsigned int ReservedB:4; // 0xf

  unsigned int Status:16;

  unsigned int ThrusterMotorTemperature:8;
  unsigned int ThrusterPowerTemperature:8;

  unsigned int MotorVoltage:16; // 0.01v

           int MotorRPM:16; // Not implemented, 1/4RPM

  unsigned int MotorCurrent:16;

  unsigned int OutputThrust:8;
}; // PGN130817 (multi-part message received from each PPC800)

struct PGN130817CanFrames {
  unsigned int FC:5;
  unsigned int SN:3;
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

struct PGN65280CanFrame {
  unsigned int ManufactureCode:11;
  unsigned int Reserved:2;
  unsigned int IndustryGroup:3;

  unsigned int ThrusterInstance:4;
  unsigned int Direction:2;
  unsigned int Retract:2;

  unsigned int Thrust:10;
  unsigned int ReservedB:6;

          long ReservedC:24;
}; // message to TX to each Thruster

struct InputType {
  INT8U SouthDoorDIO_Rx;
  INT16S SouthDoorAnalog_Rx[1];

  // Will receive PGN130817 every 100ms
  union {
    INT8U SouthThruster_Rx[8];
    struct PGN130817CanFrames SouthThrusterRx;
  };

  // South Hydraulic doesn't have Inputs

  INT8U NorthDoorDIO_Rx;
  INT16S NorthDoorAnalog_Rx[1];

  // Will receive PGN130817 every 100ms
  union {
    INT8U NorthThruster_Rx[8];
    struct PGN130817CanFrames NorthThrusterRx;
  };

  INT8U NorthHydraulic_Rx;
};

enum LatchRequestType {lr_No_Request, lr_Unlatch_Request, lr_Latch_Request};

struct OutputType {
  union {
    INT8U SouthDoorDIO_Tx;
    struct {
      INT8U South_Winter_Lock_Open:2; // LSBits
      INT8U Winter_Lock_Closed:2;
      INT8U UNUSED_SouthDoorDIO_Tx:4;
    }; // leave anonymous
  };
#define SOUTHDOOR_OUTPUT_MASK 0x0f

  // PPC needs a heartbeat of PGN65280CanFrame every 100ms (300ms timeout)
  union {
    INT8U SouthThruster_Tx[8];
    struct PGN65280CanFrame SouthThrusterTx;
  };

  union {
    INT8U SouthHydraulic_Tx;
    struct {
      INT8U Upper_South_Door:2;         // LSBits
      INT8U UNUSED_SouthHydraulic_Tx:6;
    };
  };
#define SOUTHHYDRAULIC_OUTPUT_MASK 0x03

  union {
    INT8U NorthDoorDIO_Tx;
    struct {
      INT8U North_Winter_Lock_Open:2;         // LSBits
      INT8U USED_AS_INPUTS_NorthDoorDIO_Tx:4;
      INT8U UNUSED_NorthDoorDIO_Tx;
    };
  };
#define NORTHDOOR_OUTPUT_MASK 0x03

  union {
    INT8U NorthThruster_Tx[8];
    struct PGN65280CanFrame NorthThrusterTx;
  };

  union {
    INT8U NorthHydraulic_Tx;
    struct {
      INT8U Upper_North_Door:2;                 // LSBits
      INT8U USED_AS_INPUTS_NorthHydraulic_Tx:2;
      INT8U UNUSED_NorthHydraulic_Tx:4;
    };
  };
#define NORTHHYDRAULIC_OUTPUT_MASK 0x03
};

// Elements that should be stored in EEPROM over power downs:
struct myEEType {
  struct {
    int Min, Max;
    bool Valid;
  } SouthDoor, NorthDoor;
};

extern myEEType myEE;
extern OutputType Outputs;
extern InputType  Inputs;

// GUI defines
#define STEPS_CHANGE 239 // animate when no comm ma
#define DEGREES_CHANGE 84.0
#define MAX_ANGLE 90.0
#define MIN_ANGLE (MAX_ANGLE-DEGREES_CHANGE)

// Dimensional defines, defines decision points
#define MIN_THRUST 50 // 1024ths thrust
#define ROC_RATE 300 // ms to recompute Rate Of Change during Open/Close cycle
#define ANALOG_TO_RADIANS(a) (((float)(a) / 32767.0) * 2.0 * M_PI)
#define DEGREES_TO_RADIANS(d) (((float)(d) / 180.0) * M_PI)

#endif // IODEFS_INCLUDED

// Depending upon desire to create instances of BitObjects, we'll be doing extern or not

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
       BitObject Upper_South_Door_IsOpen
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,4)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Upper_South_Door_IsClosed
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.SouthDoorDIO_Rx,5)
#endif
                                                                      ;
#define South_Winter_Door_Position         Inputs.SouthDoorAnalog_Rx[0]
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
       BitObject Upper_North_Door_IsOpen
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.NorthDoorDIO_Rx,4)
#endif
                                                                      ;
#ifndef DEFINE_BITOBJECTS
extern
#endif
       BitObject Upper_North_Door_IsClosed
#ifdef DEFINE_BITOBJECTS
                                         (&Inputs.NorthDoorDIO_Rx,5)
#endif
                                                                      ;
#define North_Winter_Door_Position         Inputs.NorthDoorAnalog_Rx[0]
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

