/**
 * simulation.js — process-state simulation module (no DOM, no UI code).
 *
 * This file is the ONLY place that knows the transition rules. The interface
 * (app.js) reads the rules from here, so the diagram can never show an arrow
 * that the simulation does not allow.
 *
 * ─── Integration contract ────────────────────────────────────────────────
 * If the simulation-logic teammate delivers their own implementation, replace
 * the body of this file (or wrap their code) so that `window.ProcessSim`
 * (browser) / `module.exports` (Node) exposes the same names:
 *
 *   STATES            { NEW, READY, RUNNING, WAITING, TERMINATED } -> strings
 *   STATE_ORDER       array of the five state names
 *   TRANSITIONS       array of { action, label, from, to, event, summary }
 *   EVENT_TYPES       { CREATE, TRANSITION, INVALID, RESET } -> strings
 *   ProcessSimulation class with:
 *     createProcess()                -> { ok, process, event }
 *     checkTransition(id, action)    -> { valid, code, reason, transition, process }
 *     transition(id, action)         -> { ok, code, reason, transition, process, event }
 *     reset()                        -> { ok, event }
 *     setSingleCpu(boolean)          optional rule: at most one Running process
 *     getProcesses()                 -> [{ id, state, transitionCount }]
 *     getProcess(id)                 -> process | null
 *     getEvents()                    -> [{ seq, type, time, processId, from, to,
 *                                          action, message }]
 *     getTransitionsFrom(state)      -> TRANSITIONS entries leaving `state`
 *
 * Every method returns plain copies, so callers cannot corrupt internal state.
 * ─────────────────────────────────────────────────────────────────────────
 */
(function (root, factory) {
  'use strict';
  const api = factory();
  if (typeof module === 'object' && module.exports) {
    module.exports = api; // Node (tests)
  } else {
    root.ProcessSim = api; // Browser
  }
})(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  const STATES = Object.freeze({
    NEW: 'New',
    READY: 'Ready',
    RUNNING: 'Running',
    WAITING: 'Waiting',
    TERMINATED: 'Terminated',
  });

  const STATE_ORDER = Object.freeze([
    STATES.NEW,
    STATES.READY,
    STATES.RUNNING,
    STATES.WAITING,
    STATES.TERMINATED,
  ]);

  const EVENT_TYPES = Object.freeze({
    CREATE: 'create',
    TRANSITION: 'transition',
    INVALID: 'invalid',
    RESET: 'reset',
  });

  /**
   * The complete rule set. `action` is the stable identifier used by the UI;
   * `event` is the short phrase shown on the diagram arrow.
   */
  const TRANSITIONS = Object.freeze(
    [
      {
        action: 'admit',
        label: 'Admit',
        from: STATES.NEW,
        to: STATES.READY,
        event: 'admit',
        summary: 'The OS admitted it to the ready queue.',
      },
      {
        action: 'dispatch',
        label: 'Dispatch',
        from: STATES.READY,
        to: STATES.RUNNING,
        event: 'scheduler dispatch',
        summary: 'The scheduler gave it the CPU.',
      },
      {
        action: 'preempt',
        label: 'Preempt',
        from: STATES.RUNNING,
        to: STATES.READY,
        event: 'timeout / preempt',
        summary: 'Its time slice ended, so it returned to the ready queue.',
      },
      {
        action: 'block',
        label: 'Block',
        from: STATES.RUNNING,
        to: STATES.WAITING,
        event: 'I/O or event wait',
        summary: 'It is now waiting for I/O or an event.',
      },
      {
        action: 'wake',
        label: 'Wake up',
        from: STATES.WAITING,
        to: STATES.READY,
        event: 'I/O or event done',
        summary: 'The I/O or event it was waiting for completed.',
      },
      {
        action: 'exit',
        label: 'Terminate',
        from: STATES.RUNNING,
        to: STATES.TERMINATED,
        event: 'exit',
        summary: 'It finished executing.',
      },
    ].map(Object.freeze)
  );

  function findTransition(action) {
    return TRANSITIONS.find((transition) => transition.action === action) || null;
  }

  function failure(code, reason, transition, process) {
    return {
      valid: false,
      code,
      reason,
      transition: transition || null,
      process: process ? { ...process } : null,
    };
  }

  class ProcessSimulation {
    /**
     * @param {object}   [options]
     * @param {boolean}  [options.singleCpu=false] allow only one Running process
     * @param {Function} [options.now]             clock returning a Date (for tests)
     */
    constructor(options = {}) {
      this._singleCpu = Boolean(options.singleCpu);
      this._now = typeof options.now === 'function' ? options.now : () => new Date();
      this._clearState();
    }

    // ── Configuration ────────────────────────────────────────────────────
    get singleCpu() {
      return this._singleCpu;
    }

    setSingleCpu(enabled) {
      this._singleCpu = Boolean(enabled);
    }

    // ── Commands ─────────────────────────────────────────────────────────
    /** Creates the next process (P1, P2, ...) in the New state. */
    createProcess() {
      const process = {
        id: `P${this._nextProcessNumber++}`,
        state: STATES.NEW,
        transitionCount: 0,
      };
      this._processes.push(process);
      const event = this._record(EVENT_TYPES.CREATE, {
        processId: process.id,
        to: STATES.NEW,
        message: `${process.id} created in the New state.`,
      });
      return { ok: true, process: { ...process }, event };
    }

    /** Asks whether `action` may be applied to the process, without changing anything. */
    checkTransition(processId, action) {
      const transition = findTransition(action);
      if (!transition) {
        return failure('UNKNOWN_ACTION', `"${action}" is not a known transition.`);
      }

      const process = this._find(processId);
      if (!process) {
        return failure('NO_PROCESS', 'Select a process first.', transition);
      }

      if (process.state === STATES.TERMINATED) {
        return failure(
          'TERMINATED',
          `${process.id} has terminated, so it cannot change state again. Create a new process to keep experimenting.`,
          transition,
          process
        );
      }

      if (process.state !== transition.from) {
        const options = this.getTransitionsFrom(process.state)
          .map((t) => `${t.label} (→ ${t.to})`)
          .join(' or ');
        return failure(
          'WRONG_STATE',
          `${transition.label} only applies to a ${transition.from} process, but ${process.id} is ${process.state}. From ${process.state} you can: ${options}.`,
          transition,
          process
        );
      }

      if (this._singleCpu && transition.to === STATES.RUNNING) {
        const holder = this._processes.find(
          (p) => p.id !== process.id && p.state === STATES.RUNNING
        );
        if (holder) {
          return failure(
            'CPU_BUSY',
            `Single CPU mode: ${holder.id} already holds the CPU. Preempt, block or terminate it before dispatching ${process.id}.`,
            transition,
            process
          );
        }
      }

      return {
        valid: true,
        code: 'OK',
        reason: '',
        transition,
        process: { ...process },
      };
    }

    /**
     * Applies `action` to the process. Successful moves and rejected attempts
     * on a real process are both written to the event log.
     */
    transition(processId, action) {
      const check = this.checkTransition(processId, action);

      if (!check.valid) {
        // Nothing meaningful to log if there was no process to act on.
        if (check.code === 'NO_PROCESS' || check.code === 'UNKNOWN_ACTION') {
          return { ok: false, code: check.code, reason: check.reason, event: null };
        }
        const event = this._record(EVENT_TYPES.INVALID, {
          processId: check.process.id,
          from: check.process.state,
          to: check.transition.to,
          action,
          message: `Rejected: ${check.reason}`,
        });
        return {
          ok: false,
          code: check.code,
          reason: check.reason,
          transition: check.transition,
          process: check.process,
          event,
        };
      }

      const process = this._find(processId);
      const { transition } = check;
      const from = process.state;
      process.state = transition.to;
      process.transitionCount += 1;

      const event = this._record(EVENT_TYPES.TRANSITION, {
        processId: process.id,
        from,
        to: transition.to,
        action,
        message: `${process.id}: ${from} → ${transition.to}. ${transition.summary}`,
      });
      return {
        ok: true,
        code: 'OK',
        reason: '',
        transition,
        process: { ...process },
        event,
      };
    }

    /** Clears processes, history and counters, then logs the reset as event #1. */
    reset() {
      this._clearState();
      const event = this._record(EVENT_TYPES.RESET, {
        message: 'Simulation reset. All processes and history were cleared.',
      });
      return { ok: true, event };
    }

    // ── Queries ──────────────────────────────────────────────────────────
    getProcesses() {
      return this._processes.map((p) => ({ ...p }));
    }

    getProcess(processId) {
      const process = this._find(processId);
      return process ? { ...process } : null;
    }

    getEvents() {
      return this._events.map((e) => ({ ...e }));
    }

    getTransitionsFrom(state) {
      return TRANSITIONS.filter((t) => t.from === state);
    }

    // ── Internals ────────────────────────────────────────────────────────
    _clearState() {
      this._processes = [];
      this._events = [];
      this._nextProcessNumber = 1;
      this._nextEventSeq = 1;
    }

    _find(processId) {
      return this._processes.find((p) => p.id === processId) || null;
    }

    _record(type, fields) {
      const event = {
        seq: this._nextEventSeq++,
        type,
        time: this._now(),
        processId: null,
        from: null,
        to: null,
        action: null,
        message: '',
        ...fields,
      };
      this._events.push(event);
      return { ...event };
    }
  }

  return { STATES, STATE_ORDER, TRANSITIONS, EVENT_TYPES, ProcessSimulation };
});
