// Test the real Jekyll output. No runtime website dependencies are required.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { mkdir, readFile } from 'node:fs/promises';
import { resolve, sep, extname } from 'node:path';
import { chromium } from 'playwright-core';

const root = resolve(process.env.UPWGEN_SITE_ROOT || '_site');
const corpus = JSON.parse(await readFile(resolve(root, 'assets/unicode-data.json'), 'utf8'));
const asciiRequirements = {
  upper: /[A-Z]/,
  lower: /[a-z]/,
  digits: /[0-9]/,
  symbols: /[!-/:-@[-`{-~]/,
};
function assertAsciiComposition(value) {
  for (const [category, expression] of Object.entries(asciiRequirements)) {
    assert.match(value, expression, `Password is missing required ASCII ${category}`);
  }
}
const mime = { '.html': 'text/html', '.css': 'text/css', '.mjs': 'text/javascript', '.js': 'text/javascript', '.json': 'application/json', '.svg': 'image/svg+xml', '.txt': 'text/plain' };
const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url, 'http://localhost');
    if (!url.pathname.startsWith('/upwgen/')) throw new Error('Not found');
    const relative = decodeURIComponent(url.pathname.slice('/upwgen/'.length));
    const path = resolve(root, relative.endsWith('/') || !relative ? `${relative}index.html` : relative);
    if (!path.startsWith(root + sep)) throw new Error('Not found');
    response.setHeader('Content-Type', mime[extname(path)] || 'application/octet-stream');
    response.end(await readFile(path));
  } catch {
    response.writeHead(404).end('Not found');
  }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const origin = `http://127.0.0.1:${server.address().port}`;
const base = `${origin}/upwgen/`;
let browser;
try {
  browser = await chromium.launch({
    headless: true,
    ...(process.env.UPWGEN_BROWSER_EXECUTABLE ? { executablePath: process.env.UPWGEN_BROWSER_EXECUTABLE } : {}),
  });
  const context = await browser.newContext({ viewport: { width: 1440, height: 1080 }, permissions: ['clipboard-read', 'clipboard-write'] });
  const page = await context.newPage();
  const errors = [];
  const requests = [];
  page.on('pageerror', error => errors.push(error.message));
  page.on('console', message => { if (message.type() === 'error') errors.push(message.text()); });
  page.on('request', request => requests.push({ url: request.url(), method: request.method() }));
  await page.goto(base);
  await page.waitForFunction(() => !document.getElementById('generate-button').disabled);
  assert.equal(await page.title(), 'Generate passwords. Decode Unicode. · upwgen');
  assert.equal(await page.locator('#stig-composition').isChecked(), true);
  assert.equal(await page.locator('#password-length').getAttribute('min'), '15');
  assert.match(await page.locator('#composition-help').innerText(), /STIG/);
  assert.match(await page.locator('#composition-help').innerText(), /15/);
  for (const category of Object.keys(asciiRequirements)) {
    for (const name of ['include', 'require']) {
      const checkbox = page.locator(`input[name=${name}][value=${category}]`);
      assert.equal(await checkbox.isChecked(), true);
      assert.equal(await checkbox.isDisabled(), true);
    }
  }
  await page.locator('#generate-button').click();
  await page.waitForFunction(() => document.querySelectorAll('.password-value').length === 1);
  const initial = await page.locator('.password-value').inputValue();
  assert.equal([...initial].length, 24);
  assert.match(initial, /^[!-~]{24}$/);
  assertAsciiComposition(initial);
  await page.locator('.copy-button').click();
  await page.waitForFunction(() => document.getElementById('generation-status').textContent.startsWith('Copied'));
  assert.equal(await page.evaluate(() => navigator.clipboard.readText()), initial);
  await mkdir('test-results', { recursive: true });
  await page.screenshot({ path: 'test-results/desktop.png', fullPage: true });

  // Batch copying must use only the generated values, without UI metadata.
  await page.locator('#password-count').fill('2');
  assert.equal(await page.locator('.password-value').count(), 0, 'Changing a constraint must clear stale passwords');
  await page.locator('#generate-button').click();
  await page.waitForFunction(() => document.querySelectorAll('.password-value').length === 2);
  const values = await page.locator('.password-value').evaluateAll(nodes => nodes.map(node => node.value));
  values.forEach(assertAsciiComposition);
  await page.locator('#copy-all').click();
  await page.waitForTimeout(100);
  assert.equal(await page.evaluate(() => navigator.clipboard.readText()), values.join('\n'));

  // Unspeakable requires every category by default, including all four ASCII groups.
  await page.locator('input[name=profile][value=unspeakable]').check();
  assert.equal(await page.locator('.password-value').count(), 0, 'Changing profiles must clear stale passwords');
  await page.locator('.advanced-options summary').click();
  assert.equal(await page.locator('input[name=require]:checked').count(), 10);
  assert.equal(await page.locator('input[name=include]:checked').count(), 10);
  await page.locator('#password-length').fill('15');
  await page.locator('#password-count').fill('3');
  assert.equal(await page.evaluate(() => {
    document.getElementById('generate-button').click();
    return [...document.querySelectorAll('#generator-form input')].every(input => input.disabled);
  }), true, 'Policy settings must remain fixed while a batch is generating');
  await page.waitForFunction(() => document.querySelectorAll('.password-value').length === 3);
  assert.equal(await page.locator('#password-length').isDisabled(), false);
  assert.equal(await page.locator('#stig-composition').isDisabled(), false);
  const unspeakable = await page.locator('.password-value').evaluateAll(nodes => nodes.map(node => node.value));
  for (const value of unspeakable) {
    assert.equal([...value].length, 15);
    assertAsciiComposition(value);
    const points = new Set(Array.from(value, character => character.codePointAt(0)));
    for (const category of corpus.categories) {
      assert.ok(category.characters.some(point => points.has(point)), `Unspeakable is missing ${category.id}`);
    }
  }
  await page.locator('#copy-all').click();
  await page.waitForTimeout(100);
  assert.equal(await page.evaluate(() => navigator.clipboard.readText()), unspeakable.join('\n'));

  // The STIG composition mode rejects short values and never leaves a stale success.
  await page.locator('#password-length').fill('14');
  assert.equal(await page.locator('.password-value').count(), 0);
  await page.locator('#generate-button').click();
  await page.waitForFunction(() => document.getElementById('generation-status').classList.contains('is-error'));
  assert.match(await page.locator('#generation-status').innerText(), /15/);
  assert.equal(await page.locator('.password-value').count(), 0);

  // Custom test cases explicitly opt out before removing the required ASCII groups.
  await page.locator('#stig-composition').uncheck();
  assert.equal(await page.locator('#password-length').getAttribute('min'), '1');
  for (const category of Object.keys(asciiRequirements)) {
    const checkbox = page.locator(`input[name=include][value=${category}]`);
    assert.equal(await checkbox.isDisabled(), false);
    assert.equal(await page.locator(`input[name=require][value=${category}]`).isChecked(), true);
  }
  // Force astral characters, then prove that "copy" copies the original text in escaped display.
  for (const input of await page.locator('input[name=include]').all()) {
    if (await input.getAttribute('value') !== 'emoji') await input.uncheck();
  }
  await page.locator('#password-length').fill('3');
  await page.locator('#password-count').fill('1');
  await page.locator('#max-bytes').fill('12');
  await page.locator('#generate-button').click();
  await page.waitForFunction(() => document.querySelectorAll('.password-value').length === 1);
  const emoji = await page.locator('.password-value').inputValue();
  assert.equal([...emoji].length, 3);
  assert.equal(Buffer.byteLength(emoji), 12);
  await page.locator('#escape-output').check();
  assert.match(await page.locator('.password-value').inputValue(), /^\\u\{/);
  await page.locator('.copy-button').click();
  await page.waitForTimeout(100);
  assert.equal(await page.evaluate(() => navigator.clipboard.readText()), emoji);

  // An impossible byte budget must never produce a truncated or stale result.
  await page.locator('#max-bytes').fill('11');
  await page.locator('#generate-button').click();
  await page.waitForFunction(() => document.getElementById('generation-status').classList.contains('is-error'));
  assert.match(await page.locator('#generation-status').innerText(), /at least 12/);
  assert.equal(await page.locator('.password-value').count(), 0);

  const input = 'Cafe\u0301\u200B\u{1F600}\u2800\n';
  await page.locator('#analysis-input').fill(input);
  await page.locator('#analyze-button').click();
  await page.waitForFunction(() => !document.getElementById('analysis-results').hidden);
  assert.equal(await page.locator('#stat-codepoints').innerText(), String([...input].length));
  assert.equal(await page.locator('#stat-bytes').innerText(), String(Buffer.byteLength(input)));
  assert.equal(await page.locator('#stat-utf16').innerText(), String(input.length));
  const breakdown = await page.locator('#character-rows').innerText();
  for (const name of ['COMBINING ACUTE ACCENT', 'ZERO WIDTH SPACE', 'GRINNING FACE', 'BRAILLE PATTERN BLANK', 'LINE FEED']) assert.ok(breakdown.includes(name), name);
  assert.match(await page.locator('#normalization-results').innerText(), /NFC\s+Changes/);

  // Pasted markup is text. No scripts, images, or outgoing requests may result.
  const before = requests.length;
  const markup = '<img src="https://invalid.test/leak" onerror="alert(1)">';
  await page.locator('#analysis-input').fill(markup);
  await page.locator('#analyze-button').click();
  assert.equal(await page.locator('#character-rows img').count(), 0);
  assert.equal(requests.length, before);
  await page.locator('#analysis-input').fill('a'.repeat(300));
  await page.locator('#analyze-button').click();
  assert.equal(await page.locator('#character-rows tr').count(), 256);
  assert.match(await page.locator('#analysis-limit').innerText(), /256 of 300/);
  await page.locator('#clear-analysis').click();
  assert.equal(await page.locator('#analysis-input').inputValue(), '');
  assert.equal(await page.locator('#analysis-results').isHidden(), true);

  // Once loaded, the tools work with no network and store no entered secrets.
  await context.setOffline(true);
  await page.locator('input[name=profile][value=portable]').check();
  await page.locator('#stig-composition').check();
  await page.locator('#escape-output').uncheck();
  await page.locator('#password-length').fill('24');
  await page.locator('#max-bytes').fill('');
  await page.locator('#generate-button').click();
  await page.waitForFunction(() => document.querySelectorAll('.password-value').length === 1);
  assertAsciiComposition(await page.locator('.password-value').inputValue());
  await page.locator('#analysis-input').fill('Offline\u{1F600}');
  await page.locator('#analyze-button').click();
  assert.equal(await page.locator('#stat-codepoints').innerText(), '8');
  assert.deepEqual(await page.evaluate(() => [localStorage.length, sessionStorage.length]), [0, 0]);
  await context.setOffline(false);

  // Permission denial must select the original text and never claim success.
  await page.evaluate(() => Object.defineProperty(navigator, 'clipboard', {
    value: { writeText: async () => { throw new Error('Permission denied'); } }, configurable: true,
  }));
  await page.locator('#escape-output').uncheck();
  await page.locator('.copy-button').click();
  await page.waitForFunction(() => document.getElementById('generation-status').textContent.includes('unavailable'));
  assert.equal(await page.locator('#generation-status').getAttribute('class'), 'form-status is-error');
  assert.equal(await page.locator('.password-value').evaluate(node => node.selectionEnd - node.selectionStart), 24);

  await page.setViewportSize({ width: 390, height: 844 });
  await page.goto(base);
  await page.waitForFunction(() => !document.getElementById('generate-button').disabled);
  assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), 'mobile page overflows horizontally');
  await page.screenshot({ path: 'test-results/mobile.png', fullPage: true });
  for (const slug of ['guide', 'cli', 'unicode', 'releases']) {
    const response = await page.goto(`${base}${slug}/`);
    assert.equal(response.status(), 200, slug);
    assert.equal(await page.locator('h1').count(), 1, slug);
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), `${slug} overflows`);
  }
  assert.deepEqual(errors, []);
  assert.ok(requests.every(request => request.url.startsWith(origin) && request.method === 'GET'));
  await context.close();

  // A failed data request leaves the actions disabled with a readable error.
  const failure = await browser.newContext();
  await failure.route('**/unicode-data.json', route => route.abort());
  const broken = await failure.newPage();
  await broken.goto(base);
  await broken.waitForFunction(() => document.getElementById('data-status').classList.contains('error'));
  assert.equal(await broken.locator('#generate-button').isDisabled(), true);
  assert.equal(await broken.locator('#analyze-button').isDisabled(), true);
  await failure.close();
  console.log('Browser checks passed: STIG composition defaults, all-category Unspeakable, generation, clipboard, constraints, Unicode, XSS, offline privacy, mobile, documentation, and data-load failure.');
} finally {
  if (browser) await browser.close();
  await new Promise(resolve => server.close(resolve));
}
