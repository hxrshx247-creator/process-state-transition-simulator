/*
 * process_simulator.h
 * ---------------------------------------------------------------------------
 * Process State Transition Simulator - public interface of the simulation
 * engine (Member 1: Process Logic & Simulation).
 *
 * This header is the ONLY file other modules (demo program, tests, a future
 * frontend bridge) need to include. All state-transition rules live in
 * process_simulator.c; nothing here depends on a user interface.
 *
 * Design notes
 *   - No global variables: every function receives a Simulator pointer, so
 *     several independent simulators can exist (handy for tests/WebAssembly).
 *   - No dynamic memory (malloc/free): everything lives in fixed-size arrays,
 *     so there are no memory leaks by construction.
 *   - Every function that can fail returns a SimStatus code.
 *
 * Documented limits (change the #defines below to resize the simulator)
 *   MAX_PROCESSES           - processes that can be created before a reset.
 *                             Terminated processes stay in the table and
 *                             their IDs are never reused.
 *   MAX_HISTORY             - successful transitions in the global log.
 *   MAX_PROCESS_TRANSITIONS - successful transitions recorded per process.
 *   When a history limit is reached, a further transition is REJECTED with
 *   SIM_ERR_HISTORY_FULL and nothing is changed (state and log stay
 *   consistent with each other).
 * ---------------------------------------------------------------------------
 */

#ifndef PROCESS_SIMULATOR_H
#define PROCESS_SIMULATOR_H

#include <stddef.h>
#include <stdio.h>

/* ----------------------------- Limits ------------------------------------ */

#define MAX_PROCESSES            16   /* size of the process table            */
#define MAX_HISTORY              128  /* size of the global transition log    */
#define MAX_PROCESS_TRANSITIONS  16   /* transitions remembered per process   */
#define PROCESS_ID_LEN           16   /* buffer size for an ID such as "P12"  */
#define EVENT_DESC_LEN           96   /* buffer size for an event description */

/* ----------------------------- Enumerations ------------------------------ */

/* The five process states. STATE_COUNT is a sentinel, not a real state. */
typedef enum {
    STATE_NEW = 0,
    STATE_READY,
    STATE_RUNNING,
    STATE_WAITING,
    STATE_TERMINATED,
    STATE_COUNT
} ProcessState;

/* The six actions that cause transitions. ACTION_COUNT is a sentinel. */
typedef enum {
    ACTION_ADMIT = 0,     /* NEW     -> READY      */
    ACTION_DISPATCH,      /* READY   -> RUNNING    */
    ACTION_INTERRUPT,     /* RUNNING -> READY      */
    ACTION_IO_REQUEST,    /* RUNNING -> WAITING    */
    ACTION_IO_COMPLETE,   /* WAITING -> READY      */
    ACTION_EXIT,          /* RUNNING -> TERMINATED */
    ACTION_COUNT
} ProcessAction;

/* Result codes returned by the engine. SIM_OK is always 0. */
typedef enum {
    SIM_OK = 0,                   /* operation succeeded                       */
    SIM_ERR_INVALID_ARGUMENT,     /* NULL pointer, buffer too small, bad index */
    SIM_ERR_INVALID_PROCESS_ID,   /* no process with that ID exists            */
    SIM_ERR_INVALID_ACTION,       /* action value is not one of the six        */
    SIM_ERR_INVALID_TRANSITION,   /* action not allowed in the current state   */
    SIM_ERR_TABLE_FULL,           /* process table capacity reached            */
    SIM_ERR_HISTORY_FULL          /* global or per-process history is full     */
} SimStatus;

/* ----------------------------- Data structures --------------------------- */

/* One successful transition in the global log. */
typedef struct {
    int           sequence;                 /* 1, 2, 3 ... order in the log    */
    char          process_id[PROCESS_ID_LEN];
    ProcessState  from_state;               /* previous state                  */
    ProcessAction action;
    ProcessState  to_state;                 /* new state                       */
    char          description[EVENT_DESC_LEN]; /* e.g. "P1: NEW --ADMIT--> READY" */
} HistoryEntry;

/* One simulated process (one row of the process table). */
typedef struct {
    char          id[PROCESS_ID_LEN];       /* "P1", "P2", ...                 */
    int           number;                   /* numeric part of the ID          */
    ProcessState  state;                    /* CURRENT state                   */
    unsigned long creation_seq;             /* logical clock tick at creation  */
    int           transition_count;         /* successful transitions so far   */
    int           history_index[MAX_PROCESS_TRANSITIONS]; /* indexes into the
                                               global log, oldest first       */
} Process;

/* The whole simulator. Treat the fields as read-only outside the engine;
 * use the functions below to change anything. */
typedef struct {
    Process       processes[MAX_PROCESSES]; /* process table                   */
    int           process_count;            /* rows in use                     */
    int           next_id_number;           /* next ID number; never reused    */
    HistoryEntry  history[MAX_HISTORY];     /* GLOBAL transition log           */
    int           history_count;            /* entries in use                  */
    unsigned long clock;                    /* logical clock (creations and
                                               transitions advance it)         */
} Simulator;

/* ----------------------------- Lifecycle --------------------------------- */

/* Prepare a simulator for use: empty table, empty log, IDs start at P1.
 * Returns SIM_ERR_INVALID_ARGUMENT if sim is NULL. */
SimStatus simulator_init(Simulator *sim);

/* Return the simulator to the state right after simulator_init():
 * all processes and history are discarded and the next process is P1 again.
 * (IDs are never reused WITHIN a run; a reset starts a fresh run.) */
SimStatus reset_simulator(Simulator *sim);

/* ----------------------------- Process management ------------------------ */

/* Create a new process in STATE_NEW with the next sequential ID.
 * out_id may be NULL; otherwise it must point to at least PROCESS_ID_LEN
 * bytes and receives the new ID (e.g. "P1").
 * Returns SIM_OK, SIM_ERR_TABLE_FULL, or SIM_ERR_INVALID_ARGUMENT.
 * On failure the simulator is unchanged. */
SimStatus create_process(Simulator *sim, char *out_id, size_t out_id_size);

/* Look up a process by ID ("P1"). Returns NULL if sim/id is NULL or no such
 * process exists. The returned pointer is read-only and stays valid until the
 * simulator is reset. */
const Process *find_process_by_id(const Simulator *sim, const char *process_id);

/* Return the process stored in row `index` (0 .. process_count-1) or NULL. */
const Process *get_process_at(const Simulator *sim, int index);

/* Store the current state of a process in *out_state.
 * Returns SIM_OK, SIM_ERR_INVALID_PROCESS_ID or SIM_ERR_INVALID_ARGUMENT. */
SimStatus get_process_state(const Simulator *sim, const char *process_id,
                            ProcessState *out_state);

/* Number of processes currently in the table. Returns 0 if sim is NULL. */
int get_process_count(const Simulator *sim);

/* ----------------------------- Transitions ------------------------------- */

/* Apply `action` to the process. The rule table decides whether it is legal:
 *
 *     NEW     --ADMIT-------> READY
 *     READY   --DISPATCH----> RUNNING
 *     RUNNING --INTERRUPT---> READY
 *     RUNNING --IO_REQUEST--> WAITING
 *     WAITING --IO_COMPLETE-> READY
 *     RUNNING --EXIT--------> TERMINATED
 *
 * Returns SIM_OK, SIM_ERR_INVALID_ARGUMENT, SIM_ERR_INVALID_ACTION,
 * SIM_ERR_INVALID_PROCESS_ID, SIM_ERR_INVALID_TRANSITION or
 * SIM_ERR_HISTORY_FULL. Anything other than SIM_OK leaves the process state,
 * its transition count and the global log exactly as they were. */
SimStatus transition_process(Simulator *sim, const char *process_id,
                             ProcessAction action);

/* Pure rule check, no simulator needed. Returns 1 if `action` is legal in
 * state `from` (and writes the resulting state to *next_state when it is not
 * NULL); returns 0 otherwise. */
int is_valid_transition(ProcessState from, ProcessAction action,
                        ProcessState *next_state);

/* ----------------------------- History ----------------------------------- */

/* Number of entries in the global transition log. */
int get_history_count(const Simulator *sim);

/* Copy log entry `index` (0 .. history_count-1) into *out_entry.
 * Returns SIM_OK or SIM_ERR_INVALID_ARGUMENT. */
SimStatus get_history_entry(const Simulator *sim, int index,
                            HistoryEntry *out_entry);

/* ----------------------------- Names ------------------------------------- */

/* Readable names. Out-of-range values return "UNKNOWN" (never NULL). */
const char *get_state_name(ProcessState state);
const char *get_action_name(ProcessAction action);
const char *get_status_message(SimStatus status);

/* ----------------------------- Printing ---------------------------------- */

/* Print the process table (CURRENT state of every process) to `out`. */
void print_processes(const Simulator *sim, FILE *out);

/* Print the GLOBAL transition log (every successful transition, in order). */
void print_transition_history(const Simulator *sim, FILE *out);

/* Print the transitions of ONE process (a filtered view of the global log).
 * Returns SIM_OK or SIM_ERR_INVALID_PROCESS_ID / SIM_ERR_INVALID_ARGUMENT. */
SimStatus print_process_history(const Simulator *sim, const char *process_id,
                                FILE *out);

#endif /* PROCESS_SIMULATOR_H */
