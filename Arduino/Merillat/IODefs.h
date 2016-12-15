// This will define the I/O points for the Metillat boathouse door project

#define IS_EXTENDED_COBID 0x80000000L // we'll set bit 31 (of a 29-bit COBID) to mean 'Extended'

// NodeIDs of CAN devices in the different boxes:
// (New) South Door control box:
#define ESD_SOUTH_DOOR_DIO_OUTPUTS 7
#define ESD_SOUTH_DOOR_DIO_INPUTS 8
#define ESD_SOUTH_DOOR_ANALOG 20
#define IMTRA_PPC800_SOUTH 120|IS_EXTENDED_COBID
// Existing South Hydraulic control box:
#define ESD_SOUTH_HYDRAULIC 10
// (New) North Door control box:
#define ESD_NORTH_DOOR_DIO 11
#define ESD_NORTH_DOOR_ANALOG 22
#define IMTRA_PPC800_NORTH 140|IS_EXTENDED_COBID
// Existing North Hydraulic control box:
#define ESD_NORTH_HYDRAULIC 33

// Enough CANOPEN stuff to define COBIDs
#define MK_COBID(ID,FN) ((ID)&0x07f | (((FN)&7)<<7) | (IS_EXTENDED_COBID&(ID)))
#define EMERGENCY 1
#define TXPDO1 3
#define TXPDO2 5
#define TXPDO3 7
#define TXPDO4 9
#define RXPDO1 4
#define RXPDO2 6
#define RXPDO3 8
#define RXPDO4 10
#define TXSDO 11
#define RXSDO 12
#define NODE_GUARD 14

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

enum LatchRequestType {lr_NoRequest, lr_Unlatch_Request, lr_Latch_Request};

struct {
  union {
    INT8U SouthDoorDIO_Tx;
    struct {
      INT8U South_Winter_Lock_Open:2;
      INT8U Winter_Lock_Closed:2;
      INT8U UNUSED_SouthDoorDIO_Tx:4;
    }; // leave anonymous
  };

  INT8U SouthThruster_Tx[8];

  union {
    INT8U SouthHydraulic_Tx;
    struct {
      INT8U Upper_South_Door:2;
      INT8U UNUSED_SouthHydraulic_Tx:6;
    };
  };

  union {
    INT8U NorthDoorDIO_Tx;
    struct {
      INT8U North_Winter_Lock_Open:2;
      INT8U USED_AS_INPUTS_NorthDoorDIO_Tx:4;
      INT8U UNUSED_NorthDoorDIO_Tx;
    };
  };

  INT8U NorthThruster_Tx[8];

  union {
    INT8U NorthHydraulic_Tx;
    struct {
      INT8U Upper_North_Door:2;
      INT8U USED_AS_INPUTS_NorthHydraulic_Tx:2;
      INT8U UNUSED_NorthHydraulic_Tx:4;
    };
  };
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
#define South_Winter_Door_Position         *(INT16U*)&Inputs.SouthDoorAnalog_Rx // dereference as 16 bit value
// North door control box
#define North_Winter_Lock_Open_IsLatched   bitRead(Inputs.NorthDoorDIO_Rx,0)
#define North_Winter_Lock_Open_IsUnlatched bitRead(Inputs.NorthDoorDIO_Rx,1)
#define Upper_North_Door_IsOpen            bitRead(Inputs.NorthDoorDIO_Rx,2)
#define Upper_North_Door_IsClosed          bitRead(Inputs.NorthDoorDIO_Rx,3)
#define North_Winter_Door_Position         *(INT16U*)&Inputs.NorthDoorAnalog_Rx // dereference as 16 bit value
// North hydraulic
#define Remote_IsRequestingOpen            bitRead(Inputs.NorthHydraulic_Rx,0)
#define Remote_IsRequestingClose           bitRead(Inputs.NorthHydraulic_Rx,1)

