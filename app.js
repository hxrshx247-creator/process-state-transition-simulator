/**
 * app.js — user interface for the Process State Transition Simulator.
 *
 * This file only draws things and wires up buttons. It never decides whether a
 * transition is allowed: it asks the simulation module (window.ProcessSim) and
 * shows the answer. The diagram's arrows are generated from the module's
 * TRANSITIONS list, so they always match the real rules.
 */
(function () {
  'use strict';

  const { ProcessSimulation, STATE_ORDER, TRANSITIONS, EVENT_TYPES } = window.ProcessSim;

  const SVG_NS = 'http://www.w3.org/2000/svg';
  const FIRING_DURATION_MS = 1500;
  const MAX_CHIPS_PER_NODE = 4;

  // ── Diagram geometry (viewBox 900 × 440) ───────────────────────────────
  const NODE_SIZE = { width: 140, height: 84 };
  const NODE_POSITIONS = {
    New: { x: 10, y: 100 },
    Ready: { x: 250, y: 100 },
    Running: { x: 520, y: 100 },
    Terminated: { x: 750, y: 100 },
    Waiting: { x: 385, y: 330 },
  };
  // One entry per rule in TRANSITIONS. Missing entries are reported at start-up.
  const EDGE_GEOMETRY = {
    admit: { d: 'M152 142 L246 142', label: { x: 199, y: 128, anchor: 'middle' } },
    dispatch: { d: 'M392 120 C435 78 475 78 516 120', label: { x: 455, y: 70, anchor: 'middle' } },
    preempt: { d: 'M518 164 C475 206 435 206 394 164', label: { x: 455, y: 226, anchor: 'middle' } },
    block: { d: 'M600 188 C610 300 580 372 529 372', label: { x: 606, y: 322, anchor: 'start' } },
    wake: { d: 'M383 372 C330 372 310 300 310 188', label: { x: 304, y: 322, anchor: 'end' } },
    exit: { d: 'M662 142 L746 142', label: { x: 704, y: 128, anchor: 'middle' } },
  };

  const LOG_TAGS = {
    [EVENT_TYPES.CREATE]: 'Created',
    [EVENT_TYPES.TRANSITION]: 'Transition',
    [EVENT_TYPES.INVALID]: 'Rejected',
    [EVENT_TYPES.RESET]: 'Reset',
  };

  const TRANSITION_BY_ACTION = Object.fromEntries(TRANSITIONS.map((t) => [t.action, t]));

  // ── Application state (UI only; simulation state lives in `sim`) ───────
  const sim = new ProcessSimulation();
  const ui = { selectedId: null, firing: null, firingTimer: null };

  const byId = (id) => document.getElementById(id);
  const dom = {
    createButton: byId('create-btn'),
    resetButton: byId('reset-btn'),
    select: byId('process-select'),
    nextMoves: byId('next-moves'),
    actionGrid: byId('action-grid'),
    status: byId('status'),
    singleCpu: byId('single-cpu'),
    diagram: byId('state-diagram'),
    diagramNote: byId('diagram-note'),
    tableWrap: byId('table-wrap'),
    tableBody: byId('process-rows'),
    tableEmpty: byId('table-empty'),
    processCount: byId('process-count'),
    logList: byId('event-log'),
    logEmpty: byId('log-empty'),
    eventCount: byId('event-count'),
  };

  const diagramParts = { edges: new Map(), nodes: new Map() };
  const actionButtons = new Map();

  // ── Small DOM helpers ──────────────────────────────────────────────────
  function svgEl(name, attrs = {}, text) {
    const node = document.createElementNS(SVG_NS, name);
    Object.entries(attrs).forEach(([key, value]) => node.setAttribute(key, value));
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function el(tag, options = {}, ...children) {
    const node = document.createElement(tag);
    if (options.className) node.className = options.className;
    if (options.text !== undefined) node.textContent = options.text;
    if (options.attrs) Object.entries(options.attrs).forEach(([k, v]) => node.setAttribute(k, v));
    if (options.dataset) Object.assign(node.dataset, options.dataset);
    children.forEach((child) => child && node.append(child));
    return node;
  }

  function cssVar(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  function getSelectedProcess() {
    return ui.selectedId ? sim.getProcess(ui.selectedId) : null;
  }

  function formatTime(date) {
    return date.toLocaleTimeString([], { hour12: false });
  }

  // ── Diagram: build once, update on every change ────────────────────────
  function buildDiagram() {
    const svg = dom.diagram;
    const defs = svgEl('defs');

    const markers = [
      ['arrow-neutral', cssVar('--edge')],
      ['arrow-available', cssVar('--ink')],
      ...STATE_ORDER.map((state) => [`arrow-to-${state}`, cssVar(`--c-${state.toLowerCase()}`)]),
    ];
    markers.forEach(([id, color]) => {
      const marker = svgEl('marker', {
        id, viewBox: '0 0 10 10', refX: 9, refY: 5, markerWidth: 6, markerHeight: 6, orient: 'auto',
      });
      marker.append(svgEl('path', { d: 'M0 0 L10 5 L0 10 z', fill: color }));
      defs.append(marker);
    });
    svg.append(defs);

    // Arrows come straight from the rule list: no rule, no arrow.
    TRANSITIONS.forEach((transition) => {
      const geometry = EDGE_GEOMETRY[transition.action];
      if (!geometry) {
        console.error(`No diagram geometry for transition "${transition.action}".`);
        return;
      }
      const group = svgEl('g', { class: 'edge', 'data-action': transition.action });
      group.style.setProperty('--fire', `var(--c-${transition.to.toLowerCase()})`);
      group.append(
        svgEl('path', { class: 'edge-path', d: geometry.d, 'marker-end': 'url(#arrow-neutral)' }),
        svgEl(
          'text',
          { class: 'edge-label', x: geometry.label.x, y: geometry.label.y, 'text-anchor': geometry.label.anchor },
          transition.event
        )
      );
      svg.append(group);
      diagramParts.edges.set(transition.action, group);
    });

    STATE_ORDER.forEach((state) => {
      const { x, y } = NODE_POSITIONS[state];
      const group = svgEl('g', { class: 'node', 'data-state': state, transform: `translate(${x} ${y})` });
      const chips = svgEl('g', { class: 'chips', transform: 'translate(0 46)' });
      const emptyLabel = svgEl('text', { class: 'node-empty', x: NODE_SIZE.width / 2, y: 62 }, 'no processes');
      group.append(
        svgEl('rect', { class: 'node-box', width: NODE_SIZE.width, height: NODE_SIZE.height, rx: 14 }),
        svgEl('text', { class: 'node-title', x: NODE_SIZE.width / 2, y: 32 }, state),
        emptyLabel,
        chips
      );
      svg.append(group);
      diagramParts.nodes.set(state, { group, chips, emptyLabel });
    });

    svg.addEventListener('click', (event) => {
      const chip = event.target.closest('.chip[data-pid]');
      if (chip) selectProcess(chip.dataset.pid);
    });
  }

  function renderChips(part, processesInState, selected) {
    part.chips.replaceChildren();
    part.emptyLabel.classList.toggle('is-hidden', processesInState.length > 0);
    if (processesInState.length === 0) return;

    let visible = processesInState;
    let hiddenCount = 0;
    if (processesInState.length > MAX_CHIPS_PER_NODE) {
      visible = processesInState.slice(0, MAX_CHIPS_PER_NODE - 1);
      hiddenCount = processesInState.length - visible.length;
      // Always keep the selected process visible.
      if (selected && !visible.some((p) => p.id === selected.id) && processesInState.some((p) => p.id === selected.id)) {
        visible[visible.length - 1] = selected;
      }
    }

    const labels = visible.map((p) => ({ pid: p.id, text: p.id, selected: selected && p.id === selected.id }));
    if (hiddenCount > 0) labels.push({ pid: null, text: `+${hiddenCount}`, selected: false });

    const chipWidth = 30;
    const gap = 3;
    const total = labels.length * chipWidth + (labels.length - 1) * gap;
    const startX = (NODE_SIZE.width - total) / 2;

    labels.forEach((label, index) => {
      const chip = svgEl('g', {
        class: `chip${label.selected ? ' is-selected' : ''}${label.pid ? '' : ' chip--more'}`,
        transform: `translate(${startX + index * (chipWidth + gap)} 0)`,
      });
      if (label.pid) chip.setAttribute('data-pid', label.pid);
      chip.append(
        svgEl('rect', { width: chipWidth, height: 22, rx: 6 }),
        svgEl('text', { x: chipWidth / 2, y: 15 }, label.text)
      );
      part.chips.append(chip);
    });
  }

  function updateDiagram(processes) {
    const selected = getSelectedProcess();

    diagramParts.edges.forEach((group, action) => {
      const transition = TRANSITION_BY_ACTION[action];
      const isFiring = Boolean(ui.firing) && ui.firing.action === action;
      const isAvailable = Boolean(selected) && selected.state === transition.from;
      group.classList.toggle('is-firing', isFiring);
      group.classList.toggle('is-available', isAvailable && !isFiring);
      group.classList.toggle('is-dim', Boolean(selected) && !isAvailable && !isFiring);
      const marker = isFiring ? `arrow-to-${transition.to}` : isAvailable ? 'arrow-available' : 'arrow-neutral';
      group.querySelector('.edge-path').setAttribute('marker-end', `url(#${marker})`);
    });

    const counts = {};
    diagramParts.nodes.forEach((part, state) => {
      const inState = processes.filter((p) => p.state === state);
      counts[state] = inState.length;
      part.group.classList.toggle('is-current', Boolean(selected) && selected.state === state);
      part.group.classList.toggle('is-entered', Boolean(ui.firing) && ui.firing.to === state);
      renderChips(part, inState, selected);
    });

    dom.diagram.setAttribute(
      'aria-label',
      'Process state diagram. ' + STATE_ORDER.map((s) => `${s}: ${counts[s]}`).join(', ') + '.'
    );

    dom.diagramNote.textContent = selected
      ? `${selected.id} is ${selected.state}. Bold arrows are the moves it may take next.`
      : processes.length
        ? 'Select a process to highlight its state and the arrows it may take next.'
        : 'Create a process to see it enter the New state.';
  }

  /** Briefly animates the arrow that was just taken and the state it arrived in. */
  function triggerFiring(transition) {
    clearTimeout(ui.firingTimer);
    ui.firing = null;
    updateDiagram(sim.getProcesses());
    dom.diagram.getBoundingClientRect(); // force reflow so repeated animations restart
    ui.firing = { action: transition.action, to: transition.to };
    ui.firingTimer = setTimeout(() => {
      ui.firing = null;
      updateDiagram(sim.getProcesses());
    }, FIRING_DURATION_MS);
  }

  // ── Controls ───────────────────────────────────────────────────────────
  function buildActionButtons() {
    TRANSITIONS.forEach((transition) => {
      const button = el(
        'button',
        { className: 'action', attrs: { type: 'button' }, dataset: { action: transition.action, state: transition.to } },
        el('span', { className: 'action-label', text: transition.label }),
        el('span', { className: 'action-route', text: `${transition.from} → ${transition.to}` }),
        el('span', { className: 'sr-only', text: 'Not valid for the selected process right now.', attrs: { hidden: '' } })
      );
      button.addEventListener('click', () => handleTransition(transition.action));
      dom.actionGrid.append(button);
      actionButtons.set(transition.action, button);
    });
  }

  function renderControls(processes) {
    const hasProcesses = processes.length > 0;
    const selected = getSelectedProcess();

    // Process selector
    dom.select.replaceChildren();
    if (!hasProcesses) {
      dom.select.append(el('option', { text: 'No processes yet', attrs: { value: '' } }));
    } else {
      if (!selected) dom.select.append(el('option', { text: 'Choose a process…', attrs: { value: '' } }));
      processes.forEach((p) => {
        dom.select.append(el('option', { text: `${p.id} — ${p.state}`, attrs: { value: p.id } }));
      });
      dom.select.value = selected ? selected.id : '';
    }
    dom.select.disabled = !hasProcesses;

    // Transition buttons
    actionButtons.forEach((button, action) => {
      const check = selected ? sim.checkTransition(selected.id, action) : null;
      const isValid = Boolean(check && check.valid);
      button.disabled = !hasProcesses;
      button.classList.toggle('is-unavailable', hasProcesses && !isValid);
      button.querySelector('.sr-only').hidden = !hasProcesses || isValid;
      button.title = !hasProcesses
        ? 'Create a process first'
        : isValid
          ? TRANSITION_BY_ACTION[action].summary
          : selected
            ? 'Not valid right now. Press to see why it is rejected.'
            : 'Select a process first';
    });

    // Hint about the next legal moves
    dom.nextMoves.replaceChildren();
    if (!hasProcesses) {
      dom.nextMoves.textContent = 'Create a process to enable the transition buttons.';
    } else if (!selected) {
      dom.nextMoves.textContent = 'Pick a process from the list or the table.';
    } else if (selected.state === 'Terminated') {
      dom.nextMoves.append(
        el('strong', { text: selected.id }),
        ` is Terminated. No further transitions are possible.`
      );
    } else {
      const moves = sim.getTransitionsFrom(selected.state).map((t) => `${t.label} (→ ${t.to})`).join(', ');
      dom.nextMoves.append(el('strong', { text: selected.id }), ` is ${selected.state}. Valid next: ${moves}.`);
    }
  }

  function setStatus(message, kind) {
    dom.status.textContent = message;
    dom.status.className = `status is-${kind}`;
    // Restart the nudge animation on repeated errors.
    if (kind === 'error') {
      dom.status.getBoundingClientRect();
    }
  }

  // ── Table and log ──────────────────────────────────────────────────────
  function renderTable(processes) {
    const focusedPid = dom.tableBody.contains(document.activeElement) ? document.activeElement.dataset.pid : null;

    dom.tableBody.replaceChildren();
    dom.tableWrap.hidden = processes.length === 0;
    dom.tableEmpty.hidden = processes.length > 0;
    dom.processCount.textContent = processes.length
      ? `${processes.length} process${processes.length === 1 ? '' : 'es'}`
      : '';

    processes.forEach((process) => {
      const isSelected = process.id === ui.selectedId;
      const classes = [isSelected ? 'is-selected' : '', process.state === 'Terminated' ? 'is-terminated' : '']
        .filter(Boolean)
        .join(' ');
      const row = el(
        'tr',
        { className: classes, dataset: { pid: process.id } },
        el('td', {}, el('button', {
          className: 'row-select',
          text: process.id,
          attrs: { type: 'button', 'aria-pressed': String(isSelected) },
          dataset: { pid: process.id },
        })),
        el('td', {}, el('span', { className: 'badge', text: process.state, dataset: { state: process.state } })),
        el('td', { className: 'num', text: String(process.transitionCount) })
      );
      dom.tableBody.append(row);
    });

    if (focusedPid) {
      const button = dom.tableBody.querySelector(`button[data-pid="${focusedPid}"]`);
      if (button) button.focus();
    }
  }

  function buildLogItem(event) {
    const time = el('time', { text: formatTime(event.time), attrs: { datetime: event.time.toISOString() } });
    return el(
      'li',
      { className: `log-item log-item--${event.type}` },
      el('div', { className: 'log-meta' }, el('span', { className: 'log-seq', text: `#${event.seq}` }), time),
      el('div', { className: 'log-body' },
        el('span', { className: 'log-tag', text: LOG_TAGS[event.type] || event.type }),
        el('span', { className: 'log-text', text: event.message }))
    );
  }

  function renderLog() {
    const events = sim.getEvents();
    dom.logList.replaceChildren(...events.slice().reverse().map(buildLogItem));
    dom.logList.hidden = events.length === 0;
    dom.logEmpty.hidden = events.length > 0;
    dom.eventCount.textContent = events.length ? `${events.length} event${events.length === 1 ? '' : 's'}` : '';
  }

  function renderAll() {
    const processes = sim.getProcesses();
    if (ui.selectedId && !sim.getProcess(ui.selectedId)) ui.selectedId = null;
    renderControls(processes);
    renderTable(processes);
    renderLog();
    updateDiagram(processes);
  }

  // ── Actions ────────────────────────────────────────────────────────────
  function selectProcess(processId) {
    ui.selectedId = processId || null;
    renderAll();
  }

  function handleCreate() {
    const { process } = sim.createProcess();
    ui.selectedId = process.id;
    setStatus(`${process.id} created in the New state. Press Admit to move it to Ready.`, 'ok');
    renderAll();
  }

  function handleTransition(action) {
    if (!ui.selectedId) {
      setStatus('Select a process first.', 'error');
      return;
    }
    const result = sim.transition(ui.selectedId, action);
    if (result.ok) {
      setStatus(result.event.message, 'ok');
      triggerFiring(result.transition);
    } else {
      setStatus(result.reason, 'error');
    }
    renderAll();
  }

  function handleReset() {
    clearTimeout(ui.firingTimer);
    ui.firing = null;
    ui.selectedId = null;
    sim.reset();
    setStatus('Simulation reset. Create a process to start again.', 'info');
    renderAll();
  }

  function handleSingleCpuToggle() {
    sim.setSingleCpu(dom.singleCpu.checked);
    setStatus(
      dom.singleCpu.checked
        ? 'Single CPU mode on: Dispatch is rejected while another process is Running.'
        : 'Single CPU mode off: several processes may be Running at once.',
      'info'
    );
    renderAll();
  }

  // ── Start-up ───────────────────────────────────────────────────────────
  function init() {
    buildDiagram();
    buildActionButtons();

    dom.createButton.addEventListener('click', handleCreate);
    dom.resetButton.addEventListener('click', handleReset);
    dom.select.addEventListener('change', () => selectProcess(dom.select.value));
    dom.singleCpu.addEventListener('change', handleSingleCpuToggle);
    dom.tableBody.addEventListener('click', (event) => {
      const row = event.target.closest('tr[data-pid]');
      if (row) selectProcess(row.dataset.pid);
    });

    dom.singleCpu.checked = sim.singleCpu;
    setStatus('Press Create process to begin.', 'info');
    renderAll();
  }

  init();
})();
