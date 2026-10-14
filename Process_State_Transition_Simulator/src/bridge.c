
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "process_simulator.h"

static Simulator sim;

static void print_processes_json(void)
{
    int i;

    printf("{\"processes\":[");
    for (i = 0; i < get_process_count(&sim); i++) {
        const Process *p = get_process_at(&sim, i);

        if (i > 0) {
            printf(",");
        }

        printf(
            "{\"id\":\"%s\",\"number\":%d,\"state\":\"%s\","
            "\"transitionCount\":%d}",
            p->id,
            p->number,
            get_state_name(p->state),
            p->transition_count
        );
    }
    printf("]}\n");
    fflush(stdout);
}

static void print_history_json(void)
{
    int i;

    printf("{\"events\":[");
    for (i = 0; i < get_history_count(&sim); i++) {
        HistoryEntry entry;

        if (get_history_entry(&sim, i, &entry) != SIM_OK) {
            continue;
        }

        if (i > 0) {
            printf(",");
        }

        printf(
            "{\"seq\":%d,\"id\":\"%s\",\"from\":\"%s\","
            "\"action\":\"%s\",\"to\":\"%s\",\"message\":\"%s\"}",
            entry.sequence,
            entry.process_id,
            get_state_name(entry.from_state),
            get_action_name(entry.action),
            get_state_name(entry.to_state),
            entry.description
        );
    }
    printf("]}\n");
    fflush(stdout);
}

static int running_process_exists(const char *except_id)
{
    int i;

    for (i = 0; i < get_process_count(&sim); i++) {
        const Process *p = get_process_at(&sim, i);

        if (p != NULL &&
            p->state == STATE_RUNNING &&
            (except_id == NULL || strcmp(p->id, except_id) != 0)) {
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    char line[256];

    simulator_init(&sim);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        char command[32] = {0};
        char process_id[PROCESS_ID_LEN] = {0};
        char action_name[32] = {0};

        if (sscanf(line, "%31s", command) != 1) {
            continue;
        }

        if (strcmp(command, "RESET") == 0) {
            reset_simulator(&sim);
            printf("{\"ok\":true,\"message\":\"Simulator reset\"}\n");
            fflush(stdout);
        } else if (strcmp(command, "CREATE") == 0) {
            SimStatus status =
                create_process(&sim, process_id, sizeof(process_id));

            if (status == SIM_OK) {
                printf("{\"ok\":true,\"id\":\"%s\"}\n", process_id);
            } else {
                printf(
                    "{\"ok\":false,\"message\":\"%s\"}\n",
                    get_status_message(status)
                );
            }
            fflush(stdout);
        } else if (strcmp(command, "TRANSITION") == 0) {
            SimStatus status;
            ProcessAction action;

            if (sscanf(line, "%31s %15s %31s",
                       command, process_id, action_name) != 3) {
                printf(
                    "{\"ok\":false,\"message\":\"Invalid TRANSITION command\"}\n"
                );
                fflush(stdout);
                continue;
            }

            if (strcmp(action_name, "ADMIT") == 0) {
                action = ACTION_ADMIT;
            } else if (strcmp(action_name, "DISPATCH") == 0) {
                action = ACTION_DISPATCH;
            } else if (strcmp(action_name, "INTERRUPT") == 0) {
                action = ACTION_INTERRUPT;
            } else if (strcmp(action_name, "IO_REQUEST") == 0) {
                action = ACTION_IO_REQUEST;
            } else if (strcmp(action_name, "IO_COMPLETE") == 0) {
                action = ACTION_IO_COMPLETE;
            } else if (strcmp(action_name, "EXIT") == 0) {
                action = ACTION_EXIT;
            } else {
                printf("{\"ok\":false,\"message\":\"Unknown action\"}\n");
                fflush(stdout);
                continue;
            }

            if (action == ACTION_DISPATCH &&
                running_process_exists(process_id)) {
                printf(
                    "{\"ok\":false,\"message\":"
                    "\"Single CPU mode: another process is already Running\"}\n"
                );
                fflush(stdout);
                continue;
            }

            status = transition_process(&sim, process_id, action);

            if (status == SIM_OK) {
                printf(
                    "{\"ok\":true,\"message\":\"Transition successful\"}\n"
                );
            } else {
                printf(
                    "{\"ok\":false,\"message\":\"%s\"}\n",
                    get_status_message(status)
                );
            }
            fflush(stdout);
        } else if (strcmp(command, "LIST") == 0) {
            print_processes_json();
        } else if (strcmp(command, "HISTORY") == 0) {
            print_history_json();
        } else {
            printf("{\"ok\":false,\"message\":\"Unknown command\"}\n");
            fflush(stdout);
        }
    }

    return 0;
}
