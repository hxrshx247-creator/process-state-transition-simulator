/*
 * main.c - command-line demonstration of the Process State Transition
 * Simulator engine.
 *
 * This file contains NO transition rules. It only calls the engine
 * (process_simulator.h) and prints what happened, so the same engine can be
 * reused later behind a different front end.
 */

#include <stdio.h>

#include "process_simulator.h"

/* Apply one action and print the outcome in a readable way. */
static void step(Simulator *sim, const char *pid, ProcessAction action)
{
    ProcessState before = STATE_NEW;
    ProcessState after  = STATE_NEW;
    SimStatus    status;
    int          known;

    known  = (get_process_state(sim, pid, &before) == SIM_OK);
    status = transition_process(sim, pid, action);

    if (status == SIM_OK) {
        (void)get_process_state(sim, pid, &after);
        printf("  [ OK ] %-3s %-11s : %s -> %s\n", pid,
               get_action_name(action), get_state_name(before),
               get_state_name(after));
    } else if (known) {
        printf("  [FAIL] %-3s %-11s : %s (state stays %s)\n", pid,
               get_action_name(action), get_status_message(status),
               get_state_name(before));
    } else {
        printf("  [FAIL] %-3s %-11s : %s\n", pid,
               get_action_name(action), get_status_message(status));
    }
}

static void heading(const char *text)
{
    printf("\n=== %s ===\n", text);
}

int main(void)
{
    Simulator sim;
    char id1[PROCESS_ID_LEN];
    char id2[PROCESS_ID_LEN];

    printf("PROCESS STATE TRANSITION SIMULATOR - demonstration\n");

    /* 1. Initialise */
    if (simulator_init(&sim) != SIM_OK) {
        fprintf(stderr, "Could not initialise the simulator.\n");
        return 1;
    }

    /* 2. Create P1 and P2 */
    heading("Step 1: create two processes");
    if (create_process(&sim, id1, sizeof(id1)) != SIM_OK ||
        create_process(&sim, id2, sizeof(id2)) != SIM_OK) {
        fprintf(stderr, "Could not create processes.\n");
        return 1;
    }
    printf("  Created %s and %s (processes in table: %d)\n", id1, id2,
           get_process_count(&sim));

    /* 3. Initial states */
    heading("Step 2: initial states");
    print_processes(&sim, stdout);

    /* 4-9. Normal life cycle of P1 */
    heading("Step 3: life cycle of P1");
    step(&sim, id1, ACTION_ADMIT);        /* NEW      -> READY      */
    step(&sim, id1, ACTION_DISPATCH);     /* READY    -> RUNNING    */
    step(&sim, id1, ACTION_IO_REQUEST);   /* RUNNING  -> WAITING    */
    step(&sim, id1, ACTION_IO_COMPLETE);  /* WAITING  -> READY      */
    step(&sim, id1, ACTION_DISPATCH);     /* READY    -> RUNNING    */
    step(&sim, id1, ACTION_EXIT);         /* RUNNING  -> TERMINATED */

    /* 10. Invalid transitions are rejected and change nothing */
    heading("Step 4: invalid transitions");
    step(&sim, id2, ACTION_DISPATCH);     /* NEW cannot go straight to RUNNING */
    step(&sim, id1, ACTION_DISPATCH);     /* TERMINATED is a final state       */
    step(&sim, "P99", ACTION_ADMIT);      /* no such process                   */

    /* 11. Final table and log */
    heading("Step 5: final results");
    print_processes(&sim, stdout);
    printf("\n");
    print_transition_history(&sim, stdout);
    printf("\n");
    (void)print_process_history(&sim, id1, stdout);

    return 0;
}
