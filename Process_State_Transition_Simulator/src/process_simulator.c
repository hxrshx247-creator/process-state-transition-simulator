/*
 * process_simulator.c
 * ---------------------------------------------------------------------------
 * Implementation of the Process State Transition Simulator engine.
 * See include/process_simulator.h for the documented public interface.
 *
 * All transition rules are kept in ONE place: TRANSITION_TABLE below.
 * Changing the rules means editing that table only.
 * ---------------------------------------------------------------------------
 */

#include "process_simulator.h"

#include <string.h>

/* ========================================================================== */
/*  Rule table and name tables                                                */
/* ========================================================================== */

typedef struct {
    ProcessState  from;
    ProcessAction action;
    ProcessState  to;
} TransitionRule;

/* The six legal transitions. Anything not listed here is rejected. */
static const TransitionRule TRANSITION_TABLE[] = {
    { STATE_NEW,     ACTION_ADMIT,       STATE_READY      },
    { STATE_READY,   ACTION_DISPATCH,    STATE_RUNNING    },
    { STATE_RUNNING, ACTION_INTERRUPT,   STATE_READY      },
    { STATE_RUNNING, ACTION_IO_REQUEST,  STATE_WAITING    },
    { STATE_WAITING, ACTION_IO_COMPLETE, STATE_READY      },
    { STATE_RUNNING, ACTION_EXIT,        STATE_TERMINATED }
};

#define RULE_COUNT (sizeof(TRANSITION_TABLE) / sizeof(TRANSITION_TABLE[0]))

static const char *const STATE_NAMES[STATE_COUNT] = {
    "NEW", "READY", "RUNNING", "WAITING", "TERMINATED"
};

static const char *const ACTION_NAMES[ACTION_COUNT] = {
    "ADMIT", "DISPATCH", "INTERRUPT", "IO_REQUEST", "IO_COMPLETE", "EXIT"
};

/* ========================================================================== */
/*  Small private helpers                                                     */
/* ========================================================================== */

/* Return the table row of the process with this ID, or -1 if not found. */
static int find_index(const Simulator *sim, const char *process_id)
{
    int i;

    if (sim == NULL || process_id == NULL) {
        return -1;
    }
    for (i = 0; i < sim->process_count; i++) {
        if (strcmp(sim->processes[i].id, process_id) == 0) {
            return i;
        }
    }
    return -1;
}

static int state_in_range(ProcessState s)
{
    return (int)s >= 0 && (int)s < (int)STATE_COUNT;
}

static int action_in_range(ProcessAction a)
{
    return (int)a >= 0 && (int)a < (int)ACTION_COUNT;
}

/* ========================================================================== */
/*  Names                                                                     */
/* ========================================================================== */

const char *get_state_name(ProcessState state)
{
    return state_in_range(state) ? STATE_NAMES[state] : "UNKNOWN";
}

const char *get_action_name(ProcessAction action)
{
    return action_in_range(action) ? ACTION_NAMES[action] : "UNKNOWN";
}

const char *get_status_message(SimStatus status)
{
    switch (status) {
    case SIM_OK:                     return "Success";
    case SIM_ERR_INVALID_ARGUMENT:   return "Invalid function argument";
    case SIM_ERR_INVALID_PROCESS_ID: return "Invalid process ID (no such process)";
    case SIM_ERR_INVALID_ACTION:     return "Invalid action";
    case SIM_ERR_INVALID_TRANSITION: return "Invalid transition for the current state";
    case SIM_ERR_TABLE_FULL:         return "Process table capacity reached";
    case SIM_ERR_HISTORY_FULL:       return "Transition history capacity reached";
    default:                         return "Unknown status";
    }
}

/* ========================================================================== */
/*  Lifecycle                                                                 */
/* ========================================================================== */

SimStatus simulator_init(Simulator *sim)
{
    if (sim == NULL) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    memset(sim, 0, sizeof(*sim));   /* zero table, log, counters, clock */
    sim->next_id_number = 1;        /* first process will be P1         */
    return SIM_OK;
}

SimStatus reset_simulator(Simulator *sim)
{
    /* A reset is a fresh run: identical to a newly initialised simulator. */
    return simulator_init(sim);
}

/* ========================================================================== */
/*  Process management                                                        */
/* ========================================================================== */

SimStatus create_process(Simulator *sim, char *out_id, size_t out_id_size)
{
    Process *p;

    if (sim == NULL) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (out_id != NULL && out_id_size < PROCESS_ID_LEN) {
        return SIM_ERR_INVALID_ARGUMENT;      /* buffer too small */
    }
    if (sim->process_count >= MAX_PROCESSES) {
        return SIM_ERR_TABLE_FULL;
    }

    p = &sim->processes[sim->process_count];
    memset(p, 0, sizeof(*p));
    p->number = sim->next_id_number;          /* counter only goes up, so an */
    sim->next_id_number++;                    /* ID is never handed out twice */
    snprintf(p->id, sizeof(p->id), "P%d", p->number);
    p->state = STATE_NEW;
    sim->clock++;
    p->creation_seq = sim->clock;
    p->transition_count = 0;
    sim->process_count++;

    if (out_id != NULL) {
        strcpy(out_id, p->id);                /* fits: checked above */
    }
    return SIM_OK;
}

const Process *find_process_by_id(const Simulator *sim, const char *process_id)
{
    int idx = find_index(sim, process_id);
    return (idx < 0) ? NULL : &sim->processes[idx];
}

const Process *get_process_at(const Simulator *sim, int index)
{
    if (sim == NULL || index < 0 || index >= sim->process_count) {
        return NULL;
    }
    return &sim->processes[index];
}

SimStatus get_process_state(const Simulator *sim, const char *process_id,
                            ProcessState *out_state)
{
    int idx;

    if (sim == NULL || process_id == NULL || out_state == NULL) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    idx = find_index(sim, process_id);
    if (idx < 0) {
        return SIM_ERR_INVALID_PROCESS_ID;
    }
    *out_state = sim->processes[idx].state;
    return SIM_OK;
}

int get_process_count(const Simulator *sim)
{
    return (sim == NULL) ? 0 : sim->process_count;
}

/* ========================================================================== */
/*  Transitions                                                               */
/* ========================================================================== */

int is_valid_transition(ProcessState from, ProcessAction action,
                        ProcessState *next_state)
{
    size_t i;

    for (i = 0; i < RULE_COUNT; i++) {
        if (TRANSITION_TABLE[i].from == from &&
            TRANSITION_TABLE[i].action == action) {
            if (next_state != NULL) {
                *next_state = TRANSITION_TABLE[i].to;
            }
            return 1;
        }
    }
    return 0;
}

SimStatus transition_process(Simulator *sim, const char *process_id,
                             ProcessAction action)
{
    int           idx;
    Process      *p;
    ProcessState  next;
    HistoryEntry *e;

    /* ---- 1. Validate everything BEFORE changing anything ---------------- */
    if (sim == NULL || process_id == NULL) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    if (!action_in_range(action)) {
        return SIM_ERR_INVALID_ACTION;
    }
    idx = find_index(sim, process_id);
    if (idx < 0) {
        return SIM_ERR_INVALID_PROCESS_ID;
    }
    p = &sim->processes[idx];

    if (!is_valid_transition(p->state, action, &next)) {
        return SIM_ERR_INVALID_TRANSITION;
    }
    if (sim->history_count >= MAX_HISTORY ||
        p->transition_count >= MAX_PROCESS_TRANSITIONS) {
        return SIM_ERR_HISTORY_FULL;          /* reject: keep state and log in sync */
    }

    /* ---- 2. All checks passed: apply the transition and log it ---------- */
    e = &sim->history[sim->history_count];
    e->sequence   = sim->history_count + 1;
    strcpy(e->process_id, p->id);             /* both buffers are PROCESS_ID_LEN */
    e->from_state = p->state;
    e->action     = action;
    e->to_state   = next;
    snprintf(e->description, sizeof(e->description), "%s: %s --%s--> %s",
             p->id, get_state_name(p->state), get_action_name(action),
             get_state_name(next));

    p->history_index[p->transition_count] = sim->history_count;
    p->transition_count++;
    sim->history_count++;
    p->state = next;
    sim->clock++;
    return SIM_OK;
}

/* ========================================================================== */
/*  History access                                                            */
/* ========================================================================== */

int get_history_count(const Simulator *sim)
{
    return (sim == NULL) ? 0 : sim->history_count;
}

SimStatus get_history_entry(const Simulator *sim, int index,
                            HistoryEntry *out_entry)
{
    if (sim == NULL || out_entry == NULL ||
        index < 0 || index >= sim->history_count) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    *out_entry = sim->history[index];
    return SIM_OK;
}

/* ========================================================================== */
/*  Printing                                                                  */
/* ========================================================================== */

void print_processes(const Simulator *sim, FILE *out)
{
    int i;

    if (sim == NULL || out == NULL) {
        return;
    }
    fprintf(out, "PROCESS TABLE (current state of each process)\n");
    fprintf(out, "+---------+------------+---------+-------------+\n");
    fprintf(out, "| PID     | State      | Created | Transitions |\n");
    fprintf(out, "+---------+------------+---------+-------------+\n");
    for (i = 0; i < sim->process_count; i++) {
        const Process *p = &sim->processes[i];
        fprintf(out, "| %-7s | %-10s | %7lu | %11d |\n",
                p->id, get_state_name(p->state), p->creation_seq,
                p->transition_count);
    }
    if (sim->process_count == 0) {
        fprintf(out, "| (no processes)                               |\n");
    }
    fprintf(out, "+---------+------------+---------+-------------+\n");
    fprintf(out, "Processes: %d / %d\n", sim->process_count, MAX_PROCESSES);
}

void print_transition_history(const Simulator *sim, FILE *out)
{
    int i;

    if (sim == NULL || out == NULL) {
        return;
    }
    fprintf(out, "GLOBAL TRANSITION LOG (successful transitions only)\n");
    fprintf(out, "+-----+---------+------------+-------------+------------+\n");
    fprintf(out, "| Seq | PID     | From       | Action      | To         |\n");
    fprintf(out, "+-----+---------+------------+-------------+------------+\n");
    for (i = 0; i < sim->history_count; i++) {
        const HistoryEntry *e = &sim->history[i];
        fprintf(out, "| %3d | %-7s | %-10s | %-11s | %-10s |\n",
                e->sequence, e->process_id, get_state_name(e->from_state),
                get_action_name(e->action), get_state_name(e->to_state));
    }
    if (sim->history_count == 0) {
        fprintf(out, "| (no transitions yet)                               |\n");
    }
    fprintf(out, "+-----+---------+------------+-------------+------------+\n");
    fprintf(out, "Entries: %d / %d\n", sim->history_count, MAX_HISTORY);
}

SimStatus print_process_history(const Simulator *sim, const char *process_id,
                                FILE *out)
{
    int i, idx;
    const Process *p;

    if (sim == NULL || process_id == NULL || out == NULL) {
        return SIM_ERR_INVALID_ARGUMENT;
    }
    idx = find_index(sim, process_id);
    if (idx < 0) {
        return SIM_ERR_INVALID_PROCESS_ID;
    }
    p = &sim->processes[idx];
    fprintf(out, "History of %s (%d transition(s)):\n", p->id,
            p->transition_count);
    for (i = 0; i < p->transition_count; i++) {
        const HistoryEntry *e = &sim->history[p->history_index[i]];
        fprintf(out, "  #%d  %s\n", e->sequence, e->description);
    }
    return SIM_OK;
}
