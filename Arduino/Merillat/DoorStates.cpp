#include "DoorStates.h"

//!
//! Constructor... Initialize all the values.
//!
DoorStates::DoorStates() :
  StateMachine(ST_MAX_STATES)
{
  Touch_IsRequestingOpen = new BitObject(&RequestStorage,0,1);
  Touch_IsRequestingClose = new BitObject(&RequestStorage,1,1);
  OpenTimer.SetTimer(INFINITE);
  Upper_South_Door_Position.Cancel();
  Upper_North_Door_Position.Cancel();
  Winter_South_Door_Position.Cancel();
  Winter_North_Door_Position.Cancel();
  ErrorTimer.SetTimer(INFINITE);
  MonitorInputs = 0; // none
  RequestStorage = 0;
  if (myEE.Data.Doors[0].Valid && myEE.Data.Doors[1].Valid && myEE.Data.Doors[2].Valid && myEE.Data.Doors[3].Valid)
    InternalEvent(eST_AwaitFullyOpenOrFullyClosed);
  else
    InternalEvent(eST_SetUpHingePositions);

  // thruster one-shot init
  memset(&Outputs.Thrusters[SOUTH_INSTANCE],0,sizeof(Outputs.Thrusters[SOUTH_INSTANCE])); // all zeros
  Outputs.Thrusters[SOUTH_INSTANCE].ManufactureCode = 306;// :11;   = 0x132
  Outputs.Thrusters[SOUTH_INSTANCE].Reserved = 0x3; // :2, all 1s
  Outputs.Thrusters[SOUTH_INSTANCE].IndustryGroup = 4; //:3;
  // first 2 bytes become 0x9932, little endian on CAN see 0x32 0x99

  Outputs.Thrusters[SOUTH_INSTANCE].ThrusterInstance = SOUTH_INSTANCE; // :4;

  Outputs.Thrusters[SOUTH_INSTANCE].Retract = NO_ACTION; // :2, unused
  Outputs.Thrusters[SOUTH_INSTANCE].ReservedB = 0; // :6, all 0s
  Outputs.Thrusters[SOUTH_INSTANCE].ReservedC = 0; // :24, all 0s
  Outputs.Thrusters[SOUTH_INSTANCE].ReservedD = 0;

  Outputs.Thrusters[NORTH_INSTANCE] = Outputs.Thrusters[SOUTH_INSTANCE]; // make the North same as the South .. all fields
  Outputs.Thrusters[NORTH_INSTANCE].ThrusterInstance = NORTH_INSTANCE; // :4;  except this one
}

//!
//! Initialize any variables used in the state machine
//!
EErrorCode DoorStates::Initialize()
{
}

void DoorStates::ClearAllOutputs()
{
  // Ensure nothing is running
  South_Winter_Latch.Write(lr_No_Request);
  Center_Winter_Latch.Write(lr_No_Request);
  Upper_South_Door.Write(lr_No_Request);
  North_Winter_Latch.Write(lr_No_Request);
  Upper_North_Door.Write(lr_No_Request);
  Inflate_North.Write(0);
  Inflate_South.Write(0);

  // thrusters too
  Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_NONE; // :2
  Outputs.Thrusters[SOUTH_INSTANCE].Thrust = NO_THRUST; // 0
  Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_NONE; // :2
  Outputs.Thrusters[NORTH_INSTANCE].Thrust = NO_THRUST; // 0
} // ClearAllOutputs


INT8U DoorStates::AnyMovementRequests()
{
  // make a bit-wise mask of all input sources
  // can be used as a bool or bitfield as the need requires
  return(Remote_IsRequestingClose.Read() << 0 |
         Touch_IsRequestingClose->Read() << 1 |
         Remote_IsRequestingOpen.Read() << 2 |
         Touch_IsRequestingOpen->Read() << 3);
}


//!
//! Default initial state upon boot if the EEPROM isn't setup
//!
void DoorStates::ST_SetUpHingePositions()
{
  ClearAllOutputs();
  OpenTimer.SetTimer(INFINITE);
  ErrorTimer.SetTimer(INFINITE);
  // Doesn't allow any movement, just sit there until stuff is valid by using "Hinge Settings" screen
  if (myEE.Data.Doors[0].Valid && myEE.Data.Doors[1].Valid && myEE.Data.Doors[2].Valid && myEE.Data.Doors[3].Valid)
    InternalEvent(eST_AwaitFullyOpenOrFullyClosed);
}


//!
//! Default initial state upon boot
//!
void DoorStates::ST_AwaitFullyOpenOrFullyClosed()
{
  ClearAllOutputs();
  OpenTimer.SetTimer(INFINITE);

  ErrorTimer.SetTimer(INFINITE);

  // Don't allow any buttons to appear to be hard-wired on
  if (AnyMovementRequests())
    return;

  if (Upper_North_Door_Position.IsOpen() &&  // Upper North Open
      Upper_South_Door_Position.IsOpen() &&  // Upper South Open
      Winter_North_Door_Position.IsOpen() &&
      Winter_South_Door_Position.IsOpen() &&
      South_Winter_Lock_Open_IsLatched.Read() &&
      !South_Winter_Lock_Open_IsUnlatched.Read() && // South Winter Locked open
      North_Winter_Lock_Open_IsLatched.Read() &&
      !North_Winter_Lock_Open_IsUnlatched.Read()) { // North Winter Locked open
    InternalEvent(eST_IsOpen);
  }

  if (Upper_North_Door_Position.IsClosed() &&
      Upper_South_Door_Position.IsClosed() &&
      Winter_North_Door_Position.IsClosed() &&
      Winter_South_Door_Position.IsClosed() &&
      !Winter_Lock_Closed_IsUnlatched.Read() &&
      Winter_Lock_Closed_IsLatched.Read()) { // Winter is locked

    InternalEvent(eST_IsClosed);
  }
  //
  InternalEvent(eST_Error);
}


bool DoorStates::InAStationaryState()
{
  if (GetCurrentState() == eST_IsOpen ||
      GetCurrentState() == eST_IsClosed ||
      GetCurrentState() == eST_AwaitFullyOpenOrFullyClosed ||
      GetCurrentState() == eST_Error)
    return(true);
  return(false);
}


void DoorStates::CheckForAbort()
{
  // test inputs that should interrupt door progress
  if (InAStationaryState()) {
    MonitorInputs = 0; // clear which ones we'll look at
    return; // nothing to do in these states, not actively trying to move
  }

  // Any reason/input says we need to stop door movement?
  // bit-wise or everything to monitor into a single word
  INT8U inputs = AnyMovementRequests();
  MonitorInputs |= ~inputs; // every bit that is off, we can monitor from now on
  if (MonitorInputs & inputs) {
    // one of the bits came on, interrupt motion
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error);
  }
} // CheckForAbort


//!
//! Sitting in the Open state, wait for the button to request close
//!
void DoorStates::ST_IsOpen()
{
  if (Remote_IsRequestingOpen.Read() ||
      Touch_IsRequestingOpen->Read())
    return; // The request to open is still active -- don't begin Close sequence

  if (!System_Enable.Read())
    return; // system isn't enabled, don't run state machine

  if (Remote_IsRequestingClose.Read() ||
      Touch_IsRequestingClose->Read()) {
    // switch to CLOSE state machine
    ErrorTimer.SetTimer(INFINITE);

    if (Upper_South_Door_Position.IsClosed() && Upper_North_Door_Position.IsClosed()) {
      Upper_North_Door.Write(lr_No_Request);
      Upper_South_Door.Write(lr_No_Request);
      OpenTimer.SetTimer(ACTUATOR_TIME);
      InternalEvent(eST_UnLockOpenWinterDoors);
    }
    else {
      OpenTimer.SetTimer(UPPER_DOORS_CLOSE_TIME); // Big doors take about a minute
      Upper_South_Door_Position.Reset(ROC_CLOSING);
      Upper_North_Door_Position.Reset(ROC_CLOSING);
      InternalEvent(eST_CloseUpperDoors);
    }
  }
}

//!
//! Sitting in the Closed state, wait for the button to request Open
//!
void DoorStates::ST_IsClosed()
{
  if (Remote_IsRequestingClose.Read() ||
      Touch_IsRequestingClose->Read())
    return; // The request to close is still active -- don't begin Open sequence

  if (!System_Enable.Read())
    return; // system isn't enabled, don't run state machine


if (Remote_IsRequestingOpen.Read() ||
    Touch_IsRequestingOpen->Read()) {
    ErrorTimer.SetTimer(INFINITE);
    // switch to an OPEN state machine

    // let's check -- if the winter doors are all the way open already,
    // skip to opening the Upper doors.  Allows restarting motion
    if (Winter_South_Door_Position.Open90Percent() &&
        South_Winter_Lock_Open_IsLatched.Read() &&
        Winter_North_Door_Position.Open90Percent() &&
        North_Winter_Lock_Open_IsLatched.Read()) {
      InternalEvent(eST_OpenUpperDoors);
      OpenTimer.SetTimer(UPPER_DOORS_OPEN_TIME);
    }
    else {
      OpenTimer.SetTimer(ACTUATOR_TIME); // Actuator takes 10s for full transition
      InternalEvent(eST_Unlock);
    }
  }
}

//!
//! Unlock the center latch, so they can move open
//! and ensure the door open latches are open too
//!
void DoorStates::ST_Unlock()
{
  // would normally already be open, but double check
  bool NorthOpen = North_Winter_Lock_Open_IsUnlatched.Read();
  if (NorthOpen) {
    North_Winter_Latch.Write(lr_No_Request);
  }
  else {
    North_Winter_Latch.Write(lr_Unlatch_Request);
  }

  // would normally already be open, but double check
  bool SouthOpen = South_Winter_Lock_Open_IsUnlatched.Read();
  if (SouthOpen) {
    South_Winter_Latch.Write(lr_No_Request);
  }
  else {
    South_Winter_Latch.Write(lr_Unlatch_Request);
  }

  bool CenterOpen = Winter_Lock_Closed_IsUnlatched.Read();
  if (CenterOpen) {
    Center_Winter_Latch.Write(lr_No_Request);
  }
  else {
    Center_Winter_Latch.Write(lr_Unlatch_Request);
  }

  if (SouthOpen && NorthOpen && CenterOpen) {
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Center_Winter_Latch.Write(lr_No_Request);
    OpenTimer.SetTimer(OPEN_WINTER_TIME);
    Winter_South_Door_Position.Reset(ROC_WINTER);
    Winter_North_Door_Position.Reset(ROC_WINTER);
    InternalEvent(eST_MoveOpenWinterDoors);
  }
  else if (OpenTimer.IsTimeout()) {
    // failed to unlatch in prescribed time
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error);
  }
}

//!
//! Use the bow thrusters to move the winter doors open
//!
void DoorStates::ST_MoveOpenWinterDoors()
{
  // if RateOfChange shows we're stalled after we've moved enough to open, move to next state
  if (Winter_South_Door_Position.IsWinterSlow() &&
      Winter_South_Door_Position.Open90Percent()) {
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = MIN_THRUST;
  }
  else
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = MAX_THRUST;
  Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_OPEN;
  if (Winter_North_Door_Position.IsWinterSlow() &&
      Winter_North_Door_Position.Open90Percent()) {
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = MIN_THRUST;
  }
  else
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = MAX_THRUST;
  Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_OPEN;
  if (Outputs.Thrusters[SOUTH_INSTANCE].Thrust == MIN_THRUST &&
      Outputs.Thrusters[NORTH_INSTANCE].Thrust == MIN_THRUST) {
    OpenTimer.SetTimer(ACTUATOR_TIME); // Actuator takes 10s for full transition
    InternalEvent(eST_LockOpenWinterDoors);
    // we leave both thrusters running on low as we lock the doors open
    return;
  }

  if (OpenTimer.IsTimeout()) {
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error);
  }

  // Still here means we're not open yet, calculate a velocity for thrust for each door
  // depending upon its current position.
  // for now, we just leave the thrusters running at the MAX_THRUST, which is ~50%

//  if (Winter_North_Door_Position.Open90Percent()) {
//    // DEGREES_CHANGE is 84 (amount of winter movement)
//    // AngleFromClosed()
//    Outputs.Thrusters[NORTH_INSTANCE].Thrust =
//  }
}

//!
//! Lock each winter door open
//!
void DoorStates::ST_LockOpenWinterDoors()
{
  bool NorthLatched = North_Winter_Lock_Open_IsLatched.Read();
  if (NorthLatched) {
    North_Winter_Latch.Write(lr_No_Request);
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = NO_THRUST;
    Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_NONE;
  }
  else {
    North_Winter_Latch.Write(lr_Latch_Request);
  }

  bool SouthLatched = South_Winter_Lock_Open_IsLatched.Read();
  if (SouthLatched) {
    South_Winter_Latch.Write(lr_No_Request);
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = NO_THRUST;
    Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_NONE;
  }
  else {
    South_Winter_Latch.Write(lr_Latch_Request);
  }

  if (SouthLatched && NorthLatched) {
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = NO_THRUST;
    Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_NONE;
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = NO_THRUST;
    Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_NONE;
    OpenTimer.SetTimer(UPPER_DOORS_OPEN_TIME);
    InternalEvent(eST_OpenUpperDoors);
  }
  else if (OpenTimer.IsTimeout()) {
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error);
  }
}


//!
//! Now move the upper doors open until they hit their limit switches
//!
void DoorStates::ST_OpenUpperDoors()
{
  bool NorthOpen = Upper_North_Door_Position.IsOpen();
  if (NorthOpen) {
    Upper_North_Door.Write(lr_No_Request);
  }
  else {
    Upper_North_Door.Write(lr_Unlatch_Request); // Unlatch == Open
  }

  bool SouthOpen = Upper_South_Door_Position.IsOpen();
  if (SouthOpen) {
    Upper_South_Door.Write(lr_No_Request);
  }
  else {
    Upper_South_Door.Write(lr_Unlatch_Request); // Unlatch == Open
  }

  if (SouthOpen && NorthOpen) {
    Upper_North_Door.Write(lr_No_Request);
    Upper_South_Door.Write(lr_No_Request);
    OpenTimer.SetTimer(INFINITE);
    InternalEvent(eST_IsOpen);
  }
}

//!
//! Now move the upper doors open until they get to their closed postition
//!
void DoorStates::ST_CloseUpperDoors()
{
  bool NorthClose = Upper_North_Door_Position.IsClosingSlow() && Upper_North_Door_Position.IsClosed();
  if (NorthClose) {
    Upper_North_Door.Write(lr_No_Request);
  }
  else {
    Upper_North_Door.Write(lr_Latch_Request);
  }

  bool SouthClose = Upper_South_Door_Position.IsClosingSlow() && Upper_South_Door_Position.IsClosed();
  if (SouthClose) {
    Upper_South_Door.Write(lr_No_Request);
  }
  else {
    Upper_South_Door.Write(lr_Latch_Request);
  }

  if (SouthClose && NorthClose) {
    Upper_North_Door.Write(lr_No_Request);
    Upper_South_Door.Write(lr_No_Request);
    OpenTimer.SetTimer(ACTUATOR_TIME);
    InternalEvent(eST_UnLockOpenWinterDoors);
  }
  else if (OpenTimer.IsTimeout()) {
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error);
  }
}

//!
//! Unlatch the two winter doors
//!
void DoorStates::ST_UnLockOpenWinterDoors()
{
  bool NorthOpen = North_Winter_Lock_Open_IsUnlatched.Read();
  if (NorthOpen) {
    North_Winter_Latch.Write(lr_No_Request);
  }
  else {
    North_Winter_Latch.Write(lr_Unlatch_Request);
  }

  bool SouthOpen = South_Winter_Lock_Open_IsUnlatched.Read();
  if (SouthOpen) {
    South_Winter_Latch.Write(lr_No_Request);
  }
  else {
    South_Winter_Latch.Write(lr_Unlatch_Request);
  }

  // Center should already be open, but let's double check before we slam it closed
  bool CenterOpen = Winter_Lock_Closed_IsUnlatched.Read();
  if (CenterOpen) {
    Center_Winter_Latch.Write(lr_No_Request);
  }
  else {
    Center_Winter_Latch.Write(lr_Unlatch_Request);
  }

  if (SouthOpen && NorthOpen && CenterOpen) {
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Center_Winter_Latch.Write(lr_No_Request);
    OpenTimer.SetTimer(CLOSE_WINTER_TIME);
    AirBagTimer.SetTimer(AIRBAG_TIME);
    Winter_South_Door_Position.Reset(ROC_WINTER);
    Winter_North_Door_Position.Reset(ROC_WINTER);
    InternalEvent(eST_MoveWinterDoorsClosed);
  }
  else if (OpenTimer.IsTimeout()) {
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error);
  }
}

//!
//! Use the thrusters to move the doors closed
//!
void DoorStates::ST_MoveWinterDoorsClosed()
{
  // if RateOfChange shows we're stalled after we've moved enough to open, move to next state
  Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_CLOSE;
  if (Winter_South_Door_Position.IsWinterSlow() &&
      Winter_South_Door_Position.Close90Percent()) {
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = MIN_THRUST;
  }
  else
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = MAX_THRUST;
  Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_CLOSE;
  float SouthFromClosed = Winter_South_Door_Position.AngleFromClosed();
  float NorthFromClosed = Winter_North_Door_Position.AngleFromClosed();
  if (Winter_North_Door_Position.IsWinterSlow() &&
      Winter_North_Door_Position.Close90Percent()) {
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = MIN_THRUST;
  }
  else if (SouthFromClosed < DEGREES_TO_RADIANS(45) &&
           Outputs.Thrusters[SOUTH_INSTANCE].Thrust > MIN_THRUST &&
           SouthFromClosed+SOUTH_DOOR_LEAD > NorthFromClosed) {
    if (SouthFromClosed < NorthFromClosed)
      Outputs.Thrusters[NORTH_INSTANCE].Thrust = MIN_THRUST; // go slower
    else
      Outputs.Thrusters[NORTH_INSTANCE].Thrust = NO_THRUST; // go slower
  }
  else
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = MAX_THRUST;
  if (AirBagTimer.IsTimeout()) {
    Inflate_North.Write(0);
    Inflate_South.Write(0);
  }
  else {
    Inflate_North.Write(1);
    Inflate_South.Write(1);
    if (Outputs.Thrusters[NORTH_INSTANCE].Thrust == MAX_THRUST)
      Outputs.Thrusters[NORTH_INSTANCE].Thrust = BOOST_THRUST;
    if (Outputs.Thrusters[SOUTH_INSTANCE].Thrust == MAX_THRUST)
      Outputs.Thrusters[SOUTH_INSTANCE].Thrust = BOOST_THRUST;
  }
  if (Outputs.Thrusters[SOUTH_INSTANCE].Thrust == MIN_THRUST &&
      Outputs.Thrusters[NORTH_INSTANCE].Thrust == MIN_THRUST) {
    Inflate_North.Write(0);
    Inflate_South.Write(0);
    OpenTimer.SetTimer(ACTUATOR_TIME); // Actuator takes 10s for full transition
    InternalEvent(eST_LatchWinterDoorsClosed);
    // we leave both thrusters running on low as we lock the doors open
    return;
  }
  // Still here means we're not open yet, calculate a velocity for thrust for each door
  // depending upon its current position.
  // for now, we just leave the thrusters running at the MAX_THRUST, which is ~50%

  if (OpenTimer.IsTimeout()) {
    // all outputs are cleared within ST_Error
    InternalEvent(eST_Error); // just for now, normally would call ST_Error
  }
}

//!
//! Latch the winter doors closed
//!
void DoorStates::ST_LatchWinterDoorsClosed()
{
  if (Winter_Lock_Closed_IsLatched.Read()) {
    Center_Winter_Latch.Write(lr_No_Request);
    Outputs.Thrusters[NORTH_INSTANCE].Thrust = NO_THRUST;
    Outputs.Thrusters[SOUTH_INSTANCE].Thrust = NO_THRUST;
    Outputs.Thrusters[NORTH_INSTANCE].Direction = DIRECTION_NONE;
    Outputs.Thrusters[SOUTH_INSTANCE].Direction = DIRECTION_NONE;
    InternalEvent(eST_IsClosed);
    OpenTimer.SetTimer(INFINITE);
  }
  else {
    Center_Winter_Latch.Write(lr_Latch_Request);
    if (OpenTimer.IsTimeout()) {
      // all outputs are cleared within ST_Error
      InternalEvent(eST_Error);
    }
  }
}

//!
//! Something went wrong
//!
void DoorStates::ST_Error()
{
  static bool AllMovementRequestsCleared;
  ClearAllOutputs();
  if (ErrorTimer.GetExpiredBy() == INFINITE) {
    // this is the first time we've entered this routine
    ErrorTimer.SetTimer(5000);  // ensure we can't switch out of error state for this long to settle
    AllMovementRequestsCleared = false;
  }
  if (ErrorTimer.IsTiming())
    return; // hasn't timed out yet, allow seeing previous state

  // When recovering from an error, we'll sit in this state until all of our
  // movement request inputs (and the minimum 5 second error timer above)
  // have stopped.  Once there have been no Close or Open commands present,
  // we can start monitoring for them and act on them by simply "strumming"
  // the ST_IsOpen() and ST_IsClosed() states, which look for their associated
  // movement request and can pick up in the middle of either movement.
  if (!AllMovementRequestsCleared &&
       AnyMovementRequests())
    return;

  if (!AllMovementRequestsCleared) {
    AllMovementRequestsCleared = true; // after this, any request can turn on for movement
  }

  // keep checking for an open or close request, from the middle of whereever we are
  ST_IsOpen(); // can ask to close if was partway thru open or close
  ST_IsClosed();
}

void DoorStates::ShowAllTimes(char *Buffer)
{
  char *ret = Buffer;
  const StateStruct* pStateMap = GetStateMap();
  for (int q=1; q<ST_MAX_STATES-1; q++) {
    char Msg[50];
    if (q == eST_IsOpen)
      sprintf(Msg,"Closing:");
    else if (q == eST_IsClosed)
      sprintf(Msg,"Opening:");
    else {
      unsigned int time = pStateMap[q].StateTime;
      sprintf(Msg,"%-22s%4d.%03d",&pStateMap[q].StateName[3],time/1000,time%1000);
    }
    if (Buffer) {
      ret += sprintf(ret,"%s",Msg); // format append message
      *ret = 0; // null terminate
      ret++; // and skip
    }
    else
      Serial.println(Msg);
  }
  *ret = 0; // double null is end of message list
}

