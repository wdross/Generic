#include <EEPROM.h>
#include <FlexiTimer2.h>
#include <PolledTouch.h>

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
  int TX,TY; // where to draw thruster indicator on winter doors
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
  static CFwTimer *Incrementer = new CFwTimer(0);
#if defined(INCREMENTING_OUTPUTS)
  if (Incrementer->IsTimeout()) {
    Outputs.SouthThrusterTx.Thrust++;
    Outputs.NorthThrusterTx.Thrust--;
    South_Winter_Lock_Open.Write(South_Winter_Lock_Open.Read()+1); // ID x11
    if (South_Winter_Lock_Open.Read() == 0) {
      Winter_Lock_Closed.Write(Winter_Lock_Closed.Read()+1);       // ID x11 to cascade bits
    }
    Upper_South_Door.Write(Upper_South_Door.Read()+1);             // ID x21
    North_Winter_Lock_Open.Write(North_Winter_Lock_Open.Read()+1); // ID x31
    Upper_North_Door.Write(Upper_North_Door.Read()+1);             // ID x41 Hydraulic

    Incrementer->IncrementTimerUnlessWayBehind(1000); // keep the jitter away
  }
  return;
#else
  // Creating a blinking LED in every cabinet (1 of the DIO cards)
  if (Incrementer->IsTimeout()) {
    Activity_Panel1.Write(!Activity_Panel1.Read()); // toggle 1 bit, same state to all
    Activity_Panel2.Write(Activity_Panel1.Read());
    Activity_Panel3.Write(Activity_Panel1.Read());
    Activity_Panel4.Write(Activity_Panel1.Read());

    Incrementer->IncrementTimerUnlessWayBehind(1000); // keep the jitter away
  }
  // no return, this can co-exist with normal logic
#endif
}


// calculate a 32-bit CRC of the data at *src for length BYTES
unsigned long CalcChecksum(INT8U *src, int length) {

  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = 0 ; index < length; ++index) {
    crc = crc_table[(crc ^ src[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (src[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}


myEEPayloadType EECopy;

// +/-45 degrees away from zero is our valid setpoints
#define FORTYFIVEDEG 4192 // these are 15-bit BDEG, so off by a power of 2
#define MINUS_FORTYFIVEDEG (32767-FORTYFIVEDEG)
#define INC_DEC 22 // There is about 91 counts to a degree, so 22 is about 1/4 degree, or 3.5"
                   // The 100ms update rate and 90 deg motion in 60 seconds yields 0.15 deg per update

typedef void (*funcOper)(int t); // I want to be able to call functions that operate on an EEPROM address
struct {
  int X,Y; // upper left for text update
  int cX, cY; // center for touch
  int Width, Height; // width of touch, X and Y
  INT16U *Target; // what value might I modify
  void *Source; // what might I get a preset from?
  funcOper fo;
} Touches[26];

CFwTimer Activity; // accessible to these helper functions
void SaveOper(int t) {
  for (int i=0; i<4; i++) {
    EECopy.Doors[i].Valid = (EECopy.Doors[i].Opened >= FORTYFIVEDEG &&
                             EECopy.Doors[i].Opened <= MINUS_FORTYFIVEDEG &&
                             EECopy.Doors[i].Closed >= FORTYFIVEDEG &&
                             EECopy.Doors[i].Closed <= MINUS_FORTYFIVEDEG);
  }
  myEE.Data = EECopy;
  myEE.CheckSum = CalcChecksum((INT8U*)&myEE,sizeof(myEE.Data));
  EEPROM.put(0,myEE); // write out the just-updated EE data

  Activity.SetTimer(0); // and exit
};
void CancelOper(int) {
  // get out without doing anything
  Activity.SetTimer(0);
};
void PrintUpdatedValue(int t) {
  char num[10];
  myGLCD.setBackColor(BLACK);
  // correctly cap within valid bounds
  if (*Touches[t].Target < FORTYFIVEDEG)
    *Touches[t].Target = FORTYFIVEDEG;
  else if (*Touches[t].Target > MINUS_FORTYFIVEDEG)
    *Touches[t].Target = MINUS_FORTYFIVEDEG;

  sprintf(num,"%5d",*Touches[t].Target);
  myGLCD.print(num,Touches[t].X,Touches[t].Y);
}
void SetValue(int t) {
  *Touches[t].Target = ((AnalogObject*)Touches[t].Source)->Value();
  Touches[t].fo = NULL;
  PrintUpdatedValue(t);
}
void ChangeValue(int t) {
  int which = Touches[t].Target;
  *Touches[which].Target += (INT16U)Touches[t].Source;
  PrintUpdatedValue(which);
}

// creates an entry in the array
// returns the index within Touches[] that was created
int TouchEntry(int Major, int Minor, int x, int y, int characters, void *Fn, INT16U *t, void *src) {
  int i = Major*6+Minor;
  Touches[i].X = x;
  Touches[i].Y = y;
  Touches[i].Width = characters * myGLCD.getFontWidth();
  Touches[i].Height = myGLCD.getFontHeight() * 3 / 2; // a little bigger than text
  Touches[i].cX = x + Touches[i].Width/2;
  Touches[i].cY = y + Touches[i].Height/2;
  Touches[i].Target = t;
  Touches[i].Source = src;
  Touches[i].fo = Fn;
#if 1
  Serial.print("["); Serial.print(i); Serial.print("] #="); Serial.print(characters); Serial.print(" @ (");
  Serial.print(Touches[i].X); Serial.print(","); Serial.print(Touches[i].Y); Serial.print(")x(");
  Serial.print(Touches[i].X+Touches[i].Width); Serial.print(","); Serial.print(Touches[i].Y+Touches[i].Height);
  Serial.print(") Source=0x"); Serial.print((long)Touches[i].Source,HEX); Serial.print(", target=0x"); Serial.println((int)Touches[i].Target,HEX);
#endif
  return(i);
}

void ClearTouches() {
  for (int i=0; i<sizeof(Touches)/sizeof(Touches[0]); i++)
    Touches[i].fo = NULL;
}

void MoveDoor(int t) { // thruster jog
  int which = (INT16U)Touches[t].Target;
  Serial.print(which); Serial.print(" ");
  if (which == NORTH_INSTANCE || SOUTH_INSTANCE) {
    Outputs.Thrusters[which].Direction = (INT16U)Touches[t].Source; // DIRECTION_OPEN or _CLOSE
    Outputs.Thrusters[which].Thrust = MAX_THRUST;
  }
} // MoveDoor

void MoveLatch(int t) { // latch jog
  int which = (INT16U)Touches[t].Target;
  Serial.print(which); Serial.print(" ");
  ((BitObject*)Touches[t].Source)->Write(which);
} // MoveLatch


void JogMode(int t) {
  int X,Y,i; // touch inputs
  char Title[20];

  myGLCD.clrScr();
  myGLCD.setColor(WHITE);
  myGLCD.setBackColor(BLACK);
  myGLCD.print("Jog Mode", CENTER, 0);
  myGLCD.print("For any action", CENTER, MAX_Y/2);
  myGLCD.print("press and hold", CENTER, MAX_Y/2+myGLCD.getFontHeight());
  myGLCD.print("NORTH   ",RIGHT,myGLCD.getFontHeight());
  myGLCD.print("   SOUTH",LEFT,myGLCD.getFontHeight());

  Activity.SetTimer(5*60*1000LL); // timout to exit this screen
  ClearTouches(); // don't have any functions defined
  // Now buttons
  myGLCD.setBackColor(BLUE);
  myGLCD.print("BACK", RIGHT, 0);
  TouchEntry(4,1,MAX_X-4*myGLCD.getFontWidth(),0,5,&CancelOper,NULL,NULL); // CANCEL

  char *TitleList[] = {"Upper","Latch","Winter"}; // each one gets an OPEN/CLOSE version
  BitObject *outList[] = {&Upper_South_Door,&South_Winter_Latch,NULL,&Upper_North_Door,&North_Winter_Latch,NULL,&Center_Winter_Latch,&Inflate_North,&Inflate_South};
  for (int ns=0; ns<2; ns++) {
    // ns==0 => NORTH;   ns==1 => SOUTH
    for (int oc=0; oc<2; oc++) {
      // oc==0 => OPEN;   oc==1 => CLOSE
      for (i=0; i<3; i++) { // which one
        sprintf((char*)&Title,"%s %s",TitleList[i],oc?"CLOSE":"OPEN");
        Y = myGLCD.getFontHeight()*(3 + (oc + i*2)*3);
        myGLCD.print(Title, ns==1?LEFT:RIGHT, Y);
        X = ns==1?0:MAX_X-myGLCD.getFontWidth()*strlen(Title);
        if (i < 2) // Latches
          TouchEntry(ns,i*2+oc,X,Y,strlen(Title),&MoveLatch,(INT16U*)(oc==1?lr_Latch_Request:lr_Unlatch_Request),outList[i+(ns==1?0:3)]);
        else       // Thrusters
          TouchEntry(ns,i*2+oc,X,Y,strlen(Title),&MoveDoor,(INT16U*)(ns==1?SOUTH_INSTANCE:NORTH_INSTANCE),oc==1?DIRECTION_CLOSE:DIRECTION_OPEN);
      } // which
    } // Open/Close
  } // North/South
  sprintf((char*)&Title,"AIR");
  Y = myGLCD.getFontHeight()*(2 + (1 + 0*2)*3);
  X = (MAX_X-myGLCD.getFontWidth()*strlen(Title)*3)/2;
  myGLCD.print(Title, X, Y);
  TouchEntry(2,0,X,Y,strlen(Title),&MoveLatch,(INT16U*)1,&Inflate_South);
  Y = myGLCD.getFontHeight()*(2 + (2 + 0*2)*3);
  X = (MAX_X+myGLCD.getFontWidth()*strlen(Title)*3)/2;
  myGLCD.print(Title, X, Y);
  TouchEntry(2,1,X,Y,strlen(Title),&MoveLatch,(INT16U*)1,&Inflate_North);

  sprintf((char*)&Title,"Center OPEN");
  Y = myGLCD.getFontHeight()*(2 + (0 + 2*2)*3);
  X = (MAX_X-myGLCD.getFontWidth()*strlen(Title))/2;
  myGLCD.print(Title, X, Y);
  TouchEntry(2,2,X,Y,strlen(Title),&MoveLatch,(INT16U*)lr_Unlatch_Request,&Center_Winter_Latch);
  sprintf((char*)&Title,"Center CLOSE");
  Y = myGLCD.getFontHeight()*(2 + (1 + 2*2)*3);
  X = (MAX_X-myGLCD.getFontWidth()*strlen(Title))/2;
  myGLCD.print(Title, X, Y);
  TouchEntry(2,3,X,Y,strlen(Title),&MoveLatch,(INT16U*)lr_Latch_Request,&Center_Winter_Latch);

  while (Activity.IsTiming()) {
    char digits[20];
    long ct = Activity.GetExpiredBy()/1000;
    sprintf(digits,"%7ld",abs(ct));
    myGLCD.setColor(WHITE);
    myGLCD.setBackColor(BLACK);
    myGLCD.print(digits,CENTER,MAX_Y-12);

    if (ToucherLoop(X,Y,500)) {
      Activity.ResetTimer(); // restart counting from 'now'
      for (i=0; i<sizeof(Touches)/sizeof(Touches[0]); i++) {
        // which one are we touching closest to?
        if (Touches[i].fo &&
            abs(Touches[i].cX - X) < max(30,Touches[i].Width/2) &&
            abs(Touches[i].cY - Y) < 19) {
          // yup, this one looks good
          Serial.print("Matched ");
          Serial.print(i); Serial.print(":");
          Touches[i].fo(i); // run it
          myGLCD.setBackColor(RED);
          myGLCD.print("      ",CENTER,myGLCD.getFontHeight());
          break; // quit looking
        }
      }
      if (i > sizeof(Touches)/sizeof(Touches[0])) {
        Serial.print("Failed to find "); Serial.print(X); Serial.print(","); Serial.println(Y);
      }
    }
    else if (ToucherStartedTouching(X,Y)) // touching, but not long enough yet
      Activity.ResetTimer(); // retart from 'now'
    else if (!ToucherStillTouching(X,Y)) {
      // kill motion they aren't touching any longer; this will get hit *a lot*
      Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_NONE;
      Outputs.Thrusters[SOUTH_INSTANCE].Thrust = NO_THRUST;
      Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_NONE;
      Outputs.Thrusters[NORTH_INSTANCE].Thrust = NO_THRUST;
      for (i=0; i<sizeof(outList)/sizeof(outList[0]); i++) {
        if (outList[i])
          outList[i]->Write(lr_No_Request); // stop motion
      }
      // Reset visual indicator
      // background is always set to BLACK for timer update
      myGLCD.print("      ",CENTER,myGLCD.getFontHeight());
    }
    else // still touching, stay on this screen
      Activity.ResetTimer(); // restart counting from 'now'
  } // while timer indicates activity

  myGLCD.clrScr(); // erase screen on the way out
}

char buff[11*32+10]; // bib stack storage location
void Console() {
  int X,Y; // touch inputs

  Activity.SetTimer(60*1000LL); // timout to exit this screen
  ClearTouches(); // don't have any functions defined
  myGLCD.clrScr();
  myGLCD.setColor(WHITE);
  myGLCD.setBackColor(BLACK);
  myGLCD.print("Console", CENTER, 0);
  g_pDoorStates->ShowAllTimes((char*)&buff);
  char *p = (char*)&buff;
  X = 2; // start on line 2
  while (*p) {
    p += myGLCD.print(p,RIGHT,myGLCD.getFontHeight()*X);
    X++; // next line
    p++; // skip one null
  }

  // Now buttons
  myGLCD.setBackColor(BLUE);
  myGLCD.print("BACK", RIGHT, myGLCD.getFontHeight());
  TouchEntry(4,1,MAX_X-2*myGLCD.getFontWidth(),myGLCD.getFontHeight()*3/2,5,&CancelOper,NULL,NULL); // CANCEL

  myGLCD.print("Hinge Settings",LEFT,MAX_Y-myGLCD.getFontHeight());
  TouchEntry(2,1,0,MAX_Y-myGLCD.getFontHeight(),14,&EESettings,NULL,NULL); // alter EESettings

  myGLCD.print("Jog Mode",RIGHT,MAX_Y-myGLCD.getFontHeight());
  TouchEntry(1,1,MAX_X-myGLCD.getFontWidth()*11,MAX_Y-myGLCD.getFontHeight(),10,&JogMode,NULL,NULL);

  while (Activity.IsTiming()) {
    if (ToucherLoop(X,Y)) {
      int i;
      Activity.ResetTimer(); // restart counting from 'now'
      for (i=0; i<sizeof(Touches)/sizeof(Touches[0]); i++) {
        // which one are we touching closest to?
        if (Touches[i].fo &&
            abs(Touches[i].cX - X) < max(15,Touches[i].Width/2) &&
            abs(Touches[i].cY - Y) < 20) {
          // yup, this one looks good
          Serial.print("Matched ");
          Serial.println(i);
          Touches[i].fo(i); // run it
          break; // quit looking
        }
      }
      if (i <= sizeof(Touches)/sizeof(Touches[0])) {
        Serial.print("Failed to find "); Serial.print(X); Serial.print(","); Serial.println(Y);
      }
    }
  } // while timer indicates activity

  myGLCD.clrScr(); // erase screen on the way out
}

void EESettings(int tt) { // doesn't use parameter passed in from Touches[] call
  int X,Y; // touch inputs

  // Stay in this routine, allowing user to update settings until SAVE or CANCEL is pressed
  EECopy = myEE.Data; // we'll change this, and decide to save or not

  Activity.SetTimer(7*60*1000LL); // after 10 minutes of no activity, exit this screen
  // Graphic routine that takes over from the standard one and shows all values for all doors
  // and contains the ability to set and modify (+/-) any paramter.
  // will also show the live value coming from the associated sensor
  myGLCD.clrScr();
  ClearTouches(); // don't have any functions defined

  myGLCD.setColor(WHITE);
  myGLCD.setBackColor(BLACK);
  myGLCD.print("Display/Set hinge values", CENTER, 0);
  myGLCD.setBackColor(BLUE);
  myGLCD.print("CANCEL", RIGHT, myGLCD.getFontHeight());
  myGLCD.print("SAVE", LEFT, myGLCD.getFontHeight());

  TouchEntry(4,0,2*myGLCD.getFontWidth(),myGLCD.getFontHeight()*3/2,5,&SaveOper,NULL,NULL); // Save
  TouchEntry(4,1,MAX_X-2*myGLCD.getFontWidth(),myGLCD.getFontHeight()*3/2,5,&CancelOper,NULL,NULL); // CANCEL

  for (int i=0; i<4; i++) {
    int j, op, cl;
    int row = i / 2 + 1; // 1 or 2 (Winter or Upper)
    int col = i % 2; // 0 or 1 (for South or North)
    if (row == 2)
      DoorInfo[i].Y = MAX_Y/3 * row - myGLCD.getFontHeight();
    else
      DoorInfo[i].Y = MAX_Y/4 - myGLCD.getFontHeight();
    myGLCD.setBackColor(BLACK);
    myGLCD.print(DoorInfo[i].Name,col?RIGHT:LEFT,DoorInfo[i].Y);
    if (col == 0) {
      myGLCD.print("Opened",CENTER,DoorInfo[i].Y+2*myGLCD.getFontHeight());
      myGLCD.print("Closed",CENTER,DoorInfo[i].Y+5*myGLCD.getFontHeight());
    }
    int X;
    if (col) // RIGHT
      X = MAX_X - 10 * myGLCD.getFontWidth();
    else // LEFT
      X = 5 * myGLCD.getFontWidth();
    if (EECopy.Doors[i].Valid) {
      myGLCD.setBackColor(BLACK);
      op = TouchEntry(i,0,X,DoorInfo[i].Y + 2*myGLCD.getFontHeight(),5,NULL,&EECopy.Doors[i].Opened,NULL);
      PrintUpdatedValue(op);
      cl = TouchEntry(i,1,X,DoorInfo[i].Y + 5*myGLCD.getFontHeight(),5,NULL,&EECopy.Doors[i].Closed,NULL);
      PrintUpdatedValue(cl);
    }
    else {
      myGLCD.setBackColor(BLUE);
      op = TouchEntry(i,0,X,DoorInfo[i].Y + 2*myGLCD.getFontHeight(),5,&SetValue,&EECopy.Doors[i].Opened,DoorInfo[i].Position);
      myGLCD.print("-SET-",Touches[op].X,Touches[op].Y);
      cl = TouchEntry(i,1,X,DoorInfo[i].Y + 5*myGLCD.getFontHeight(),5,&SetValue,&EECopy.Doors[i].Closed,DoorInfo[i].Position);
      myGLCD.print("-SET-",Touches[cl].X,Touches[cl].Y);
    }
    myGLCD.setBackColor(BLUE);
    j = TouchEntry(i,2,X-4*myGLCD.getFontWidth(),DoorInfo[i].Y + 2*myGLCD.getFontHeight(),3,&ChangeValue,op,-INC_DEC);
    myGLCD.print("-",Touches[j].X,Touches[j].Y);
    j = TouchEntry(i,3,X+8*myGLCD.getFontWidth(),DoorInfo[i].Y + 2*myGLCD.getFontHeight(),3,&ChangeValue,op,+INC_DEC);
    myGLCD.print("+",Touches[j].X,Touches[j].Y);

    j = TouchEntry(i,4,X-4*myGLCD.getFontWidth(),DoorInfo[i].Y + 5*myGLCD.getFontHeight(),3,&ChangeValue,cl,-INC_DEC);
    myGLCD.print("-",Touches[j].X,Touches[j].Y);
    j = TouchEntry(i,5,X+8*myGLCD.getFontWidth(),DoorInfo[i].Y + 5*myGLCD.getFontHeight(),3,&ChangeValue,cl,+INC_DEC);
    myGLCD.print("+",Touches[j].X,Touches[j].Y);
  }

  while (Activity.IsTiming()) {
    for (int i=0; i<4; i++) {
      char value[12];
      int row = i / 2 + 1; // 1 or 2
      int col = i % 2; // 0 or 1 (for South or North)
      // update values from the CAN Analog modules
      myGLCD.setBackColor(BLACK);
      if (CanPollElapsedFromLastRxByCOBID(DoorInfo[i].COBID) > NOT_TALKING_TIMEOUT)
        sprintf(value,"------");
      else {
        INT16U v = DoorInfo[i].Position->Value();
        if (v > FORTYFIVEDEG &&
            MINUS_FORTYFIVEDEG > v)
          myGLCD.setColor(GREEN);
        else
          myGLCD.setColor(RED);

        sprintf(value,"%6d",DoorInfo[i].Position->Value());
      }
      int X;
      if (col) // RIGHT
        X = MAX_X - 11 * myGLCD.getFontWidth();
      else // LEFT
        X = 4 * myGLCD.getFontWidth();
      myGLCD.print(value,X,DoorInfo[i].Y - myGLCD.getFontHeight());
    }
    char digits[20];
    long ct = Activity.GetExpiredBy()/1000;
    if (ct == INFINITE)
      sprintf(digits,"   -   ");
    else
      sprintf(digits,"%7ld",abs(ct));
    myGLCD.setColor(WHITE);
    myGLCD.print(digits,CENTER,MAX_Y-12);

    if (ToucherLoop(X,Y)) {
      Activity.ResetTimer(); // restart counting from 'now'
      for (int i=0; i<sizeof(Touches)/sizeof(Touches[0]); i++) {
        // which one are we touching closest to?
        if (Touches[i].fo &&
            abs(Touches[i].X - X) < 15 &&
            abs(Touches[i].Y - Y) < 15) {
          // yup, this one looks good
          Touches[i].fo(i); // run it
          Serial.print("Matched ");
          Serial.println(i);
          break; // quit looking
        }
      }
    }
  } // while timer indicates activity

  myGLCD.clrScr(); // erase screen on the way out
}


void EEDisplay() {
  // show the contents of our EE structure on the serial port
  char sig[50];
  sprintf(sig,"EEPROM contents: '%s' (%d bytes)",myEE.Data.Signature,myEE.Data.TotalSize);
  Serial.println(sig);
  for (int i=0; i<4; i++) {
    Serial.print(DoorInfo[i].Name);
    Serial.print(": ");
    if (myEE.Data.Doors[i].Valid) {
      Serial.print(myEE.Data.Doors[i].Closed);
      Serial.print("  ");
      Serial.println(myEE.Data.Doors[i].Opened);
    }
    else
      Serial.println("---");
  }
  Serial.print(myEE.CheckSum,HEX);
  Serial.print("  ");
  if (CalcChecksum((INT8U*)&myEE.Data,sizeof(myEE.Data)) == myEE.CheckSum)
    Serial.println("OK");
  else
    Serial.println("bad");
}


void ThrusterParser() {
  static int LastFC = -1; // invalid
  // just received a CAN frame into Inputs.ThrusterRx.*
  // depending upon the state of things, we'll be either starting to parse a new message
  // (when SN/SequenceNumber == 0) or processing the next packet in a series (we see the 
  // same FC/FrameCounter with the next sequential SN)

  // Assemble frame into Inputs.Thrusters[0] (3 service calls to get a whole one)
  // Then copy Inputs.Thrusters[0] into [1] or [2] based upon InstanceID

  if (Inputs.ThrusterRx.fc_sn.SN == 0) {
    // start of frame sequence
    if (Inputs.ThrusterRx.NBD == 16) {
      // correct size, this is the first of a 3 frame sequence we need
      Inputs.Thrusters[0].LastRxTime.SetTimer(0); // record 'now'
      memcpy(&Inputs.Thrusters[0].fc_sn,&Inputs.ThrusterRx.fc_sn,8); // all 8 bytes, gets FrameCounter & SequenceNumber too
    }
  }
  else if (Inputs.ThrusterRx.fc_sn.FC == Inputs.Thrusters[0].fc_sn.FC &&
           Inputs.ThrusterRx.fc_sn.SN == Inputs.Thrusters[0].fc_sn.SN+1) {
    // Next message in sequence, retain bytes
    byte *dst = (byte*)&Inputs.Thrusters[0].NDB; // point to base target
    dst += 7*Inputs.ThrusterRx.fc_sn.SN; // increment by SequenceNumber we're getting
    Inputs.Thrusters[0].fc_sn = Inputs.ThrusterRx.fc_sn; // capture new FrameCounter & SequenceNumber
    memcpy(dst,&Inputs.Thruster_Rx[1],7); // skip FC & SN
    if (Inputs.Thrusters[0].fc_sn.SN == 2 && // last Sequence Number
        Inputs.Thrusters[0].SleipnerDeviceInstance > 0 && // valid Instance number
        Inputs.Thrusters[0].SleipnerDeviceInstance < 3) {
      // last frame, copy it over
      Inputs.Thrusters[Inputs.Thrusters[0].SleipnerDeviceInstance] = Inputs.Thrusters[0];
#if 0
      char line[100];
      sprintf(line,"NDB=%d, MC=%d, IG=%d, Sdt=%d Sdi=%d Status=%d, MT=%d PT=%d, V=%d, Amps=%d, OpenThrust=%d",
              Inputs.Thrusters[0].NDB,Inputs.Thrusters[0].ManufacturerCode,Inputs.Thrusters[0].IndustryGroup,
              Inputs.Thrusters[0].SleipnerDeviceType,Inputs.Thrusters[0].SleipnerDeviceInstance,Inputs.Thrusters[0].Status,
              Inputs.Thrusters[0].ThrusterMotorTemperature,Inputs.Thrusters[0].ThrusterPowerTemperature,Inputs.Thrusters[0].MotorVoltage,
              Inputs.Thrusters[0].MotorCurrent,Inputs.Thrusters[0].OutputThrust);
      Serial.println(line);
#endif
      Inputs.Thrusters[0].fc_sn.FC = -1; // tag such that can't accept another packet
    }
  }
} // ThrusterParser

#define MY_EEPROM_SIGNATURE "$EEM"
INT8U MyConstState = 0x5; // broadcasts "I'm Operational"

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

  memset(&myEE,0,sizeof(myEE)); // wipe it all out, makes !Valid too
  if (EEPROM.length() >= sizeof(myEE)) {
    EEPROM.get(0,myEE); // pull full structure from EEPROM
    if (strncmp(myEE.Data.Signature,MY_EEPROM_SIGNATURE,sizeof(myEE.Data.Signature)-1) == 0 &&
        myEE.Data.TotalSize == sizeof(myEE) &&
        CalcChecksum((INT8U*)&myEE,sizeof(myEE.Data)) == myEE.CheckSum) {
      // all was OK, checksum worked out the same
      // this means the data as-is in RAM is OK
      Serial.println("Retrieved data from EEPROM OK");
    }
    else {
      // checksum didn't work out, so we can't use these values
      // but do set up the structure so it can be saved back into EEPROM
      memset(&myEE,0,sizeof(myEE.Data)); // wipe it all out again, we just read some stuff that wasn't valid
      strcpy(myEE.Data.Signature,MY_EEPROM_SIGNATURE);
      myEE.Data.TotalSize = sizeof(myEE);
      Serial.println("Invalid EE data, initialized structure");
    }
    EEDisplay();
  }
  else {
    Serial.println("Not enough EEPROM available!");
    // the local structure will be full of zeros (!Valid) and shouldn't be (can't!) be written to EEPROM
  }

  // Setup the LCD
  myGLCD.InitLCD(LANDSCAPE);
  myGLCD.setFont(SmallFont);

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
  CanPollSetRx(THRUSTER_RX_COBID,       sizeof(Inputs.Thruster_Rx),       (INT8U*)&Inputs.Thruster_Rx,      ThrusterParser);
  CanPollSetRx(NORTHDOORDIO_RX_COBID,   sizeof(Inputs.NorthDoorDIO_Rx),   (INT8U*)&Inputs.NorthDoorDIO_Rx);
  CanPollSetRx(NORTHDOORANALOG_RX_COBID,sizeof(Inputs.NorthDoorAnalog_Rx),(INT8U*)&Inputs.NorthDoorAnalog_Rx);
  CanPollSetRx(NORTHHYDRAULIC_RX_COBID, sizeof(Inputs.NorthHydraulic_Rx), (INT8U*)&Inputs.NorthHydraulic_Rx);

  // Data we'll transmit (evenly spaced) every CAN_TX_INTERVAL ms
  //           COBID,                  NumberOfBytesToTransmit,          AddressOfDataToTransmit            MaskOfDigitalOutputs
  CanPollSetTx(0x80,                   0,                                NULL,                              0);
  CanPollSetTx(SOUTHDOORDIO_TX_COBID,  sizeof(Outputs.SouthDoorDIO_Tx),  (INT8U*)&Outputs.SouthDoorDIO_Tx,  SOUTHDOOR_OUTPUT_MASK);
  CanPollSetTx(THRUSTER_TX_COBID,      sizeof(Outputs.Thrusters[SOUTH_INSTANCE]), (INT8U*)&Outputs.Thrusters[SOUTH_INSTANCE], 0);
  CanPollSetTx(SOUTHHYDRAULIC_TX_COBID,sizeof(Outputs.SouthHydraulic_Tx),(INT8U*)&Outputs.SouthHydraulic_Tx,SOUTHHYDRAULIC_OUTPUT_MASK);
  CanPollSetTx(NORTHDOORDIO_TX_COBID,  sizeof(Outputs.NorthDoorDIO_Tx),  (INT8U*)&Outputs.NorthDoorDIO_Tx,  NORTHDOOR_OUTPUT_MASK);
  CanPollSetTx(THRUSTER_TX_COBID,      sizeof(Outputs.Thrusters[NORTH_INSTANCE]), (INT8U*)&Outputs.Thrusters[NORTH_INSTANCE], 0);
  CanPollSetTx(NORTHHYDRAULIC_TX_COBID,sizeof(Outputs.NorthHydraulic_Tx),(INT8U*)&Outputs.NorthHydraulic_Tx,NORTHHYDRAULIC_OUTPUT_MASK);
  CanPollSetTx(0x701,                  1,                                &MyConstState,                     0); // This will serve as a HeartBeat

  // Initialize our state machine(s)
  for (int iCount = 0; iCount < lStateMachines.size(); iCount++) {
    lStateMachines[iCount]->Initialize();
  }

  CanPollDisplay(3); // show everything we've configured

  ToucherSetup(); // initialize the lines for reading the touch input

  FlexiTimer2::set(1, 0.00035, CanPoller); // Every 0.35 ms (350us) or 2857 times per second
  FlexiTimer2::start();
}


// loop() no longer services the CAN I/O; that is now serviced directly
// by the configured Timer2 hitting CanPoller() every ms
void loop()
{
  static byte PaintStatics = 0;

  if (Serial.available()) {
    char key = Serial.read();
    if (key == 'd')
      CanPollDisplay(3);
    else if (key == 't' || key == 'T')
      g_pDoorStates->ShowAllTimes();
    else if (key == 'e' || key == 'E')
      EEDisplay();
    else if (key == 'w' || key == 'W') {
      // request to wipe all EEPROM data
      memset(&myEE,0,sizeof(myEE));
      Serial.println("Erased copy of EEPROM");
    }
    else {
      Serial.println();
      Serial.print("OK=");
      Serial.print(OK);
      Serial.print(", Fault=");
      Serial.println(Fault);
    }
  }

  DoorControl();
  g_pDoorStates->CheckForAbort(); // state-less opportunity to interrupt sequence
#if !defined(INCREMENTING_OUTPUTS)
  // Execute all the state machines so that they can process timeout values, etc.
  for (int iCount = 0; iCount < lStateMachines.size(); iCount++) {
    lStateMachines[iCount]->Execute();
  }
#endif


#ifdef DO_LOGGING
#error Undefine that DO_LOGGING in libraries\CanOpen\CanOpen.h to see anything on the GUI!
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
#else
  // Show communications status
  static CFwTimer GUITimer;
  static bool LastComm = true;
  static bool Cleared = false;
  if (HaveComm != LastComm) {
    myGLCD.clrScr();
    GUITimer.SetTimer(0); // expire right away
    LastComm = HaveComm;
    PaintStatics = HaveComm; // draw all if we just got Comm
  }

  int X,Y; // place to put ToucherLoop() results
  if (!HaveComm) {
    static bool lastToggle = true;
#define TIMEOUT_GUI_UPDATE 750
    if (ToucherLoop(X,Y)) {
      if (GUITimer.GetRemaining() <= TIMEOUT_GUI_UPDATE*2) {
        // first press, set up for nice, continually updating display
        myGLCD.clrScr(); // black
        myGLCD.setColor(WHITE);
        myGLCD.setBackColor(BLACK);
        myGLCD.print("ModuleID  Message late by (ms)",0,myGLCD.getFontHeight());
        myGLCD.print("'----' means CAN module is not talking",0,MAX_Y-myGLCD.getFontHeight());
      }
      GUITimer.SetTimer(60000);
    }

    if (GUITimer.IsTimeout()) {
      static int mX = 0;
      static int mY = 0;
      lastToggle = !lastToggle;
#define MISSING "MISSING COMMS"
      static int mlen = strlen(MISSING);
      myGLCD.setColor(BLACK);
      myGLCD.setBackColor(BLACK);
      myGLCD.print(MISSING,mX,mY); // erase old message (BLACK/BLACK on BLACK)
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
      GUITimer.IncrementTimerUnlessWayBehind(TIMEOUT_GUI_UPDATE);
    }
    else if (GUITimer.GetRemaining() > TIMEOUT_GUI_UPDATE*2) {
      // more than a second, so we were/are looking at NodeIDs and last response timers
      // just always repaint everything -- see how it behaves
      for (int j=0; j<NUM_IN_BUFFERS; j++) {
        if (CanInBuffers[j].Can.COBID) {
          char rxBuf[45];
          long elapsed = CanPollElapsedFromLastRxByIndex(j);
          sprintf(rxBuf,"      %02X  %6d",CanInBuffers[j].Can.COBID&0x7f,elapsed);
          if (elapsed > NOT_TALKING_TIMEOUT)
            strcpy(&rxBuf[12],"----");
          if (CanInBuffers[j].Can.COBID&IS_EXTENDED_COBID)
            strcpy(&rxBuf[16],"  S-Link gateway");
          myGLCD.print(rxBuf, 0, myGLCD.getFontHeight()*(j+2));
        }
      }
    }
    else if (GUITimer.GetRemaining() > TIMEOUT_GUI_UPDATE) {
      // we were watching things, now go to blank screen
      myGLCD.clrScr(); // black
    }
  }
  else { // HaveComm, see about updating any changed elements
#define BUMP_GUI_TIMER if (GUITimer.IsTimeout()) PaintStatics=2; GUITimer.SetTimer(60000);
    float SouthAngle = Winter_South_Door_Position.Angle(); // get Analog value as MIN_ANGLE..PI/2 radians
    float NorthAngle = Winter_North_Door_Position.Angle(); // get Analog value as MIN_ANGLE..PI/2 radians

    NewLeftDoor.Y = -sin(SouthAngle)*doorLen; // rise
    NewLeftDoor.X = cos(SouthAngle)*doorLen; // run
    NewLeftDoor.Y += lDoorY; // add in our offset, same for left and right
    NewLeftDoor.X += lDoorX;
#define DOOR_CHANGE 2 // more than this many pixels change before we draw it
#define THRUST_X 3
#define THRUST_Y 2
    static unsigned int LastDir[3] = {0,0,0}; // unused, North, South
    if (PaintStatics ||
        abs(NewLeftDoor.X - LeftDoor.X) > DOOR_CHANGE ||
        abs(NewLeftDoor.Y - LeftDoor.Y) > DOOR_CHANGE) {
      // erase previous door position (in black)
      myGLCD.setColor(BLACK);
      myGLCD.setBackColor(BLACK);
      myGLCD.drawLine(lDoorX,lDoorY,LeftDoor.X,LeftDoor.Y);
      myGLCD.print(" ",LeftDoor.TX,LeftDoor.TY); // erase any active thruster indicator
      // draw new positions in Green
      myGLCD.setColor(GREEN);
      myGLCD.drawLine(lDoorX, lDoorY,NewLeftDoor.X,NewLeftDoor.Y);
      // Where does Thruster indicator belong?
      NewLeftDoor.TY = -sin(SouthAngle)*doorLen*3/4; // 3/4 of the way out along the door
      NewLeftDoor.TX = cos(SouthAngle)*doorLen*3/4; // run
      NewLeftDoor.TY += lDoorY + THRUST_Y; // add in our offset, same for left and right
      NewLeftDoor.TX += lDoorX + THRUST_X;

      LeftDoor = NewLeftDoor;
      LastDir[SOUTH_INSTANCE] = 5; // invalid, forcing a re-draw below
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
      myGLCD.setBackColor(BLACK);
      myGLCD.drawLine(rDoorX,rDoorY,RightDoor.X,RightDoor.Y);
      myGLCD.print(" ",RightDoor.TX,RightDoor.TY); // erase any active thruster indicator
      // draw new positions in Green
      myGLCD.setColor(GREEN);
      myGLCD.drawLine(rDoorX, rDoorY,NewRightDoor.X,NewRightDoor.Y);
      // Where does Thruster indicator belong?
      NewRightDoor.TY = -sin(NorthAngle)*doorLen*3/4; // rise
      NewRightDoor.TY += rDoorY + THRUST_Y; // add in offset
      NewRightDoor.TX = rDoorX - THRUST_X - myGLCD.getFontWidth() + cos(NorthAngle)*doorLen*3/4; // run

      RightDoor = NewRightDoor;
      LastDir[NORTH_INSTANCE] = 5; // invalid, forcing a re-draw below
      BUMP_GUI_TIMER;
    }

    int dir = Outputs.Thrusters[SOUTH_INSTANCE].Direction | (Outputs.Thrusters[SOUTH_INSTANCE].Thrust << 2); // data tear
    if (dir != LastDir[SOUTH_INSTANCE]) {
      char Str[2] = {0,0}; // null string
      char Text[] = " V^X";
      sprintf(Str,"%c",Text[dir&0x3]);
      if ((dir >> 2) > MIN_THRUST)
        myGLCD.setColor(YELLOW);
      else
        myGLCD.setColor(GREEN);
      myGLCD.setBackColor(BLACK);
      myGLCD.print(Str,LeftDoor.TX,LeftDoor.TY);
      LastDir[SOUTH_INSTANCE] = dir;
    }
    dir = Outputs.Thrusters[NORTH_INSTANCE].Direction | (Outputs.Thrusters[NORTH_INSTANCE].Thrust << 2); // data tear
    if (dir != LastDir[NORTH_INSTANCE]) {
      char Str[2] = {0,0}; // null string
      char Text[] = " V^X";
      sprintf(Str,"%c",Text[dir&0x3]);
      if ((dir >> 2) > MIN_THRUST)
        myGLCD.setColor(YELLOW);
      else
        myGLCD.setColor(GREEN);
      myGLCD.setBackColor(BLACK);
      myGLCD.print(Str,RightDoor.TX,RightDoor.TY);
      LastDir[NORTH_INSTANCE] = dir;
    }

#define UPPER_DOOR_OFFSET 2
    float UpperAngle = Upper_South_Door_Position.Angle();
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

    UpperAngle = Upper_North_Door_Position.Angle();
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
      {"Remote Open", MAX_X/2-myGLCD.getFontWidth()*9,myGLCD.getFontHeight()*3, true,&Remote_IsRequestingOpen},
      {"Remote Close",MAX_X/2-myGLCD.getFontWidth()*9,myGLCD.getFontHeight()*4, true,&Remote_IsRequestingClose},
      {"Touch",       MAX_X/2+myGLCD.getFontWidth()*4,myGLCD.getFontHeight()*3, true,g_pDoorStates->Touch_IsRequestingOpen},
      {"Touch",       MAX_X/2+myGLCD.getFontWidth()*4,myGLCD.getFontHeight()*4, true,g_pDoorStates->Touch_IsRequestingClose},
      {"System Enabled",(MAX_X-myGLCD.getFontWidth()*14)/2,myGLCD.getFontHeight()*5, true,&System_Enable},
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
      {" A",         2,MAX_Y/3+(myGLCD.getFontHeight()*12)/2,-1,&Inflate_South},
      {" A",  MAX_X-1-myGLCD.getFontWidth(),MAX_Y/3+(myGLCD.getFontHeight()*12)/2,-1,&Inflate_North},
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
        sprintf(digits,"%7ld",abs(ct/1000));
      myGLCD.setColor(YELLOW);
      myGLCD.setBackColor(BLACK);
      myGLCD.print(digits,RIGHT,myGLCD.getFontHeight()*2);
    }

    if (GUITimer.IsTiming() &&   // screen saver isn't running
        ToucherLoop(X,Y,1000) && // pressed for a second
        abs(X - MAX_X/2) < 30) { // tight in center left to right
      if (abs(Y - 3*MAX_Y/5) < 30) { // below center
        Console(); // gets stuck in here for a long time
        PaintStatics = 2; // redraw the main screen when we return
      }
      else if (abs(Y - myGLCD.getFontHeight()*2) < 30) {
        // upper button, "Open"
        g_pDoorStates->Touch_IsRequestingOpen->Write(true);
        Serial.println("Open");
      }
      else if (abs(Y - myGLCD.getFontHeight()*6) < 30) {
        // lower button, "Close"
        g_pDoorStates->Touch_IsRequestingClose->Write(true);
        Serial.println("Close");
      }
    }
    else if (ToucherLoop(X,Y)) {
      BUMP_GUI_TIMER; // prevent or wake up from sleep
    }
    else if (!ToucherStillTouching(X,Y)) {
      // clear button flags (leaves output on as long as user holds)
      g_pDoorStates->Touch_IsRequestingOpen->Write(false);
      g_pDoorStates->Touch_IsRequestingClose->Write(false);
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
      myGLCD.setColor(WHITE);
      myGLCD.setBackColor(BLUE);
      myGLCD.print("C",MAX_X/2-myGLCD.getFontWidth()/2,3*MAX_Y/5-myGLCD.getFontHeight()/2);
      PaintStatics--; // should end up with 2 passes to draw everything back
      BUMP_GUI_TIMER; // prevent or wake up from sleep
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

