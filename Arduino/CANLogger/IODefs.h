#if !defined(IODEFS_INCLUDED)
#define IODEFS_INCLUDED

// This will define the I/O points for the Metillat boathouse door project

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
#define NORTHDOORANALOG_RX_COBID MK_COBID(ESD_NORTH_DOOR_ANALOG,TXPDO1)
#define NORTHTHRUSTER_RX_COBID   (IMTRA_PPC800_NORTH|RX_PGN) // Extended!
#define NORTHHYDRAULIC_RX_COBID  MK_COBID(ESD_NORTH_HYDRAULIC,TXPDO1)
// Outputs
#define SOUTHDOORDIO_TX_COBID   MK_COBID(ESD_SOUTH_DOOR_DIO_OUTPUTS,RXPDO1)
#define SOUTHTHRUSTER_TX_COBID  (IMTRA_PPC800_SOUTH)  // Extended!
#define SOUTHHYDRAULIC_TX_COBID MK_COBID(ESD_SOUTH_HYDRAULIC,RXPDO1)
#define NORTHDOORDIO_TX_COBID   MK_COBID(ESD_NORTH_DOOR_DIO,RXPDO1)
#define NORTHTHRUSTER_TX_COBID  (IMTRA_PPC800_NORTH)  // Extended!
#define NORTHHYDRAULIC_TX_COBID MK_COBID(ESD_NORTH_HYDRAULIC,RXPDO1)


struct {
  INT8U SouthDoorDIO_Rx;
  INT16U SouthDoorAnalog_Rx[1];
  INT8U SouthThruster_Rx[8];

  // South Hydraulic doesn't have Inputs

  INT8U NorthDoorDIO_Rx;
  INT16U NorthDoorAnalog_Rx[1]; // only the first 2 (of 8 bytes) bytes are what we want
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
// Make use of pre-defined bitwise defines from Arduino.h:
//#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
//#define bitSet(value, bit) ((value) |= (1UL << (bit)))
//#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
//#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

// South door control box
#define South_Winter_Lock_Open_IsLatched   bitRead(Inputs.SouthDoorDIO_Rx,0)
#define South_Winter_Lock_Open_IsUnlatched bitRead(Inputs.SouthDoorDIO_Rx,1)
#define Winter_Lock_Closed_IsLatched       bitRead(Inputs.SouthDoorDIO_Rx,2)
#define Winter_Lock_Closed_IsUnlatched     bitRead(Inputs.SouthDoorDIO_Rx,3)
#define Upper_South_Door_IsOpen            bitRead(Inputs.SouthDoorDIO_Rx,4)
#define Upper_South_Door_IsClosed          bitRead(Inputs.SouthDoorDIO_Rx,5)
#define South_Winter_Door_Position         Inputs.SouthDoorAnalog_Rx[0]
// North door control box
#define North_Winter_Lock_Open_IsLatched   bitRead(Inputs.NorthDoorDIO_Rx,0)
#define North_Winter_Lock_Open_IsUnlatched bitRead(Inputs.NorthDoorDIO_Rx,1)
#define Upper_North_Door_IsOpen            bitRead(Inputs.NorthDoorDIO_Rx,2)
#define Upper_North_Door_IsClosed          bitRead(Inputs.NorthDoorDIO_Rx,3)
#define North_Winter_Door_Position         *(INT16U*)&Inputs.NorthDoorAnalog_Rx // dereference as 16 bit value
// North hydraulic
#define Remote_IsRequestingOpen            bitRead(Inputs.NorthHydraulic_Rx,0)
#define Remote_IsRequestingClose           bitRead(Inputs.NorthHydraulic_Rx,1)

#endif // IODEFS_INCLUDED

