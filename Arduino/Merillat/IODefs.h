// This will define the I/O points for the Metillat boathouse door project

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
#define MK_COBID(ID,FN) ((ID)&0x07f | (((FN)&7)<<7))
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

struct {
  INT8U SouthDoorDIO_Tx;
  INT8U SouthThruster_Tx[8];

  INT8U SouthHydraulic_Tx;

  INT8U NorthDoorDIO_Tx;
  INT8U NorthThruster_Tx[8];

  INT8U NorthHydraulic_Tx;
} Outputs;

// South door control box
#define South_Winter_Lock_Open_IsLatched   BIT(0,Inputs.SouthDoorDIO_Rx)
#define South_Winter_Lock_Open_IsUnlatched BIT(1,Inputs.SouthDoorDIO_Rx)
#define Winter_Lock_Closed_IsLatched       BIT(2,Inputs.SouthDoorDIO_Rx)
#define Winter_Lock_Closed_IsUnlatched     BIT(3,Inputs.SouthDoorDIO_Rx)
#define Upper_South_Door_IsOpen            BIT(4,Inputs.SouthDoorDIO_Rx)
#define Upper_South_Door_IsClosed          BIT(5,Inputs.SouthDoorDIO_Rx)
#define South_Winter_Door_Position         *(INT16U*)&Inputs.SouthDoorAnalog_Rx // dereference as 16 bit value
// North door control box
#define North_Winter_Lock_Open_IsLatched   BIT(0,Inputs.NorthDoorDIO_Rx)
#define North_Winter_Lock_Open_IsUnlatched BIT(1,Inputs.NorthDoorDIO_Rx)
#define Upper_North_Door_IsOpen            BIT(2,Inputs.NorthDoorDIO_Rx)
#define Upper_North_Door_IsClosed          BIT(3,Inputs.NorthDoorDIO_Rx)
#define North_Winter_Door_Position         *(INT16U*)&Inputs.NorthDoorAnalog_Rx // dereference as 16 bit value
// North hydraulic
#define Remote_IsRequestingOpen            BIT(0,Inputs.NorthHydraulic_Rx)
#define Remote_IsRequestingClose           BIT(1,Inputs.NorthHydraulic_Rx)

