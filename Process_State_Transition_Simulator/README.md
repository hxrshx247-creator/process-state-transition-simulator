# Process State Transition Simulator (C)

An undergraduate Operating Systems mini project. This repository is
**Member 1 - Process Logic & Simulation**: the C simulation engine, transition
rules, process management, validation, history and automated tests. The
frontend is built separately (Replit) and will be connected later.

**Status:** the engine, demo and tests were compiled with
`-std=c11 -Wall -Wextra -Wpedantic` (no warnings) and the 27 automated tests
passed on Linux with GCC 13.3.0 (also under AddressSanitizer/UBSan). They have
**not** yet been run on Windows - do that yourself (steps below) and record
your own result.

---

## 1. What it does

Simulates the life cycle of processes with five states and six legal actions.

| Current state | Action        | Next state |
|---------------|---------------|------------|
| NEW           | `ADMIT`       | READY      |
| READY         | `DISPATCH`    | RUNNING    |
| RUNNING       | `INTERRUPT`   | READY      |
| RUNNING       | `IO_REQUEST`  | WAITING    |
| WAITING       | `IO_COMPLETE` | READY      |
| RUNNING       | `EXIT`        | TERMINATED |

Everything else is rejected, the process is left untouched, and a status code
explains why. See `docs/state_transition_diagram.md` for the diagram.

## 2. Architecture

```
 main.c (demo)  ─┐
 tests          ─┼──►  process_simulator.h  ──►  process_simulator.c
 future bridge  ─┘      (public interface)        (ALL rules live here)
```

- **Engine** (`include/process_simulator.h`, `src/process_simulator.c`):
  rules, process table, history, validation. No printing of menus, no input.
- **Demo** (`src/main.c`): calls the engine and prints results. Contains no rules.
- **Tests** (`tests/test_simulator.c`): call the engine directly. Independent of the demo.
- The rules are one table (`TRANSITION_TABLE` in `process_simulator.c`).
- No global variables and no `malloc`: all state is inside a `Simulator`
  struct that the caller owns, so there are no memory leaks by design.

### Limits (change the `#define`s in the header to resize)

| Constant                  | Value | Meaning |
|---------------------------|-------|---------|
| `MAX_PROCESSES`           | 16    | Processes per run. Terminated processes keep their slot and ID (IDs are never reused). |
| `MAX_HISTORY`             | 128   | Entries in the global transition log. |
| `MAX_PROCESS_TRANSITIONS` | 16    | Transitions remembered per process. |

When a history limit is reached, the next transition is **rejected** with
`SIM_ERR_HISTORY_FULL` and nothing changes, so a process's state and the log
never disagree. `reset_simulator()` starts a fresh run (next process is `P1`).

### Status codes

| Code                           | Meaning |
|--------------------------------|---------|
| `SIM_OK`                       | Success |
| `SIM_ERR_INVALID_ARGUMENT`     | NULL pointer, buffer too small, index out of range |
| `SIM_ERR_INVALID_PROCESS_ID`   | No process with that ID |
| `SIM_ERR_INVALID_ACTION`       | Action value not one of the six |
| `SIM_ERR_INVALID_TRANSITION`   | Action not allowed in the current state |
| `SIM_ERR_TABLE_FULL`           | Process table full |
| `SIM_ERR_HISTORY_FULL`         | Global or per-process history full |

### Main functions

| Function | Purpose |
|----------|---------|
| `simulator_init(sim)` / `reset_simulator(sim)` | Start / restart a run |
| `create_process(sim, out_id, size)` | New process in NEW with the next ID |
| `find_process_by_id(sim, "P1")` | Read-only lookup (NULL if missing) |
| `get_process_state(sim, "P1", &state)` | Current state |
| `transition_process(sim, "P1", ACTION_ADMIT)` | Validate and apply an action |
| `is_valid_transition(from, action, &next)` | Pure rule check |
| `get_process_count(sim)` / `get_history_count(sim)` | Counts |
| `get_history_entry(sim, i, &entry)` | Read a log entry |
| `get_state_name` / `get_action_name` / `get_status_message` | Readable text |
| `print_processes` / `print_transition_history` / `print_process_history` | Output to a `FILE *` |

Full contracts are in the comments of `include/process_simulator.h`.

---

## 3. Folder structure

```
Process_State_Transition_Simulator/
├── include/
│   └── process_simulator.h
├── src/
│   ├── process_simulator.c
│   └── main.c
├── tests/
│   └── test_simulator.c
├── docs/
│   ├── state_transition_diagram.md
│   ├── test_cases.md
│   ├── project_report.md
│   └── viva_preparation.md
├── Makefile
├── README.md
└── .gitignore
```

### Creating it by hand in VS Code (if you are not using the ZIP)

1. Open VS Code, **File > Open Folder...**, create a new folder named
   `Process_State_Transition_Simulator`, and open it.
2. In the Explorer panel use the **New Folder** icon to create `include`,
   `src`, `tests` and `docs`.
3. Select a folder, click **New File**, and create each file with the exact
   name from the tree above. Paste in the contents.
4. Save every file with **Ctrl+S**.

---

## 4. Windows setup (GCC with MSYS2)

### 4.1 Check whether GCC is already installed

Open **PowerShell** (Start menu > type `powershell`) and run:

```
gcc --version
```

If it prints something like `gcc (Rev..., Built by MSYS2 project) 13.x.x`, skip to section 5.
If you see *"'gcc' is not recognized as the name of a cmdlet..."*, install it:

### 4.2 Install GCC (MinGW-w64 via MSYS2)

1. Download the installer from <https://www.msys2.org> and run it. Keep the
   default folder `C:\msys64`.
2. When it finishes, open **"MSYS2 UCRT64"** from the Start menu (a terminal window).
3. Install the compiler and make:
   ```
   pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
   ```
   Press Enter to accept the defaults and `Y` when asked.
4. Add the compiler to Windows PATH:
   - Start menu > search **"Edit the system environment variables"** > **Environment Variables...**
   - Under *User variables*, select **Path** > **Edit** > **New**
   - Add `C:\msys64\ucrt64\bin` > **OK** on every window.
5. **Close and reopen** PowerShell and VS Code, then check:
   ```
   gcc --version
   ```

`make` is optional. If you want it in PowerShell, the MSYS2 toolchain provides
`mingw32-make`; use it in place of `make`. If neither works, use the direct
`gcc` commands below - they are always enough.

---

## 5. Build, run and test

Open the project folder in VS Code, then **Terminal > New Terminal**
(this opens in the project folder). Check with `dir` that you can see
`include`, `src`, `tests`.

### 5.1 Compile the demonstration

```
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/process_simulator.c src/main.c -o simulator.exe
```

### 5.2 Run the demonstration

```
.\simulator.exe
```

(In Git Bash / MSYS2 shell use `./simulator.exe`.)

### 5.3 Compile the automated tests

```
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/process_simulator.c tests/test_simulator.c -o test_simulator.exe
```

### 5.4 Run the tests

```
.\test_simulator.exe
```

You should see a `[PASS]` line per test and finally
`Tests run: 27, passed: 27, failed: 0` / `ALL TESTS PASSED`. If a test fails,
the `[FAIL]` line is followed by the line number of the failed check.

### 5.5 Using make (optional)

```
make          # build simulator
make test     # build and run tests
make clean    # delete generated executables
```

(Use `mingw32-make` if `make` is not found. `make clean` uses `rm`, which is
available in an MSYS2 shell; in plain PowerShell delete the `.exe` files with
`del simulator.exe, test_simulator.exe`.)

---

## 6. Troubleshooting

| Message / symptom | Cause and fix |
|---|---|
| `'gcc' is not recognized` | GCC not installed or not on PATH. Redo section 4.2, then close and reopen the terminal. |
| `fatal error: process_simulator.h: No such file or directory` | Missing `-Iinclude`, or you are not in the project root folder. Run `dir` and check you see `include`. |
| `undefined reference to 'create_process'` (or any engine function) | `src/process_simulator.c` is missing from the command. Both `.c` files must be listed. |
| `undefined reference to 'main'` | You compiled only `process_simulator.c`, or `main.c` is empty/unsaved. Save the files (Ctrl+S). |
| `multiple definition of 'main'` | You listed both `src/main.c` and `tests/test_simulator.c`. Use one of them per program. |
| `cannot open output file simulator.exe: Permission denied` | The program is still running, or antivirus is locking the file. Close it and retry. |
| `.\simulator.exe` is not recognized | You are in the wrong folder, or the compile failed. Check the compile output first. |
| `make: command not found` | Use `mingw32-make`, or the direct `gcc` commands. |
| Changes do not appear | The file was not saved. Look for a dot on the editor tab; press Ctrl+S. |

---

## 7. Sharing the engine with your teammate

The frontend only needs the **engine**, not the demo or tests:

- `include/process_simulator.h`
- `src/process_simulator.c`

Send these two files (or the whole ZIP / a Git repository). Tell your teammate
not to change the transition table; rule changes should go through you so
there is only one source of truth.

---

## 8. Connecting to the Replit frontend later

The engine is plain C with no global state and no input/output of its own, so
it can be wrapped without touching the rules. Which wrapper is right depends
on the frontend's actual technology - **ask your friend what the Replit
project is built with** (browser-only JavaScript, Node.js backend, Python
backend, ...). C does not run in a browser or as a server by itself; it must be
compiled first.

| Frontend situation | Simplest safe option |
|---|---|
| Has a backend (Node.js, Python, ...) that can start a program | **Option B**: compile the engine into a small command-line program and let the backend start it and read its output. Keep it one long-running process (reading commands from stdin), because a new process per request would lose the simulator state. Replit must have a C compiler available in its environment (not assumed here - check). |
| Browser-only, no backend | **Option C**: compile to WebAssembly with Emscripten and call a few exported functions from JavaScript. The state then lives in the browser. |
| Wants a text protocol | **Option A**: see below. |

**Option A - JSON protocol.** Do not parse arbitrary JSON in C with string
matching. Safer: use a small, fixed *line* protocol for requests
(for example `TRANSITION P1 ADMIT`, `CREATE`, `RESET`, `LIST`), which C can
parse reliably with `sscanf`/`strcmp`, and have the engine write one JSON line
per response (that direction is easy to get right). If full JSON requests are
required, use an established parser library (for example cJSON) rather than
hand-written code.

**Option B - HTTP wrapper.** Add a thin server (Node/Express or Flask) that
exposes URLs such as `POST /transition`, translates each request to one
protocol line for the C program, and returns its reply. The wrapper adds no
rules.

**Option C - WebAssembly.** Because the engine uses fixed arrays, no `malloc`
and no globals, it is a good fit. A small wrapper file with a few exported
functions (`create`, `transition`, `get_state`, `reset`) keeps the
JavaScript side simple.

None of these has been built or tested yet; they are **future work** (the
wrapper files are not part of this repository). Whichever is chosen, the
engine files stay unchanged.

---

## 9. Submission checklist

- [ ] Project compiles with **no warnings** on your own machine
- [ ] `simulator.exe` demo runs and output matches the expected sequence
- [ ] `test_simulator.exe` prints `ALL TESTS PASSED` on **your** machine
- [ ] Your own test run recorded in `docs/test_cases.md` (date, compiler, counts)
- [ ] `docs/project_report.md` title page filled in (names, register numbers, department, college, year)
- [ ] Sample output section of the report matches what you see on your machine
- [ ] State diagram renders (or use the plain-text version)
- [ ] You can explain every file and each function (see `docs/viva_preparation.md`)
- [ ] `.exe` files not included in the submission (see `.gitignore`)
- [ ] Header + engine file shared with your teammate, integration approach agreed

## 10. References

- Silberschatz, Galvin, Gagne - *Operating System Concepts*, Wiley.
- Tanenbaum, Bos - *Modern Operating Systems*, Pearson.
- Stallings - *Operating Systems: Internals and Design Principles*, Pearson.
- Kernighan, Ritchie - *The C Programming Language*, Prentice Hall.
- ISO/IEC 9899:2011 (C11); GCC documentation at <https://gcc.gnu.org/onlinedocs/>.
