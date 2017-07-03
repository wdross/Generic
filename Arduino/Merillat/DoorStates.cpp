/*
 * FileReceive.cpp
 *
 *  Created on: Dec 15, 2016
 *      Author: G.Ross
 */

#include "DoorStates.h"

#define ACTUATOR_TIME 12000
#define UPPER_DOORS_CLOSE_TIME 6500
#define UPPER_DOORS_OPEN_TIME 5500
#define CLOSE_WINTER_TIME 6000
#define OPEN_WINTER_TIME 6000

//!
//! Constructor... Initialize all the values.
//!
DoorStates::DoorStates() :
  StateMachine(ST_MAX_STATES)
{
  OpenTimer.SetTimer(INFINITE);
  RateOfChangeTimer.SetTimer(INFINITE);
  ErrorTimer.SetTimer(INFINITE);
  InternalEvent(eST_AwaitFullyOpenOrFullyClosed);
}

//!
//! Initialize any variables used in the state machine
//!
EErrorCode DoorStates::Initialize()
{
}

//!
//! Default initial state upon boot
//!
void DoorStates::ST_AwaitFullyOpenOrFullyClosed()
{
  // Ensure nothing is running
  South_Winter_Latch.Write(lr_No_Request);
  Center_Winter_Latch.Write(lr_No_Request);
  Upper_South_Door.Write(lr_No_Request);
  North_Winter_Latch.Write(lr_No_Request);
  Upper_North_Door.Write(lr_No_Request);
  Outputs.SouthThrusterTx.ManufactureCode = 306;// :11;
  Outputs.SouthThrusterTx.Reserved = 0x3; // :2, all 1s
  Outputs.SouthThrusterTx.IndustryGroup = 4; //:3;
  Outputs.SouthThrusterTx.ThrusterInstance = SOUTH_THRUSTER_INSTANCE; // :4;
  
  Outputs.SouthThrusterTx.Direction = NO_DIRECTION; // :2
  Outputs.SouthThrusterTx.Retract = NO_ACTION; // :2, unused
  Outputs.SouthThrusterTx.ReservedB = 0; // :6, all 0s
  Outputs.SouthThrusterTx.ReservedC = 0; // :24, all 0s
  
  Outputs.NorthThrusterTx = Outputs.SouthThrusterTx; // make the North same as the South .. all fields
  Outputs.NorthThrusterTx.ThrusterInstance = NORTH_THRUSTER_INSTANCE; // :4;  except this one

  OpenTimer.SetTimer(INFINITE);

  if (!Upper_North_Door_IsClosed.Read() &&
      Upper_North_Door_IsOpen.Read() &&  // Upper North Open
      !Upper_South_Door_IsClosed.Read() &&
      Upper_South_Door_IsOpen.Read() &&  // Upper South Open
      South_Winter_Lock_Open_IsLatched.Read() &&
      !South_Winter_Lock_Open_IsUnlatched.Read() && // South Winter Locked open
      North_Winter_Lock_Open_IsLatched.Read() &&
      !North_Winter_Lock_Open_IsUnlatched.Read()) { // North Winter Locked open
    InternalEvent(eST_IsOpen);
  }

  if (Upper_North_Door_IsClosed.Read() &&
      !Upper_North_Door_IsOpen.Read() &&  // Upper North closed
      Upper_South_Door_IsClosed.Read() &&
      !Upper_South_Door_IsOpen.Read() &&  // Upper South closed
      !Winter_Lock_Closed_IsUnlatched.Read() &&
      Winter_Lock_Closed_IsLatched.Read() && // Winter is locked
      !South_Winter_Lock_Open_IsLatched.Read()) {
    InternalEvent(eST_IsClosed);
  }
}


//!
//! Sitting in the Open state, wait for the button to request close
//!
void DoorStates::ST_IsOpen()
{
  // do whatever we need to clear state machine and begin closing things
  // establish what 'opened' looks like
  myEE.SouthDoor.Max = South_Winter_Door_Position;
  myEE.NorthDoor.Max = North_Winter_Door_Position;
  ErrorTimer.SetTimer(INFINITE);
  if (Remote_IsRequestingClose.Read()) {
    // switch to CLOSE state machine
    OpenTimer.SetTimer(UPPER_DOORS_CLOSE_TIME); // Big doors take about a minute
    InternalEvent(eST_CloseUpperDoors);
  }
}

//!
//! Sitting in the Closed state, wait for the button to request Open
//!
void DoorStates::ST_IsClosed()
{
  // do whatever we need to clear state machine and begin opening things
  // establish what 'closed' looks like
  myEE.SouthDoor.Min = South_Winter_Door_Position;
  myEE.NorthDoor.Min = North_Winter_Door_Position;
  ErrorTimer.SetTimer(INFINITE);
  if (Remote_IsRequestingOpen.Read()) {
    // switch to OPEN state machine
    OpenTimer.SetTimer(ACTUATOR_TIME); // Actuator takes 10s for full transition
    InternalEvent(eST_Unlock);
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
    OpenTimer.SetTimer(UPPER_DOORS_OPEN_TIME); // how long will the doors to become open enough to latch
    RateOfChangeTimer.SetTimer(ROC_RATE);
    LastSouthPosition = South_Winter_Door_Position;
    LastNorthPosition = North_Winter_Door_Position;
    InternalEvent(eST_MoveOpenWinterDoors);
  }
  else if (OpenTimer.IsTimeout()) {
    // failed to unlatch in prescribed time
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Center_Winter_Latch.Write(lr_No_Request);
    InternalEvent(eST_Error);
  }
}

//!
//! Use the bow thrusters to move the winter doors open
//!
void DoorStates::ST_MoveOpenWinterDoors()
{
  // once we trace manual movement data, we'll have a pretty good idea what
  // this should look like.
  if (RateOfChangeTimer.IsTimeout()) {
    // what is the rate of change?
    INT16S CurrentPosition = South_Winter_Door_Position;
    SouthMovement = CurrentPosition - LastSouthPosition;
    LastSouthPosition = CurrentPosition;
    CurrentPosition = North_Winter_Door_Position;
    NorthMovement = CurrentPosition - LastNorthPosition;
    LastNorthPosition = CurrentPosition;

    RateOfChangeTimer.IncrementTimer(ROC_RATE);
  }
  // if RateOfChange shows we're stalled after we've moved enough to open, move to next state

  if (SouthMovement < 10 &&
      ANALOG_TO_RADIANS(LastSouthPosition - myEE.SouthDoor.Min) > DEGREES_TO_RADIANS(DEGREES_CHANGE*9.0/10.0)) {
    Outputs.SouthThrusterTx.Thrust = MIN_THRUST;
  }
  if (NorthMovement < 10 &&
      ANALOG_TO_RADIANS(LastNorthPosition - myEE.NorthDoor.Min) > DEGREES_TO_RADIANS(DEGREES_CHANGE*9.0/10.0)) {
    Outputs.NorthThrusterTx.Thrust = MIN_THRUST;
  }
  if (Outputs.SouthThrusterTx.Thrust == MIN_THRUST &&
      Outputs.SouthThrusterTx.Thrust == MIN_THRUST) {
    OpenTimer.SetTimer(ACTUATOR_TIME); // Actuator takes 10s for full transition
    InternalEvent(eST_LockOpenWinterDoors);
    // we leave both thrusters running on low as we lock the doors open
    return;
  }
  // Still here means we're not open yet, calculate a velocity for thrust for each door
  // depending upon its current position.

  if (OpenTimer.IsTimeout()) {
    OpenTimer.SetTimer(ACTUATOR_TIME); // Actuator takes 10s for full transition
    InternalEvent(eST_LockOpenWinterDoors); // just for now, normally would call ST_Error
  }

}

//!
//! Lock each winter door open
//!
void DoorStates::ST_LockOpenWinterDoors()
{
  bool NorthLatched = North_Winter_Lock_Open_IsLatched.Read();
  if (NorthLatched) {
    North_Winter_Latch.Write(lr_No_Request);
    Outputs.NorthThrusterTx.Thrust = 0;
  }
  else {
    North_Winter_Latch.Write(lr_Latch_Request);
  }

  bool SouthLatched = South_Winter_Lock_Open_IsLatched.Read();
  if (SouthLatched) {
    South_Winter_Latch.Write(lr_No_Request);
    Outputs.SouthThrusterTx.Thrust = 0;
  }
  else {
    South_Winter_Latch.Write(lr_Latch_Request);
  }

  if (SouthLatched && NorthLatched) {
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Outputs.SouthThrusterTx.Thrust = 0;
    Outputs.NorthThrusterTx.Thrust = 0;
    OpenTimer.SetTimer(UPPER_DOORS_OPEN_TIME);
    InternalEvent(eST_OpenUpperDoors);
  }
  else if (OpenTimer.IsTimeout()) {
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Outputs.SouthThrusterTx.Thrust = 0;
    Outputs.NorthThrusterTx.Thrust = 0;
    InternalEvent(eST_Error);
  }
}


//!
//! Now move the upper doors open until they hit their limit switches
//!
void DoorStates::ST_OpenUpperDoors()
{
  bool NorthOpen = Upper_North_Door_IsOpen.Read();
  if (NorthOpen) {
    Upper_North_Door.Write(lr_No_Request);
  }
  else {
    Upper_North_Door.Write(lr_Unlatch_Request); // Unlatch == Open
  }

  bool SouthOpen = Upper_South_Door_IsOpen.Read();
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
//! Now move the upper doors open until they hit their limit switches
//!
void DoorStates::ST_CloseUpperDoors()
{
  bool NorthClose = Upper_North_Door_IsClosed.Read();
  if (NorthClose) {
    Upper_North_Door.Write(lr_No_Request);
  }
  else {
    Upper_North_Door.Write(lr_Latch_Request);
  }

  bool SouthClose = Upper_South_Door_IsClosed.Read();
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
    Upper_North_Door.Write(lr_No_Request);
    Upper_South_Door.Write(lr_No_Request);
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
    InternalEvent(eST_MoveWinterDoorsClosed);
  }
  else if (OpenTimer.IsTimeout()) {
    North_Winter_Latch.Write(lr_No_Request);
    South_Winter_Latch.Write(lr_No_Request);
    Center_Winter_Latch.Write(lr_No_Request);
    InternalEvent(eST_Error);
  }
}

//!
//! Use the thrusters to move the doors closed
//!
void DoorStates::ST_MoveWinterDoorsClosed()
{
  // Once we record the manual movement, we'll have an idea how this should look
  // Model it after DoorStates::ST_MoveOpenWinterDoors()

  // at the exit of this state, we leave the winter doors commanded slow, to
  // hold them against the hard-stops while we actuate the center latch actuator
  if (OpenTimer.IsTimeout()) {
    OpenTimer.SetTimer(ACTUATOR_TIME);
    InternalEvent(eST_LatchWinterDoorsClosed);
  }
}

//!
//! Latch the winter doors closed
//!
void DoorStates::ST_LatchWinterDoorsClosed()
{
  if (Winter_Lock_Closed_IsLatched.Read()) {
    Center_Winter_Latch.Write(lr_No_Request);
    Outputs.SouthThrusterTx.Thrust = 0;
    Outputs.NorthThrusterTx.Thrust = 0;
    InternalEvent(eST_IsClosed);
    OpenTimer.SetTimer(INFINITE);
  }
  else {
    Center_Winter_Latch.Write(lr_Latch_Request);
    if (OpenTimer.IsTimeout()) {
      Center_Winter_Latch.Write(lr_No_Request);
      Outputs.SouthThrusterTx.Thrust = 0;
      Outputs.NorthThrusterTx.Thrust = 0;
      InternalEvent(eST_Error);
    }
  }
}

//!
//! Something went wrong
//!
void DoorStates::ST_Error()
{
  if (ErrorTimer.GetExpiredBy() == INFINITE) {
    // this is the first time we've entered this routine
    ErrorTimer.SetTimer(15000);  // ensure we can't switch out of error state for this long so
                                 // we can see what the error was before possibly reverting to OK
  }
  if (ErrorTimer.IsTiming())
    return; // hasn't timed out yet, allow seeing previous state

  // call the initializing state.  This has us kill all motion and monitor for inputs
  // to indicate the doors are either fully open or fully closed, from which state we can
  // begin again the open/close processing.
  ST_AwaitFullyOpenOrFullyClosed();
}

