/*
 * test_simulator.c - automated tests for the simulation engine.
 *
 * Independent of main.c, needs no keyboard input. Exit code is 0 when every
 * test passes and 1 otherwise, so it also works in scripts and CI.
 *
 * A tiny home-made harness is used instead of assert() because assert() is
 * switched off by -DNDEBUG and stops at the first failure.
 */

#include <stdio.h>
#include <string.h>

#include "process_simulator.h"

/* ------------------------------ Mini harness ----------------------------- */

static int g_checks_failed_in_case = 0;
static int g_cases_run    = 0;
static int g_cases_failed = 0;

#define EXPECT(cond)                                                         \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("         check failed (line %d): %s\n", __LINE__, #cond);\
            g_checks_failed_in_case++;                                       \
        }                                                                    \
    } while (0)

#define RUN(fn)                                                              \
    do {                                                                     \
        g_checks_failed_in_case = 0;                                         \
        fn();                                                                \
        g_cases_run++;                                                       \
        if (g_checks_failed_in_case == 0) {                                  \
            printf("[PASS] %s\n", #fn);                                      \
        } else {                                                             \
            printf("[FAIL] %s\n", #fn);                                      \
            g_cases_failed++;                                                \
        }                                                                    \
    } while (0)

/* ------------------------------ Helpers ---------------------------------- */

/* Fresh simulator containing one process, P1, in NEW. */
static void fresh(Simulator *sim)
{
    EXPECT(simulator_init(sim) == SIM_OK);
    EXPECT(create_process(sim, NULL, 0) == SIM_OK);
}

/* Drive P1 (which must be NEW) to `target` using only legal actions. */
static void drive_p1_to(Simulator *sim, ProcessState target)
{
    switch (target) {
    case STATE_NEW:
        break;
    case STATE_READY:
        EXPECT(transition_process(sim, "P1", ACTION_ADMIT) == SIM_OK);
        break;
    case STATE_RUNNING:
        drive_p1_to(sim, STATE_READY);
        EXPECT(transition_process(sim, "P1", ACTION_DISPATCH) == SIM_OK);
        break;
    case STATE_WAITING:
        drive_p1_to(sim, STATE_RUNNING);
        EXPECT(transition_process(sim, "P1", ACTION_IO_REQUEST) == SIM_OK);
        break;
    case STATE_TERMINATED:
        drive_p1_to(sim, STATE_RUNNING);
        EXPECT(transition_process(sim, "P1", ACTION_EXIT) == SIM_OK);
        break;
    default:
        EXPECT(0);
        break;
    }
}

static ProcessState state_of(const Simulator *sim, const char *id)
{
    ProcessState s = STATE_COUNT;           /* invalid marker if lookup fails */
    (void)get_process_state(sim, id, &s);
    return s;
}

/* ------------------------------ Test cases ------------------------------- */

/* 1 */
static void test_01_init(void)
{
    Simulator sim;
    EXPECT(simulator_init(&sim) == SIM_OK);
    EXPECT(get_process_count(&sim) == 0);
    EXPECT(get_history_count(&sim) == 0);
    EXPECT(simulator_init(NULL) == SIM_ERR_INVALID_ARGUMENT);
}

/* 2 */
static void test_02_first_process_is_P1(void)
{
    Simulator sim;
    char id[PROCESS_ID_LEN];
    EXPECT(simulator_init(&sim) == SIM_OK);
    EXPECT(create_process(&sim, id, sizeof(id)) == SIM_OK);
    EXPECT(strcmp(id, "P1") == 0);
    EXPECT(find_process_by_id(&sim, "P1") != NULL);
}

/* 3 */
static void test_03_unique_sequential_ids(void)
{
    Simulator sim;
    char a[PROCESS_ID_LEN], b[PROCESS_ID_LEN], c[PROCESS_ID_LEN];
    EXPECT(simulator_init(&sim) == SIM_OK);
    EXPECT(create_process(&sim, a, sizeof(a)) == SIM_OK);
    EXPECT(create_process(&sim, b, sizeof(b)) == SIM_OK);
    EXPECT(create_process(&sim, c, sizeof(c)) == SIM_OK);
    EXPECT(strcmp(a, "P1") == 0);
    EXPECT(strcmp(b, "P2") == 0);
    EXPECT(strcmp(c, "P3") == 0);
    EXPECT(get_process_count(&sim) == 3);
}

/* 4 */
static void test_04_new_process_starts_in_NEW(void)
{
    Simulator sim;
    fresh(&sim);
    EXPECT(state_of(&sim, "P1") == STATE_NEW);
    EXPECT(find_process_by_id(&sim, "P1")->transition_count == 0);
}

/* 5 */
static void test_05_new_to_ready(void)
{
    Simulator sim;
    fresh(&sim);
    EXPECT(transition_process(&sim, "P1", ACTION_ADMIT) == SIM_OK);
    EXPECT(state_of(&sim, "P1") == STATE_READY);
}

/* 6 */
static void test_06_ready_to_running(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_READY);
    EXPECT(transition_process(&sim, "P1", ACTION_DISPATCH) == SIM_OK);
    EXPECT(state_of(&sim, "P1") == STATE_RUNNING);
}

/* 7 */
static void test_07_running_to_ready(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_RUNNING);
    EXPECT(transition_process(&sim, "P1", ACTION_INTERRUPT) == SIM_OK);
    EXPECT(state_of(&sim, "P1") == STATE_READY);
}

/* 8 */
static void test_08_running_to_waiting(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_RUNNING);
    EXPECT(transition_process(&sim, "P1", ACTION_IO_REQUEST) == SIM_OK);
    EXPECT(state_of(&sim, "P1") == STATE_WAITING);
}

/* 9 */
static void test_09_waiting_to_ready(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_WAITING);
    EXPECT(transition_process(&sim, "P1", ACTION_IO_COMPLETE) == SIM_OK);
    EXPECT(state_of(&sim, "P1") == STATE_READY);
}

/* 10 */
static void test_10_running_to_terminated(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_RUNNING);
    EXPECT(transition_process(&sim, "P1", ACTION_EXIT) == SIM_OK);
    EXPECT(state_of(&sim, "P1") == STATE_TERMINATED);
}

/* 11 */
static void test_11_new_to_running_rejected(void)
{
    Simulator sim;
    fresh(&sim);
    EXPECT(transition_process(&sim, "P1", ACTION_DISPATCH)
           == SIM_ERR_INVALID_TRANSITION);
    EXPECT(state_of(&sim, "P1") == STATE_NEW);
}

/* 12 */
static void test_12_ready_to_waiting_rejected(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_READY);
    EXPECT(transition_process(&sim, "P1", ACTION_IO_REQUEST)
           == SIM_ERR_INVALID_TRANSITION);
    EXPECT(state_of(&sim, "P1") == STATE_READY);
}

/* 13 */
static void test_13_waiting_to_terminated_rejected(void)
{
    Simulator sim;
    fresh(&sim);
    drive_p1_to(&sim, STATE_WAITING);
    EXPECT(transition_process(&sim, "P1", ACTION_EXIT)
           == SIM_ERR_INVALID_TRANSITION);
    EXPECT(state_of(&sim, "P1") == STATE_WAITING);
}

/* 14 */
static void test_14_terminated_is_final(void)
{
    Simulator sim;
    int a;
    fresh(&sim);
    drive_p1_to(&sim, STATE_TERMINATED);
    for (a = 0; a < (int)ACTION_COUNT; a++) {
        EXPECT(transition_process(&sim, "P1", (ProcessAction)a)
               == SIM_ERR_INVALID_TRANSITION);
    }
    EXPECT(state_of(&sim, "P1") == STATE_TERMINATED);
}

/* 15 */
static void test_15_unknown_process_id_rejected(void)
{
    Simulator sim;
    ProcessState s;
    fresh(&sim);
    EXPECT(transition_process(&sim, "P99", ACTION_ADMIT)
           == SIM_ERR_INVALID_PROCESS_ID);
    EXPECT(transition_process(&sim, "", ACTION_ADMIT)
           == SIM_ERR_INVALID_PROCESS_ID);
    EXPECT(transition_process(&sim, "p1", ACTION_ADMIT)   /* case-sensitive */
           == SIM_ERR_INVALID_PROCESS_ID);
    EXPECT(transition_process(&sim, NULL, ACTION_ADMIT)
           == SIM_ERR_INVALID_ARGUMENT);
    EXPECT(transition_process(NULL, "P1", ACTION_ADMIT)
           == SIM_ERR_INVALID_ARGUMENT);
    EXPECT(get_process_state(&sim, "P99", &s) == SIM_ERR_INVALID_PROCESS_ID);
    EXPECT(find_process_by_id(&sim, "P99") == NULL);
    EXPECT(get_history_count(&sim) == 0);
}

/* 16 */
static void test_16_invalid_action_rejected(void)
{
    Simulator sim;
    fresh(&sim);
    EXPECT(transition_process(&sim, "P1", ACTION_COUNT)
           == SIM_ERR_INVALID_ACTION);
    EXPECT(transition_process(&sim, "P1", (ProcessAction)99)
           == SIM_ERR_INVALID_ACTION);
    EXPECT(transition_process(&sim, "P1", (ProcessAction)-1)
           == SIM_ERR_INVALID_ACTION);
    EXPECT(state_of(&sim, "P1") == STATE_NEW);
    EXPECT(strcmp(get_action_name((ProcessAction)99), "UNKNOWN") == 0);
    EXPECT(strcmp(get_state_name((ProcessState)99), "UNKNOWN") == 0);
}

/* 17 */
static void test_17_rejected_does_not_change_state(void)
{
    Simulator sim;
    int a;
    fresh(&sim);
    drive_p1_to(&sim, STATE_WAITING);
    for (a = 0; a < (int)ACTION_COUNT; a++) {
        if (a == (int)ACTION_IO_COMPLETE) {
            continue;                       /* the only legal action here */
        }
        EXPECT(transition_process(&sim, "P1", (ProcessAction)a)
               == SIM_ERR_INVALID_TRANSITION);
        EXPECT(state_of(&sim, "P1") == STATE_WAITING);
    }
}

/* 18 */
static void test_18_rejected_does_not_change_history(void)
{
    Simulator sim;
    int count_before, p_count_before;
    fresh(&sim);
    drive_p1_to(&sim, STATE_RUNNING);       /* 2 successful transitions */
    count_before   = get_history_count(&sim);
    p_count_before = find_process_by_id(&sim, "P1")->transition_count;
    EXPECT(count_before == 2);

    EXPECT(transition_process(&sim, "P1", ACTION_ADMIT)
           == SIM_ERR_INVALID_TRANSITION);
    EXPECT(transition_process(&sim, "P1", ACTION_IO_COMPLETE)
           == SIM_ERR_INVALID_TRANSITION);
    EXPECT(transition_process(&sim, "P7", ACTION_EXIT)
           == SIM_ERR_INVALID_PROCESS_ID);
    EXPECT(transition_process(&sim, "P1", (ProcessAction)50)
           == SIM_ERR_INVALID_ACTION);

    EXPECT(get_history_count(&sim) == count_before);
    EXPECT(find_process_by_id(&sim, "P1")->transition_count == p_count_before);
}

/* 19 */
static void test_19_processes_are_independent(void)
{
    Simulator sim;
    EXPECT(simulator_init(&sim) == SIM_OK);
    EXPECT(create_process(&sim, NULL, 0) == SIM_OK);   /* P1 */
    EXPECT(create_process(&sim, NULL, 0) == SIM_OK);   /* P2 */
    EXPECT(create_process(&sim, NULL, 0) == SIM_OK);   /* P3 */

    EXPECT(transition_process(&sim, "P1", ACTION_ADMIT) == SIM_OK);
    EXPECT(transition_process(&sim, "P1", ACTION_DISPATCH) == SIM_OK);
    EXPECT(transition_process(&sim, "P2", ACTION_ADMIT) == SIM_OK);

    EXPECT(state_of(&sim, "P1") == STATE_RUNNING);
    EXPECT(state_of(&sim, "P2") == STATE_READY);
    EXPECT(state_of(&sim, "P3") == STATE_NEW);
    EXPECT(find_process_by_id(&sim, "P1")->transition_count == 2);
    EXPECT(find_process_by_id(&sim, "P2")->transition_count == 1);
    EXPECT(find_process_by_id(&sim, "P3")->transition_count == 0);
}

/* 20 */
static void test_20_capacity_handled_safely(void)
{
    Simulator sim;
    char id[PROCESS_ID_LEN];
    int i;
    EXPECT(simulator_init(&sim) == SIM_OK);
    for (i = 0; i < MAX_PROCESSES; i++) {
        EXPECT(create_process(&sim, NULL, 0) == SIM_OK);
    }
    EXPECT(get_process_count(&sim) == MAX_PROCESSES);

    /* One more must fail and change nothing. */
    EXPECT(create_process(&sim, id, sizeof(id)) == SIM_ERR_TABLE_FULL);
    EXPECT(get_process_count(&sim) == MAX_PROCESSES);

    /* A terminated process does not free its slot or its ID. */
    EXPECT(transition_process(&sim, "P1", ACTION_ADMIT) == SIM_OK);
    EXPECT(transition_process(&sim, "P1", ACTION_DISPATCH) == SIM_OK);
    EXPECT(transition_process(&sim, "P1", ACTION_EXIT) == SIM_OK);
    EXPECT(create_process(&sim, id, sizeof(id)) == SIM_ERR_TABLE_FULL);

    /* Bad arguments. */
    EXPECT(create_process(NULL, id, sizeof(id)) == SIM_ERR_INVALID_ARGUMENT);
    EXPECT(simulator_init(&sim) == SIM_OK);
    EXPECT(create_process(&sim, id, 2) == SIM_ERR_INVALID_ARGUMENT);
    EXPECT(get_process_count(&sim) == 0);
}

/* 21 */
static void test_21_reset_works_as_documented(void)
{
    Simulator sim;
    char id[PROCESS_ID_LEN];
    fresh(&sim);
    drive_p1_to(&sim, STATE_WAITING);
    EXPECT(get_history_count(&sim) == 3);

    EXPECT(reset_simulator(&sim) == SIM_OK);
    EXPECT(get_process_count(&sim) == 0);
    EXPECT(get_history_count(&sim) == 0);
    EXPECT(find_process_by_id(&sim, "P1") == NULL);

    /* Documented: a reset starts a fresh run, so numbering restarts at P1. */
    EXPECT(create_process(&sim, id, sizeof(id)) == SIM_OK);
    EXPECT(strcmp(id, "P1") == 0);
    EXPECT(state_of(&sim, "P1") == STATE_NEW);
    EXPECT(reset_simulator(NULL) == SIM_ERR_INVALID_ARGUMENT);
}

/* ---- Extra tests beyond the required 21 -------------------------------- */

/* 22: exactly six legal (state, action) pairs exist in the whole 5x6 grid. */
static void test_22_rule_table_has_exactly_six_rules(void)
{
    int s, a, legal = 0;
    ProcessState next;
    for (s = 0; s < (int)STATE_COUNT; s++) {
        for (a = 0; a < (int)ACTION_COUNT; a++) {
            if (is_valid_transition((ProcessState)s, (ProcessAction)a, &next)) {
                legal++;
            }
        }
    }
    EXPECT(legal == 6);
    EXPECT(is_valid_transition(STATE_NEW, ACTION_ADMIT, &next) && next == STATE_READY);
    EXPECT(is_valid_transition(STATE_READY, ACTION_DISPATCH, &next) && next == STATE_RUNNING);
    EXPECT(is_valid_transition(STATE_RUNNING, ACTION_INTERRUPT, &next) && next == STATE_READY);
    EXPECT(is_valid_transition(STATE_RUNNING, ACTION_IO_REQUEST, &next) && next == STATE_WAITING);
    EXPECT(is_valid_transition(STATE_WAITING, ACTION_IO_COMPLETE, &next) && next == STATE_READY);
    EXPECT(is_valid_transition(STATE_RUNNING, ACTION_EXIT, &next) && next == STATE_TERMINATED);
}

/* 23: history entries record process, previous state, action, new state, seq. */
static void test_23_history_records_details(void)
{
    Simulator sim;
    HistoryEntry e;
    fresh(&sim);
    drive_p1_to(&sim, STATE_RUNNING);
    EXPECT(get_history_count(&sim) == 2);
    EXPECT(get_history_entry(&sim, 1, &e) == SIM_OK);
    EXPECT(e.sequence == 2);
    EXPECT(strcmp(e.process_id, "P1") == 0);
    EXPECT(e.from_state == STATE_READY);
    EXPECT(e.action == ACTION_DISPATCH);
    EXPECT(e.to_state == STATE_RUNNING);
    EXPECT(strcmp(e.description, "P1: READY --DISPATCH--> RUNNING") == 0);
    EXPECT(get_history_entry(&sim, 2, &e) == SIM_ERR_INVALID_ARGUMENT);
    EXPECT(get_history_entry(&sim, -1, &e) == SIM_ERR_INVALID_ARGUMENT);
}

/* 24: per-process history limit rejects safely and keeps state/log in sync. */
static void test_24_per_process_history_limit(void)
{
    Simulator sim;
    int i, ok = 0;
    SimStatus st = SIM_OK;
    ProcessState before;

    fresh(&sim);
    drive_p1_to(&sim, STATE_RUNNING);       /* 2 transitions used */
    ok = 2;
    for (i = 0; i < 100; i++) {             /* alternate INTERRUPT / DISPATCH */
        ProcessAction act = (i % 2 == 0) ? ACTION_INTERRUPT : ACTION_DISPATCH;
        before = state_of(&sim, "P1");
        st = transition_process(&sim, "P1", act);
        if (st != SIM_OK) {
            EXPECT(st == SIM_ERR_HISTORY_FULL);
            EXPECT(state_of(&sim, "P1") == before);   /* unchanged */
            break;
        }
        ok++;
    }
    EXPECT(st == SIM_ERR_HISTORY_FULL);
    EXPECT(ok == MAX_PROCESS_TRANSITIONS);
    EXPECT(find_process_by_id(&sim, "P1")->transition_count == MAX_PROCESS_TRANSITIONS);
    EXPECT(get_history_count(&sim) == MAX_PROCESS_TRANSITIONS);
}

/* 25: global history limit rejects safely. */
static void test_25_global_history_limit(void)
{
    Simulator sim;
    int p, i;
    SimStatus st;
    char pid[PROCESS_ID_LEN];
    ProcessState before;

    EXPECT(simulator_init(&sim) == SIM_OK);
    for (p = 0; p < MAX_PROCESSES; p++) {
        EXPECT(create_process(&sim, NULL, 0) == SIM_OK);
    }
    /* Fill the log: each process does ADMIT then DISPATCH/INTERRUPT cycles. */
    for (p = 0; p < MAX_PROCESSES; p++) {
        snprintf(pid, sizeof(pid), "P%d", p + 1);
        for (i = 0; i < MAX_PROCESS_TRANSITIONS + 2; i++) {
            ProcessAction act;
            ProcessState cur = state_of(&sim, pid);
            if (cur == STATE_NEW)          act = ACTION_ADMIT;
            else if (cur == STATE_READY)   act = ACTION_DISPATCH;
            else                           act = ACTION_INTERRUPT;
            before = cur;
            st = transition_process(&sim, pid, act);
            if (st != SIM_OK) {
                EXPECT(st == SIM_ERR_HISTORY_FULL);
                EXPECT(state_of(&sim, pid) == before);
                break;
            }
        }
    }
    EXPECT(get_history_count(&sim) == MAX_HISTORY);   /* never exceeds it */
    EXPECT(transition_process(&sim, "P16", ACTION_ADMIT) == SIM_ERR_HISTORY_FULL
           || state_of(&sim, "P16") != STATE_NEW);
}

/* 26: per-process history is a correct filtered view of the global log. */
static void test_26_per_process_history_view(void)
{
    Simulator sim;
    const Process *p;
    EXPECT(simulator_init(&sim) == SIM_OK);
    EXPECT(create_process(&sim, NULL, 0) == SIM_OK);
    EXPECT(create_process(&sim, NULL, 0) == SIM_OK);
    EXPECT(transition_process(&sim, "P1", ACTION_ADMIT) == SIM_OK);    /* log 0 */
    EXPECT(transition_process(&sim, "P2", ACTION_ADMIT) == SIM_OK);    /* log 1 */
    EXPECT(transition_process(&sim, "P1", ACTION_DISPATCH) == SIM_OK); /* log 2 */
    p = find_process_by_id(&sim, "P1");
    EXPECT(p != NULL && p->transition_count == 2);
    EXPECT(p != NULL && p->history_index[0] == 0 && p->history_index[1] == 2);
    p = find_process_by_id(&sim, "P2");
    EXPECT(p != NULL && p->transition_count == 1 && p->history_index[0] == 1);
}

/* 27: names and status messages are never NULL. */
static void test_27_names_and_messages(void)
{
    int i;
    EXPECT(strcmp(get_state_name(STATE_NEW), "NEW") == 0);
    EXPECT(strcmp(get_state_name(STATE_TERMINATED), "TERMINATED") == 0);
    EXPECT(strcmp(get_action_name(ACTION_IO_COMPLETE), "IO_COMPLETE") == 0);
    for (i = 0; i <= (int)SIM_ERR_HISTORY_FULL + 1; i++) {
        EXPECT(get_status_message((SimStatus)i) != NULL);
    }
}

/* ------------------------------ main ------------------------------------- */

int main(void)
{
    printf("Process State Transition Simulator - automated tests\n");
    printf("----------------------------------------------------\n");

    RUN(test_01_init);
    RUN(test_02_first_process_is_P1);
    RUN(test_03_unique_sequential_ids);
    RUN(test_04_new_process_starts_in_NEW);
    RUN(test_05_new_to_ready);
    RUN(test_06_ready_to_running);
    RUN(test_07_running_to_ready);
    RUN(test_08_running_to_waiting);
    RUN(test_09_waiting_to_ready);
    RUN(test_10_running_to_terminated);
    RUN(test_11_new_to_running_rejected);
    RUN(test_12_ready_to_waiting_rejected);
    RUN(test_13_waiting_to_terminated_rejected);
    RUN(test_14_terminated_is_final);
    RUN(test_15_unknown_process_id_rejected);
    RUN(test_16_invalid_action_rejected);
    RUN(test_17_rejected_does_not_change_state);
    RUN(test_18_rejected_does_not_change_history);
    RUN(test_19_processes_are_independent);
    RUN(test_20_capacity_handled_safely);
    RUN(test_21_reset_works_as_documented);
    RUN(test_22_rule_table_has_exactly_six_rules);
    RUN(test_23_history_records_details);
    RUN(test_24_per_process_history_limit);
    RUN(test_25_global_history_limit);
    RUN(test_26_per_process_history_view);
    RUN(test_27_names_and_messages);

    printf("----------------------------------------------------\n");
    printf("Tests run: %d, passed: %d, failed: %d\n", g_cases_run,
           g_cases_run - g_cases_failed, g_cases_failed);
    printf("%s\n", g_cases_failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return g_cases_failed == 0 ? 0 : 1;
}
