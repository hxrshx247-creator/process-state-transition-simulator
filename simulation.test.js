/**
 * Tests for simulation.js. Run with:  node tests/simulation.test.js
 * Uses only Node's built-in assert module.
 */
const assert = require('assert');
const { STATES, STATE_ORDER, TRANSITIONS, EVENT_TYPES, ProcessSimulation } = require('./simulation.js');
let passed = 0;
function test(name, fn) {
  try {
    fn();
    passed += 1;
    console.log(`  ok  ${name}`);
  } catch (error) {
    console.error(`FAIL  ${name}\n      ${error.message}`);
    process.exitCode = 1;
  }
}

/** Action sequences that bring a fresh New process into each state. */
const PATH_TO = {
  New: [],
  Ready: ['admit'],
  Running: ['admit', 'dispatch'],
  Waiting: ['admit', 'dispatch', 'block'],
  Terminated: ['admit', 'dispatch', 'exit'],
};
function processIn(sim, state) {
  const { process } = sim.createProcess();
  PATH_TO[state].forEach((action) => assert.ok(sim.transition(process.id, action).ok));
  assert.strictEqual(sim.getProcess(process.id).state, state);
  return process.id;
}

test('rule set matches the specification exactly', () => {
  const pairs = TRANSITIONS.map((t) => `${t.from}->${t.to}`).sort();
  assert.deepStrictEqual(pairs, [
    'New->Ready', 'Ready->Running', 'Running->Ready',
    'Running->Terminated', 'Running->Waiting', 'Waiting->Ready',
  ].sort());
  assert.strictEqual(new Set(TRANSITIONS.map((t) => t.action)).size, TRANSITIONS.length);
});

test('creates processes with unique sequential IDs in the New state', () => {
  const sim = new ProcessSimulation();
  const ids = [sim.createProcess(), sim.createProcess(), sim.createProcess()].map((r) => r.process.id);
  assert.deepStrictEqual(ids, ['P1', 'P2', 'P3']);
  sim.getProcesses().forEach((p) => assert.strictEqual(p.state, STATES.NEW));
});

test('performs every supported transition', () => {
  const sim = new ProcessSimulation();
  const { process } = sim.createProcess();
  const id = process.id;
  const walk = [
    ['admit', 'Ready'], ['dispatch', 'Running'], ['preempt', 'Ready'], ['dispatch', 'Running'],
    ['block', 'Waiting'], ['wake', 'Ready'], ['dispatch', 'Running'], ['exit', 'Terminated'],
  ];
  walk.forEach(([action, expected]) => {
    const result = sim.transition(id, action);
    assert.strictEqual(result.ok, true, `${action} should succeed`);
    assert.strictEqual(sim.getProcess(id).state, expected);
  });
  assert.strictEqual(sim.getProcess(id).transitionCount, walk.length);
  const covered = new Set(sim.getEvents().filter((e) => e.type === 'transition').map((e) => e.action));
  assert.strictEqual(covered.size, TRANSITIONS.length, 'every rule was exercised');
});

test('every (state, action) pair is accepted only if it is a rule', () => {
  STATE_ORDER.forEach((state) => {
    TRANSITIONS.forEach((t) => {
      const sim = new ProcessSimulation();
      const id = processIn(sim, state);
      const before = sim.getEvents().length;
      const result = sim.transition(id, t.action);
      const shouldPass = t.from === state;
      assert.strictEqual(result.ok, shouldPass, `${state} + ${t.action}`);
      if (shouldPass) {
        assert.strictEqual(sim.getProcess(id).state, t.to);
      } else {
        assert.strictEqual(sim.getProcess(id).state, state, 'state unchanged after rejection');
        assert.ok(result.reason.length > 10, 'helpful reason given');
        const logged = sim.getEvents().slice(before);
        assert.strictEqual(logged.length, 1);
        assert.strictEqual(logged[0].type, EVENT_TYPES.INVALID);
      }
    });
  });
});

test('a terminated process rejects everything with a clear message', () => {
  const sim = new ProcessSimulation();
  const id = processIn(sim, 'Terminated');
  TRANSITIONS.forEach((t) => {
    const result = sim.transition(id, t.action);
    assert.strictEqual(result.ok, false);
    assert.strictEqual(result.code, 'TERMINATED');
  });
});

test('acting with no process or an unknown action is rejected without logging', () => {
  const sim = new ProcessSimulation();
  assert.strictEqual(sim.transition(null, 'admit').code, 'NO_PROCESS');
  assert.strictEqual(sim.transition('P99', 'admit').code, 'NO_PROCESS');
  const { process } = sim.createProcess();
  assert.strictEqual(sim.transition(process.id, 'teleport').code, 'UNKNOWN_ACTION');
  assert.strictEqual(sim.getEvents().length, 1, 'only the creation was logged');
});

test('event log records type, counter, timestamp and message accurately', () => {
  let tick = 0;
  const sim = new ProcessSimulation({ now: () => new Date(2026, 0, 1, 12, 0, tick++) });
  const { process } = sim.createProcess();
  sim.transition(process.id, 'admit');
  sim.transition(process.id, 'exit'); // invalid: Ready -> Terminated
  const events = sim.getEvents();
  assert.deepStrictEqual(events.map((e) => e.seq), [1, 2, 3]);
  assert.deepStrictEqual(events.map((e) => e.type), ['create', 'transition', 'invalid']);
  assert.strictEqual(events[1].from, 'New');
  assert.strictEqual(events[1].to, 'Ready');
  assert.ok(events[2].message.startsWith('Rejected:'));
  assert.ok(events.every((e) => e.time instanceof Date));
  assert.ok(events[0].time < events[2].time);
});

test('reset clears processes and history, restarts IDs, logs the reset', () => {
  const sim = new ProcessSimulation();
  sim.createProcess();
  sim.createProcess();
  sim.transition('P1', 'admit');
  const result = sim.reset();
  assert.strictEqual(result.ok, true);
  assert.strictEqual(sim.getProcesses().length, 0);
  const events = sim.getEvents();
  assert.strictEqual(events.length, 1);
  assert.strictEqual(events[0].type, 'reset');
  assert.strictEqual(events[0].seq, 1);
  assert.strictEqual(sim.createProcess().process.id, 'P1');
  assert.strictEqual(sim.transition('P1', 'admit').ok, true, 'works after reset');
});

test('single CPU mode allows only one Running process', () => {
  const sim = new ProcessSimulation({ singleCpu: true });
  const a = processIn(sim, 'Ready');
  const b = processIn(sim, 'Ready');
  assert.strictEqual(sim.transition(a, 'dispatch').ok, true);
  const blocked = sim.transition(b, 'dispatch');
  assert.strictEqual(blocked.ok, false);
  assert.strictEqual(blocked.code, 'CPU_BUSY');
  sim.transition(a, 'preempt');
  assert.strictEqual(sim.transition(b, 'dispatch').ok, true);
  sim.setSingleCpu(false);
  assert.strictEqual(sim.transition(a, 'dispatch').ok, true, 'allowed again when disabled');
});

test('returned objects are copies; callers cannot corrupt state', () => {
  const sim = new ProcessSimulation();
  sim.createProcess();
  sim.getProcesses()[0].state = 'Terminated';
  sim.getEvents()[0].message = 'hacked';
  assert.strictEqual(sim.getProcess('P1').state, 'New');
  assert.notStrictEqual(sim.getEvents()[0].message, 'hacked');
});

console.log(`\n${passed} test(s) passed${process.exitCode ? ' — with failures' : ''}`);
