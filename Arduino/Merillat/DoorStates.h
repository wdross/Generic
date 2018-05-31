#ifndef DOORSTATES_H
#define DOORSTATES_H

#include "IODefs.h"
#include "CFwTimer.h"
#include "StateMachine.h"
#include "BitObject.h"

#undef CLOSE_WINTER_DOORS // new request to just move upper doors
// This undef will still check that the Winter doors are actually locked open,
// but won't move them closed when the remote close request comes in.
// When everything is working as intended, this will be turned on as #define

#define ACTUATOR_TIME 12000
#define UPPER_DOORS_CLOSE_TIME 70000
#define UPPER_DOORS_OPEN_TIME 70000
#define CLOSE_WINTER_TIME (3*60LL)*1000LL
#define OPEN_WINTER_TIME (3*60LL)*1000LL
#define AIRBAG_TIME 11000
#define SOUTH_DOOR_LEAD DEGREES_TO_RADIANS(4.3) // South door needs to lead North door by 5.3 degrees so they overlap correctly

#define MIN_THRUST 300  // 1024ths thrust
#define MAX_THRUST 512 // 1024ths thrust
#define BOOST_THRUST 700 // 70%
#define NO_THRUST 0

// the door control state machine class
class DoorStates : public StateMachine
{
public:
  DoorStates();

  virtual EErrorCode Initialize();
  inline long CurrentTime() {
    return OpenTimer.GetExpiredBy();
  }
  BitObject *Touch_IsRequestingOpen;
  BitObject *Touch_IsRequestingClose;

  void ShowAllTimes(char *Buffer=NULL);
  bool InAStationaryState(); // returns true if stationary
  void CheckForAbort(); // needs to interrupt internal state sequence if another button is pressed

private:
  // state machine state functions
#define STATE_ITEM(x) void x();
#include "DoorStatesFns.h" // defines all the void functions listed of the form "void x();"
#undef STATE_ITEM

  // state map to define state function order
  BEGIN_STATE_MAP
#define STATE_ITEM(x) STATE_MAP_ENTRY(&DoorStates::x,x)
#include "DoorStatesFns.h" // makes an entry pair a state structure map "{&DoorStates::ST_StateName, "ST_StateName"}"
#undef STATE_ITEM
  END_STATE_MAP

  // state enumeration order must match the order of state
  // method entries in the state map
  enum E_States {
#define STATE_ITEM(x) e##x, // makes the enumeration list of the form "eST_StateName,"
#include "DoorStatesFns.h"
#undef STATE_ITEM
      ST_MAX_STATES // always the last one, not refered to elsewhere
  };

  CFwTimer OpenTimer;
  CFwTimer AirBagTimer;
  CFwTimer ErrorTimer;
  void ClearAllOutputs();
  INT8U AnyMovementRequests(); // makes bitfield of any Open or Close request
  INT8U MonitorInputs; // which bits of Open or Close request can be monitored
  INT8U RequestStorage; // pointed to by BitObjects
};
#endif // DOORSTATES_H
