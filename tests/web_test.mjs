// Standalone browser integration test. No owner profile or desktop is used.
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import { createServer } from 'node:http';
import { readFile, writeFile, mkdtemp, mkdir, stat, rm } from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
if (!process.argv.includes('--private-display')) {
  const env = { ...process.env };
  for (const key of ['DISPLAY', 'WAYLAND_DISPLAY', 'XAUTHORITY', 'DBUS_SESSION_BUS_ADDRESS'])
    delete env[key];
  const result = spawnSync(
    'xvfb-run',
    [
      '-a',
      process.execPath,
      fileURLToPath(import.meta.url),
      '--private-display',
      ...process.argv.slice(2),
    ],
    { cwd: root, env, stdio: 'inherit' },
  );
  process.exit(result.status ?? 1);
}
const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function until(check, message) {
  const deadline = performance.now() + 30000;
  while (performance.now() < deadline) {
    try {
      if (await check()) return;
    } catch {}
    await delay(100);
  }
  throw new Error(message);
}
const site = process.argv.includes('--site');
const webRoot = path.join(root, site ? 'build/site' : 'build/dist/web');
const scratch = await mkdtemp(path.join(os.tmpdir(), 'uku-browser-'));
const artifacts = path.join(root, 'build/smoke');
await mkdir(artifacts, { recursive: true });
const server = createServer(async (req, res) => {
  const requested = new URL(req.url, 'http://localhost').pathname;
  const file = path.resolve(webRoot, '.' + (requested === '/' ? '/index.html' : requested));
  if (!file.startsWith(webRoot + path.sep)) {
    res.writeHead(403).end();
    return;
  }
  try {
    const mime =
      {
        '.html': 'text/html',
        '.js': 'text/javascript',
        '.wasm': 'application/wasm',
        '.svg': 'image/svg+xml',
      }[path.extname(file)] || 'application/octet-stream';
    res
      .writeHead(200, { 'Content-Type': mime, 'X-Content-Type-Options': 'nosniff' })
      .end(await readFile(file));
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const url = process.env.UKU_WEB_URL || `http://127.0.0.1:${server.address().port}/`;
const browser = spawn(
  process.env.CHROMIUM || 'chromium',
  [
    '--headless=new',
    '--no-sandbox',
    '--disable-gpu',
    '--disable-dev-shm-usage',
    '--disable-background-networking',
    '--no-first-run',
    '--remote-debugging-port=0',
    `--user-data-dir=${scratch}`,
    url,
  ],
  { stdio: ['ignore', 'ignore', 'pipe'] },
);
let browserLog = '';
browser.stderr.on('data', (data) => {
  browserLog = (browserLog + data).slice(-8000);
});
let socket;
try {
  const portFile = path.join(scratch, 'DevToolsActivePort');
  await until(async () => (await stat(portFile)).size > 0, 'Chromium did not start: ' + browserLog);
  const port = (await readFile(portFile, 'utf8')).split('\n')[0];
  const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  socket = new WebSocket(targets.find((target) => target.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.onopen = resolve;
    socket.onerror = reject;
  });
  let nextId = 0;
  const pending = new Map();
  const exceptions = [];
  const consoleMessages = [];
  socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.method === 'Runtime.exceptionThrown')
      exceptions.push(message.params.exceptionDetails);
    if (message.method === 'Runtime.consoleAPICalled')
      consoleMessages.push(
        message.params.args.map((arg) => arg.value ?? arg.description).join(' '),
      );
    if (message.id && pending.has(message.id)) {
      const { resolve, reject } = pending.get(message.id);
      pending.delete(message.id);
      message.error ? reject(new Error(JSON.stringify(message.error))) : resolve(message.result);
    }
  };
  const command = (method, params = {}) =>
    new Promise((resolve, reject) => {
      const id = ++nextId;
      const timeout = setTimeout(() => {
        pending.delete(id);
        reject(new Error('Timed out: ' + method));
      }, 30000);
      pending.set(id, {
        resolve: (result) => {
          clearTimeout(timeout);
          resolve(result);
        },
        reject: (error) => {
          clearTimeout(timeout);
          reject(error);
        },
      });
      socket.send(JSON.stringify({ id, method, params }));
    });
  const evaluate = async (expression) => {
    const result = await command('Runtime.evaluate', { expression, returnByValue: true });
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  await command('Runtime.enable');
  await command('Page.enable');
  await command('Network.enable');
  const networkFailures = [];
  const requestUrls = new Map();
  const oldMessage = socket.onmessage;
  socket.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.method === 'Network.requestWillBeSent')
      requestUrls.set(message.params.requestId, message.params.request.url);
    if (message.method === 'Network.loadingFailed' && !message.params.canceled)
      networkFailures.push({
        url: requestUrls.get(message.params.requestId),
        error: message.params.errorText,
        blockedReason: message.params.blockedReason,
        type: message.params.type,
      });
    if (message.method === 'Network.responseReceived' && message.params.response.status >= 400)
      networkFailures.push(message.params.response.url);
    oldMessage(event);
  };
  await command('Page.addScriptToEvaluateOnNewDocument', {
    source: `
    const now = Date.now.bind(Date);
    Date.now = () => now() + Number(sessionStorage.testClock || 0);
  `,
  });
  let mobile = false;
  await command('Emulation.setDeviceMetricsOverride', {
    width: 1000,
    height: 760,
    deviceScaleFactor: 1,
    mobile,
  });
  if (site) {
    await until(
      () => evaluate('Boolean(document.querySelector(\'a[href="/build/web/index.html"]\'))'),
      'Website did not offer the web app',
    );
    await until(
      () =>
        evaluate(
          'Array.from(document.images).every(image => image.complete && image.naturalWidth > 0)',
        ),
      'Website screenshots or logos did not load',
    );
    await evaluate('document.querySelector(\'a[href="/build/web/index.html"]\').click()');
    await until(
      () =>
        evaluate(
          "['/build/web/index.html', '/build/web/'].includes(location.pathname) && typeof Module !== 'undefined'",
        ),
      'Website Run web app link did not open Ukuvota',
    );
  }
  await command('Page.reload');
  const ready = () =>
    evaluate(
      "typeof FS !== 'undefined' && typeof Module !== 'undefined' && FS.analyzePath('/data/uku.sqlite3').exists && document.getElementById('status').textContent === '' && document.title === 'Ukuvota'",
    );
  try {
    await until(ready, 'Browser app did not initialize');
  } catch (error) {
    console.error(
      await evaluate(
        "JSON.stringify({title:document.title,status:document.getElementById('status').textContent,filesystem:typeof FS,fields:typeof fields,bridgeError:typeof __ziranWeb!=='undefined'?__ziranWeb.perm[__ziranWeb.error]?.stack:null,fieldsCount:document.querySelectorAll('#fields input').length})",
      ),
    );
    console.error(JSON.stringify({ exceptions, consoleMessages, networkFailures }));
    throw error;
  }
  await delay(600);
  async function click(x, y) {
    if (mobile) {
      await command('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y }] });
      await delay(100);
      await command('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    } else {
      await command('Input.dispatchMouseEvent', { type: 'mouseMoved', x, y });
      await command('Input.dispatchMouseEvent', {
        type: 'mousePressed',
        x,
        y,
        button: 'left',
        clickCount: 1,
      });
      await delay(100);
      await command('Input.dispatchMouseEvent', {
        type: 'mouseReleased',
        x,
        y,
        button: 'left',
        clickCount: 1,
      });
    }
    await delay(200);
  }
  async function scroll(amount) {
    await command('Input.dispatchMouseEvent', {
      type: 'mouseMoved',
      x: mobile ? 200 : 700,
      y: 400,
    });
    await command('Input.dispatchMouseEvent', {
      type: 'mouseWheel',
      x: mobile ? 200 : 700,
      y: 400,
      deltaX: 0,
      deltaY: amount,
    });
    await delay(200);
  }
  async function swipe(startY, endY) {
    const x = 375;
    await command('Input.dispatchTouchEvent', {
      type: 'touchStart',
      touchPoints: [{ x, y: startY }],
    });
    await delay(100);
    for (let step = 1; step <= 8; step++) {
      const y = startY + ((endY - startY) * step) / 8;
      await command('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x, y }] });
      await delay(30);
    }
    await command('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    await delay(200);
  }
  async function text() {
    const screenshot = await command('Page.captureScreenshot', { format: 'png' });
    const file = path.join(artifacts, 'browser-current.png');
    await writeFile(file, Buffer.from(screenshot.data, 'base64'));
    const scale = await evaluate('devicePixelRatio');
    const rows = [];
    const prepared = spawnSync('python3', [path.join(root, 'tests/browser_ocr.py'), file], {
      encoding: 'utf8',
    });
    assert.equal(prepared.status, 0, prepared.stderr);
    const sources = JSON.parse(prepared.stdout).map((region) => ({
      ...region,
      mode: region.mode ?? 7,
    }));
    for (const source of sources) {
      const result = spawnSync(
        process.env.TESSERACT || 'tesseract',
        [source.file, 'stdout', '--psm', String(source.mode), '-c', 'tessedit_create_tsv=1'],
        { encoding: 'utf8', env: { ...process.env, OMP_THREAD_LIMIT: '1' } },
      );
      assert.equal(result.status, 0, 'OCR failed: ' + result.stderr);
      const lines = new Map();
      for (const line of result.stdout.trim().split('\n').slice(1)) {
        const cols = line.split('\t');
        if (cols[0] !== '5' || !cols[11]?.trim()) continue;
        const word = {
          text: cols[11].trim(),
          x: (Number(cols[6]) + source.x) / scale,
          y: (Number(cols[7]) + Number(cols[9]) + source.y) / scale,
          width: Number(cols[8]) / scale,
        };
        rows.push(word);
        const key = cols.slice(1, 5).join(':');
        const words = lines.get(key) || [];
        words.push(word);
        lines.set(key, words);
      }
      for (const words of lines.values()) {
        for (let start = 0; start < words.length; start++)
          for (let count = 2; count <= 8 && start + count <= words.length; count++) {
            const group = words.slice(start, start + count);
            rows.push({
              text: group.map((w) => w.text).join(' '),
              x: group[0].x,
              y: Math.max(...group.map((w) => w.y)),
              width: group.at(-1).x + group.at(-1).width - group[0].x,
            });
          }
      }
    }
    return rows;
  }
  const normalize = (value) =>
    value
      .toLowerCase()
      .replace(/[^\p{L}\p{N}+\-]+/gu, ' ')
      .trim();
  const matches = (value, label) =>
    normalize(value) === normalize(label) ||
    (label.startsWith('Choose café') && normalize(value).startsWith('choose café dinner'));
  async function visible(label) {
    const rows = await text();
    return rows.filter(
      (row) => matches(row.text, label) && row.y > 0 && row.y < (mobile ? 810 : 725),
    );
  }
  async function clickText(label, { scrollTo = true, occurrence = 0 } = {}) {
    for (let i = 0; i < 24; i++) {
      const found = (await visible(label))[occurrence];
      if (found) {
        await click(found.x + found.width / 2, found.y - 5);
        return;
      }
      if (!scrollTo) break;
      const rows = (await text()).filter((row) => matches(row.text, label));
      await scroll(rows.length && rows[occurrence]?.y < 64 ? -250 : 250);
    }
    throw new Error(
      'Cannot find visible control: ' +
        label +
        '\n' +
        JSON.stringify((await text()).filter((row) => row.text.length > 5).slice(0, 30)),
    );
  }
  async function see(part) {
    await until(
      async () => (await text()).some((row) => row.text.toLowerCase().includes(part.toLowerCase())),
      'Expected rendered text: ' + part,
    );
  }
  async function input(id, value) {
    const box = await evaluate(
      `(() => { const field = document.getElementById('field-${id}'); if (!field) return null; const r=field.getBoundingClientRect(); return {x:r.x+r.width/2,y:r.y+r.height/2}; })()`,
    );
    assert.ok(box, 'Missing native field ' + id);
    await click(box.x, box.y);
    assert.equal(await evaluate('document.activeElement.id'), `field-${id}`);
    await command('Input.dispatchKeyEvent', {
      type: 'keyDown',
      key: 'a',
      code: 'KeyA',
      windowsVirtualKeyCode: 65,
      modifiers: 2,
    });
    await command('Input.dispatchKeyEvent', {
      type: 'keyUp',
      key: 'a',
      code: 'KeyA',
      windowsVirtualKeyCode: 65,
      modifiers: 2,
    });
    await command('Input.insertText', { text: value });
    await delay(200);
    assert.equal(await evaluate('document.activeElement.value'), value);
  }
  async function stored() {
    await until(
      () => evaluate('!Module.ukuSaving && !Module.ukuSaveQueued && !Module.ukuSaveFailed'),
      'Browser saving failed',
    );
    const database = path.join(artifacts, 'browser.sqlite3');
    await writeFile(
      database,
      Buffer.from(await evaluate("Array.from(FS.readFile('/data/uku.sqlite3'))")),
    );
    const result = spawnSync(
      'python3',
      [
        '-c',
        'import sqlite3,json,sys; c=sqlite3.connect(sys.argv[1]); c.row_factory=sqlite3.Row; print(json.dumps({t:[dict(r) for r in c.execute("select * from "+t)] for t in ["processes","proposals","votes","participants","settings"]},ensure_ascii=False))',
        database,
      ],
      { encoding: 'utf8' },
    );
    assert.equal(result.status, 0, result.stderr);
    return JSON.parse(result.stdout);
  }
  async function shot(name) {
    const screenshot = await command('Page.captureScreenshot', { format: 'png' });
    await writeFile(
      path.join(artifacts, `browser-${name}.png`),
      Buffer.from(screenshot.data, 'base64'),
    );
  }
  async function score(title, value) {
    await scroll(-10000);
    for (let i = 0; i < 24; i++) {
      const proposal = (await text()).find((row) => matches(row.text, title));
      const field = await evaluate(
        "(() => {const r=document.getElementById('field-10').getBoundingClientRect();return {x:r.x,width:r.width};})()",
      );
      const count = field.width - 28 < 352 ? 4 : 8;
      const index = value === '—' ? 7 : Number(value) + 3;
      const y = proposal && proposal.y + 50 + Math.floor(index / count) * 52;
      if (
        proposal &&
        proposal.y > 64 &&
        y < (await evaluate('Module.canvas.getBoundingClientRect().height-10'))
      ) {
        await click(field.x + 14 + (((index % count) + 0.5) * (field.width - 28)) / count, y);
        return;
      }
      await scroll(150);
    }
    throw new Error('Cannot score ' + title + ' with ' + value);
  }
  await shot('dashboard-empty');
  await clickText('Settings', { scrollTo: false });
  await command('Emulation.setEmulatedMedia', {
    features: [{ name: 'prefers-color-scheme', value: 'dark' }],
  });
  await click(327, 216); // System appearance
  await until(
    () => evaluate("Module.canvas.getContext('2d').getImageData(10,70,1,1).data[0]<100"),
    'System appearance did not follow dark preference',
  );
  await command('Emulation.setEmulatedMedia', {
    features: [{ name: 'prefers-color-scheme', value: 'light' }],
  });
  await until(
    () => evaluate("Module.canvas.getContext('2d').getImageData(10,70,1,1).data[0]>200"),
    'System appearance did not follow light preference',
  );
  await clickText('Dark');
  assert.ok((await stored()).settings.some((row) => row.key === 'theme_mode' && row.value === '2'));
  await command('Page.reload');
  await delay(300);
  await until(ready, 'Saved browser data did not reopen');
  assert.ok((await stored()).settings.some((row) => row.key === 'theme_mode' && row.value === '2'));
  await clickText('Make New Process', { scrollTo: false });
  console.log('Settings persisted; testing native editing and decision creation');
  await shot('setup-process');
  await input(0, 'Choose café dinner 🍲');
  await command('Input.insertText', { text: '🍲'.repeat(100) });
  await delay(200);
  const boundedTopic = await evaluate("document.getElementById('field-0').value");
  assert.ok(
    Buffer.byteLength(boundedTopic, 'utf8') <= 179,
    'Native input exceeded the topic byte capacity',
  );
  assert.ok(!boundedTopic.includes('\ufffd'), 'Native input split a Unicode character');
  await input(0, 'Choose café dinner 🍲');
  await input(1, 'A shared decision with international text.');
  await clickText('Continue');
  await click(381, 255);
  await click(381, 255);
  await clickText('Continue');
  await shot('schedule');
  await clickText('Continue');
  await shot('review');
  await clickText('Create Process');
  await see('Add proposal');
  const initial = await stored();
  assert.equal(initial.processes.length, 1);
  assert.equal(initial.processes[0].topic, 'Choose café dinner 🍲');
  assert.equal(initial.processes[0].quorum_votes, 2);
  assert.equal(initial.proposals.length, 2);
  await input(10, 'Alice');
  await input(11, 'Pasta');
  await input(12, 'Vegetarian pasta for everyone.');
  await clickText('Submit proposal');
  assert.equal((await stored()).proposals.length, 3);
  await shot('proposals');
  await evaluate('sessionStorage.testClock=3601000');
  await see('Your ballot');
  console.log('Proposal saved; testing ballots and participants');
  await clickText('Submit vote');
  assert.equal((await stored()).votes.length, 0, 'An entirely abstained ballot must not save');
  await score('Status quo', '+3');
  await score('Repeat process', '-1');
  await score('Pasta', '+3');
  await clickText('Submit vote');
  const first = await stored();
  assert.equal(first.votes.length, 1);
  assert.equal(first.votes[0].display_name, 'Alice');
  assert.equal(first.votes[0].scores, 'status-quo=3;repeat-process=-1;local-3=3');
  await clickText('Next participant');
  await input(10, 'Alice');
  await score('Status quo', '-1');
  await score('Repeat process', '-1');
  await score('Pasta', '-1');
  await clickText('Submit vote');
  assert.equal(
    (await stored()).votes.length,
    1,
    'Duplicate name must not overwrite another ballot',
  );
  await scroll(-3000);
  await input(10, 'Bob');
  await clickText('Submit vote');
  const two = await stored();
  assert.equal(two.votes.length, 2);
  assert.notEqual(two.votes[0].voter_user_id, two.votes[1].voter_user_id);
  const bob = two.votes.find((row) => row.display_name === 'Bob');
  await clickText('Alice');
  await input(10, 'Alice 🍀');
  await clickText('Update vote');
  const edited = await stored();
  assert.equal(edited.votes.length, 2);
  assert.deepEqual(
    edited.votes.find((row) => row.display_name === 'Bob'),
    bob,
  );
  assert.equal(
    edited.votes.find((row) => row.voter_user_id === first.votes[0].voter_user_id).display_name,
    'Alice 🍀',
  );
  await scroll(-10000);
  await shot('voting');
  await command('Page.reload');
  await delay(300);
  await until(ready, 'Ballots failed to reopen');
  assert.deepEqual((await stored()).votes, edited.votes);
  await shot('dashboard-active');
  await clickText('Choose café dinner 🍲');
  await see('Your ballot');
  await evaluate('sessionStorage.testClock=7201000');
  await see('Results');
  await see('no unique winner');
  console.log('Distinct ballots persisted and edit isolation passed; testing tie and quorum');
  await shot('results');
  await clickText('Bob');
  await see('No quorum');
  await clickText('Bob');
  await see('no unique winner');
  mobile = true;
  await command('Emulation.setDeviceMetricsOverride', {
    width: 390,
    height: 844,
    deviceScaleFactor: 2,
    mobile,
  });
  await command('Emulation.setTouchEmulationEnabled', { enabled: true });
  await delay(500);
  await shot('mobile-results');
  assert.equal(
    await evaluate('document.documentElement.scrollWidth<=innerWidth'),
    true,
    'Mobile page overflows horizontally',
  );
  await click(65, 30); // Back in the mobile toolbar.
  await clickText('Make New Process', { scrollTo: false });
  await input(0, 'Choose tea 茶');
  await input(1, 'Touch input and browser-native editing.');
  await clickText('Voting Phase only');
  await clickText('Continue');
  await clickText('Continue');
  await input(2, 'Green tea');
  await input(3, 'Fresh tea.');
  await clickText('Add Proposal');
  await clickText('Continue');
  await clickText('Continue');
  await clickText('Create Process');
  await see('Your ballot');
  await input(10, 'Chloé');
  const nameFieldY = () =>
    evaluate("document.getElementById('field-10').getBoundingClientRect().y");
  const beforeSwipe = await nameFieldY();
  await swipe(660, 340);
  assert.ok(
    (await nameFieldY()) < beforeSwipe - 100,
    'Mobile touch drag did not scroll the ballot',
  );
  await score('Green tea', '+2');
  await clickText('Submit vote');
  assert.equal((await stored()).votes.length, 3);
  console.log('Mobile touch entry and voting passed; testing backups and storage failures');
  await shot('mobile-voting');
  await command('Browser.setDownloadBehavior', { behavior: 'allow', downloadPath: scratch });
  await evaluate("document.getElementById('backup').click()");
  const backup = path.join(scratch, 'ukuvota-backup.sqlite3');
  await until(async () => (await stat(backup)).size > 0, 'Backup download failed');
  const backupCheck = spawnSync(
    'python3',
    [
      '-c',
      'import sqlite3,sys; c=sqlite3.connect(sys.argv[1]); assert c.execute("pragma integrity_check").fetchone()==("ok",); assert c.execute("select count(*) from votes").fetchone()==(3,)',
      backup,
    ],
    { encoding: 'utf8' },
  );
  assert.equal(backupCheck.status, 0, backupCheck.stderr);
  await evaluate(
    "Module.originalSync=FS.syncfs; FS.syncfs=(populate,callback)=>callback(new Error('test')); Module.saveUkuData()",
  );
  assert.equal(await evaluate('Module.ukuSaveFailed'), true);
  await see('Could not save browser data');
  await evaluate('FS.syncfs=Module.originalSync');
  await evaluate("document.getElementById('retry').click()");
  await until(
    () => evaluate('!Module.ukuSaving && !Module.ukuSaveFailed'),
    'Saving did not recover',
  );
  assert.equal(await evaluate("document.getElementById('retry').hidden"), true);
  // A synchronous storage failure must also clear the saving flag and allow retry.
  await evaluate(
    "FS.syncfs=()=>{throw new Error('test synchronous failure')}; Module.saveUkuData()",
  );
  assert.equal(await evaluate('Module.ukuSaving'), false);
  assert.equal(await evaluate('Module.ukuSaveFailed'), true);
  await evaluate('FS.syncfs=Module.originalSync');
  await evaluate("document.getElementById('retry').click()");
  await until(
    () => evaluate('!Module.ukuSaving && !Module.ukuSaveFailed'),
    'Synchronous save failure did not recover',
  );
  const blocked = await command('Page.addScriptToEvaluateOnNewDocument', {
    source: "indexedDB.open=()=>{throw new DOMException('test storage denied','SecurityError')}",
  });
  await command('Page.reload');
  await until(
    () =>
      evaluate(
        "document.getElementById('status').textContent.includes('Could not open saved data')",
      ),
    'Denied browser storage did not show a recovery message',
  );
  assert.equal(
    await evaluate("document.getElementById('backup').hidden"),
    true,
    'Unavailable data must not offer an empty backup',
  );
  await command('Page.removeScriptToEvaluateOnNewDocument', { identifier: blocked.identifier });
  await command('Page.reload');
  await until(ready, 'Saved data did not reopen after storage access returned');
  assert.equal((await stored()).votes.length, 3, 'Storage startup failure changed saved ballots');
  assert.equal(
    await evaluate("typeof __ziranWeb==='undefined'||!__ziranWeb.error"),
    true,
    'The public browser bridge recorded an error',
  );
  assert.deepEqual(exceptions, [], 'Unexpected browser runtime exceptions');
  assert.deepEqual(networkFailures, [], 'Failed browser resource requests');
  console.log(
    `Browser ${site ? 'website' : 'distribution'}: native international editing, proposal/voting/results, separate participants, editing, tie/quorum, reload persistence, mobile touch, backup and save recovery: pass`,
  );
} finally {
  socket?.close();
  browser.kill('SIGTERM');
  await new Promise((resolve) => {
    if (browser.exitCode !== null) resolve();
    else browser.once('exit', resolve);
  });
  await new Promise((resolve) => server.close(resolve));
  await rm(scratch, { recursive: true, force: true });
}
