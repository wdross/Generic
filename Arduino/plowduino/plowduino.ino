#include "WiiChuck.h"

WiiChuck chuck = WiiChuck();

#define PLOW_DOWN 3
#define PLOW_UP   4
#define RIGHT_OUT 5
#define RIGHT_IN  6
#define LEFT_OUT  7
#define LEFT_IN   8
#define PUMP      9

#define OFF HIGH
#define ON  LOW

#define JOY_ON 90

void setup() {
  Serial.begin(115200);
  chuck.begin();
  chuck.update();

  pinMode(PLOW_DOWN, OUTPUT);
  pinMode(PLOW_UP, OUTPUT);
  pinMode(RIGHT_OUT, OUTPUT);
  pinMode(RIGHT_IN, OUTPUT);
  pinMode(LEFT_OUT, OUTPUT);
  pinMode(LEFT_IN, OUTPUT);
  pinMode(PUMP, OUTPUT);

  digitalWrite(PLOW_DOWN, OFF);
  digitalWrite(PLOW_UP, OFF);
  digitalWrite(RIGHT_OUT, OFF);
  digitalWrite(RIGHT_IN, OFF);
  digitalWrite(LEFT_OUT, OFF);
  digitalWrite(LEFT_IN, OFF);
  digitalWrite(PUMP, OFF);
}

void loop() {
  delay(20);
  chuck.update(); 

  Serial.print(chuck.readJoyX());
    Serial.print(", ");  
  Serial.print(chuck.readJoyY());
    Serial.print(", ");  

  if (chuck.buttonZ) {
     Serial.print("Z");
  } else  {
     Serial.print("-");
  }

  Serial.print(", ");  

  if (chuck.buttonC) {
     Serial.print("C");
  } else  {
     Serial.print("-");
  }

  Serial.println();

  bool pump = false;
  if (chuck.buttonZ && !chuck.buttonC) {
    // Put Plow down because Z is the only pressed button
    digitalWrite(PLOW_DOWN, ON);
    digitalWrite(PLOW_UP, OFF);
  }
  else if (!chuck.buttonZ && chuck.buttonC) {
    // raise plow, the C button is the only pressed button
    digitalWrite(PLOW_DOWN, OFF);
    digitalWrite(PLOW_UP, ON);
    pump = true;
  }
  else {
    // neither raise nor lower
    digitalWrite(PLOW_DOWN, OFF);
    digitalWrite(PLOW_UP, OFF);
  }

  if (chuck.readJoyX() < -JOY_ON && chuck.readJoyY() > JOY_ON) {
    // left wing out since joystick is in upper left pos
    digitalWrite(RIGHT_OUT, OFF);
    digitalWrite(RIGHT_IN, OFF);
    digitalWrite(LEFT_OUT, ON);
    digitalWrite(LEFT_IN, OFF);
    pump = true;
  }
  else if (chuck.readJoyX() > JOY_ON && chuck.readJoyY() > JOY_ON) {
    // right wing out since joystick is in upper right pos
    digitalWrite(RIGHT_OUT, ON);
    digitalWrite(RIGHT_IN, OFF);
    digitalWrite(LEFT_OUT, OFF);
    digitalWrite(LEFT_IN, OFF);
    pump = true;
  }
  else if (chuck.readJoyX() > JOY_ON && chuck.readJoyY() < -JOY_ON) {
    // right wing in since joystick is in lower right pos
    digitalWrite(RIGHT_OUT, OFF);
    digitalWrite(RIGHT_IN, ON);
    digitalWrite(LEFT_OUT, OFF);
    digitalWrite(LEFT_IN, OFF);
  }
  else if (chuck.readJoyX() < -JOY_ON && chuck.readJoyY() < -JOY_ON) {
    // left wing in since joystick is in lower left pos
    digitalWrite(RIGHT_OUT, OFF);
    digitalWrite(RIGHT_IN, OFF);
    digitalWrite(LEFT_OUT, OFF);
    digitalWrite(LEFT_IN, ON);
  }
  else if (chuck.readJoyY() > JOY_ON) {
    // scoop since joystick is straight up
    digitalWrite(RIGHT_OUT, ON);
    digitalWrite(RIGHT_IN, OFF);
    digitalWrite(LEFT_OUT, ON);
    digitalWrite(LEFT_IN, OFF);
    pump = true;
  }
  else if (chuck.readJoyY() < -JOY_ON) {
    // vee since joystick is straight down
    digitalWrite(RIGHT_OUT, OFF);
    digitalWrite(RIGHT_IN, ON);
    digitalWrite(LEFT_OUT, OFF);
    digitalWrite(LEFT_IN, ON);
  }
  else if (chuck.readJoyX() < -JOY_ON) {
    // angle left because joystick is straight left
    digitalWrite(RIGHT_OUT, ON);
    digitalWrite(RIGHT_IN, OFF);
    digitalWrite(LEFT_OUT, OFF);
    digitalWrite(LEFT_IN, ON);
    pump = true;
  }
  else if (chuck.readJoyX() > JOY_ON) {
    // angle right because joystick is straight right
    digitalWrite(RIGHT_OUT, OFF);
    digitalWrite(RIGHT_IN, ON);
    digitalWrite(LEFT_OUT, ON);
    digitalWrite(LEFT_IN, OFF);
    pump = true;
  }
  else {
    digitalWrite(RIGHT_OUT, OFF);
    digitalWrite(RIGHT_IN, OFF);
    digitalWrite(LEFT_OUT, OFF);
    digitalWrite(LEFT_IN, OFF);
  }

  if (pump)
    digitalWrite(PUMP,ON);
  else
    digitalWrite(PUMP,OFF);
}
