#include <EEPROM.h>
#include <FlexiTimer2.h>

#include "array.h"

#include "CanPoller.h"
#include "IODefs.h"
#include "DoorStates.h" // a StateMachine

// get rid of "deprecated conversion from string constant to 'char*'" messages
#pragma GCC diagnostic ignored "-Wwrite-strings"

/* Merillat Boathouse Controller:
 *  Designing to run on:
 *  - Arduino MEGA module (from SainSmart)
 *  - CAN-BUS Shield v1.2 from Seeed Studio
 *  - 3.2" 240x320 touch screen (SainSmart)
 *
 *  The CAN-BUS Shield sits on the MEGA, but 2 pins on each side make
 *  no connection to the MEGA module.  This is OK.
 *
 *  v1.6.12 of the Arduino IDE
 *  v1.6.18 of the Boards Manager
 *  Tools->Board: "Arduino/Genuino Mega or Mega 2560"
 *
 *  The touch screen connection is a set of parallel pins on a dual-row header
 *  that needs to be extended (a row of 2x18 pass-thru header is required, 5/8" tall).
 *  Also need 2 nylon stand offs (1/8" dia screw x 7/16" body) with 2 nylon nuts, super
 *    glued to the bottom of the display board.
 */

#include <ITDB02_Graph16.h>
// Declare which fonts we will be using
extern uint8_t SmallFont[];
ITDB02 myGLCD(38,39,40,41);
int doorLen, lDoorX, lDoorY, rDoorX, rDoorY;

OutputType Outputs;
InputType  Inputs;

// Create BitObject elements
#define DEFINE_BITOBJECTS
#include "IODefs.h"
#undef DEFINE_BITOBJECTS

struct LastCoords {
  int X,Y;
} LeftDoor, RightDoor, NewLeftDoor, NewRightDoor,
  UpperLeftDoor, UpperRightDoor, NewUpperLeftDoor, NewUpperRightDoor;

// screen dimentions:
// our perpective, as we are doing landscape (the wide way)
#define MAX_X myGLCD.getYSize() // 319
#define MAX_Y myGLCD.getXSize() // 239

myEEType myEE;

const byte rawSize = 1;
DoorStates *g_pDoorStates = new DoorStates();
StateMachine *rawArray[rawSize] = {g_pDoorStates};
Array<StateMachine *> lStateMachines = Array<StateMachine *>(rawArray, rawSize);

void DoorControl()
{
#undef INCREMENTING_OUTPUTS
#if defined(INCREMENTING_OUTPUTS)
  static CFwTimer Incrementer(0);
  if (Incrementer.IsTimeout()) {
    Outputs.SouthThrusterTx.Thrust++;
    South_Winter_Lock_Open.Write(South_Winter_Lock_Open.Read()+1); // ID x11
    if (South_Winter_Lock_Open.Read() == 0) {
      Winter_Lock_Closed.Write(Winter_Lock_Closed.Read()+1);       // ID x11 to cascade bits
    }
    Upper_South_Door.Write(Upper_South_Door.Read()+1);             // ID x21
    North_Winter_Lock_Open.Write(North_Winter_Lock_Open.Read()+1); // ID x31
    Upper_North_Door.Write(Upper_North_Door.Read()+1);             // ID x41 Hydraulic
    Incrementer.IncrementTimer(1000);
  }
  return;
#endif

  // Constantly monitor angle from hinges and maintain them in EEPROM to be
  // handled over power loss.
//  if (CanPollElapsedFromLastRxByCOBID(SOUTHDOORANALOG_RX_COBID) < 250) {
//    // update is recent
//    if (South_Winter_Door_Position < myEE.SouthDoor.Min)
//      myEE.SouthDoor.Min = South_Winter_Door_Position;
//    if (South_Winter_Door_Position > myEE.SouthDoor.Max)
//      myEE.SouthDoor.Max = South_Winter_Door_Position;
//  }
//  if (CanPollElapsedFromLastRxByCOBID(NORTHDOORANALOG_RX_COBID) < 250) {
//    // update is recent
//    if (North_Winter_Door_Position < myEE.NorthDoor.Min)
//      myEE.NorthDoor.Min = North_Winter_Door_Position;
//    if (North_Winter_Door_Position > myEE.NorthDoor.Max)
//      myEE.NorthDoor.Max = North_Winter_Door_Position;
//  }
}


void setup()
{
  Serial.begin(115200);
  Serial.println("Merillat boathouse controller");
  CanPollerInit();
  while (CAN_OK != CAN.begin(CAN_250KBPS))              // init can bus : baudrate = 500k
  {
      Serial.println("CAN BUS Shield init fail");
      Serial.println(" Init CAN BUS Shield again");
      delay(100);
  }
  Serial.println("CAN BUS Shield init ok!");
  Serial.print("EEPROM size = ");
  Serial.println(EEPROM.length());

  // Setup the LCD
  myGLCD.InitLCD(LANDSCAPE);
  myGLCD.setFont(SmallFont);

  // Clear the screen and draw the frame
  myGLCD.clrScr();

  // define door placement
  doorLen = 2*MAX_Y/3-12;
  lDoorX = 12; // indent enough for a letter to the left of us
  lDoorY = 5*MAX_Y/6-12;
  // mirror the right to the left
  rDoorX = MAX_X - lDoorX - 1;
  rDoorY = lDoorY;

  LeftDoor.X = lDoorX;
  LeftDoor.Y = lDoorY;
  RightDoor.X = rDoorX;
  RightDoor.Y = rDoorY;

  // Data we want to collect from the bus
  //           COBID,                   NumberOfBytesToReceive,           AddressOfDataStorage
  CanPollSetRx(SOUTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx);
  CanPollSetRx(SOUTHDOORANALOG_RX_COBID,sizeof(Inputs.SouthDoorAnalog_Rx),(INT8U*)&Inputs.SouthDoorAnalog_Rx);
//  CanPollSetRx(SOUTHTHRUSTER_RX_COBID,  sizeof(Inputs.SouthThruster_Rx),  (INT8U*)&Inputs.SouthThruster_Rx);
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx);
  CanPollSetRx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx);
//  CanPollSetRx(NORTHTHRUSTER_RX_COBID,  sizeof(Inputs.NorthThruster_Rx),  (INT8U*)&Inputs.NorthThruster_Rx);
  CanPollSetRx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx);

  // Data we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  //           COBID,                  NumberOfBytesToTransmit,          AddressOfDataToTransmit            MaskOfDigitalOutputs
  CanPollSetTx(0x80,                   0,                                NULL,                              0);
  CanPollSetTx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  (INT8U*)&Outputs.SouthDoorDIO_Tx,  SOUTHDOOR_OUTPUT_MASK);
  CanPollSetTx(SOUTHTHRUSTER_TX_COBID, sizeof(Outputs.SouthThruster_Tx), (INT8U*)&Outputs.SouthThruster_Tx, 0);
  CanPollSetTx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),(INT8U*)&Outputs.SouthHydraulic_Tx,SOUTHHYDRAULIC_OUTPUT_MASK);
  CanPollSetTx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  (INT8U*)&Outputs.NorthDoorDIO_Tx,  NORTHDOOR_OUTPUT_MASK);
  CanPollSetTx(NORTHTHRUSTER_TX_COBID, sizeof(Outputs.NorthThruster_Tx), (INT8U*)&Outputs.NorthThruster_Tx, 0);
  CanPollSetTx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),(INT8U*)&Outputs.NorthHydraulic_Tx,NORTHHYDRAULIC_OUTPUT_MASK);

  // Initialize our state machines
  for (int iCount = 0; iCount < lStateMachines.size(); iCount++) {
    lStateMachines[iCount]->Initialize();
  }

  CanPollDisplay(3); // show everything we've configured

  FlexiTimer2::set(1, 0.0004, CanPoller); // Every 0.4 ms (400us)
  FlexiTimer2::start();
}


// loop() no longer services the CAN I/O; that is now serviced directly
// by the configured Timer2 hitting CanPoller() every ms
void loop()
{
  if (Serial.available()) {
    char key = Serial.read();
    if (key == 'd')
      CanPollDisplay(3);
    else {
      Serial.println();
      Serial.print("OK=");
      Serial.print(OK);
      Serial.print(", Fault=");
      Serial.println(Fault);
    }
  }

  DoorControl();
#if !defined(INCREMENTING_OUTPUTS)
  // Execute all the state machines so that they can process timeout values, etc.
  for (int iCount = 0; iCount < lStateMachines.size(); iCount++) {
    lStateMachines[iCount]->Execute();
  }
#endif


#ifdef DO_LOGGING
  if (Head != Tail) {
    // not caught up, print out the next message we have
    Tail = (Tail + 1) % NUM_BUFFS;

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
#else
  // Show communications status
  static CFwTimer GUITimer;
  static bool LastComm = true;
  static byte PaintStatics = 0;
  static bool Cleared = false;
  if (HaveComm != LastComm) {
    myGLCD.clrScr();
    GUITimer.SetTimer(0); // expire right away
    LastComm = HaveComm;
    PaintStatics = HaveComm; // draw all if we just got Comm
  }
  if (!HaveComm) {
    static bool lastToggle = true;
    if (GUITimer.IsTimeout()) {
      static int mX = 0;
      static int mY = 0;
      lastToggle = !lastToggle;
#define MISSING "MISSING COMMS"
      static int mlen = strlen(MISSING);
      myGLCD.setColor(BLACK);
      myGLCD.setBackColor(BLACK);
      myGLCD.print(MISSING,mX,mY);
      if (lastToggle) {
        myGLCD.setColor(RED);
        myGLCD.setBackColor(GREEN);
      }
      else {
        myGLCD.setColor(GREEN);
        myGLCD.setBackColor(RED);
      }
      mX += myGLCD.getFontWidth()*2;
      mX = mX % (MAX_X-myGLCD.getFontWidth()*mlen);
      mY += myGLCD.getFontHeight();
      mY = mY % (MAX_Y-myGLCD.getFontHeight());
      myGLCD.print(MISSING,mX,mY);
      GUITimer.IncrementTimer(750);
    }
  }
  else { // HaveComm, see about updating any changed elements
#define BUMP_GUI_TIMER if (GUITimer.IsTimeout()) PaintStatics=2; GUITimer.SetTimer(60000);
    float SouthAngle = ANALOG_TO_RADIANS(South_Winter_Door_Position); // convert 0..4096 into 0..2pi
    if (SouthAngle > M_PI/2) // over 90 deg?
      SouthAngle = M_PI/2; // saturate at 90 deg
    else if (SouthAngle < DEGREES_TO_RADIANS(MIN_ANGLE)) // below Min (or negative?)
      SouthAngle = DEGREES_TO_RADIANS(MIN_ANGLE); // saturate at Min (closed)

    float NorthAngle = ANALOG_TO_RADIANS(North_Winter_Door_Position); // convert 0..4096 into 0..2pi
    if (NorthAngle > M_PI/2) // over 90 deg?
      NorthAngle = M_PI/2; // saturate at 90 deg
    else if (NorthAngle < DEGREES_TO_RADIANS(MIN_ANGLE)) // below Min (or negative?)
      NorthAngle = DEGREES_TO_RADIANS(MIN_ANGLE); // saturate at Min (closed)
    NorthAngle = M_PI - NorthAngle; // mirror angle across 180

    NewLeftDoor.Y = -sin(SouthAngle)*doorLen; // rise
    NewLeftDoor.X = cos(SouthAngle)*doorLen; // run
    NewLeftDoor.Y += lDoorY; // add in our offset, same for left and right
    NewLeftDoor.X += lDoorX;
#define DOOR_CHANGE 2 // more than this many pixels change before we draw it
    if (PaintStatics ||
        abs(NewLeftDoor.X - LeftDoor.X) > DOOR_CHANGE ||
        abs(NewLeftDoor.Y - LeftDoor.Y) > DOOR_CHANGE) {
      // erase previous door position (in black)
      myGLCD.setColor(BLACK);
      myGLCD.drawLine(lDoorX,lDoorY,LeftDoor.X,LeftDoor.Y);
      // draw new positions in Green
      myGLCD.setColor(GREEN);
      myGLCD.drawLine(lDoorX, lDoorY,NewLeftDoor.X,NewLeftDoor.Y);
      LeftDoor = NewLeftDoor;
      BUMP_GUI_TIMER;
    }

    NewRightDoor.Y = -sin(NorthAngle)*doorLen; // rise
    NewRightDoor.Y += rDoorY; // add in offset
    NewRightDoor.X = rDoorX + cos(NorthAngle)*doorLen; // run
    if (PaintStatics ||
        abs(NewRightDoor.X - RightDoor.X) > DOOR_CHANGE ||
        abs(NewRightDoor.Y - RightDoor.Y) > DOOR_CHANGE) {
      // erase previous door position (in black)
      myGLCD.setColor(BLACK);
      myGLCD.drawLine(rDoorX,rDoorY,RightDoor.X,RightDoor.Y);
      // draw new positions in Green
      myGLCD.setColor(GREEN);
      myGLCD.drawLine(rDoorX, rDoorY,NewRightDoor.X,NewRightDoor.Y);
      RightDoor = NewRightDoor;
      BUMP_GUI_TIMER;
    }

#define UPPER_DOOR_OFFSET 2
    byte UpperDoorState = Upper_South_Door_IsOpen.Read() + Upper_South_Door_IsClosed.Read()*2;
    float UpperAngle;
    switch (UpperDoorState) {
      case 0: // Neither -- somewhere in between
        UpperAngle = DEGREES_TO_RADIANS(MAX_ANGLE/2.0);
        break;
      case 1: // Open
        UpperAngle = DEGREES_TO_RADIANS(MAX_ANGLE);
        break;
      case 2: // Closed
        UpperAngle = DEGREES_TO_RADIANS(0);
        break;
      default: // Invalid
        UpperAngle = DEGREES_TO_RADIANS(-4); // looks 'over closed'
    }
    NewUpperLeftDoor.Y = -sin(UpperAngle)*doorLen; // rise
    NewUpperLeftDoor.X = cos(UpperAngle)*doorLen; // run
    NewUpperLeftDoor.Y += lDoorY+UPPER_DOOR_OFFSET; // add in our offset, same for left and right
    NewUpperLeftDoor.X += lDoorX+UPPER_DOOR_OFFSET;
    if (PaintStatics ||
        NewUpperLeftDoor.X != UpperLeftDoor.X ||
        NewUpperLeftDoor.Y != UpperLeftDoor.Y) {
      // erase previous door position (in black)
      myGLCD.setColor(BLACK);
      myGLCD.drawLine(lDoorX+UPPER_DOOR_OFFSET,lDoorY+UPPER_DOOR_OFFSET,UpperLeftDoor.X,UpperLeftDoor.Y);
      // draw new position
      myGLCD.setColor(RED);
      myGLCD.drawLine(lDoorX+UPPER_DOOR_OFFSET,lDoorY+UPPER_DOOR_OFFSET,NewUpperLeftDoor.X,NewUpperLeftDoor.Y);
      UpperLeftDoor = NewUpperLeftDoor;
      BUMP_GUI_TIMER;
    }

    UpperDoorState = Upper_North_Door_IsOpen.Read() + Upper_North_Door_IsClosed.Read()*2;
    switch (UpperDoorState) {
      case 0: // Neither -- somewhere in between
        UpperAngle = DEGREES_TO_RADIANS(MAX_ANGLE/2.0);
        break;
      case 1: // Open
        UpperAngle = DEGREES_TO_RADIANS(MAX_ANGLE);
        break;
      case 2: // Closed
        UpperAngle = DEGREES_TO_RADIANS(0);
        break;
      default: // Invalid
        UpperAngle = DEGREES_TO_RADIANS(-4); // looks 'over closed'
    }
    UpperAngle = M_PI - UpperAngle; // mirror angle across 180
    NewUpperRightDoor.Y = -sin(UpperAngle)*doorLen; // rise
    NewUpperRightDoor.Y += rDoorY+UPPER_DOOR_OFFSET; // add in offset
    NewUpperRightDoor.X = rDoorX-UPPER_DOOR_OFFSET + cos(UpperAngle)*doorLen; // run
    if (PaintStatics ||
        NewUpperRightDoor.X != UpperRightDoor.X ||
        NewUpperRightDoor.Y != UpperRightDoor.Y) {
      // erase previous door position (in black)
      myGLCD.setColor(BLACK);
      myGLCD.drawLine(rDoorX-UPPER_DOOR_OFFSET,rDoorY+UPPER_DOOR_OFFSET,UpperRightDoor.X,UpperRightDoor.Y);
      // draw new position
      myGLCD.setColor(RED);
      myGLCD.drawLine(rDoorX-UPPER_DOOR_OFFSET,rDoorY+UPPER_DOOR_OFFSET,NewUpperRightDoor.X,NewUpperRightDoor.Y);
      UpperRightDoor = NewUpperRightDoor;
      BUMP_GUI_TIMER;
    }


    static struct {
      char *Text;
      INT32S X,Y;
      bool LastState;
      BitObject *Source;
    } DI_display[] = { // Digital Input
      // Requests coming from remote control:
      {"Request Open", (MAX_X-myGLCD.getFontWidth()*12)/2,myGLCD.getFontHeight()*4, true,&Remote_IsRequestingOpen},
      {"Request Close",(MAX_X-myGLCD.getFontWidth()*13)/2,myGLCD.getFontHeight()*5, true,&Remote_IsRequestingClose},
      // south latch
      {"U",    2,                                  MAX_Y/3,                         true,&South_Winter_Lock_Open_IsUnlatched},
      {"L",    2,                                  MAX_Y/3+myGLCD.getFontHeight()*3,true,&South_Winter_Lock_Open_IsLatched},
      //  north latch
      {"U",    MAX_X-1-myGLCD.getFontWidth(),      MAX_Y/3,                         true,&North_Winter_Lock_Open_IsUnlatched},
      {"L",    MAX_X-1-myGLCD.getFontWidth(),      MAX_Y/3+myGLCD.getFontHeight()*3,true,&North_Winter_Lock_Open_IsLatched},
      // winter/center latch
      {"U",    MAX_X/2-myGLCD.getFontWidth()*2,    MAX_Y-5*myGLCD.getFontHeight()/2,true,&Winter_Lock_Closed_IsUnlatched},
      {"L",    MAX_X/2+myGLCD.getFontWidth()*2,    MAX_Y-5*myGLCD.getFontHeight()/2,true,&Winter_Lock_Closed_IsLatched}
    };

    // include display of digital inputs being active (letter(s) with background green)
    // inactive inputs get same letters with background black
    for (int z=0; z<sizeof(DI_display)/sizeof(DI_display[0]); z++) {
      bool current = DI_display[z].Source->Read();
      if (PaintStatics ||
          DI_display[z].LastState != current) {
        if (current) {
          myGLCD.setColor(BLUE);
          myGLCD.setBackColor(GREEN);
        }
        else {
          myGLCD.setColor(YELLOW);
          myGLCD.setBackColor(BLACK);
        }
        myGLCD.print(DI_display[z].Text, DI_display[z].X, DI_display[z].Y);
        DI_display[z].LastState = current; // update, wait until a change to do again
        BUMP_GUI_TIMER;
      }
    }

    static struct {
      char *Text; // depending upon value of Source->Read(), which character will be displayed
      INT32S X,Y;
      byte LastState;
      BitObject *Source;
    } BO_output[] = { // Bit Object outputs
      {" <>X", MAX_X/2,MAX_Y-5*myGLCD.getFontHeight()/2,-1,&Center_Winter_Latch},
      {" ^VX",       2,MAX_Y/3+(myGLCD.getFontHeight()*3)/2,-1,&South_Winter_Latch},
      {" ^VX",MAX_X-1-myGLCD.getFontWidth(),MAX_Y/3+(myGLCD.getFontHeight()*3)/2,-1,&North_Winter_Latch},
      {" <>X",myGLCD.getFontWidth()*5,MAX_Y-8*myGLCD.getFontHeight()/2,-1,&Upper_South_Door},
      {" ><X",MAX_X-myGLCD.getFontWidth()*5,MAX_Y-8*myGLCD.getFontHeight()/2,-1,&Upper_North_Door}
    };
    for (int z=0; z<sizeof(BO_output)/sizeof(BO_output[0]); z++) {
      INT8U current = BO_output[z].Source->Read();
      if (PaintStatics ||
          BO_output[z].LastState != current) {
        myGLCD.setColor(YELLOW);
        myGLCD.setBackColor(BLACK);
        char Str[2];
        sprintf(Str,"%c",BO_output[z].Text[current]);
        myGLCD.print(Str, BO_output[z].X, BO_output[z].Y);
        BO_output[z].LastState = current; // update, wait until a change to do again
        BUMP_GUI_TIMER;
      }
    }

    // paints the text of the current state (and previous state, if in Error)
    // time remaining before error in current state is also displayed
    INT16S current = g_pDoorStates->GetCurrentState();
    static INT16S LastState = -1;
    if (PaintStatics ||
        LastState != current) {
      myGLCD.setColor(YELLOW);
      myGLCD.setBackColor(BLACK);
      char state[40];
      sprintf(state,"%-30s",g_pDoorStates->GetCurrentStateName());
      state[30] = 0; // truncate
      myGLCD.print(state,myGLCD.getFontWidth()*8,myGLCD.getFontHeight());
      LastState = current;
      if (current == DoorStates::eST_Error)
        sprintf(state,"%-30s",g_pDoorStates->GetPreviousStateName());
      else
        sprintf(state,"%-30s"," ");
      myGLCD.print(state,myGLCD.getFontWidth()*8,myGLCD.getFontHeight()*2);
      BUMP_GUI_TIMER;
    }
    if (!Cleared) {
      // print the time as long as something else is updating
      char digits[20];
      long ct = g_pDoorStates->CurrentTime();
      if (ct == INFINITE)
        sprintf(digits,"   -   ");
      else
        sprintf(digits,"%7ld",ct);
      myGLCD.setColor(YELLOW);
      myGLCD.setBackColor(BLACK);
      myGLCD.print(digits,RIGHT,myGLCD.getFontHeight()*2);
    }

    if (PaintStatics) {
      myGLCD.setColor(64, 64, 64);
      myGLCD.fillRect(0, MAX_Y-13, MAX_X, MAX_Y); // bottom 13 pixels
      myGLCD.setColor(WHITE);
      myGLCD.setBackColor(RED);
      myGLCD.print("** Merillat Boathouse door controller **", CENTER, 1);
      myGLCD.setColor(255, 128, 128);
      myGLCD.setBackColor(64, 64, 64);
      myGLCD.print("Wayne Ross", LEFT, MAX_Y-12);
      myGLCD.print("(C)2017", RIGHT, MAX_Y-12);
      PaintStatics--; // should end up with 2 passes to draw everything back
    }
    if (GUITimer.IsTimeout() && !Cleared) {
      // nothing has been updated lately, so let's invoke 'screen saver'
      GUITimer.SetTimer(0); // ensure it cannot wrap/wake up
      myGLCD.clrScr(); // black
      Cleared = true;
    }
    else if (!GUITimer.IsTimeout())
      Cleared = false; // latch reset
  } // HaveComm
#endif
}

