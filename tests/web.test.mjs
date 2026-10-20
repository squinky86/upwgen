import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { webcrypto } from 'node:crypto';
import { PROFILE_SETS, PROFILE_REQUIREMENTS, STIG_MIN_LENGTH, STIG_REQUIRED_SETS, LIMITS, buildPolicy, createRandomSource, generatePassword, randomBelow, integerOption, utf8Length, escapeCodePoints } from '../docs/assets/generator.mjs';
import { createAnalyzer, analyzeText, inRange } from '../docs/assets/analyzer.mjs';

const data = JSON.parse(await readFile(new URL('../docs/assets/unicode-data.json', import.meta.url)));
const lookup = createAnalyzer(data);
const options = { profile: 'portable', length: 24, count: 1, stig: false, require: [] };
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

test('default profiles require STIG length and all four ASCII categories', () => {
  assert.equal(STIG_MIN_LENGTH, 15);
  assert.deepEqual(STIG_REQUIRED_SETS, ['upper', 'lower', 'digits', 'symbols']);
  for (const profile of Object.keys(PROFILE_SETS)) {
    const policy = buildPolicy(data, { profile, length: 24 });
    assert.equal(policy.stig, true);
    assert.deepEqual(policy.requiredIds, PROFILE_REQUIREMENTS[profile]);
    const result = generatePassword(policy, createRandomSource(webcrypto));
    for (const expression of [/[A-Z]/, /[a-z]/, /[0-9]/, /[!-/:-@[-`{-~]/]) {
      assert.match(result.value, expression);
    }
    assert.throws(() => buildPolicy(data, { profile, length: 14 }), /at least 15/);
    const mandatory = buildPolicy(data, { profile, length: 15, require: [] });
    assert.deepEqual(mandatory.requiredIds, STIG_REQUIRED_SETS);
    assert.throws(() => buildPolicy(data, { profile, length: 15, require: [], exclude: ['digits'] }), /required digits/);
  }
  assert.deepEqual(PROFILE_REQUIREMENTS.unspeakable, PROFILE_SETS.unspeakable);
  assert.throws(() => buildPolicy(data, { profile: 'portable', length: 24, stig: 'false' }), /true or false/);
});

test('custom test data explicitly opts out of STIG and default category requirements', () => {
  const custom = buildPolicy(data, {
    profile: 'unspeakable', length: 1, include: ['emoji'], require: [], stig: false,
  });
  assert.equal(custom.stig, false);
  assert.deepEqual(custom.requiredIds, []);
  assert.equal(generatePassword(custom, fixedWords(0)).codePoints, 1);
  assert.throws(() => buildPolicy(data, {
    profile: 'unspeakable', length: 15, include: ['emoji'], stig: false,
  }), /must also be allowed/);
});

test('all ten default Unspeakable sets fit a full batch at the STIG minimum length', () => {
  const policy = buildPolicy(data, { profile: 'unspeakable', length: 15, count: LIMITS.count });
  // Rank zero is valid on every draw, so the work assertion is deterministic.
  const source = fixedWords(0);
  const budget = { remaining: LIMITS.draws };
  for (let index = 0; index < policy.count; index += 1) {
    const result = generatePassword(policy, source, budget);
    assert.equal(result.codePoints, 15);
    assert.ok(policy.required.every(set => result.points.some(cp => set.has(cp))));
  }
  assert.ok(budget.remaining > LIMITS.draws - 2000, 'completion sampling needs no rare-category rejection');
  assert.deepEqual(buildPolicy(data, { profile: 'stress', length: 15 }).requiredIds, PROFILE_REQUIREMENTS.unspeakable);
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

test('required sets and maximum bytes constrain sampled candidates', () => {
  const tiny = { categories: [
    { id: 'upper', characters: [65], visible: [65] },
    { id: 'digits', characters: [49], visible: [49] },
    { id: 'emoji', characters: [0x1f600], visible: [0x1f600] },
  ] };
  const required = buildPolicy(tiny, { ...options, length: 2, include: ['upper', 'digits'], require: ['digits'] });
  assert.equal(generatePassword(required, fixedWords(2)).value, 'A1');
  const bytes = buildPolicy(tiny, { ...options, profile: 'unicode', length: 1, include: ['upper', 'emoji'], maxBytes: 1 });
  assert.equal(generatePassword(bytes, fixedWords(1, 0)).value, 'A');
  assert.throws(() => buildPolicy(tiny, { ...options, profile: 'unicode', length: 1, include: ['emoji'], maxBytes: 3 }), /at least 4/);
  assert.throws(() => generatePassword(required, fixedWords(0), { remaining: 2 }), /too restrictive/);
});

test('exact completion ranks enumerate every satisfying string exactly once', () => {
  const tiny = { categories: [
    { id: 'upper', characters: [65, 66], visible: [65, 66] },
    { id: 'digits', characters: [49], visible: [49] },
    { id: 'symbols', characters: [33], visible: [33] },
  ] };
  const policy = buildPolicy(tiny, {
    ...options, length: 3, include: ['upper', 'digits', 'symbols'], require: ['upper', 'digits'],
  });
  const expected = new Set();
  for (const first of 'AB1!') {
    for (const second of 'AB1!') {
      for (const third of 'AB1!') {
        const value = first + second + third;
        if (/[AB]/.test(value) && value.includes('1')) expected.add(value);
      }
    }
  }
  const actual = new Set();
  for (let rank = 0; rank < expected.size; rank += 1) {
    const result = generatePassword(policy, fixedWords(rank));
    assert.ok(!actual.has(result.value), 'each rank must select a different string');
    actual.add(result.value);
  }
  assert.equal(expected.size, 30);
  assert.deepEqual(actual, expected);
  assert.equal(generatePassword(policy, fixedWords(31, 0)).value, generatePassword(policy, fixedWords(0)).value);
  assert.throws(() => generatePassword(policy, fixedWords(31), { remaining: 10 }), /too restrictive/);
  assert.throws(() => generatePassword(policy, fixedWords(-1)), /invalid value/);
});

test('completion ranks preserve full precision beyond one 32-bit random word', () => {
  const tiny = { categories: [
    { id: 'upper', characters: [65], visible: [65] },
    { id: 'digits', characters: [49], visible: [49] },
  ] };
  const policy = buildPolicy(tiny, {
    ...options, length: 33, include: ['upper', 'digits'], require: ['digits'],
  });
  assert.equal(generatePassword(policy, fixedWords(0, 0)).value, '1'.repeat(33));
  assert.equal(generatePassword(policy, fixedWords(0, 0xffffffff)).value, `1${'A'.repeat(32)}`);
  assert.equal(generatePassword(policy, fixedWords(1, 0)).value, `A${'1'.repeat(32)}`);
  assert.equal(generatePassword(policy, fixedWords(1, 0xfffffffe)).value, `${'A'.repeat(32)}1`);
  assert.equal(generatePassword(policy, fixedWords(1, 0xffffffff, 0, 0)).value, '1'.repeat(33));
});

test('required-set sampling works at both sides of its bounded table limit', () => {
  for (const length of [64, 65, LIMITS.length]) {
    const policy = buildPolicy(data, { profile: 'unspeakable', length });
    const result = generatePassword(policy, createRandomSource(webcrypto));
    assert.equal(result.codePoints, length);
    assert.ok(policy.required.every(set => result.points.some(cp => set.has(cp))));
  }
});

test('completion sampling retains byte-cap rejection and bounded failure', () => {
  const tiny = { categories: [
    { id: 'upper', characters: [65], visible: [65] },
    { id: 'emoji', characters: [0x1f600], visible: [0x1f600] },
  ] };
  const policy = buildPolicy(tiny, {
    ...options, profile: 'unicode', length: 2, include: ['upper', 'emoji'], require: ['upper'], maxBytes: 2,
  });
  assert.equal(generatePassword(policy, fixedWords(1, 0)).value, 'AA');
  assert.throws(() => generatePassword(policy, fixedWords(1), { remaining: 12 }), /too restrictive/);
  const unrestricted = buildPolicy(tiny, { ...options, length: 1, include: ['upper'] });
  assert.throws(() => generatePassword(unrestricted, fixedWords(0), { remaining: 1 }), /too restrictive/);
});

test('custom overlapping requirements retain unbiased whole-candidate rejection', () => {
  const tiny = { categories: [
    { id: 'upper', characters: [65, 66], visible: [65, 66] },
    { id: 'digits', characters: [66, 67], visible: [66, 67] },
  ] };
  const policy = buildPolicy(tiny, {
    ...options, length: 2, include: ['upper', 'digits'], require: ['upper', 'digits'],
  });
  let draws = 0;
  const words = [0, 0, 0, 1];
  const result = generatePassword(policy, () => words[draws++]);
  assert.equal(result.value, 'AB');
  assert.equal(draws, 4);
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
