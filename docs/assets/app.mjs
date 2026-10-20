import { PROFILE_SETS, LIMITS, buildPolicy, createRandomSource, generatePassword, escapeCodePoints } from './generator.mjs';
import { createAnalyzer, analyzeText } from './analyzer.mjs';

const $ = id => document.getElementById(id);
const profileDescriptions = {
  portable: 'ASCII letters, digits, and symbols. A practical starting point for real credentials.',
  unicode: 'Assigned, visible Unicode characters. Confirm that your service and password manager preserve them.',
  unspeakable: 'Authentication test data, including invisible tags and combining marks. Use this profile for stress testing.',
};
const setLabels = {
  symbols: 'ASCII symbols', upper: 'Uppercase letters', lower: 'Lowercase letters', digits: 'Digits',
  tags: 'Invisible tags', combining: 'Combining marks', emoji: 'Emoji',
  historic: 'Historic scripts', braille: 'Braille patterns', games: 'Game symbols',
};
let data;
let lookup;
let generated = [];
let busy = false;
const currentProfile = () => document.querySelector('input[name="profile"]:checked').value;
const updateStatus = (id, message, error = false) => {
  $(id).textContent = message;
  $(id).classList.toggle('is-error', error);
};
function element(tag, className, text) {
  const result = document.createElement(tag);
  if (className) result.className = className;
  if (text !== undefined) result.textContent = text;
  return result;
}
function renderSets() {
  const profile = currentProfile();
  const defaults = PROFILE_SETS[profile];
  $('profile-description').textContent = profileDescriptions[profile];
  $('profile-description').classList.toggle('stress-description', profile === 'unspeakable');
  const rows = document.createDocumentFragment();
  for (const category of data.categories) {
    const available = profile === 'portable' ? PROFILE_SETS.portable.includes(category.id) :
      profile === 'unicode' ? category.visible.length > 0 : category.characters.length > 0;
    const row = element('div', `set-row${available ? '' : ' unavailable'}`);
    const label = element('span', 'set-label', setLabels[category.id] ?? category.description);
    const allowLabel = element('label', 'checkbox-cell');
    const allow = element('input');
    allow.type = 'checkbox';
    allow.name = 'include';
    allow.value = category.id;
    allow.checked = available && defaults.includes(category.id);
    allow.disabled = !available;
    allow.setAttribute('aria-label', `Allow ${label.textContent}`);
    const requireLabel = element('label', 'checkbox-cell');
    const require = element('input');
    require.type = 'checkbox';
    require.name = 'require';
    require.value = category.id;
    require.disabled = !allow.checked;
    require.setAttribute('aria-label', `Require ${label.textContent}`);
    allow.addEventListener('change', () => {
      require.disabled = !allow.checked;
      if (!allow.checked) require.checked = false;
    });
    allowLabel.append(allow);
    requireLabel.append(require);
    row.append(label, allowLabel, requireLabel);
    rows.append(row);
  }
  $('character-sets').replaceChildren(rows);
}

async function copyText(value, statusId, fallbackInput) {
  try {
    if (!globalThis.navigator.clipboard?.writeText) throw new Error('Clipboard API unavailable');
    await globalThis.navigator.clipboard.writeText(value);
    updateStatus(statusId, 'Copied to clipboard. Save it in your password manager.');
    return true;
  } catch {
    if (fallbackInput) {
      fallbackInput.focus();
      fallbackInput.select();
    } else {
      const fallback = element('textarea', 'manual-copy');
      fallback.value = value;
      fallback.readOnly = true;
      fallback.setAttribute('aria-label', 'Passwords selected for manual copying');
      $('password-results').append(fallback);
      fallback.focus();
      fallback.select();
    }
    updateStatus(statusId, 'Clipboard access was unavailable. The original text is selected; press Ctrl+C or ⌘C to copy.', true);
    return false;
  }
}
function renderPasswords() {
  const escaped = $('escape-output').checked;
  const fragment = document.createDocumentFragment();
  generated.forEach((result, index) => {
    const row = element('div', 'password-item');
    const header = element('div', 'password-meta');
    header.append(element('span', 'result-index', String(index + 1).padStart(2, '0')),
      element('span', '', `${result.codePoints} code points · ${result.bytes} UTF-8 bytes`));
    const output = element('textarea', 'password-value');
    output.rows = result.value.length > 100 ? 3 : 2;
    output.value = escaped ? escapeCodePoints(result.value) : result.value;
    output.readOnly = true;
    output.spellcheck = false;
    output.setAttribute('aria-label', `Generated password ${index + 1}${escaped ? ', escaped display' : ''}`);
    output.setAttribute('dir', 'ltr');
    const actions = element('div', 'result-actions');
    const copy = element('button', 'copy-button', 'Copy password');
    copy.type = 'button';
    copy.addEventListener('click', async () => {
      // Clipboard always receives the exact generated text, including when display is escaped.
      const fallback = escaped ? null : output;
      const success = await copyText(result.value, 'generation-status', fallback);
      if (success) {
        copy.textContent = 'Copied ✓';
        setTimeout(() => { copy.textContent = 'Copy password'; }, 2000);
      }
    });
    const inspect = element('button', 'text-button', 'Inspect ↗');
    inspect.type = 'button';
    inspect.addEventListener('click', () => {
      $('analysis-input').value = result.value;
      performAnalysis();
      $('analysis-results').scrollIntoView({ behavior: 'auto', block: 'start' });
    });
    actions.append(inspect, copy);
    row.append(header, output, actions);
    fragment.append(row);
  });
  $('password-results').replaceChildren(fragment);
  $('copy-all').hidden = generated.length < 2;
  $('clear-passwords').hidden = !generated.length;
}
function clearPasswords() {
  generated = [];
  const empty = element('div', 'empty-state');
  const label = element('span', '', '[ ready ]');
  label.setAttribute('aria-hidden', 'true');
  empty.append(label, element('p', '', 'Your next password starts here.'),
    element('small', '', 'Generate, copy, and save it in your password manager.'));
  $('password-results').replaceChildren(empty);
  $('copy-all').hidden = true;
  $('clear-passwords').hidden = true;
}

$('generator-form').addEventListener('submit', async event => {
  event.preventDefault();
  if (!data || busy) return;
  busy = true;
  $('generate-button').disabled = true;
  clearPasswords();
  updateStatus('generation-status', 'Generating securely…');
  try {
    const selected = name => [...document.querySelectorAll(`input[name="${name}"]:checked`)].map(input => input.value);
    const policy = buildPolicy(data, {
      profile: currentProfile(), length: $('password-length').value, count: $('password-count').value,
      maxBytes: $('max-bytes').value, include: selected('include'), require: selected('require'),
    });
    const random = createRandomSource();
    const budget = { remaining: LIMITS.draws };
    const results = [];
    // Yield before each sample so input and the busy state remain responsive for batches.
    for (let i = 0; i < policy.count; i += 1) {
      await new Promise(resolve => setTimeout(resolve, 0));
      results.push(generatePassword(policy, random, budget));
    }
    generated = results;
    renderPasswords();
    updateStatus('generation-status', `Generated ${results.length} password${results.length === 1 ? '' : 's'}. Copy always uses the original text.`);
  } catch (error) {
    updateStatus('generation-status', error.message || 'Generation failed. Please try again.', true);
  } finally {
    busy = false;
    $('generate-button').disabled = false;
  }
});
$('password-count').addEventListener('input', () => { $('generate-plural').textContent = Number($('password-count').value) > 1 ? 's' : ''; });
$('escape-output').addEventListener('change', () => { if (generated.length) renderPasswords(); });
$('clear-passwords').addEventListener('click', () => {
  clearPasswords();
  updateStatus('generation-status', 'Results cleared from this page. Your clipboard is unchanged.');
  $('generate-button').focus();
});
$('copy-all').addEventListener('click', () => { void copyText(generated.map(result => result.value).join('\n'), 'generation-status'); });
for (const radio of document.querySelectorAll('input[name="profile"]')) radio.addEventListener('change', () => { if (data) renderSets(); });

function performAnalysis() {
  if (!lookup) return;
  const value = $('analysis-input').value;
  if (!value.length) {
    clearAnalysis(false);
    updateStatus('analysis-status', 'Paste some text or choose a sample to inspect.');
    return;
  }
  if (value.length > 32768) {
    updateStatus('analysis-status', 'Keep input within 32,768 UTF-16 units.', true);
    return;
  }
  const analysis = analyzeText(value, lookup);
  $('stat-codepoints').textContent = analysis.codePoints.toLocaleString();
  $('stat-graphemes').textContent = analysis.graphemes === null ? 'N/A' : analysis.graphemes.toLocaleString();
  $('stat-bytes').textContent = analysis.bytes.toLocaleString();
  $('stat-utf16').textContent = analysis.utf16Units.toLocaleString();
  const normalizations = document.createDocumentFragment();
  for (const item of analysis.normalization) {
    const tile = element('div', `normalization-tile${item.changed ? ' normalization-changed' : ''}`);
    tile.append(element('span', 'normalization-form', item.form),
      element('strong', '', item.changed ? 'Changes' : 'Unchanged'),
      element('small', '', `${item.codePoints} code points · ${item.bytes} bytes`));
    normalizations.append(tile);
  }
  $('normalization-results').replaceChildren(normalizations);
  const rows = document.createDocumentFragment();
  analysis.rows.forEach((row, index) => {
    const tr = element('tr');
    tr.append(element('td', 'row-index', String(index + 1)));
    const glyphCell = element('td', 'glyph-cell');
    const glyph = element('bdi', row.glyph.length > 4 ? 'glyph-label' : 'glyph', row.glyph);
    glyph.setAttribute('dir', 'ltr');
    glyphCell.append(glyph);
    const name = element('td', 'character-name');
    name.append(element('code', '', row.code), element('span', '', row.name));
    if (row.flags.length) {
      const flags = element('div', 'character-flags');
      row.flags.forEach(flag => flags.append(element('span', '', flag)));
      name.append(flags);
    }
    const category = element('td', 'character-category');
    category.append(element('span', '', `${row.category} · ${row.categoryName}`), element('small', '', row.script));
    const bytes = element('td', 'encoding-cell');
    bytes.append(element('code', '', row.utf8Bytes));
    tr.append(glyphCell, name, category, bytes, element('td', 'encoding-cell', String(row.utf16Units)),
      element('td', `nfc-cell${row.nfcChanged ? ' changed' : ''}`, row.nfcChanged ? row.nfc : 'Same'));
    rows.append(tr);
  });
  $('character-rows').replaceChildren(rows);
  $('analysis-results').hidden = false;
  $('analysis-limit').textContent = analysis.truncated
    ? `Showing the first ${analysis.rows.length} of ${analysis.codePoints.toLocaleString()} code points. Measurements and normalization cover the complete input.`
    : `${analysis.codePoints} code point${analysis.codePoints === 1 ? '' : 's'}, in input order.`;
  const warning = analysis.hasLoneSurrogate
    ? ' This input contains a lone UTF-16 surrogate; UTF-8 encoding replaces it with U+FFFD.'
    : analysis.graphemes === null ? ' Grapheme segmentation is unavailable in this browser.' : '';
  updateStatus('analysis-status', `Analyzed locally. Character details appear below.${warning}`, Boolean(warning));
}
function clearAnalysis(clearInput = true) {
  if (clearInput) $('analysis-input').value = '';
  for (const id of ['stat-codepoints', 'stat-graphemes', 'stat-bytes', 'stat-utf16']) $(id).textContent = '—';
  $('character-rows').replaceChildren();
  $('normalization-results').replaceChildren();
  $('analysis-results').hidden = true;
  updateStatus('analysis-status', '');
}
$('analyzer-form').addEventListener('submit', event => { event.preventDefault(); performAnalysis(); });
$('analysis-input').addEventListener('keydown', event => {
  if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') { event.preventDefault(); performAnalysis(); }
});
$('clear-analysis').addEventListener('click', () => { clearAnalysis(); $('analysis-input').focus(); });
const samples = { accent: 'Caf\u0065\u0301', emoji: '\u{1F469}\u200D\u{1F4BB}', hidden: 'hello\u200Bworld\u00A0' };
for (const button of document.querySelectorAll('[data-sample]')) {
  button.addEventListener('click', () => { $('analysis-input').value = samples[button.dataset.sample]; performAnalysis(); });
}

try {
  const response = await fetch($('tool-app').dataset.unicodeUrl, { cache: 'force-cache' });
  if (!response.ok) throw new Error('Unicode database could not be loaded.');
  data = await response.json();
  if (!Array.isArray(data.categories) || !Array.isArray(data.characters) || !data.unicodeVersion) {
    throw new Error('Unicode database is invalid.');
  }
  lookup = createAnalyzer(data);
  renderSets();
  $('unicode-version').textContent = data.unicodeVersion;
  $('data-status').hidden = true;
  $('generate-button').disabled = false;
  $('analyze-button').disabled = false;
  document.querySelectorAll('[data-sample]').forEach(button => { button.disabled = false; });
} catch {
  $('data-status').textContent = 'The Unicode database could not be loaded. Reload the page to try again. Documentation and the native generator are still available.';
  $('data-status').classList.add('error');
}
