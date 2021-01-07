/*
 * power.h
 * Copyright 2020 Greg Green <ggreen@bit-builder.com>
 */
#ifndef POWER_H_
#define POWER_H_

/*--------------------------------------------------------*/

typedef enum {
    Start,
    WaitEntry,
    Wait,
    SignaledOnEntry,
    SignaledOn,
    MCURunningEntry,
    MCURunning,
    IdleEntry,
    Idle,
    SignaledOffEntry,
    SignaledOff,
    MCUOffEntry,
    MCUOff,
    PowerDownEntry,
    PowerDown
} StateMachine;

/*--------------------------------------------------------
  globals
  --------------------------------------------------------*/

extern StateMachine machine_state;
extern StateMachine prev_state;

// extern fn defs
extern void change_state(StateMachine new_state);

#endif /* POWER_H_ */
