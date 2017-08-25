/*
 * DoorStates.h
 *
 *  Created on: Jun 23, 2017
 *      Author: W.Ross
 */

#ifndef DOORSTATES_H
#define DOORSTATES_H

#include "IODefs.h"
#include "CFwTimer.h"
#include "StateMachine.h"

// the door control state machine class
class DoorStates : public StateMachine
{
public:
  DoorStates();

  virtual EErrorCode Initialize();
  inline long CurrentTime() {
    return OpenTimer.GetExpiredBy();
  }

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
    CFwTimer ErrorTimer;
};
#endif // DOORSTATES_H
