#include <assert.h>
#include "StateMachine.h"

StateMachine::StateMachine(int maxStates) :
    currentState(0),
    _maxStates(maxStates),
    _eventGenerated(false),
    _pEventData(NULL)
{
  Timer.SetTimer(0);
}

// generates an external event. called once per external event
// to start the state machine executing
void StateMachine::ExternalEvent(unsigned char newState,
                                 EventData* pData)
{
    // if we are supposed to ignore this event
    if (newState == EVENT_IGNORED) {
        // just delete the event data, if any
        if (pData)
            delete pData;
    }
    else {
        // generate the event and execute the state engine
        InternalEvent(newState, pData);
        StateEngine();
    }
}

// generates an internal event. called from within a state
// function to transition to a new state
void StateMachine::InternalEvent(unsigned char newState,
                                 EventData* pData)
{
    _pEventData = pData;
    _eventGenerated = true;
    if (currentState != newState) { // actually transitioning to a new state
      _previousState = currentState;
      StateStruct* pStateMap = GetStateMap();
      pStateMap[currentState].StateTime = Timer.GetExpiredBy();
      Timer.SetTimer(0);
    }
    currentState = newState;
}

// the state engine executes the state machine states
void StateMachine::StateEngine(void)
{
    EventData* pDataTemp = NULL;

    // TBD - lock semaphore here
    // while events are being generated keep executing states
    while (_eventGenerated) {
        pDataTemp = _pEventData;  // copy of event data pointer
        _pEventData = NULL;       // event data used up, reset ptr
        _eventGenerated = false;  // event used up, reset flag

        assert(currentState < _maxStates);

        // execute the state passing in event data, if any
        const StateStruct* pStateMap = GetStateMap();
        (this->*pStateMap[currentState].pStateFunc)(pDataTemp);

        // if event data was used, then delete it
        if (pDataTemp) {
            delete pDataTemp;
            pDataTemp = NULL;
        }
    }
    // TBD - unlock semaphore here
}

char* StateMachine::GetCurrentStateName() {
  char *ret = GetStateMap()[currentState].StateName;
  return ret+3; // get rid of the constant prefix "ST_"
}

char* StateMachine::GetPreviousStateName() {
  char *ret = GetStateMap()[_previousState].StateName;
  return ret+3;
}


