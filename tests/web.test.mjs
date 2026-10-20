import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { webcrypto } from 'node:crypto';
import { PROFILE_SETS, LIMITS, buildPolicy, createRandomSource, generatePassword, randomBelow, integerOption, utf8Length, escapeCodePoints } from '../docs/assets/generator.mjs';
import { createAnalyzer, analyzeText, inRange } from '../docs/assets/analyzer.mjs';

const data = JSON.parse(await readFile(new URL('../docs/assets/unicode-data.json', import.meta.url)));
const lookup = createAnalyzer(data);
const options = { profile: 'portable', length: 24, count: 1 };
const fixedWords = (...values) => {
  let index = 0;
  return () => values[index++ % values.length];
};

test('bounded random draws reject the modulo-bias tail and invalid sources', () => {
  assert.equal(randomBelow(10, fixedWords(0xffffffff, 0xfffffffa, 42)), 2);
  assert.equal(randomBelow(0x100000000, fixedWords(0xffffffff)), 0xffffffff);
  assert.equal(randomBelow(1, fixedWords(0xffffffff)), 0);
  assert.throws(() => randomBelow(0, fixedWords(0)), /bound/);
  assert.throws(() => randomBelow(2, fixedWords(-1)), /invalid value/);
  assert.throws(() => randomBelow(10, fixedWords(0xffffffff)), /usable value/);
});

test('random source uses getRandomValues, refills, and fails closed', () => {
  let calls = 0;
  const source = createRandomSource({ getRandomValues(buffer) { calls += 1; buffer.fill(calls); return buffer; } });
  for (let i = 0; i < 256; i += 1) assert.equal(source(), 1);
  assert.equal(source(), 2);
  assert.equal(calls, 2);
  assert.throws(() => createRandomSource(null), /Secure randomness/);
  const broken = createRandomSource({ getRandomValues() { throw new Error('OS failure'); } });
  assert.throws(() => broken(), /OS failure/);
});

test('numeric arguments reject partial, negative, fractional and overflowing inputs', () => {
  for (const value of ['24px', '-1', '1.5', '1e2', '', ' ', 'Infinity', '9999999999999999999999999999']) {
    assert.throws(() => integerOption(value, 'Length', 1, 1024));
  }
  assert.equal(integerOption('24', 'Length', 1, 1024), 24);
  assert.throws(() => buildPolicy(data, { ...options, length: LIMITS.length + 1 }));
  assert.throws(() => buildPolicy(data, { ...options, length: 1024, count: 9 }), /batch/);
  assert.throws(() => buildPolicy(data, { ...options, maxBytes: '0' }), /UTF-8/);
});

test('portable passwords honor exact lengths including less than category count', () => {
  for (const length of [1, 2, 9, 24, 1024]) {
    const policy = buildPolicy(data, { ...options, length });
    const result = generatePassword(policy, createRandomSource(webcrypto));
    assert.equal(Array.from(result.value).length, length);
    assert.equal(result.bytes, length);
    assert.ok([...result.value].every(ch => ch.codePointAt(0) >= 0x21 && ch.codePointAt(0) <= 0x7e));
  }
});

test('profiles and include/exclude requirements produce explicit validated pools', () => {
  const unicode = buildPolicy(data, { ...options, profile: 'unicode' });
  const unspeakable = buildPolicy(data, { ...options, profile: 'unspeakable' });
  assert.ok(unicode.pool.length < unspeakable.pool.length);
  assert.deepEqual(buildPolicy(data, { ...options, profile: 'stress' }).pool, unspeakable.pool);
  assert.ok(unicode.pool.every(cp => !inRange(cp, data.defaultIgnorable)));
  assert.ok(unicode.pool.every(cp => !lookup(cp).category.startsWith('M')));
  assert.ok(unicode.pool.every(cp => String.fromCodePoint(cp).normalize('NFC') === String.fromCodePoint(cp)));
  assert.throws(() => buildPolicy(data, { ...options, include: [] }), /at least one/);
  assert.throws(() => buildPolicy(data, { ...options, include: ['tags'] }), /ASCII/);
  assert.throws(() => buildPolicy(data, { ...options, profile: 'unicode', include: ['tags'] }), /no characters/);
  assert.throws(() => buildPolicy(data, { ...options, include: ['made-up'] }), /Unknown/);
  assert.throws(() => buildPolicy(data, { ...options, exclude: ['upper'], require: ['upper'] }), /must also be allowed/);
  assert.throws(() => buildPolicy(data, { ...options, length: 1, require: ['upper', 'digits'] }), /at least 2/);
  assert.equal(buildPolicy(data, { ...options, include: ['digits'], exclude: ['upper'] }).pool.length, 10);
  assert.deepEqual(PROFILE_SETS.portable, ['symbols', 'upper', 'lower', 'digits']);
});

test('required sets and maximum bytes constrain whole candidates without forced characters', () => {
  const tiny = { categories: [
    { id: 'upper', characters: [65], visible: [65] },
    { id: 'digits', characters: [49], visible: [49] },
    { id: 'emoji', characters: [0x1f600], visible: [0x1f600] },
  ] };
  const required = buildPolicy(tiny, { ...options, length: 2, include: ['upper', 'digits'], require: ['digits'] });
  assert.equal(generatePassword(required, fixedWords(0, 0, 0, 1)).value, 'A1');
  const bytes = buildPolicy(tiny, { ...options, profile: 'unicode', length: 1, include: ['upper', 'emoji'], maxBytes: 1 });
  assert.equal(generatePassword(bytes, fixedWords(1, 0)).value, 'A');
  assert.throws(() => buildPolicy(tiny, { ...options, profile: 'unicode', length: 1, include: ['emoji'], maxBytes: 3 }), /at least 4/);
  assert.throws(() => generatePassword(required, fixedWords(0), { remaining: 4 }), /too restrictive/);
});

test('UTF-8 lengths and escaping include astral code points and reject surrogates', () => {
  assert.deepEqual([0x7f, 0x80, 0x7ff, 0x800, 0xffff, 0x10000, 0x10ffff].map(utf8Length), [1, 2, 2, 3, 3, 4, 4]);
  assert.throws(() => utf8Length(0xd800), /non-scalar/);
  assert.throws(() => utf8Length(-1), /non-scalar/);
  assert.equal(escapeCodePoints('A\u200b😀'), '\\u{41}\\u{200B}\\u{1F600}');
});

test('Unicode names, algorithmic names, scripts, aliases and markers are accurate', () => {
  assert.equal(data.unicodeVersion, '16.0.0');
  assert.equal(lookup(0x41).name, 'LATIN CAPITAL LETTER A');
  assert.equal(lookup(0x41).script, 'Latin');
  assert.equal(lookup(0xac00).name, 'HANGUL SYLLABLE GA');
  assert.equal(lookup(0xd7a3).name, 'HANGUL SYLLABLE HIH');
  assert.equal(lookup(0x4e00).name, 'CJK UNIFIED IDEOGRAPH-4E00');
  assert.equal(lookup(0x17000).name, 'TANGUT IDEOGRAPH-17000');
  assert.equal(lookup(0x0a).name, 'LINE FEED');
  assert.equal(lookup(0x200b).glyph, 'HIDDEN');
  assert.ok(lookup(0x0301).flags.includes('Combining mark'));
  assert.ok(lookup(0x0378).flags.includes('Unassigned'));
  assert.equal(lookup(0xe000).name, 'PRIVATE USE');
  assert.equal(lookup(0x2800).glyph, 'BLANK');
  assert.ok(lookup(0xffff).flags.includes('Noncharacter'));
  assert.equal(lookup(0x1f600).utf8Bytes, 'F0 9F 98 80');
  assert.equal(lookup(0x1f600).utf16Units, 2);
});

test('analyzer distinguishes code points, graphemes, UTF-8, UTF-16 and normalization', () => {
  const decomposed = analyzeText('e\u0301', lookup);
  assert.equal(decomposed.codePoints, 2);
  assert.equal(decomposed.graphemes, 1);
  assert.equal(decomposed.bytes, 3);
  assert.equal(decomposed.utf16Units, 2);
  assert.equal(decomposed.normalization.find(item => item.form === 'NFC').changed, true);
  assert.equal(decomposed.normalization.find(item => item.form === 'NFC').codePoints, 1);
  const emoji = analyzeText('👩‍💻', lookup);
  assert.equal(emoji.codePoints, 3);
  assert.equal(emoji.graphemes, 1);
  assert.equal(emoji.utf16Units, 5);
  assert.equal(emoji.bytes, 11);
  assert.equal(analyzeText('\u00a0', lookup).normalization.find(item => item.form === 'NFKC').changed, true);
});

test('analyzer bounds rendered rows but measures all input, handles unpaired surrogates', () => {
  const large = analyzeText('x'.repeat(500), lookup, 256);
  assert.equal(large.rows.length, 256);
  assert.equal(large.truncated, 244);
  assert.equal(large.codePoints, 500);
  assert.equal(large.bytes, 500);
  assert.equal(analyzeText('abc', lookup, 256, null).graphemes, null);
  const malformed = analyzeText('\ud800', lookup);
  assert.equal(malformed.hasLoneSurrogate, true);
  assert.equal(malformed.rows[0].utf8Bytes, 'EF BF BD');
  assert.equal(malformed.rows[0].glyph, 'INVALID');
  assert.equal(analyzeText('😀', lookup).hasLoneSurrogate, false);
});

test('browser sources use no unsafe HTML, non-cryptographic randomness or secret persistence', async () => {
  for (const name of ['app.mjs', 'generator.mjs', 'analyzer.mjs']) {
    const source = await readFile(new URL(`../docs/assets/${name}`, import.meta.url), 'utf8');
    assert.doesNotMatch(source, /Math\.random|innerHTML|outerHTML|insertAdjacentHTML|localStorage|sessionStorage|console\.(?:log|info|debug)/);
  }
});
