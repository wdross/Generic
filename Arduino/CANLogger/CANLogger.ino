
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

const byte RawArray[] PROGMEM = {
0x60,0x10,0x32,0x99,0x00,0xF2,0x00,0x00,
0x61,0x3D,0x1D,0x00,0xFE,0x04,0x00,0x00,
0x62,0x00,0x00,0x02,0xFF,0xFF,0xFF,0xFF,
0xC0,0x10,0x32,0x99,0x00,0xF1,0x00,0x00,
0xC1,0x44,0x1C,0x00,0xEB,0x04,0x00,0x00,
0xC2,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF};

const struct CAN_Entry {
  int kBaud;
  int CAN_Konstant;
  const char *rate;
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

struct DoorOpenCloseInfo {
  INT16U Closed, Opened;
};
enum DoorEnum {deWinterSouthDoor, deWinterNorthDoor, deUpperSouthDoor, deUpperNorthDoor, deNUM_DOORS};
struct DoorOpenCloseInfo Doors[deNUM_DOORS] = { // 4
  /*Winter_South_Door_Position*/ {15908,21802},
  /*Upper_South_Door_Position */ {15705, 7727},
  /*Winter_North_Door_Position*/ {15878, 8033},
  /*Upper_North_Door_Position*/  {16389,24515}
  };


bool cycle = false;
CFwTimer SYNCTimer;

unsigned int LastThrust[3] = {0,0,0};
unsigned int LastDir[3] = {0,0,0};
unsigned int LastInst = 0;

void IncomingThrust() {
  // called when we get a motor controller packet
  int instance = Outputs.Thrusters[0].ThrusterInstance;
  if (instance == 1 || instance == 2) {
    // valid Thruster ID, file it into the appropriate slot
    memcpy(&Outputs.Thrusters[instance],&Outputs.Thrusters[0],sizeof(Outputs.Thrusters[0]));
    // Outputs.Thrusters[instance] = Outputs.Thrusters[0];
    // these get looked at when the SYNC message comes in
    if (LastInst == instance)
      Serial.println("HEY!");
    LastInst = instance;
  }
}

void IncomingSYNC() {
  static CFwTimer RemoteTimer(INFINITE);
  static INT8U Last_South_Winter_Latch = 0;
  static CFwTimer South_Winter_Latch_Timer(INFINITE);
  static INT8U Last_North_Winter_Latch = 0;
  static CFwTimer North_Winter_Latch_Timer(INFINITE);
  static INT8U Last_Center_Winter_Latch = 0;
  static CFwTimer Center_Winter_Latch_Timer(INFINITE);

  // we saw a SYNC message
  // use this interval to decide what movements to report happening this SYNC
  // i.e. if SouthThrusterClose active, then alter AnalogWinter input closer to CLOSED
  // if NorthHydraulicOpen output active then alter AnalogUpper input closer to OPENED
  // monitor for edges (start moving actuator, start timing, 10s later, show correct limit switch changed state)

  // Monitor and respond to upper door movement request
#define UPPER_DOOR_MOVEMENT_RATE 125
  static bool UpperSouthLatch = false;
  if (Upper_South_Door_CLOSE) {
    Upper_South_Door_Position += UPPER_DOOR_MOVEMENT_RATE;
    if (!UpperSouthLatch)
      Serial.println("Upper South Closing");
    UpperSouthLatch = true;
  }
  else if (Upper_South_Door_OPEN) {
    Upper_South_Door_Position -= UPPER_DOOR_MOVEMENT_RATE;
    if (!UpperSouthLatch)
      Serial.println("Upper South Opening");
    UpperSouthLatch = true;
  }
  else
    UpperSouthLatch = false;
  static bool UpperNorthLatch = false;
  if (Upper_North_Door_CLOSE) {
    Upper_North_Door_Position -= 3*UPPER_DOOR_MOVEMENT_RATE/2;
    if (!UpperNorthLatch)
      Serial.println("Upper North Closing");
    UpperNorthLatch = true;
  }
  else if (Upper_North_Door_OPEN) {
    Upper_North_Door_Position += 4*UPPER_DOOR_MOVEMENT_RATE/5;
    if (!UpperNorthLatch)
      Serial.println("Upper North Opening");
    UpperNorthLatch = true;
  }
  else
    UpperNorthLatch = false;

  // monitor South Winter latch OPEN/Unlatch as well as CLOSE/Latch
  if (South_Winter_Latch == lr_Latch_Request)
    bitClear(Inputs.SouthDoorDIO_Rx,1); // first scan, show !latched
  else if (South_Winter_Latch == lr_Unlatch_Request)
    bitClear(Inputs.SouthDoorDIO_Rx,0); // first scan, show !unlatched
  if (Last_South_Winter_Latch != South_Winter_Latch && South_Winter_Latch) {
    South_Winter_Latch_Timer.SetTimer(6000);
    Serial.println("South...");
  }
  Last_South_Winter_Latch = South_Winter_Latch;
  if (South_Winter_Latch && South_Winter_Latch_Timer.IsTimeout()) {
    if (South_Winter_Latch == lr_Unlatch_Request) {
      South_Winter_Lock_Open_IsUnlatched;
      Serial.println("South Unlatched");
    }
    else {
      South_Winter_Lock_Open_IsLatched;
      Serial.println("South Latched");
    }
  }

  // respond to North Winter latch OPEN/Unlatch as well as CLOSE/Latch
  if (North_Winter_Latch == lr_Latch_Request)
    bitClear(Inputs.NorthDoorDIO_Rx,3);
  else if (North_Winter_Latch == lr_Unlatch_Request)
    bitClear(Inputs.NorthDoorDIO_Rx,2);
  if (Last_North_Winter_Latch != North_Winter_Latch && North_Winter_Latch) {
    North_Winter_Latch_Timer.SetTimer(5000);
    Serial.println("North...");
  }
  Last_North_Winter_Latch = North_Winter_Latch;
  if (North_Winter_Latch && North_Winter_Latch_Timer.IsTimeout()) {
    if (North_Winter_Latch == lr_Unlatch_Request) {
      North_Winter_Lock_Open_IsUnlatched;
      Serial.println("North Unlatched");
    }
    else {
      North_Winter_Lock_Open_IsLatched;
      Serial.println("North Latched");
    }
  }

  // respond to center lock movement OPEN/Unlatch as well as CLOSE/Latch
  if (Center_Winter_Latch == lr_Latch_Request)
    bitClear(Inputs.SouthDoorDIO_Rx,3); // clear 'latched' of Winter_Lock_Closed_IsLatched
  else if (Center_Winter_Latch == lr_Unlatch_Request)
    bitClear(Inputs.SouthDoorDIO_Rx,2);
  if (Last_Center_Winter_Latch != Center_Winter_Latch && Center_Winter_Latch) {
    Center_Winter_Latch_Timer.SetTimer(7000);
    Serial.println("Center...");
  }
  Last_Center_Winter_Latch = Center_Winter_Latch;
  if (Center_Winter_Latch && Center_Winter_Latch_Timer.IsTimeout()) {
    if (Center_Winter_Latch == lr_Unlatch_Request) {
      Winter_Lock_Closed_IsUnlatched;
      Serial.println("Center Unlatched");
    }
    else {
      Winter_Lock_Closed_IsLatched; // show closed
      Serial.println("Center Latched");
    }
  }

  static bool LastInflateNorth = false;
  if (Inflate_North != LastInflateNorth) {
    if (Inflate_North)
      Serial.println("Inflating North");
    else
      Serial.println("Deflating North");
    LastInflateNorth = Inflate_North;
  }
  static bool LastInflateSouth = false;
  if (Inflate_South != LastInflateSouth) {
    if (Inflate_South)
      Serial.println("Inflating South");
    else
      Serial.println("Deflating South");
    LastInflateSouth = Inflate_South;
  }

  // monitor any outgoing Open/Close requests: allow on for 4-seconds only
  if (Inputs.NorthHydraulic_Rx) {
    if (RemoteTimer.GetRemaining() == INFINITE)
      RemoteTimer.SetTimer(4000);
    else if (RemoteTimer.IsTimeout())
      Inputs.NorthHydraulic_Rx = 0;
  }
  else
    RemoteTimer.SetTimer(INFINITE);

  // look at both data elements, see what has changed, maybe print something
  // MUST leave this here, as we use the latched Last* entries to make our
  // decisions on action just below this...
  if (Outputs.Thrusters[NORTH_INSTANCE].Thrust != LastThrust[NORTH_INSTANCE] ||
      Outputs.Thrusters[NORTH_INSTANCE].Direction != LastDir[NORTH_INSTANCE] ||
      Outputs.Thrusters[SOUTH_INSTANCE].Thrust != LastThrust[SOUTH_INSTANCE] ||
      Outputs.Thrusters[SOUTH_INSTANCE].Direction != LastDir[SOUTH_INSTANCE]) {
    LastThrust[NORTH_INSTANCE] = Outputs.Thrusters[NORTH_INSTANCE].Thrust;
    LastDir[NORTH_INSTANCE] = Outputs.Thrusters[NORTH_INSTANCE].Direction;
    LastThrust[SOUTH_INSTANCE] = Outputs.Thrusters[SOUTH_INSTANCE].Thrust;
    LastDir[SOUTH_INSTANCE] = Outputs.Thrusters[SOUTH_INSTANCE].Direction;
    Serial.print("N:");
    Serial.print(LastDir[NORTH_INSTANCE]);
    Serial.print("@");
    Serial.print(LastThrust[NORTH_INSTANCE]);
    Serial.print(";  S:");
    Serial.print(LastDir[SOUTH_INSTANCE]);
    Serial.print("@");
    Serial.println(LastThrust[SOUTH_INSTANCE]);
  }

  // see if the thrusters are being asked to move, and change the winter hinge position if so
  if (LastDir[NORTH_INSTANCE] && LastThrust[NORTH_INSTANCE] > MIN_THRUST) {
    INT16S pos = Winter_North_Door_Position;
    if (LastDir[NORTH_INSTANCE] == DIRECTION_CLOSE) {
      pos += Outputs.Thrusters[NORTH_INSTANCE].Thrust/4;
      if (pos < Winter_North_Door_Position)
        pos = Winter_North_Door_Position; // don't allow wrapping
    }
    else {
      pos -= Outputs.Thrusters[NORTH_INSTANCE].Thrust/4;
      if (pos > Winter_North_Door_Position)
        pos = Winter_North_Door_Position; // don't allow wrapping
    }
    Winter_North_Door_Position = pos;
  }
  if (LastDir[SOUTH_INSTANCE] &&
      LastThrust[SOUTH_INSTANCE] > MIN_THRUST) {
    INT16S pos = Winter_South_Door_Position;
    if (LastDir[SOUTH_INSTANCE] == DIRECTION_OPEN) { // opposite direction as NORTH
      pos += Outputs.Thrusters[SOUTH_INSTANCE].Thrust/6;
      if (pos < Winter_South_Door_Position)
        pos = Winter_South_Door_Position; // don't allow wrapping
    }
    else {
      pos -= Outputs.Thrusters[SOUTH_INSTANCE].Thrust/6;
      if (pos > Winter_South_Door_Position)
        pos = Winter_South_Door_Position; // don't allow wrapping
    }
    Winter_South_Door_Position = pos;
  }

  // mark all non-extended outgoing messages for 'now'
  for (int j=0; j<NUM_OUT_BUFFERS && CanOutBuffers[j].Can.COBID; j++) {
    if ((CanOutBuffers[j].Can.COBID & IS_EXTENDED_COBID) == 0) {
      // this is a non-extended frame, so we send this in response to a SYNC
      CanOutBuffers[j].NextSendTime.SetTimer(j); // 1ms apart
    }
  }
}


void setup()
{
  // initialize the pushbutton pin as an input:
  pinMode(DOOR_SIMULATE_PIN, INPUT);
  sizeofRawArray = sizeof(RawArray);
  pRawArray = (uint32_t*)&RawArray; // (uint32_t*)RawArray;

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
  CanPollSetRx(0x80,                    0,                                0,                                IncomingSYNC);
  CanPollSetRx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  (INT8U*)&Outputs.SouthDoorDIO_Tx,  NULL);
  CanPollSetRx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),(INT8U*)&Outputs.SouthHydraulic_Tx,NULL);
  CanPollSetRx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  (INT8U*)&Outputs.NorthDoorDIO_Tx,  NULL);
  CanPollSetRx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),(INT8U*)&Outputs.NorthHydraulic_Tx,NULL);
  CanPollSetRx(THRUSTER_TX_COBID,      sizeof(Outputs.Thrusters[0]),     (INT8U*)&Outputs.Thrusters[0],     IncomingThrust);
  memset(&Outputs.Thrusters,0,sizeof(Outputs.Thrusters)); // zero all of them out

  // Data we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  //           COBID,                  NumberOfBytesToTransmit,          AddressOfDataToTransmit
  CanPollSetTx(0x19FF0100L|IS_EXTENDED_COBID,8,RawArray); // Thrusters assumed to be [0], so declare first!!
  CanPollSetTx(SOUTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx);
  CanPollSetTx(SOUTHDOORANALOG_RX_COBID,sizeof(Inputs.SouthDoorAnalog_Rx),(INT8U*)&Inputs.SouthDoorAnalog_Rx);
  CanPollSetTx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx);
  CanPollSetTx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx);
  CanPollSetTx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx);

  CanOutBuffers[0].NextSendTime.SetTimer(0); // send now, priming the pump
  OutputSetTimer = 40; // cause to always cycle

  CanPollDisplay(3);

  // initialize state of sent outputs to be what we want: all zeros
  Inputs.SouthDoorDIO_Rx = 0;
  Inputs.SouthDoorAnalog_Rx[0] = Inputs.SouthDoorAnalog_Rx[1] = 0;
  Inputs.NorthDoorDIO_Rx = 0;
  Inputs.NorthDoorAnalog_Rx[0] = Inputs.NorthDoorAnalog_Rx[1] = 0;
  Inputs.NorthHydraulic_Rx = 0;

  // now what bits to be on to show everything open:
  South_Winter_Lock_Open_IsLatched;
  North_Winter_Lock_Open_IsLatched;
  System_Enable;
  Winter_South_Door_Position = 21802;
  Upper_South_Door_Position = 7727;
  Winter_North_Door_Position = 8033;
  Upper_North_Door_Position = 24515;

  FlexiTimer2::set(1, 0.0004, CanPoller); // Every 0.4 ms (400us)
  FlexiTimer2::start();
}


void loop()
{
  char serialRead;

  serialRead = Serial.read();
  if (serialRead == 'z' || serialRead == 'Z') {
    CanOutBuffers[0].Can.pMessage += 8; // advance to next buffer
    if (CanOutBuffers[0].Can.pMessage >= RawArray + sizeof(RawArray))
      CanOutBuffers[0].Can.pMessage = RawArray; // wrap
    CanOutBuffers[0].NextSendTime.SetTimer(0); // send now
  }
  else if (serialRead == 'r' || serialRead == 'R') // RUN
  {
    if (OutputSetTimer == INFINITE) {
      Serial.println("Running");
      OutputSetTimer = 40;
      CanOutBuffers[0].NextSendTime.SetTimer(0); // send now, priming the pump
    }
    else {
      Serial.println("Monitoring");
      OutputSetTimer = INFINITE;
    }
  }
  else if (serialRead == 'S' || serialRead == 's') // SYNC transmit
  {
    SYNCsend();
  }
  else if (serialRead == 'n' || serialRead == 'N') // NMT 'start all nodes'
  {
    NMTsend();
  }
  else if (serialRead == 'd' || serialRead == 'D') // display output buffers
  {
//    SDOread(ESD_SOUTH_DOOR_ANALOG,0x1a01,0);
    CanPollDisplay(3);
  }
  else if (serialRead == 'w' || serialRead == 'W') // Write sdo
  {
    SDOwrite(ESD_SOUTH_DOOR_ANALOG,0x1801,2,1,1);
  }
//  else if (serialRead == 'c' || serialRead == 'C') // Cycle
//  {
//    cycle = !cycle;
//    if (cycle) {
//      SYNCTimer.SetTimer(100);
//    }
//  }
  else if (serialRead == 'c' || serialRead == 'C') { // close
    Remote_IsRequestingClose;
  }
  else if (serialRead == 'o' || serialRead == 'O') { // open
    Remote_IsRequestingOpen;
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

#ifdef DO_LOGGING
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
#endif
}

