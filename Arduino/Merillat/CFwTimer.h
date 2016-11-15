//:---------------------------------------------------------------------------
//:Description
//
// The CFwTimer class is a support base class for classes that measure elpsed time
// or simple timing support. Its static methods provide the framework's system time
// access for converting between milliseconds and clock ticks.
//
//:---------------------------------------------------------------------------
#include <limits.h>

#if !defined(CFWTIMER_INCLUDED)
#define CFWTIMER_INCLUDED

//:Special
#define _ThisClass    CFwTimer
#define _NumBaseClass  0

//:Custom
#ifndef INFINITE
#define INFINITE  (LONG_MAX) // value for no timeout on wait (always timing)
#endif

#define NOWAIT    (0)    // value to never wait, (always a timeout)
//:End Custom

//:Class
class CFwTimer
{
  public:

  CFwTimer(
    long lTimeout = 0);      // timeout period in ms

  long GetElapsed(void);

  long GetExpiredBy(void);

  long GetRemaining(void);

  static long GetSystemTime(void);

  void IncrementTimer(
    long lPeriod);          // timer period to add to last set time

  bool IsTimeout(void);

  bool IsTiming(void);

  void ResetTimer(void);

  void SetTimer(
    long lTimeout);

  inline long getStartTime();

  private:

  long m_lStart;
  long m_lTimerPeriod;
};

inline long CFwTimer::getStartTime() {
  return m_lStart+m_lTimerPeriod;
}

//:Special
#undef _ThisClass
#undef _NumBaseClass

#endif                  // CFWTIMER_INCLUDED

