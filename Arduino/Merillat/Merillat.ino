#include <EEPROM.h>
#include <FlexiTimer2.h>

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
 *  that needs to be extended (a row of 2x18 pass-thru header is required, 5/8" tall).
 *  Also need 2 nylon stand offs (1/8" dia screw x 7/16" body) with 2 nylon nuts, super
 *    glued to the bottom of the display board.
 */

#include <ITDB02_Graph16.h>
// Declare which fonts we will be using
extern uint8_t SmallFont[];
ITDB02 myGLCD(38,39,40,41);
int doorLen, lDoorX, lDoorY, rDoorX, rDoorY;

struct LastCoords {
  int X,Y;
} LeftDoor, RightDoor;

// defines to be used as parameters to setColor() calls:
#define BLACK 0,0,0
#define WHITE 255,255,255
#define RED 255,0,0
#define GREEN 0,255,0
#define BLUE 0,0,255

// screen dimentions:
#define WIDTH myGLCD.getYSize()
#define HEIGHT myGLCD.getXSize()
// our perpective, as we are doing landscape (the wide way)
#define MAX_X (WIDTH-1)  // 319
#define MAX_Y (HEIGHT-1) // 239

long delayUntil;

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

  // Setup the LCD
  myGLCD.InitLCD(LANDSCAPE);
  myGLCD.setFont(SmallFont);

  // Clear the screen and draw the frame
  myGLCD.clrScr();

  myGLCD.setColor(RED);
  myGLCD.fillRect(0, 0, MAX_X, 13); // top 13 pixels
  myGLCD.setColor(64, 64, 64);
  myGLCD.fillRect(0, MAX_Y-13, MAX_X, MAX_Y);
  myGLCD.setColor(WHITE);
  myGLCD.setBackColor(RED);
  myGLCD.print("** Merillat Boathouse door controller **", CENTER, 1);
  myGLCD.setColor(255, 128, 128);
  myGLCD.setBackColor(64, 64, 64);
  myGLCD.print("Wayne Ross", LEFT, MAX_Y-12);
  myGLCD.print("(C)2016", RIGHT, MAX_Y-12);

  // black screen to work on
  myGLCD.setColor(BLACK);
  myGLCD.fillRect(0, 14, MAX_X-1, MAX_Y-14);

  doorLen = 2*HEIGHT/3-12;
  lDoorX = 12; // indent enough for a letter to the left of us
  lDoorY = 5*HEIGHT/6;
  // mirror the right to the left
  rDoorX = MAX_X - lDoorX - 1;
  rDoorY = lDoorY;

  LeftDoor.X = lDoorX;
  LeftDoor.Y = lDoorY;
  RightDoor.X = rDoorX;
  RightDoor.Y = rDoorY;

  myGLCD.setColor(BLUE);
  myGLCD.setBackColor(BLACK);

  // place some letters to be used as Input state indicators
  // left latch
  myGLCD.print("U", 2, MAX_Y/3);
  myGLCD.print("L", 2, MAX_Y/3+myGLCD.getFontHeight());
  // right latch
  myGLCD.print("U", MAX_X-1-myGLCD.getFontWidth(), MAX_Y/3);
  myGLCD.print("L", MAX_X-1-myGLCD.getFontWidth(), MAX_Y/3+myGLCD.getFontHeight());
  // center latch
  myGLCD.print("U", MAX_X/2-myGLCD.getFontWidth(), MAX_Y-5*myGLCD.getFontHeight()/2);
  myGLCD.print("L", MAX_X/2,MAX_Y-5*myGLCD.getFontHeight()/2);

  // Data we want to collect from the bus
  //           COBID,                   NumberOfBytesToReceive,           AddressOfDataStorage
  CanPollSetRx(SOUTHDOORDIO_RX_COBID,   sizeof(Inputs.SouthDoorDIO_Rx),   (INT8U*)&Inputs.SouthDoorDIO_Rx);
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

  FlexiTimer2::set(1, 1, CanPoller); // Every ms!?  Seems to work!!
  FlexiTimer2::start();

  delayUntil = millis(); // set to 'now'
}

#define STEPS_CHANGE 239
#define DEGREES_CHANGE 84.0
#define MIN_ANGLE (90.0-DEGREES_CHANGE)

int i = 0;
int dir = 1;

// loop() no longer services the CAN I/O; that is now serviced directly
// by the configured Timer2 hitting CanPoller() every ms
void loop()
{
  if (Serial.available()) {
    char key = Serial.read();
    if (key == 'd')
      CanPollDisplay(1);
    else {
      Serial.println();
      Serial.print("OK=");
      Serial.print(OK);
      Serial.print(", Fault=");
      Serial.println(Fault);
    }
  }

  if (millis() < delayUntil)
    return;

  // Draw pivoting lines as doors
  if ((i>=STEPS_CHANGE && dir>0) ||
      (i<=0 && dir<0)) {
    dir*=-1;
    delayUntil += 1500;
    return;
  }

  // Pre-calculate as much of the new angle as possible
  i+=3*dir;
  float Angle = MIN_ANGLE + ((float)i*DEGREES_CHANGE/STEPS_CHANGE); // degrees
  Angle = Angle / 180.0 * M_PI; // radians

  // erase previous door position (in black)
  myGLCD.setColor(BLACK);
  myGLCD.drawLine(lDoorX,lDoorY,LeftDoor.X,LeftDoor.Y);
  myGLCD.drawLine(rDoorX,rDoorY,RightDoor.X,RightDoor.Y);
  // draw new positions in Green
  myGLCD.setColor(GREEN);
  LeftDoor.Y = -sin(Angle)*doorLen; // rise
  LeftDoor.X = cos(Angle)*doorLen; // run
  LeftDoor.Y += lDoorY; // add in our offset, same for left and right
  RightDoor.Y = LeftDoor.Y;
  RightDoor.X = rDoorX - LeftDoor.X; // right's X is a mirror of left's
  LeftDoor.X += lDoorX;
  myGLCD.drawLine(lDoorX, lDoorY,LeftDoor.X,LeftDoor.Y);
  myGLCD.drawLine(rDoorX, rDoorY,RightDoor.X,RightDoor.Y);
  delayUntil += 130; // Increment timer relative, steady moving doors
}

