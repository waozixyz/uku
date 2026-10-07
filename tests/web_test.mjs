// Standalone browser integration test. No owner profile or desktop is used.
import assert from 'node:assert/strict';
import {spawn, spawnSync} from 'node:child_process';
import {createServer} from 'node:http';
import {readFile, writeFile, mkdtemp, mkdir, stat, rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv.includes('--private-display')) {
  const env = {...process.env};
  for (const key of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS']) delete env[key];
  const result = spawnSync('xvfb-run', ['-a', process.execPath, fileURLToPath(import.meta.url), '--private-display'], {cwd: root, env, stdio: 'inherit'});
  process.exit(result.status ?? 1);
}
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function until(check, message) {
  for (let i = 0; i < 300; i++) {
    try { if (await check()) return; } catch {}
    await delay(100);
  }
  throw new Error(message);
}
const webRoot = path.join(root, 'build/dist/web');
const scratch = await mkdtemp(path.join(os.tmpdir(), 'uku-browser-'));
const artifacts = path.join(root, 'build/smoke');
await mkdir(artifacts, {recursive: true});
const server = createServer(async (req, res) => {
  const requested = new URL(req.url, 'http://localhost').pathname;
  const file = path.resolve(webRoot, '.' + (requested === '/' ? '/index.html' : requested));
  if (!file.startsWith(webRoot + path.sep)) { res.writeHead(403).end(); return; }
  try {
    const mime = {'.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.svg': 'image/svg+xml'}[path.extname(file)] || 'application/octet-stream';
    res.writeHead(200, {'Content-Type': mime}).end(await readFile(file));
  } catch { res.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const url = `http://127.0.0.1:${server.address().port}/`;
const browser = spawn(process.env.CHROMIUM || 'chromium', ['--headless=new', '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage', '--disable-background-networking', '--no-first-run', '--remote-debugging-port=0', `--user-data-dir=${scratch}`, url], {stdio: ['ignore', 'ignore', 'pipe']});
let browserLog = '';
browser.stderr.on('data', data => { browserLog = (browserLog + data).slice(-8000); });
let socket;
try {
  const portFile = path.join(scratch, 'DevToolsActivePort');
  await until(async () => (await stat(portFile)).size > 0, 'Chromium did not start: ' + browserLog);
  const port = (await readFile(portFile, 'utf8')).split('\n')[0];
  const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  socket = new WebSocket(targets.find(target => target.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
  let nextId = 0;
  const pending = new Map();
  const exceptions = [];
  socket.onmessage = event => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.exceptionThrown') exceptions.push(message.params.exceptionDetails);
    if (message.id && pending.has(message.id)) {
      const {resolve, reject} = pending.get(message.id);
      pending.delete(message.id);
      message.error ? reject(new Error(JSON.stringify(message.error))) : resolve(message.result);
    }
  };
  const command = (method, params = {}) => new Promise((resolve, reject) => {
    const id = ++nextId;
    pending.set(id, {resolve, reject});
    socket.send(JSON.stringify({id, method, params}));
  });
  const evaluate = async expression => {
    const result = await command('Runtime.evaluate', {expression, returnByValue: true});
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  await command('Runtime.enable');
  await command('Page.enable');
  await command('Emulation.setDeviceMetricsOverride', {width: 1000, height: 760, deviceScaleFactor: 1, mobile: false});
  const ready = () => evaluate("typeof FS !== 'undefined' && typeof Module !== 'undefined' && FS.analyzePath('/data/uku.sqlite3').exists && document.getElementById('status').textContent === ''");
  await until(ready, 'Browser app did not initialize: ' + browserLog);
  await delay(600);
  async function click(x, y) {
    await command('Input.dispatchMouseEvent', {type: 'mouseMoved', x, y});
    await command('Input.dispatchMouseEvent', {type: 'mousePressed', x, y, button: 'left', clickCount: 1});
    await delay(100);
    await command('Input.dispatchMouseEvent', {type: 'mouseReleased', x, y, button: 'left', clickCount: 1});
    await delay(200);
  }
  await click(920, 30); // Settings
  await click(660, 220); // Dark appearance
  const settingsShot = await command('Page.captureScreenshot', {format: 'png'});
  await writeFile(path.join(artifacts, 'browser-settings.png'), Buffer.from(settingsShot.data, 'base64'));
  await until(() => evaluate('!Module.ukuSaving && !Module.ukuSaveFailed'), 'Browser saving failed');
  async function checkStoredTheme() {
    const bytes = await evaluate("Array.from(FS.readFile('/data/uku.sqlite3'))");
    const database = path.join(artifacts, 'browser.sqlite3');
    await writeFile(database, Buffer.from(bytes));
    const check = spawnSync('python3', ['-c', 'import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); assert c.execute("select value from settings where key=\'theme_mode\'").fetchone() == (\'2\',)', database], {encoding: 'utf8'});
    assert.equal(check.status, 0, check.stderr);
  }
  await checkStoredTheme();
  await command('Page.reload');
  await delay(300);
  await until(ready, 'Saved browser data did not reopen');
  await checkStoredTheme();
  await click(650, 30);
  await click(500, 248);
  for (const key of 'Choose dinner') {
    await command('Input.dispatchKeyEvent', {type: 'keyDown', key, text: key});
    await command('Input.dispatchKeyEvent', {type: 'keyUp', key});
    await delay(30);
  }
  await click(650, 390);
  await click(650, 375);
  await click(650, 395);
  await click(650, 375);
  await delay(300);
  await until(() => evaluate('!Module.ukuSaving && !Module.ukuSaveFailed'), 'Created process did not persist');
  const createdBytes = await evaluate("Array.from(FS.readFile('/data/uku.sqlite3'))");
  const createdFile = path.join(artifacts, 'browser.sqlite3');
  await writeFile(createdFile, Buffer.from(createdBytes));
  const processCheck = spawnSync('python3', ['-c', 'import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); assert c.execute("select topic from processes").fetchall() == [("Choose dinner",)]; assert c.execute("select count(*) from proposals").fetchone() == (2,)', createdFile], {encoding: 'utf8'});
  assert.equal(processCheck.status, 0, processCheck.stderr);
  await command('Browser.setDownloadBehavior', {behavior: 'allow', downloadPath: scratch});
  await evaluate("document.getElementById('backup').click()");
  const backup = path.join(scratch, 'ukuvota-backup.sqlite3');
  await until(async () => (await stat(backup)).size > 0, 'Backup download failed');
  assert.equal((await readFile(backup)).subarray(0, 15).toString(), 'SQLite format 3');
  const screenshot = await command('Page.captureScreenshot', {format: 'png'});
  await writeFile(path.join(artifacts, 'browser.png'), Buffer.from(screenshot.data, 'base64'));
  await evaluate("Module.originalSync = FS.syncfs; FS.syncfs = (populate, callback) => callback(new Error('test')); Module.saveUkuData()");
  assert.equal(await evaluate('Module.ukuSaveFailed'), true);
  await evaluate('FS.syncfs = Module.originalSync; Module.saveUkuData()');
  await until(() => evaluate('!Module.ukuSaving && !Module.ukuSaveFailed'), 'Saving did not recover');
  assert.deepEqual(exceptions, [], 'Unexpected browser runtime exceptions');
  console.log('Browser startup, reload persistence, process creation, backup download and save recovery: pass');
} finally {
  socket?.close();
  browser.kill('SIGTERM');
  await new Promise(resolve => { if (browser.exitCode !== null) resolve(); else browser.once('exit', resolve); });
  await new Promise(resolve => server.close(resolve));
  await rm(scratch, {recursive: true, force: true});
}
