# Viva Preparation

Short answers you can expand on. Answers about *this project* refer to the real code.

**1. What is a process?**
A program in execution: the program code plus its current activity (program
counter, registers, stack, data) and the OS bookkeeping about it.

**2. What is a process state?**
The condition a process is in at a moment - for example running on the CPU or
waiting for I/O. The OS changes it as events happen. This project models five:
NEW, READY, RUNNING, WAITING, TERMINATED.

**3. Difference between Ready and Running?**
Both are *able* to execute. A RUNNING process is on the CPU right now; a READY
process is in the queue waiting for the CPU. On one CPU only one process is
RUNNING at a time.

**4. Difference between Waiting and Ready?**
A WAITING process cannot run even if the CPU is free, because it is blocked on
an event such as I/O. A READY process could run immediately if given the CPU.
When the I/O finishes, WAITING becomes READY (not RUNNING).

**5. What is a context switch?**
Saving the state of the running process (registers, program counter) and
loading the saved state of another so the CPU switches processes. It is pure
overhead. In this project it is only represented by the `INTERRUPT` / `DISPATCH`
transitions; the registers are not actually saved.

**6. What is a time slice (quantum)?**
The maximum time a process may run before the scheduler pre-empts it
(Round Robin). When it expires the process goes RUNNING -> READY (our
`INTERRUPT`). Time slices are **not** simulated here.

**7. Why is an enum useful?**
It gives readable names to a fixed set of values (`STATE_READY` instead of 1),
prevents mixing up magic numbers, makes `switch` and tables clear, and lets us
use `STATE_COUNT` for array sizes and range checks.

**8. Why use a header file?**
It publishes the interface (types, constants, function prototypes) so other
files can use the engine without seeing its implementation. The compiler also
checks that calls match the declarations.

**9. Why separate the engine from `main.c`?**
So the rules can be reused and tested without any user interface: the demo,
the tests and a future frontend all call the same code. Changing the UI never
risks breaking the rules.

**10. How are invalid transitions detected?**
`transition_process()` looks up (current state, action) in the six-row
`TRANSITION_TABLE` via `is_valid_transition()`. If no row matches it returns
`SIM_ERR_INVALID_TRANSITION` before touching anything.

**11. How is process history stored?**
Every successful transition appends a `HistoryEntry` (sequence, process ID,
previous state, action, new state, description) to a fixed array in the
`Simulator`. Each `Process` also stores the indexes of its own entries.
Rejected requests are never logged.

**12. Limitations of a fixed-size process table?**
A hard maximum (16 here); creation fails with `SIM_ERR_TABLE_FULL` when full,
and terminated processes keep their slot. Memory is wasted if fewer are used.
The benefit is simplicity and no memory-management bugs. A dynamic table
(`malloc`/`realloc`) would remove the limit but needs careful freeing.

**13. What does modular programming mean?**
Splitting a program into independent parts with clear interfaces, each with one
responsibility: here the engine, the demo and the tests. Parts can be
developed, tested and replaced separately.

**14. How could a frontend communicate with the C engine?**
Not directly - C must be compiled. Options: (A) a small command protocol over
stdin/stdout, replying with JSON; (B) an HTTP server (Node/Flask) that talks to
the compiled C program; (C) compile the engine to WebAssembly and call it from
JavaScript. The choice depends on the frontend's technology. None is built yet.

**15. How would the simulator be extended to model CPU scheduling?**
Add a ready queue, a burst time per process and a scheduler function (FCFS,
SJF, Round Robin with a time slice, priority) that picks the next process and
calls `transition_process()` with `DISPATCH` / `INTERRUPT` / `EXIT` itself, plus
a clock and statistics (waiting and turnaround time). The rule table stays the same.

**16. Why does a rejected transition leave data unchanged?**
All validation happens first; modification starts only after every check has
passed. So there is no partial update.

**17. Why are process IDs never reused?**
A counter (`next_id_number`) only increases, and terminated processes stay in
the table. This keeps every log entry unambiguous.

**18. What happens when the history is full?**
The transition is rejected with `SIM_ERR_HISTORY_FULL` and nothing changes, so
a process's state never disagrees with the log.

**19. Why no `malloc`?**
Fixed arrays inside a struct mean no leaks and no forgotten `free`, which suits
a beginner project; the cost is the fixed limits.

**20. What is the difference between a status code and an exception?**
C has no exceptions; functions return a code (`SimStatus`) that the caller
must check. `SIM_OK` is 0, and `get_status_message()` gives readable text.
