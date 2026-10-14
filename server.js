/**
 * server.js
 * Serves the website and connects it to the C simulator bridge.
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const readline = require('readline');

const PORT = process.env.PORT || 3000;
const ROOT = __dirname;

const BRIDGE_PATH = path.join(
  ROOT,
  'Process_State_Transition_Simulator',
  process.platform === 'win32' ? 'bridge.exe' : 'bridge'
);

const PUBLIC_FILES = {
  '/index.html': 'text/html; charset=utf-8',
  '/style.css': 'text/css; charset=utf-8',
  '/simulation.js': 'text/javascript; charset=utf-8',
  '/c-api.js': 'text/javascript; charset=utf-8',
  '/app.js': 'text/javascript; charset=utf-8',
};

let bridge;
let bridgeReady = false;
let pending = [];
let requestQueue = Promise.resolve();

function startBridge() {
  if (bridge && bridgeReady) return;

  bridge = spawn(BRIDGE_PATH, [], {
    stdio: ['pipe', 'pipe', 'pipe'],
  });
  bridgeReady = true;

  const output = readline.createInterface({
    input: bridge.stdout,
  });

  output.on('line', (line) => {
    let result;

    try {
      result = JSON.parse(line);
    } catch {
      console.error('Invalid bridge response:', line);
      return;
    }

    const callback = pending.shift();
    if (callback) callback.resolve(result);
  });

  bridge.stderr.on('data', (data) => {
    console.error('C bridge:', data.toString());
  });

  bridge.on('error', (error) => {
    bridgeReady = false;
    console.error('Could not start C bridge:', error.message);

    while (pending.length) {
      pending.shift().reject(error);
    }
  });

  bridge.on('exit', (code) => {
    bridgeReady = false;
    console.error('C bridge exited with code:', code);

    while (pending.length) {
      pending.shift().reject(new Error('C bridge stopped'));
    }
  });
}

function sendCommand(command) {
  requestQueue = requestQueue.then(() => new Promise((resolve, reject) => {
    if (!bridgeReady || !bridge || bridge.stdin.destroyed) {
      try {
        startBridge();
      } catch (error) {
        reject(error);
        return;
      }
    }

    pending.push({ resolve, reject });

    bridge.stdin.write(command + '\n', (error) => {
      if (error) {
        const index = pending.length - 1;
        if (index >= 0) pending.splice(index, 1);
        reject(error);
      }
    });
  }));

  return requestQueue;
}

function sendJson(res, status, data) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-cache',
  });

  res.end(JSON.stringify(data));
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';

    req.on('data', (chunk) => {
      body += chunk;

      if (body.length > 8192) {
        reject(new Error('Request body too large'));
        req.destroy();
      }
    });

    req.on('end', () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch {
        reject(new Error('Invalid JSON body'));
      }
    });

    req.on('error', reject);
  });
}

const server = http.createServer(async (req, res) => {
  let pathname;

  try {
    pathname = decodeURIComponent(
      new URL(req.url, 'http://localhost').pathname
    );
  } catch {
    res.writeHead(400).end('Bad request');
    return;
  }

  // List all processes.
  if (req.method === 'GET' && pathname === '/api/processes') {
    try {
      const result = await sendCommand('LIST');
      sendJson(res, 200, result);
    } catch (error) {
      sendJson(res, 500, {
        ok: false,
        message: error.message,
      });
    }
    return;
  }

  // Retrieve transition history.
  if (req.method === 'GET' && pathname === '/api/history') {
    try {
      const result = await sendCommand('HISTORY');
      sendJson(res, 200, result);
    } catch (error) {
      sendJson(res, 500, {
        ok: false,
        message: error.message,
      });
    }
    return;
  }

  // Reset the simulator.
  if (req.method === 'POST' && pathname === '/api/reset') {
    try {
      const result = await sendCommand('RESET');
      sendJson(res, result.ok ? 200 : 400, result);
    } catch (error) {
      sendJson(res, 500, {
        ok: false,
        message: error.message,
      });
    }
    return;
  }

  // Create a new process.
  if (req.method === 'POST' && pathname === '/api/processes') {
    try {
      const result = await sendCommand('CREATE');
      sendJson(res, result.ok ? 200 : 400, result);
    } catch (error) {
      sendJson(res, 500, {
        ok: false,
        message: error.message,
      });
    }
    return;
  }

  // Apply a state transition.
  if (req.method === 'POST' && pathname === '/api/transition') {
    try {
      const body = await readBody(req);
      const id = String(body.id || '');
      const action = String(body.action || '');

      if (!/^P[0-9]+$/.test(id) || !/^[A-Z_]+$/.test(action)) {
        sendJson(res, 400, {
          ok: false,
          message: 'Invalid process ID or action',
        });
        return;
      }

      const result = await sendCommand(`TRANSITION ${id} ${action}`);

      sendJson(res, result.ok ? 200 : 400, result);
    } catch (error) {
      sendJson(res, 500, {
        ok: false,
        message: error.message,
      });
    }
    return;
  }

  // Reject unsupported HTTP methods.
  if (req.method !== 'GET') {
    res.writeHead(405, {
      'Content-Type': 'text/plain; charset=utf-8',
    });
    res.end('Method not allowed');
    return;
  }

  // Serve the frontend.
  if (pathname === '/') pathname = '/index.html';

  const contentType = PUBLIC_FILES[pathname];

  if (!contentType) {
    res.writeHead(404, {
      'Content-Type': 'text/plain; charset=utf-8',
    });
    res.end('Not found');
    return;
  }

  fs.readFile(path.join(ROOT, pathname), (error, data) => {
    if (error) {
      res.writeHead(500, {
        'Content-Type': 'text/plain; charset=utf-8',
      });
      res.end('Could not read file');
      return;
    }

    res.writeHead(200, {
      'Content-Type': contentType,
      'Cache-Control': 'no-cache',
    });

    res.end(data);
  });
});

server.listen(PORT, '0.0.0.0', () => {
  console.log(`Process State Transition Simulator running on port ${PORT}`);
});

process.on('SIGINT', () => {
  if (bridge) bridge.kill();

  server.close(() => process.exit(0));
});