#include "WiiChuck.h"
#include "CFwTimer.h"

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

int u_d_latch = -1;
int old_u_d_latch = -1;
#define DEBOUNCE 150
#define RAISE_TIMER 2500
CFwTimer u_d_timer;
CFwTimer autoRaiseTimer;
bool floatDown = false;
bool autoRaise = false;

void loop() {
  delay(20);
  chuck.update();

  bool pump = false;
  if (chuck.buttonZ && !chuck.buttonC) {
    // Put Plow down because Z is the only pressed button
    if (u_d_latch == 1 && u_d_timer.GetExpiredBy() > DEBOUNCE) {
      // pressed and held for DEBOUNCE time
      digitalWrite(PLOW_DOWN, ON);
      digitalWrite(PLOW_UP, OFF);
      floatDown = false;
    }
    else if (old_u_d_latch == 1 && u_d_timer.GetExpiredBy() < DEBOUNCE*2) {
      floatDown = true;
    }
    else if (u_d_latch != 1) {
      // first time we've been through here
      if (floatDown)
        u_d_timer.SetTimer(-DEBOUNCE);
      else {
        u_d_timer.SetTimer(0);
        digitalWrite(PLOW_UP, OFF);
      }
      u_d_latch = 1;
      old_u_d_latch = -1;
      autoRaise = false;
    }
  }
  else if (!chuck.buttonZ && chuck.buttonC) {
    // raise plow, the C button is the only pressed button
    if (u_d_latch == 2 && u_d_timer.GetExpiredBy() > DEBOUNCE) {
      digitalWrite(PLOW_DOWN, OFF);
      digitalWrite(PLOW_UP, ON);
      pump = true;
      autoRaise = false;
    }
    else if (old_u_d_latch == 2 && u_d_timer.GetExpiredBy() < DEBOUNCE*2) {
      autoRaise = true;
    }
    else if (u_d_latch != 2) {
      // first time we've been thru here
      if (autoRaise) {
        u_d_timer.SetTimer(-DEBOUNCE);
        pump = true;
      }
      else
        u_d_timer.SetTimer(0);
      u_d_latch = 2;
      old_u_d_latch = -1;
      floatDown = false;
    }
  }
  else {
    // neither raise nor lower
    if (u_d_latch != -1) {
      autoRaiseTimer.SetTimer(RAISE_TIMER);
      old_u_d_latch = u_d_latch;
    }
    if (autoRaise && autoRaiseTimer.IsTimeout()) {
      autoRaise = false;
    }
    digitalWrite(PLOW_DOWN, floatDown?ON:OFF);
    digitalWrite(PLOW_UP, autoRaise?ON:OFF);
    pump = autoRaise;
    u_d_latch = -1;
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
