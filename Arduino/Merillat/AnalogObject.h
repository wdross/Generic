#ifndef ANALOGOBJECT_H
#define ANALOGOBJECT_H

#include <ITDB02_Graph16.h>
#include "CFwTimer.h"
extern ITDB02 myGLCD;

#define ROC_RATE 1000 // ms to recompute Rate Of Change during Open/Close cycle
#define CLOSED_TOLERANCE 45 // there is about 91 counts in a degree
#define OPENED_TOLERANCE CLOSED_TOLERANCE

// rough calculation shows 84 deg/140 sec = 0.6 deg/sec, so 22/1000 is ~0.25dg/sec
#define SLOW_TOLERANCE 22 // smaller than this counts in ROC time means 'slow'

#define ANALOG_TO_RADIANS(a) (((float)(a) / 32767.0) * 2.0 * M_PI)
#define DEGREES_TO_RADIANS(d) (((float)(d) / 180.0) * M_PI)

// GUI defines
#define DEGREES_CHANGE 84.0
#define MAX_ANGLE 90.0
#define MIN_ANGLE (MAX_ANGLE-DEGREES_CHANGE)

struct DoorOpenCloseInfo {
  // This info is needed for each door, and is stored in EEPROM
  INT16U Closed, Opened;
  INT8U  Valid;
};

class AnalogObject
{
public:
  // Address of left-justified 14 bit input
  inline AnalogObject(INT16U *address, DoorOpenCloseInfo *di, bool north, bool upper);
  inline bool IsClosed();
  inline bool IsOpen();
  inline bool IsSlow();
  inline INT16U Value();
  inline bool Open90Percent();
  inline bool Close90Percent();
  inline float Angle(); // returns the current Angle in Radians
  inline float AngleFromClosed(); // Radians from the closed position

  // for doing Rate Of Change determination
  inline void Cancel();
  inline void Reset();

private:
  INT16U *Address;
  DoorOpenCloseInfo *_DI;
  bool North; // north side gets PI subtracted from Angle()
  bool Upper; // is it the upper door (that moves 90 deg) or Winter (that moves less?)

  CFwTimer RateOfChangeTimer;
  INT16U LastPosition;
  INT16U LastMovement;
};

inline AnalogObject::AnalogObject(INT16U *address, DoorOpenCloseInfo *di, bool north, bool upper) {
  // save access information locally
  Address = address;
  _DI = di;
  North = north;
  Upper = upper;
}

inline bool AnalogObject::IsClosed() {
  if (!_DI->Valid)
    return false; // can never say for sure if we're not set up
  INT16U reading = *Address;
  if (_DI->Opened > _DI->Closed) {
    // increasing numbers for opening (Opened > Closed)
    if (reading <= _DI->Closed)
      return(true); // it is 'over closed'
  }
  else {
    // decreasing numbers for opening
    if (reading >= _DI->Closed)
      return(true); // it is 'over closed'
  }
  return(abs(reading - _DI->Closed) < CLOSED_TOLERANCE);
}

inline bool AnalogObject::IsOpen()
{
  if (!_DI->Valid)
    return false;
  INT16U reading = *Address;
  if (_DI->Opened > _DI->Closed) {
    // increasing numbers for opening (Opened > Closed)
    if (reading >= _DI->Opened)
      return(true); // it is 'over opened'
  }
  else {
    // decreasing numbers for opening
    if (reading <= _DI->Opened)
      return(true); // it is 'over opened'
  }
  return(abs(reading - _DI->Opened) < OPENED_TOLERANCE);
}

inline INT16U AnalogObject::Value() {
  return(*Address); // current reading
}

inline float AnalogObject::AngleFromClosed() {
  float Angle = M_PI/4; // invalid = 45 degrees
  if (_DI->Valid) {
    INT16U reading = *Address;
    if (_DI->Opened > _DI->Closed) {
      // increasing numbers for opening (Opened > Closed)
      if (reading > _DI->Opened)
        reading = _DI->Opened;
      else if (reading < _DI->Closed)
        reading = _DI->Closed;
      // now we know Closed <= reading <= Opened
      Angle = ((reading - _DI->Closed) / float(_DI->Opened - _DI->Closed));
    }
    else { // Closed > Opened
      // decreasing numbers for opening
      if (reading > _DI->Closed)
        reading = _DI->Closed;
      else if (reading < _DI->Opened)
        reading = _DI->Opened;
      // now we know Opened <= reading <= Closed and
      //             Closed >= reading >= Opened
      Angle = ((_DI->Closed - reading) / float(_DI->Closed - _DI->Opened));
    }
  }
  if (Upper)
    Angle *= DEGREES_TO_RADIANS(MAX_ANGLE); // scale across 90 degrees
  else
    Angle *= DEGREES_TO_RADIANS(DEGREES_CHANGE); // only 84 degrees
  return Angle;
} // AngleFromClosed()

inline float AnalogObject::Angle() {
  float Angle = AngleFromClosed();
  if (!Upper)
    Angle = Angle + DEGREES_TO_RADIANS(MIN_ANGLE);
  if (North)
    return(M_PI-Angle); // mirror angle across Y-axis
  return(Angle);
}

inline void AnalogObject::Cancel() {
  // cancels the timer, making it not looking for a Rate Of Change
  RateOfChangeTimer.SetTimer(INFINITE);
}

inline void AnalogObject::Reset() {
  RateOfChangeTimer.SetTimer(ROC_RATE);
  LastMovement = *Address;
}

inline bool AnalogObject::Open90Percent() {
  return(IsOpen());
}

inline bool AnalogObject::Close90Percent() {
  return(IsClosed());
}

inline bool AnalogObject::IsSlow() {
  // once we trace manual movement data, we'll have a pretty good idea what
  // this should look like.
  if (RateOfChangeTimer.IsTimeout()) {
    INT16U CurrentPosition = *Address;
    LastMovement = abs(CurrentPosition - LastPosition);
    LastPosition = CurrentPosition;
    RateOfChangeTimer.ResetTimer();
  }
  return(LastMovement < SLOW_TOLERANCE);
}
#endif // ANALOGOBJECT_H
