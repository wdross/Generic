
// Tools->Board: Arduino Duemilanove or Diecimila, ATmega328

#include <CFwTimer.h>
#include <CFwDebouncedDigitalInput.h>

int enablePin = 11; // PWM output for controlling voltage
int in1Pin = 10;
int in2Pin = 9;

// a test mechanism: when defined, JUST_IN_OUT_SWITCHES will activate full speed request in the direction
// of switch activation.  Just so we can see how the motor works the first time it is installed.
#undef JUST_IN_OUT_SWITCHES

// These I/O item run to actual switches or outputs
#define LAUNDRY_SWITCH      PIN_A0
#define BATHROOM_SWITCH     PIN_A1
#define FULLY_OPEN_LAUNDRY  PIN_A2
#define FULLY_OPEN_BATHROOM PIN_A3
#define CENTERED_DRAWER     PIN_A4

// Upper Drawer
#define UNLATCH_REQUEST     12 // this is the easily accessible button
#define REPROGRAM_REQUEST    6  // this is the in-drawer button
#define REPROGRAM_ACKNOWLEDGE 7 // this is the in-drawer indicator of reprogramming active


#define BATHROOM_OPEN_DELAY 850
#define LAUNDRY_OPEN_DELAY BATHROOM_OPEN_DELAY
#define MAXIMUM_TRAVEL_TIME 4800


// I/O that go to other components on our breadboard
#define UNLATCH_ENABLE_OUT 4
#define UNLATCH_PWM 5
#define LED_PIN 13 // The built in LED


// We'll have the desired state of the drawer be encapsulated by the
// value of desiredDrawer: 1=Bathroom; 0=Centered/Closed; -1:Laundry
enum desiredDrawerEnum { ddBathroom=-1, ddCentered, ddLaundry } desiredDrawer = ddCentered;
desiredDrawerEnum actualDrawer = ddCentered;
desiredDrawerEnum watchingDrawer;
CFwTimer MaxTravelTimer;
CFwTimer WaitBeforeMoveTimer;
CFwTimer IgnoreSwitches;
bool laundryEverReleased;
enum { waitingForClosed, delayOpening, isClosed, isOpening, waitingForPress, waitingForRelease } inputState = waitingForClosed;


CFwDebouncedDigitalInput* bathroom;
CFwDebouncedDigitalInput* laundry;
CFwDebouncedDigitalInput* bathroomDrawerLimitSwitch;
CFwDebouncedDigitalInput* laundryDrawerLimitSwitch;
CFwDebouncedDigitalInput* centeredDrawerLimitSwitch;



CFwTimer LatchTimer;
int LatchState = 0; // 0 = Latched (unpowered); 1 = UnLatched (powered)

void setup()
{
  // initialize pin modes first
  pinMode(LED_PIN, OUTPUT);
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(enablePin, OUTPUT);

  pinMode(UNLATCH_ENABLE_OUT,OUTPUT);
  pinMode(UNLATCH_REQUEST, INPUT_PULLUP);

  pinMode(REPROGRAM_REQUEST,INPUT_PULLUP);
  pinMode(REPROGRAM_ACKNOWLEDGE,OUTPUT);

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // set up input and seed initial values with actual readings
  bathroom = new CFwDebouncedDigitalInput(BATHROOM_SWITCH);
  laundry  = new CFwDebouncedDigitalInput(LAUNDRY_SWITCH);
  bathroomDrawerLimitSwitch = new CFwDebouncedDigitalInput(FULLY_OPEN_BATHROOM);
  laundryDrawerLimitSwitch  = new CFwDebouncedDigitalInput(FULLY_OPEN_LAUNDRY);
  centeredDrawerLimitSwitch = new CFwDebouncedDigitalInput(CENTERED_DRAWER);
  IgnoreSwitches.SetTimer(0); // watch right away
}

int  speed = 0;

void setMotor(int speed, boolean reverse)
{
  if (speed < 0)
    speed = 0;
  else if (speed > 255)
    speed = 255;
  analogWrite(enablePin, speed);
  digitalWrite(in1Pin, ! reverse);
  digitalWrite(in2Pin, reverse);
}

void loop()
{
  if (Serial.available()) {
    // This will become the center switch detection
    char input = Serial.read();
  }

  // we'll build this input state machine upon the following premise:
  // bathroom: 0 when cupboard door is closed (switch depressed)
  //           1 when cupboard door is open   (switch released)
  // laundry:  0 when cupboard door is closed (switch depressed)
  //           1 when cupboard door is open   (switch released)
  // we also debounce the switch inputs, assuming a continual
  // reading in a single state will be enough to stabilize.

  bathroom->Process();
  laundry->Process();

  // The three limit switches below the drawer will roll on the bottom of the drawer.
  // Centered:
  // - When the drawer is centered, we'll see !centeredDrawerLimitSwitch->GetState()
  // - This switch triggers on the laundry room edge of the drawer (plus a small block
  //   that yields ~1.25" of travel) that will close the limit switch
  // Bathroom:
  // - As the drawer opens in a direction (i.e. ddBathroom), the bathroomDrawerLimitSwitch
  //   will be seen as going from a 0 (pushed) to a 1 (released) just a fraction of an inch
  //   before the end of travel (as the drawer is pushed far enough to let the button up)
  // - This limit switch rolls on the full edge of the drawer, so the switch is pressed
  //   until just before the end of travel.
  // - As the drawer opens the opposite direction (Laundry) this switch is also released
  //   but is not used for any state machine decisions
  // Laundry:
  // - Likewise going into the ddLaundry direction (laundryDrawerLimitSwitch)
  // - This switch runs on the opposide drawer edge and releases just prior to fully extended
  //   to the Laundry room
  // - This switch is also relelased as the drawer moves toward the Bathroom but is not used
  //   for any state machine decisions
  //
  bathroomDrawerLimitSwitch->Process();
  laundryDrawerLimitSwitch->Process();
  centeredDrawerLimitSwitch->Process();


  int thisGo = (desiredDrawer&0x3)    | // 0b11, 0b00, 0b01
               (actualDrawer &0x3)<<2 | // 0b11, 0b00, 0b01
               (bathroom->GetState()&0x1)<<4      | // 0 or 1
               (laundry->GetState()&0x1)<<5       | // 0 or 1
               (bathroomDrawerLimitSwitch->GetState()&0x1)<<6      | // 0 or 1
               (laundryDrawerLimitSwitch->GetState()&0x1)<<7       | // 0 or 1
               (centeredDrawerLimitSwitch->GetState()&0x1)<<8       | // 0 or 1
               inputState<<9;
  static int lastGo = 999;
  if (thisGo != lastGo) {
    Serial.print(desiredDrawer);
    Serial.print(" ");
    Serial.print(actualDrawer);
    Serial.print(" ");
    Serial.print(inputState);
    Serial.print(" ");
    Serial.print(bathroom->GetState());
    Serial.print(",");
    Serial.print(laundry->GetState());
    Serial.print(",");
    Serial.print(bathroomDrawerLimitSwitch->GetState());
    Serial.print(",");
    Serial.print(laundryDrawerLimitSwitch->GetState());
    Serial.print(",");
    Serial.print(centeredDrawerLimitSwitch->GetState());
    Serial.print("  ");
#if !defined(JUST_IN_OUT_SWITCHES)
    Serial.println(); // put on same line
#endif
    lastGo = thisGo;
  }

#if defined(JUST_IN_OUT_SWITCHES)
  if (!bathroom->GetState())
    speed = -255;
  else if (!laundry->GetState())
    speed = 255;
  else
    speed = 0;
  static int lastSpeed = -999;
  if (speed != lastSpeed) {
    Serial.println(speed);
    lastSpeed = speed;
  }
#else

// desiredDrawer: ddBathroom=-1, ddCentered, ddLaundry
// inputState: waitingForClosed, delayOpening, isClosed, isOpening, waitingForPress, waitingForRelease

  if (desiredDrawer == actualDrawer) {
    // only watch for changes if stable
    if (IgnoreSwitches.IsTimeout()) {
      switch (desiredDrawer) {
        case ddCentered:
          // drawer is centered.  Make sure inputs return to both cupboards closed
          // before deciding which way to open.
          switch (inputState) {
             case waitingForClosed:
              // want to see both doors closed and the drawer physically centered
              if (!bathroom->GetState() && !laundry->GetState() && !centeredDrawerLimitSwitch->GetState())
                inputState = isClosed;
              break;
            case isClosed:
              if (bathroom->GetState()) {
                watchingDrawer = ddBathroom;
                inputState = delayOpening;
                WaitBeforeMoveTimer.SetTimer(BATHROOM_OPEN_DELAY);
              }
              else if (laundry->GetState()) {
                watchingDrawer = ddLaundry;
                inputState = delayOpening;
                WaitBeforeMoveTimer.SetTimer(LAUNDRY_OPEN_DELAY);
              }
              break;
            case delayOpening:
              switch (watchingDrawer) {
                case ddBathroom:
                  // switch was released, give some time for the door to get far enough
                  // out of the way to start moving the drawer.  If the input state reverts,
                  // we should abort and go back to asking/being Closed
                  if (!bathroom->GetState()) {
                    inputState = waitingForClosed;
                    break;
                  }
                  if (WaitBeforeMoveTimer.IsTimeout()) {
                    inputState = isOpening;
                    desiredDrawer = ddBathroom;
                  }
                  break;
                case ddLaundry:
                  if (!laundry->GetState()) {
                    inputState = waitingForClosed;
                    break;
                  }
                  if (WaitBeforeMoveTimer.IsTimeout()) {
                    inputState = isOpening;
                    desiredDrawer = ddLaundry;
                    laundryEverReleased = false;
                  }
                  break;
              } // watchingSide
              break;

            default:
              inputState = waitingForClosed;
          } // inputState while Centered
          break;

        case ddBathroom:
          // drawer is extending/moving into the bathroom, or already fully extended into the bathroom
          switch (inputState) {
            // from ddCentered, inputState starts as delayOpening and the MaxTravelTimer is running
            case isOpening:
              // switch might have been pressed to stop the drawer, make sure released
              if (bathroom->GetState()) {
                inputState = waitingForPress;
                // user is pushing the button, now let's wait until they let go
              }
              break;
            case waitingForPress:
              // made sure switch was released (1) in order to get into this state
              // so now we wait until someone pushes just the switch (can't have the
              // cupboard door do that because the drawer is in the way!).  When
              // they let go, that's our clue to retract the door.
              if (!bathroom->GetState()) {
                inputState = waitingForRelease;
                // user is pushing the button, now let's wait until they let go
              }
              break;
            case waitingForRelease:
              if (bathroom->GetState()) {
                // user has let go, so let's get out of the way
                desiredDrawer = ddCentered;
              }
          } // inputState of ddBathroom
          break;

        case ddLaundry:
          // drawer is extending/moving into the laundry, or already fully extended into the laundry
          switch (inputState) {
            // from ddCentered, inputState starts as isOpening
            case isOpening:
              // switch might have been pressed to stop the drawer, make sure released
              if (laundry->GetState()) {
                inputState = waitingForPress;
                // user is pushing the button, now let's wait until they let go
              }
              break;
            case waitingForPress:
              // make sure switch was released (1) in order to get into this state
              // so now we wait until someone pushes just the switch (can't have the
              // cupboard door do that because the drawer is in the way!).  When
              // they let go, that's our clue to retract the door.
              if (!laundry->GetState()) {
                inputState = waitingForRelease;
                // user is pushing the button, now let's wait until they let go
              }
              break;
            case waitingForRelease:
              if (laundry->GetState()) {
                // user has let go, so let's get out of the way
                desiredDrawer = ddCentered;
              }
          } // inputState of ddLaundry
          break;
      }
      IgnoreSwitches.SetTimer(0); // continually reset timer for cleanest operation
    }
    speed = 0;
  }
  else { // desiredDrawer != actualDrawer

    // now that we know what the switches want us to do, let's actually get
    // that drawer moved to make actualDrawer match desiredDrawer
    static desiredDrawerEnum desiredDrawerLatch;
    // we have to move to fix this scenario
    if (desiredDrawer != desiredDrawerLatch) { // 1 scan edge of change
      MaxTravelTimer.SetTimer(MAXIMUM_TRAVEL_TIME);
      desiredDrawerLatch = desiredDrawer;
    }

    // we'll get the direction from where we are to where we want to be.
    // speed ends up being -1, +1, 0 (maybe +2 or -2, but state machine should prevent)
    speed = (desiredDrawer-actualDrawer);

    if (desiredDrawer == ddBathroom &&
        (bathroomDrawerLimitSwitch->GetState() ||
         !bathroom->GetState())) {
      // we were starting from center, so we are watching to see the switch hit end of travel
      // or if the user presses the switch again, a signal to stop coming towards us!
      Serial.println("\n\rBathroom limit, complete");
      actualDrawer = desiredDrawer;
      speed = 0;
    }
    else if (desiredDrawer == ddLaundry) {
      if (laundryDrawerLimitSwitch->GetState()) {
        // we were starting from center, so we are watching to see the switch hit end of travel
        Serial.println("\n\rLaundry limit, complete");
        actualDrawer = desiredDrawer;
        speed = 0;
      }
      else if (!laundryEverReleased) {
        if (!laundry->GetState())
          laundryEverReleased = true;
      }
      else if (laundryEverReleased &&
                laundry->GetState()) {
        // or if the user presses the switch again, a signal to stop coming towards us!
        Serial.println("\n\rUser limit, complete");
        actualDrawer = desiredDrawer;
        speed = 0;
      }
    } // ddLaundry
    else if (desiredDrawer == ddCentered &&
             !centeredDrawerLimitSwitch->GetState()) {
      // moving towards center, limit switch is enough
      Serial.println("\n\rCenter limit, complete");
      actualDrawer = desiredDrawer;
      speed = 0;
    }
    if (speed) {
      // still want to move, calculate velocity based upon range
#define MAX_SPEED 255 // largest PWM rate out of analogWrite()
      int velocity = MAX_SPEED;
      speed *= velocity; // if we overshoot, there could be a sign change as velocity will be negative
    }
    if (desiredDrawer == actualDrawer) {
      // just became correct state
      IgnoreSwitches.SetTimer(1250);
      Serial.print("with ");
      Serial.print(MaxTravelTimer.GetRemaining());
      Serial.println("ms to spare");
    }
  } // desiredDrawer != actualDrawer

  if (speed && MaxTravelTimer.IsTimeout()) {
    Serial.println("\n\nTimeout");
    actualDrawer = desiredDrawer; // prevents state machine from starting again
    speed = 0;
  }
#endif // !JUST_IN_OUT_SWITCHES

  setMotor(abs(speed), speed<0);


  // tie the desire to UNLATCH (UNLATCH_REQUEST) to the PWM enable output
  // The multiply by the digitalRead() forces a zero request
  int unlatch = !digitalRead(UNLATCH_REQUEST); // 0 or 1
  static int unlatch_LAST = 0;
  if (unlatch != unlatch_LAST &&
      unlatch) {
    LatchState = 1; // rising edge, request unlatch
    Serial.println("Unlatch requested");
  }
  unlatch_LAST = unlatch;



  // Solinoid needs 9-12v, but emperical study sees we need to ask the motor
  // controller for at least 17v (180) in order to move the latch.  We back
  // off after that, to 12v
  static int _Last_LatchState = 0;
  if (LatchState) {
    if (!_Last_LatchState) {
      // rising edge of an unlatch
      LatchTimer.SetTimer(150); // higher voltage for this amount of ms
    }
    if (LatchTimer.GetExpiredBy() > 2000) {
      LatchState = 0;
      Serial.println("Unlatch timer expired");
    }
  }
  _Last_LatchState = LatchState;
  int ana = (LatchTimer.IsTiming()?200:127) * LatchState; // 180=17v for until Timer expired, then 127=12v
  analogWrite(UNLATCH_PWM,ana);
  digitalWrite(UNLATCH_ENABLE_OUT,LatchState?HIGH:LOW); // opposite state

  // testing the Reprogramming I/O
  digitalWrite(REPROGRAM_ACKNOWLEDGE,!digitalRead(REPROGRAM_REQUEST));
}
