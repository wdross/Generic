// Tools->Board: Arduino Duemilanove or Diecimila, ATmega328

#include <CFwTimer.h>
#include <CFwDebouncedDigitalInput.h>

int enablePin = 11;
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

#define ENCODER_A           6
#define ENCODER_B           7

// Upper Drawer
#define UNLATCH_REQUEST     12


#define BATHROOM_OPEN_DELAY 750
#define LAUNDRY_OPEN_DELAY BATHROOM_OPEN_DELAY


// I/O that go to other components on our breadboard
#define UNLATCH_ENABLE_OUT 4
#define UNLATCH_PWM 5
#define LED_PIN 13 // The built in LED


#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
Encoder encoder(ENCODER_A, ENCODER_B);
int32_t lastEncoder = 0;
CFwTimer lastEncoderDisplay(0);
#define ENCODER_OUTPUT_NOFASTER_THAN 1000
bool encoderWorking = false; // by default, assume that darn encoder doesn't work

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


// We need 19.5" of travel in either positive or negative direction,
// depending upon switch activation.  With our 22 tooth pulley (exactly 4.4" circumference),
// we need 19.5/4.4 = 4.43 revolutions on our 8192 cpr encoder, yielding a movement of 36305 counts
#define IDLER_PULLEY_TOOTH_COUNT 22.0
// Our belt has 5 teeth for every inch, so 0.2"/tooth
#define IDLER_PULLEY_CIRCUMFERENCE (0.2*IDLER_PULLEY_TOOTH_COUNT) // 4.4 inches
#define CPI (8192.0/IDLER_PULLEY_CIRCUMFERENCE) // 1861.8 counts in an inch of movement
#define DESIRED_MOVEMENT (19.5*CPI) // 36305
#define COUNTS_TO_INCHES(counts) (counts/CPI)




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
    if (input == 'r') { // reset
      encoder.write(0);
    }
    else if (input == 's') { // show
      lastEncoderDisplay.SetTimer(0); // immediate timeout
      lastEncoder =  encoder.read() + 2000; // force large difference
    }
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

  // the limit switches below the drawer will roll on the bottom of the drawer.
  // when the drawer is centered, both switches will be pressed (0's)
  // as the drawer opens in a direction (i.e. ddBathroom), the bathroomDrawerLimitSwitch
  // will be seen as going from a 0 to a 1 just a fraction of an inch before the end of travel
  // likewise going into the ddLaundry direction (laundryDrawerLimitSwitch)
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

  int32_t newEncoder = encoder.read();

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
    
    // we learned that a constant update of a digital output (our status LED for one)
    // seems to cause a miscount of the encoder.read().  Might be attributed to accessing
    // the port for an output bit doesn't correctly handle adjacent bits that are inputs.

    // now that we know what the switches want us to do, let's actually get
    // that drawer moved to make actualDrawer match desiredDrawer
    static desiredDrawerEnum desiredDrawerLatch;
    // we have to move to fix this scenario
    if (desiredDrawer != desiredDrawerLatch) { // 1 scan edge of change
      MaxTravelTimer.SetTimer(4000);
    }
    desiredDrawerLatch = desiredDrawer;

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
    if (desiredDrawer != ddCentered) {
      if (abs(newEncoder) >= DESIRED_MOVEMENT) {
        Serial.println("\n\rOutward travel complete"); 
        actualDrawer = desiredDrawer;
        speed = 0;
        encoderWorking = true;
      }
    }
    // when returning to center, accept a band in the middle or crossing zero as complete
    if (encoderWorking && desiredDrawer == ddCentered && (abs(newEncoder) < 1000 || (newEncoder<0) != (actualDrawer<0))) {
      Serial.println("\n\rCentered"); 
      actualDrawer = desiredDrawer;
      speed = 0;
    }
    if (speed) {
      // still want to move, calculate velocity based upon range
      // compute remaining distance and set velocity based on range to target
      // range will be positive when matching desired direction of travel, negative on overshoot
      int32_t range = newEncoder - desiredDrawer * DESIRED_MOVEMENT;
#define MAX_SPEED 255 // largest PWM rate out of analogWrite()
#define RAMP_LENGTH (5*CPI) // 2" of counts = 3724
#define RAMP_MIN (100)
      // if range is over RAMP_LENGTH, use max speed
      int velocity = MAX_SPEED;
//      if (abs(range)<RAMP_LENGTH) {
//        // below RAMP_LENGTH, taper down to RAMP_MIN
//        // formula that works in Excel:
//        // =-(D5/RAMP_LENGTH*(MAX_SPEED-RAMP_MIN)-RAMP_MIN*B5)
//        BUT also need to gate on encoderWorking
//        velocity = -(range/RAMP_LENGTH * (MAX_SPEED - RAMP_MIN) - RAMP_MIN*speed);
//      }
      speed *= velocity; // if we overshoot, there could be a sign change as velocity will be negative
    }
    if (desiredDrawer == actualDrawer) {
      // just became correct state
      IgnoreSwitches.SetTimer(1250);
    }
  } // desiredDrawer != actualDrawer

  if (speed && MaxTravelTimer.IsTimeout()) {
    Serial.println("\n\nTimeout");
    actualDrawer = desiredDrawer; // prevents state machine from starting again
    speed = 0;
  }
#endif // !JUST_IN_OUT_SWITCHES

  setMotor(abs(speed), speed<0);


  
  int32_t diff = newEncoder - lastEncoder;
  if (abs(diff) > 100 &&
      lastEncoderDisplay.IsTimeout()) {
    Serial.print("\n\rInch = "); // don't cover up our other diagnostic
    Serial.print(COUNTS_TO_INCHES(newEncoder),2);
    Serial.print("  diff=");
    Serial.print(COUNTS_TO_INCHES(diff),2);
    lastEncoder = newEncoder;
    lastEncoderDisplay.SetTimer(ENCODER_OUTPUT_NOFASTER_THAN);
  }

  // tie the desire to UNLATCH (UNLATCH_REQUEST) to the PWM enable output
  // The multiply by the digitalRead() forces a zero request
  int unlatch = !digitalRead(UNLATCH_REQUEST); // 0 or 1
  static int unlatch_LAST = 0;
  if (unlatch != unlatch_LAST &&
      unlatch)
    LatchState = 1; // rising edge, request unlatch
  unlatch_LAST = unlatch;


  
  // Solinoid needs 9-12v, but emperical study sees we need to ask the motor
  // controller for at least 17v (180) in order to move the latch.  We back
  // off after that, to 12v
  static int _Last_LatchState = 0;
  if (LatchState) {
    if (!_Last_LatchState) {
      // rising edge of an unlatch
      LatchTimer.SetTimer(100); // higher voltage for this amount of ms
    }
    if (LatchTimer.GetExpiredBy() > 3000)
      LatchState = 0;
  }
  _Last_LatchState = LatchState;
//  digitalWrite(LED_PIN, LatchState);
  int ana = (LatchTimer.IsTiming()?190:127) * LatchState; // 180=17v for until Timer expired, then 127=12v
  analogWrite(UNLATCH_PWM,ana);
  digitalWrite(UNLATCH_ENABLE_OUT,LatchState?HIGH:LOW); // opposite state
}

