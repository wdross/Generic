#if !defined(IODEFS_INCLUDED)
#define IODEFS_INCLUDED

// This will define the I/O points for the Metillat boathouse door project

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
#define SOUTHDOORDIO_RX_COBID    MK_COBID(ESD_SOUTH_DOOR_DIO_INPUTS,TXPDO1)
#define SOUTHDOORANALOG_RX_COBID MK_COBID(ESD_SOUTH_DOOR_ANALOG,TXPDO2)
#define SOUTHTHRUSTER_RX_COBID   (IMTRA_PPC800_SOUTH|RX_PGN) // Extended!
#define NORTHDOORDIO_RX_COBID    MK_COBID(ESD_NORTH_DOOR_DIO,TXPDO1)
#define NORTHDOORANALOG_RX_COBID MK_COBID(ESD_NORTH_DOOR_ANALOG,TXPDO2)
#define NORTHTHRUSTER_RX_COBID   (IMTRA_PPC800_NORTH|RX_PGN) // Extended!
#define NORTHHYDRAULIC_RX_COBID  MK_COBID(ESD_NORTH_HYDRAULIC,TXPDO1)
// Outputs
#define SOUTHDOORDIO_TX_COBID   MK_COBID(ESD_SOUTH_DOOR_DIO_OUTPUTS,RXPDO1)
#define THRUSTER_TX_COBID       (IMTRA_PPC800)  // Extended!
#define SOUTHHYDRAULIC_TX_COBID MK_COBID(ESD_SOUTH_HYDRAULIC,RXPDO1)
#define NORTHDOORDIO_TX_COBID   MK_COBID(ESD_NORTH_DOOR_DIO,RXPDO1)
#define NORTHHYDRAULIC_TX_COBID MK_COBID(ESD_NORTH_HYDRAULIC,RXPDO1)


// Direction field of PGN65280CanFrame
#define DIRECTION_NONE 0
#define DIRECTION_STARBOARD 1
#define DIRECTION_PORT 2
#define DIRECTION_CLOSE DIRECTION_STARBOARD
#define DIRECTION_OPEN  DIRECTION_PORT


// ThrusterInstance field of PGN65280CanFrame and index into array
enum {INPUT_INSTANCE, NORTH_INSTANCE, SOUTH_INSTANCE, NUM_INSTANCES};

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

enum LatchRequestType {lr_No_Request, lr_Unlatch_Request, lr_Latch_Request};

struct {
  INT8U SouthDoorDIO_Rx;
  INT16U SouthDoorAnalog_Rx[2];
  INT8U SouthThruster_Rx[8];

  // South Hydraulic doesn't have Inputs

  INT8U NorthDoorDIO_Rx;
  INT16U NorthDoorAnalog_Rx[2]; // only the first 4 (of 8 bytes) bytes are what we want
  INT8U NorthThruster_Rx[8];

  INT8U NorthHydraulic_Rx;
} Inputs;

struct {
  INT8U SouthDoorDIO_Tx;

  INT8U SouthHydraulic_Tx;

  INT8U NorthDoorDIO_Tx;

  INT8U NorthHydraulic_Tx;

  struct PGN65280CanFrame Thrusters[NUM_INSTANCES]; // Unused, North, South (Instance ID of Sleipner frame)
} Outputs;

// Dimensional defines, defines decision points
#define MIN_THRUST 50  // 1024ths thrust
#define MAX_THRUST 512 // 1024ths thrust
#define NO_THRUST 0


// Make use of pre-defined bitwise defines from Arduino.h:
//#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
//#define bitSet(value, bit) ((value) |= (1UL << (bit)))
//#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
//#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#define FIELD(value,bit,size) (((value) >> (bit)) & ((1UL<<(size))-1))

// South door control box
#define South_Winter_Lock_Open_IsLatched   {bitSet(Inputs.SouthDoorDIO_Rx,0);bitClear(Inputs.SouthDoorDIO_Rx,1);}
#define South_Winter_Lock_Open_IsUnlatched {bitSet(Inputs.SouthDoorDIO_Rx,1);bitClear(Inputs.SouthDoorDIO_Rx,0);}
#define Winter_Lock_Closed_IsLatched       {bitSet(Inputs.SouthDoorDIO_Rx,2);bitClear(Inputs.SouthDoorDIO_Rx,3);}
#define Winter_Lock_Closed_IsUnlatched     {bitSet(Inputs.SouthDoorDIO_Rx,3);bitClear(Inputs.SouthDoorDIO_Rx,2);}
#define System_Enable                      bitSet(Inputs.SouthDoorDIO_Rx,7)
#define Winter_South_Door_Position         Inputs.SouthDoorAnalog_Rx[0]
#define Upper_South_Door_Position          Inputs.SouthDoorAnalog_Rx[1]
// North door control box
#define North_Winter_Lock_Open_IsLatched   {bitSet(Inputs.NorthDoorDIO_Rx,2);bitClear(Inputs.NorthDoorDIO_Rx,3);}
#define North_Winter_Lock_Open_IsUnlatched {bitSet(Inputs.NorthDoorDIO_Rx,3);bitClear(Inputs.NorthDoorDIO_Rx,2);}
#define Winter_North_Door_Position         Inputs.NorthDoorAnalog_Rx[0]
#define Upper_North_Door_Position          Inputs.NorthDoorAnalog_Rx[1]
// North hydraulic
#define Remote_IsRequestingOpen            bitSet(Inputs.NorthHydraulic_Rx,2)
#define Remote_IsRequestingClose           bitSet(Inputs.NorthHydraulic_Rx,3)

//                            OUR ACTUAL INPUTS
#define South_Winter_Latch                 FIELD(Outputs.SouthDoorDIO_Tx,0,2) // 2 bits, 0 & 1
#define Center_Winter_Latch                FIELD(Outputs.SouthDoorDIO_Tx,2,2) // 2 bits, 2 & 3
#define Inflate_South                      bitRead(Outputs.SouthDoorDIO_Tx,4)
#define North_Winter_Latch                 FIELD(Outputs.NorthDoorDIO_Tx,0,2)
#define Inflate_North                      bitRead(Outputs.NorthDoorDIO_Tx,6)

#define Upper_South_Door_OPEN              bitRead(Outputs.SouthHydraulic_Tx,0)
#define Upper_South_Door_CLOSE             bitRead(Outputs.SouthHydraulic_Tx,1)

#define Upper_North_Door_OPEN              bitRead(Outputs.NorthHydraulic_Tx,0)
#define Upper_North_Door_CLOSE             bitRead(Outputs.NorthHydraulic_Tx,1)

#endif // IODEFS_INCLUDED

