//:---------------------------------------------------------------------------
//:Description
//
// The CFwTimer class is a support base class for classes that measure elapsed time
// or simple timing support. Its static method needs the system's time in milliseconds.
//
//:---------------------------------------------------------------------------

//:ClassInc
#include "CFwTimer.h"

//:Include
#include <Arduino.h>

//:Special
#define _ThisClass    CFwTimer
#define _NumBaseClass  0

//:>                  +-----------------------------------+
//:>------------------|    Member Function Definitions    |-------------------
//:>                  +-----------------------------------+

// +---------------+
// |  Constructor  |
// +---------------+

//:Description
//
// Set timer to timeout value.
//
// Default timeout period is 0 (not timing).
//
CFwTimer::CFwTimer(
  long lTimeout):            // timeout period in ms

  //:>Private data initialization
  m_lStart(
    0),
  m_lTimerPeriod(
    0)
{
  SetTimer(lTimeout);
}

// +----------------+
// |  GetElapsed()  |
// +----------------+

//:Description
//
// Get the elasped time since timer set.
//
// Returns elasped time in ms
//
long CFwTimer::GetElapsed(void)
{
  return(GetSystemTime() - m_lStart);
}

// +------------------+
// |  GetExpiredBy()  |
// +------------------+

//:Description
//
// Get the amount of time the timer has expired.
//
// returns long ms. Value is positive if expired, negative if unexpired and
// INFINITE if timer is disabled.
//
long CFwTimer::GetExpiredBy(void)
{
  if(m_lTimerPeriod == INFINITE)
      return(-INFINITE);  // special handling
  long lDelta = GetSystemTime() - (m_lStart + m_lTimerPeriod);
  return (lDelta);
}

// +------------------+
// |  GetRemaining()  |
// +------------------+

//:Description
//
// Get the remaining time in timeout period.
//
// If the period is set to INFINITE then returns INFINITE.
// If the timer is beyound the timeout period returns 0.
//
// NOTE: This routine uses the fact that twos complement subtraction will always
// yield a signed value that is the difference of the two values. This type of
// operation does not have to check for boundary conditions around sign changes of
// values. This is important when one value is at the max negative value and the
// other value has incremented over the boundary to be the max positive value.
//
// Returns remaining time in ms or zero if expired
//
long CFwTimer::GetRemaining(void)
{
  if(m_lTimerPeriod == INFINITE)
      return(INFINITE);
  long lDelta = (m_lStart + m_lTimerPeriod) - GetSystemTime();
  return((lDelta < 0) ? 0 : lDelta);
}

// +-------------------+
// |  GetSystemTime()  |
// +-------------------+

//:Description
//
// Get system time in milliseconds since system boot.
// We'll use the Arduino's included millis() function, which is actually
// an UNSIGNED LONG.  We'll use it here as SIGNED LONG, which allows for handling
// clock wrap correctly.
//
long CFwTimer::GetSystemTime() {
  return(millis());
}

// +--------------------+
// |  IncrementTimer()  |
// +--------------------+

//:Description
//
// Increments previous timeout period by the new period.
//
// If the timer was disabled (SetTimer(INFINITE)) then calls SetTime() to start
// timing.
//
// This method is used where timing must maintain fixed intervals.
//
// returns void
//
void CFwTimer::IncrementTimer(
  long lPeriod)            // timer period to add to last set time
{
  switch(m_lTimerPeriod)
  {
  case INFINITE:
  case NOWAIT:  // timer disabled
      SetTimer(lPeriod);  // set starting now
      break;

  default:  // timer was set
      if(lPeriod == INFINITE)
      {  // turning off timer
        m_lTimerPeriod = lPeriod;
      }
      else
      {  // increment period
        m_lTimerPeriod += lPeriod;
      }
      break;
  }
}

// +---------------+
// |  IsTimeout()  |
// +---------------+

//:Description
//
// Check if timeout has occured
//
// returns true if timeout, else false if still timing
//
bool CFwTimer::IsTimeout(void)
{
  return(!IsTiming());
}

// +--------------+
// |  IsTiming()  |
// +--------------+

//:Description
//
// Checks if timer is still timing.
//
// Returns true is still timing, false if timed out.
//
bool CFwTimer::IsTiming(void)
{
  if(GetRemaining() == 0)
  {  // Timed out
      return(false);
  }

  return(true);
}

// +----------------+
// |  ResetTimer()  |
// +----------------+

//:Description
//
// Reset timer to using current timeout period.
// The registered time-out period (m_lTimerPeriod) remains unchanged,
// causing the timer to 'restart' at the current time.
//
// returns void
//
void CFwTimer::ResetTimer(void)
{
  m_lStart = GetSystemTime();
}

// +--------------+
// |  SetTimer()  |
// +--------------+

//:Description
//
// Set timer to timeout period.
//
// returns void
//
void CFwTimer::SetTimer(
  long lTimeout)
{
  m_lStart = GetSystemTime();
  m_lTimerPeriod = lTimeout;
}

//:Special
#undef _ThisClass
#undef _NumBaseClass

