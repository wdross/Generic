// Tools->Board: Arduino Duemilanove or Diecimila, ATmega328

#include <CFwTimer.h>

int enablePin = 11;
int in1Pin = 10;
int in2Pin = 9;


#define LAUNDRY_SWITCH  PIN_A0
#define BATHROOM_SWITCH PIN_A1

#define BATHROOM_OPEN_DELAY 500
#define LAUNDRY_OPEN_DELAY BATHROOM_OPEN_DELAY


#define UNLATCH_ENABLE_OUT 4
#define UNLATCH_PWM 5
#define UNLATCH_REQUEST 12
#define LED_PIN 13 // The built in LED


#define ENCODER_A 6
#define ENCODER_B 7
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
Encoder encoder(ENCODER_A, ENCODER_B);
int32_t lastEncoder = 0;
CFwTimer lastEncoderDisplay(0);
#define ENCODER_OUTPUT_NOFASTER_THAN 1000

// We'll have the desired state of the drawer be encapsulated by the
// value of desiredDrawer: 1=Bathroom; 0=Centered/Closed; -1:Laundry
enum desiredDrawerEnum { ddBathroom=-1, ddCentered, ddLaundry } desiredDrawer = ddCentered;
desiredDrawerEnum actualDrawer = ddCentered;
desiredDrawerEnum watchingDrawer;
CFwTimer MaxTravelTimer;
enum { waitingForClosed, delayOpening, isClosed, isOpening, waitingForRelease } inputState = waitingForClosed;

bool lastBathroom;
bool bathroom;
bool lastLaundry;
bool laundry;



CFwTimer LatchTimer;
int LatchState = 0; // 0 = Latched (unpowered); 1 = UnLatched (powered)

void setup()
{
  // initialize pin modes first
  pinMode(LED_PIN, OUTPUT); 
  pinMode(in1Pin, OUTPUT);
  pinMode(in2Pin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(BATHROOM_SWITCH, INPUT_PULLUP);
  pinMode(LAUNDRY_SWITCH, INPUT_PULLUP);

  pinMode(UNLATCH_ENABLE_OUT,OUTPUT);
  pinMode(UNLATCH_REQUEST, INPUT_PULLUP);

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // seed initial values with actual readings
  lastBathroom = (digitalRead(BATHROOM_SWITCH));
  bathroom = lastBathroom;
  lastLaundry = (digitalRead(LAUNDRY_SWITCH));
  laundry = lastLaundry;
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
#define DEBOUNCE 50
  bool thisBathroom = (digitalRead(BATHROOM_SWITCH));
  static CFwTimer bathroomTimer;
  if (thisBathroom != lastBathroom) {
    bathroomTimer.SetTimer(DEBOUNCE);
  }
  lastBathroom = thisBathroom;
  if (bathroom != thisBathroom &&
      bathroomTimer.IsTimeout()) {
    bathroom = thisBathroom;
  }

  bool thisLaundry =  (digitalRead(LAUNDRY_SWITCH));
  static CFwTimer laundryTimer;
  if (thisLaundry != lastLaundry) {
    laundryTimer.SetTimer(DEBOUNCE);
  }
  lastLaundry = thisLaundry;
  if (laundry != thisLaundry &&
      laundryTimer.IsTimeout()) {
    laundry = thisLaundry;
  }

int thisGo = (desiredDrawer&0x3)    | // 0b11, 0b00, 0b01
             (actualDrawer &0x3)<<2 | // 0b11, 0b00, 0b01
             (bathroom&0x1)<<4      | // 0 or 1
             (laundry&0x1)<<5       | // 0 or 1
             inputState<<6;
static int lastGo = 999;
if (thisGo != lastGo) {
  Serial.print(desiredDrawer);
  Serial.print(" ");
  Serial.print(actualDrawer);
  Serial.print(" ");
  Serial.print(inputState);
  Serial.print(" ");
  Serial.print(bathroom);
  Serial.print(",");
  Serial.print(laundry);
  Serial.print("  ");
  Serial.print("\r"); // put on same line
  lastGo = thisGo;
}
// desiredDrawer: ddBathroom=-1, ddCentered, ddLaundry
// inputState: waitingForClosed, delayOpening, isClosed, isOpening, waitingForRelease

  switch (desiredDrawer) {
    case ddCentered:
      // drawer is centered.  Make sure inputs return to both cupboards closed
      // before deciding which way to open.
      switch (inputState) {
          break;
        case waitingForClosed:
          if (!bathroom && !laundry)
            inputState = isClosed;
          break;
        case isClosed:
          if (bathroom) {
            watchingDrawer = ddBathroom;
            inputState = delayOpening;
            MaxTravelTimer.SetTimer(BATHROOM_OPEN_DELAY);
          }
          else if (laundry) {
            watchingDrawer = ddLaundry;
            inputState = delayOpening;
            MaxTravelTimer.SetTimer(LAUNDRY_OPEN_DELAY);
          }
          break;
        case delayOpening:
          switch (watchingDrawer) {
            case ddBathroom:
              // switch was released, give some time for the door to get far enough
              // out of the way to start moving the drawer.  If the input state reverts,
              // we should abort and go back to asking/being Closed
              if (!bathroom) {
                inputState = waitingForClosed;
                break;
              }
              if (MaxTravelTimer.IsTimeout()) {
                inputState = isOpening;
                desiredDrawer = ddBathroom;
              }
              break;
            case ddLaundry:
              if (!laundry) {
                inputState = waitingForClosed;
                break;
              }
              if (MaxTravelTimer.IsTimeout()) {
                inputState = isOpening;
                desiredDrawer = ddLaundry;
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
          // switch was released (1) in order to get into this state
          // so now we wait until someone pushes just the switch (can't have the
          // cupboard door do that because the drawer is in the way!).  When
          // they let go, that's our clue to retract the door.
          if (!bathroom) {
            inputState = waitingForRelease;
            // user is pushing the button, now let's wait until they let go
          }
          break;
        case waitingForRelease:
          if (bathroom) {
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
          // switch was released (1) in order to get into this state
          // so now we wait until someone pushes just the switch (can't have the
          // cupboard door do that because the drawer is in the way!).  When
          // they let go, that's our clue to retract the door.
          if (!laundry) {
            inputState = waitingForRelease;
            // user is pushing the button, now let's wait until they let go
          }
          break;
        case waitingForRelease:
          if (laundry) {
            // user has let go, so let's get out of the way
            desiredDrawer = ddCentered;
          }
      } // inputState of ddLaundry
      break;
  }


  int32_t newEncoder = encoder.read();
  // now that we know what the switches want us to do, let's actually get
  // that drawer moved to make actualDrawer match desiredDrawer
static desiredDrawerEnum desiredDrawerLatch;
  if (desiredDrawer != actualDrawer) {
    // we have to move to fix this scenario
    // We need 19.5" of travel in either positive or negative direction,
    // depending upon switch activation.  With our 22 tooth pulley (exactly 4.4" diameter),
    // we need 19.5/4.4 = 4.43 revolutions on our 8192 cpr encoder, yielding a movement of 36305 counts
#define IDLER_PULLEY_TOOTH_COUNT 22.0
// Our belt has 5 teeth for every inch, so 0.2"/tooth
#define IDLER_PULLEY_CIRCUMFERENCE (0.2*IDLER_PULLEY_TOOTH_COUNT) // 4.4 inches 
#define CPI (8192.0/IDLER_PULLEY_CIRCUMFERENCE) // 1861.8 counts in an inch of movement
#define DESIRED_MOVEMENT (19.5*CPI) // 36305
#define COUNTS_TO_INCHES(counts) (counts/CPI)
    if (desiredDrawer != desiredDrawerLatch) { // 1 scan edge of change
      MaxTravelTimer.SetTimer(5000);
    }
    desiredDrawerLatch = desiredDrawer;

    // we'll get the direction from where we are to where we want to be.
    // speed ends up being -1, +1, 0 (maybe +2 or -2, but state machine should prevent)
    speed = 255*(desiredDrawer-actualDrawer);

    if (MaxTravelTimer.IsTimeout()) {
      Serial.println("\n\nTimeout"); 
      actualDrawer = desiredDrawer;
      speed = 0;
    }
    if (desiredDrawer != ddCentered) {
      if (abs(newEncoder) >= DESIRED_MOVEMENT) {
        Serial.println("\n\rOutward travel complete"); 
        actualDrawer = desiredDrawer;
        speed = 0;
      }
    }
    // when returning to center, accept a band in the middle or crossing zero as complete
    if (desiredDrawer == ddCentered && (abs(newEncoder) < 1000 || (newEncoder<0) != (actualDrawer<0))) {
      Serial.println("\n\rCentered"); 
      actualDrawer = desiredDrawer;
      speed = 0;
    }
    if (speed) {
//      // still want to move, calculate velocity based upon range
//      // compute remaining distance and set velocity based on range to target
//      // range will be positive when matching desired direction of travel, negative on overshoot
//      int32_t range = newEncoder - desiredDrawer * DESIRED_MOVEMENT;
//#define MAX_SPEED 255 // largest PWM rate out of analogWrite()
//#define RAMP_LENGTH (2*CPI) // 2" of counts = 3724
//#define RAMP_MIN (100)
//      // if range is over RAMP_LENGTH, use max speed
//      int velocity;
//      if (abs(range)>RAMP_LENGTH) {
//        velocity = MAX_SPEED*range/abs(range);
//        digitalWrite(LED_PIN, LOW);
//      }
//      else {
//        // below RAMP_LENGTH, taper down to RAMP_MIN
//        velocity = range/RAMP_LENGTH * (MAX_SPEED - RAMP_MIN) + RAMP_MIN;
//        digitalWrite(LED_PIN, HIGH);
//      }
//      speed *= velocity; // if we overshoot, there could be a sign change as velocity will be negative
    }
  } // desiredDrawer != actualDrawer
  else
    speed = 0;
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

