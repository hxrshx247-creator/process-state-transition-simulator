
/**
 * c-api.js
 * Client-side helper for communicating with the C simulator through server.js.
 */
(function () {
  'use strict';

  async function request(url, options = {}) {
    const response = await fetch(url, {
      headers: { 'Content-Type': 'application/json' },
      ...options,
    });

    const data = await response.json();

    if (!response.ok || data.ok === false) {
      throw new Error(data.message || `Request failed (${response.status})`);
    }

    return data;
  }

  function toUiState(state) {
    const states = {
      NEW: 'New',
      READY: 'Ready',
      RUNNING: 'Running',
      WAITING: 'Waiting',
      TERMINATED: 'Terminated',
    };

    return states[state] || state;
  }

  function toCAction(action) {
    const actions = {
      admit: 'ADMIT',
      dispatch: 'DISPATCH',
      preempt: 'INTERRUPT',
      block: 'IO_REQUEST',
      wake: 'IO_COMPLETE',
      exit: 'EXIT',
    };

    const converted = actions[action];
    if (!converted) throw new Error(`Unknown transition action: ${action}`);
    return converted;
  }

  window.CSimulatorAPI = {
    async listProcesses() {
      const data = await request('/api/processes');
      return data.processes.map((process) => ({
        ...process,
        state: toUiState(process.state),
        transitionCount: 0,
      }));
    },

    async createProcess() {
      const data = await request('/api/processes', { method: 'POST' });
      return data;
    },

    async transitionProcess(id, action) {
      return request('/api/transition', {
        method: 'POST',
        body: JSON.stringify({ id, action: toCAction(action) }),
      });
    },

    async reset() {
      return request('/api/reset', { method: 'POST' });
    },
  };
})();
