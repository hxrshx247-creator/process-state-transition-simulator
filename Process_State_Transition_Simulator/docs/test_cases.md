# Test Cases

Source file: `tests/test_simulator.c` (a small self-written harness; no
keyboard input needed; exit code 0 = all passed).

Run with:

```
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/process_simulator.c tests/test_simulator.c -o test_simulator.exe
.\test_simulator.exe
```

## Required cases (1-21)

| #  | Test function                              | What is checked                                                        | Expected result |
|----|--------------------------------------------|------------------------------------------------------------------------|-----------------|
| 1  | `test_01_init`                             | After `simulator_init`: 0 processes, 0 history entries; `NULL` rejected | `SIM_OK`, counts 0, `SIM_ERR_INVALID_ARGUMENT` for NULL |
| 2  | `test_02_first_process_is_P1`              | First created process gets ID `P1`                                      | ID == "P1", findable |
| 3  | `test_03_unique_sequential_ids`            | Three processes get `P1`, `P2`, `P3`                                    | unique, sequential |
| 4  | `test_04_new_process_starts_in_NEW`        | Initial state and transition count                                      | `STATE_NEW`, 0 |
| 5  | `test_05_new_to_ready`                     | NEW + ADMIT                                                             | READY |
| 6  | `test_06_ready_to_running`                 | READY + DISPATCH                                                        | RUNNING |
| 7  | `test_07_running_to_ready`                 | RUNNING + INTERRUPT                                                     | READY |
| 8  | `test_08_running_to_waiting`               | RUNNING + IO_REQUEST                                                    | WAITING |
| 9  | `test_09_waiting_to_ready`                 | WAITING + IO_COMPLETE                                                   | READY |
| 10 | `test_10_running_to_terminated`            | RUNNING + EXIT                                                          | TERMINATED |
| 11 | `test_11_new_to_running_rejected`          | NEW + DISPATCH                                                          | `SIM_ERR_INVALID_TRANSITION`, still NEW |
| 12 | `test_12_ready_to_waiting_rejected`        | READY + IO_REQUEST                                                      | `SIM_ERR_INVALID_TRANSITION`, still READY |
| 13 | `test_13_waiting_to_terminated_rejected`   | WAITING + EXIT                                                          | `SIM_ERR_INVALID_TRANSITION`, still WAITING |
| 14 | `test_14_terminated_is_final`              | All six actions on a TERMINATED process                                 | all rejected, still TERMINATED |
| 15 | `test_15_unknown_process_id_rejected`      | `P99`, empty string, lower-case `p1`, NULL arguments                    | `SIM_ERR_INVALID_PROCESS_ID` / `SIM_ERR_INVALID_ARGUMENT` |
| 16 | `test_16_invalid_action_rejected`          | `ACTION_COUNT`, 99, -1 as actions                                       | `SIM_ERR_INVALID_ACTION`, state unchanged |
| 17 | `test_17_rejected_does_not_change_state`   | Every illegal action on a WAITING process                               | state stays WAITING |
| 18 | `test_18_rejected_does_not_change_history` | Rejected calls of several kinds after 2 good transitions                | global log count and per-process count unchanged |
| 19 | `test_19_processes_are_independent`        | P1, P2, P3 moved differently                                            | each keeps its own state and count |
| 20 | `test_20_capacity_handled_safely`          | Fill table to `MAX_PROCESSES`, create one more, terminate P1 and retry, bad arguments | `SIM_ERR_TABLE_FULL`, count unchanged, slot not freed |
| 21 | `test_21_reset_works_as_documented`        | Reset after activity                                                    | 0 processes, 0 history, next ID is `P1` again |

## Extra cases (22-27)

| #  | Test function                                  | What is checked                                                |
|----|------------------------------------------------|----------------------------------------------------------------|
| 22 | `test_22_rule_table_has_exactly_six_rules`     | Of all 5 x 6 (state, action) pairs exactly 6 are legal, with the right next states |
| 23 | `test_23_history_records_details`              | Log entry holds process, from-state, action, to-state, sequence number, description |
| 24 | `test_24_per_process_history_limit`            | At `MAX_PROCESS_TRANSITIONS` the next transition is rejected with `SIM_ERR_HISTORY_FULL` and the state is unchanged |
| 25 | `test_25_global_history_limit`                  | The global log never exceeds `MAX_HISTORY`; a rejected transition leaves the state unchanged |
| 26 | `test_26_per_process_history_view`             | Per-process record points to the right global log entries     |
| 27 | `test_27_names_and_messages`                   | State/action names and status messages are never NULL          |

## Results of the run performed during development

| Item                    | Value |
|-------------------------|-------|
| Date of run             | 2026-10-14 |
| Platform                | Linux (Ubuntu 24.04), GCC 13.3.0 |
| Compile flags           | `-std=c11 -Wall -Wextra -Wpedantic` (no warnings) |
| Result                  | Tests run: 27, passed: 27, failed: 0 |
| Extra check             | Same tests rebuilt with `-fsanitize=address,undefined`: all passed, no sanitizer reports |

These results come from an actual run on Linux. **Re-run the tests on your own
Windows machine and record the output of that run for your submission**:

| Item                    | Your value |
|-------------------------|------------|
| Date of your run        | ____________ |
| Compiler version (`gcc --version`) | ____________ |
| Tests run / passed / failed | ____ / ____ / ____ |
