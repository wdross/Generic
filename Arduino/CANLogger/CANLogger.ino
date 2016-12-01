
#include <SPI.h>
#include "mcp_can.h"

const int SPI_CS_PIN = 9;

MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin

int CurrentCANBaud = 0;

struct CAN_Entry {
  int kBaud;
  int CAN_Konstant;
  char *rate;
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


void setup()
{
  Serial.flush();
  delay(100);
  Serial.begin(115200);
  delay(100);

  CurrentCANBaud = CAN_500KBPS;
  while (CAN_OK != CAN.begin(CurrentCANBaud))              // init can bus
  {
    Serial.println("CAN BUS Shield init fail");
    Serial.println(" Init CAN BUS Shield again");
    delay(100);
  }
  Serial.print("Setup complete for CAN baudrate of ");
  Serial.println(CANBaudLookup[CurrentCANBaud].rate);
  Serial.println();

  startUpText();
  Serial.flush();
}

void startUpText()
{
  Serial.println("--------  Arduino CAN BUS Monitor Program  ---------");
  Serial.println();
  Serial.println("To PAUSE the data: send a 'P' ");
  Serial.println();
  Serial.println("To RESUME the data: send a 'R' ");
  Serial.println();
  Serial.println("To END the data log: send a 'X' ");
  Serial.println();
  Serial.println("To change the BAUD rate: send a 'Bnnn', like B500 for 500kBaud");
  Serial.println();
  Serial.println("Paste data into the CAN_Messages_Tool.xlsm for parsing");
  Serial.println();
  Serial.println("-----------------------------------------------------");
  Serial.println();
  Serial.println(" Init CAN BUS Shield OKAY!");
  Serial.println();
/*  
  Serial.println(" Hit ENTER Key to go on the BUS.");
  Serial.println();

  while(Serial.read() != '\n' ){}
  */
}


void print_hex(int v, int num_places)
{
  int mask=0, n, num_nibbles, digit;

  for (n=1; n<=num_places; n++)
  {
    mask = (mask << 1) | 0x0001;
  }
  v = v & mask; // truncate v to specified number of places

  num_nibbles = num_places / 4;
  if ((num_places % 4) != 0)
  {
    ++num_nibbles;
  }

  do
  {
    digit = ((v >> (num_nibbles-1) * 4)) & 0x0f;
    Serial.print(digit, HEX);
  } while(--num_nibbles);
}


void loop()
{
  uint8_t len = 0;
  uint8_t buf[8];
  float time;
  int count = 1;
  signed int mySteer = 0;
  signed int mySpeed = 0;
  char serialRead;

  serialRead = Serial.read();
  if(serialRead == 'P')    // Type p in the Serial monitor bar p=pause
  {
    while(Serial.read() != 'R' ){}
  }
  else if(serialRead == 'X')    // Type p in the Serial monitor bar p=pause
  {
    Serial.end();
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

  if(CAN_MSGAVAIL == CAN.checkReceive())            // check if data coming
  {
    time = (0.000001)*micros(); // time-stamp as early as possible as float
    CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf

    unsigned int canId = CAN.getCanId();

    Serial.print(" 0    ");
    print_hex(canId, 32);
    Serial.print("         ");
    Serial.print(len);
    Serial.print("  ");
    for(int i = 0; i<8; i++)    // print the data
    {
      if(i >= len)
        Serial.print("  ");   // Print 8-bytes no matter what
      else
        print_hex(buf[i],8);
      Serial.print("  ");
    }
    Serial.print("     ");
    Serial.println(time, 6);
  }
}

