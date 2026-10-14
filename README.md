# Process State Transition Simulator

An interactive Operating Systems project that simulates and visualises how a process moves between
**New, Ready, Running, Waiting and Terminated**. Plain HTML, CSS and JavaScript. No framework, no
backend, no database, no dependencies.

## Run it

**On Replit:** import the folder (or upload the files) and press **Run**. `.replit` starts
`node server.js`, which serves the app on port 3000 (shown in the Webview).

**Locally:** run `node server.js` and open http://localhost:3000, or just double-click `index.html`.
The server is optional.

**Tests:** `node tests/simulation.test.js` (or `npm test`). Uses only Node's built-in `assert`.

> Fonts load from Google Fonts. Offline, the app falls back to system fonts and works the same.

## Files

| File | Purpose |
|---|---|
| `index.html` | Page structure: title, diagram, controls, process table, event log. |
| `style.css` | Design tokens, per-state colours, layout and responsive rules. |
| `simulation.js` | **Simulation module.** Transition rules, processes, event log. No DOM code. |
| `app.js` | **UI layer.** Draws the diagram, table and log, and wires the buttons. No rules. |
| `server.js` | Zero-dependency static server for Replit. |
| `tests/simulation.test.js` | Logic tests (rules, rejections, log, reset, single-CPU mode). |

## Transition rules (defined once, in `simulation.js`)

| Action | From → To | Meaning |
|---|---|---|
| Admit | New → Ready | OS admits the process to the ready queue |
| Dispatch | Ready → Running | Scheduler gives it the CPU |
| Preempt | Running → Ready | Time slice ends |
| Block | Running → Waiting | Waits for I/O or an event |
| Wake up | Waiting → Ready | I/O or event completes |
| Terminate | Running → Terminated | Process exits |

Anything else is rejected with an explanation, never applied, and logged as *Rejected*.
`app.js` builds the diagram arrows from this same list, so the picture cannot contradict the rules.

## Integrating the teammate's simulation logic

Everything the UI needs is in the interface documented at the top of `simulation.js`. To integrate:

1. Keep `app.js`, `index.html` and `style.css` as they are.
2. Replace (or wrap) `simulation.js` so that `window.ProcessSim` exposes `STATES`, `STATE_ORDER`,
   `TRANSITIONS`, `EVENT_TYPES` and a `ProcessSimulation` class with the same methods
   (`createProcess`, `checkTransition`, `transition`, `reset`, `setSingleCpu`, `getProcesses`,
   `getProcess`, `getEvents`, `getTransitionsFrom`).
3. If their rules add a transition, add one entry for it to `EDGE_GEOMETRY` in `app.js`
   (the console reports any rule that has no arrow geometry).

## Main functions

**simulation.js** — `ProcessSimulation`
- `createProcess()` makes P1, P2, … in New and logs it.
- `checkTransition(id, action)` is a dry run: says whether a move is legal and why not.
- `transition(id, action)` applies a legal move or logs a rejection, and returns the result.
- `reset()` clears processes, history and counters, then logs the reset as event #1.

**app.js**
- `buildDiagram()` creates the SVG nodes and arrows once from the rule list.
- `updateDiagram()` highlights the selected process's state and its valid next arrows, and shows process chips inside each state.
- `triggerFiring()` animates the arrow just taken and the state it arrived in.
- `renderControls()`, `renderTable()`, `renderLog()`, `renderAll()` redraw the UI from simulation state.
- `handleCreate()`, `handleTransition()`, `handleReset()` are the button handlers.

## Using it in a demo

1. Press **Create process**. P1 appears in New.
2. Press **Admit**, then **Dispatch**. Watch the arrow animate and P1 move to Running.
3. Press **Block**, **Wake up**, **Dispatch**, **Preempt**. Every arrow in the diagram gets used.
4. Press a dimmed button (for example **Wake up** on a Running process) to show a rejection and its log entry.
5. Turn on **Single CPU mode** and dispatch two processes to show why the CPU is a contended resource.
6. Press **Terminate**, then try any action on that process. Press **Reset simulation** to start over.
