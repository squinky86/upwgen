export const PROFILE_SETS = Object.freeze({
  portable: ['symbols', 'upper', 'lower', 'digits'],
  unicode: ['symbols', 'upper', 'lower', 'digits', 'emoji', 'historic', 'braille', 'games'],
  unspeakable: ['symbols', 'upper', 'lower', 'digits', 'tags', 'combining', 'emoji', 'historic', 'braille', 'games'],
});

export const LIMITS = Object.freeze({ length: 1024, count: 50, total: 8192, draws: 2_000_000, attempts: 100_000 });
const UINT32_RANGE = 0x100000000;
const encoder = new TextEncoder();

/** An OS-backed browser source, buffered without ever seeding a PRNG. */
export function createRandomSource(cryptoProvider = globalThis.crypto) {
  if (!cryptoProvider || typeof cryptoProvider.getRandomValues !== 'function') {
    throw new Error('Secure randomness is unavailable. Open this site in a current browser over HTTPS.');
  }
  const buffer = new Uint32Array(256);
  let position = buffer.length;
  return () => {
    if (position === buffer.length) {
      cryptoProvider.getRandomValues(buffer);
      position = 0;
    }
    const value = buffer[position];
    buffer[position++] = 0;
    return value;
  };
}

/** Rejection removes the bias introduced by a simple random word % bound. */
export function randomBelow(bound, randomWord) {
  if (!Number.isInteger(bound) || bound < 1 || bound > UINT32_RANGE) {
    throw new Error('The random selection bound is invalid.');
  }
  const ceiling = UINT32_RANGE - (UINT32_RANGE % bound);
  for (let attempt = 0; attempt < 1024; attempt += 1) {
    const word = randomWord();
    if (!Number.isInteger(word) || word < 0 || word >= UINT32_RANGE) {
      throw new Error('The random source returned an invalid value.');
    }
    if (word < ceiling) return word % bound;
  }
  throw new Error('The random source failed to produce a usable value.');
}

export function integerOption(value, label, min, max) {
  if (!/^[0-9]+$/.test(String(value))) throw new Error(`${label} must be a whole number.`);
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed < min || parsed > max) {
    throw new Error(`${label} must be between ${min} and ${max}.`);
  }
  return parsed;
}

export function buildPolicy(data, options) {
  const profile = options.profile === 'stress' ? 'unspeakable' : options.profile;
  if (!Object.hasOwn(PROFILE_SETS, profile)) throw new Error('Choose a valid profile.');
  const length = integerOption(options.length, 'Length', 1, LIMITS.length);
  const count = integerOption(options.count ?? 1, 'Count', 1, LIMITS.count);
  if (length * count > LIMITS.total) {
    throw new Error(`A batch can contain at most ${LIMITS.total.toLocaleString('en-US')} code points. Reduce length or count.`);
  }
  const maxBytes = options.maxBytes === '' || options.maxBytes == null
    ? null : integerOption(options.maxBytes, 'Maximum UTF-8 bytes', 1, LIMITS.length * 4);
  const selected = [...new Set(options.include ?? PROFILE_SETS[profile])];
  const excluded = new Set(options.exclude ?? []);
  const requiredIds = [...new Set(options.require ?? [])];
  const registry = new Map(data.categories.map(category => [category.id, category]));
  for (const id of [...selected, ...excluded, ...requiredIds]) {
    if (!registry.has(id)) throw new Error(`Unknown character set: ${id}.`);
  }
  const categories = selected.filter(id => !excluded.has(id)).map(id => {
    if (profile === 'portable' && !PROFILE_SETS.portable.includes(id)) {
      throw new Error('The portable profile supports ASCII character sets only.');
    }
    const category = registry.get(id);
    const characters = profile === 'unicode' ? category.visible : category.characters;
    if (!characters.length) throw new Error(`${category.description} has no characters in this profile.`);
    return { ...category, characters };
  });
  if (!categories.length) throw new Error('Allow at least one character set.');
  const allowedIds = new Set(categories.map(category => category.id));
  for (const id of requiredIds) {
    if (!allowedIds.has(id)) throw new Error(`The required ${id} set must also be allowed.`);
  }
  const pool = [...new Set(categories.flatMap(category => category.characters))];
  const required = categories.filter(category => requiredIds.includes(category.id))
    .map(category => new Set(category.characters));
  // Catalog sets are disjoint. For custom overlapping data this is a conservative limit.
  if (length < required.length) throw new Error(`Length must be at least ${required.length} for these required sets.`);
  const byteLengths = new Map(pool.map(cp => [cp, utf8Length(cp)]));
  const minBytes = Math.min(...byteLengths.values()) * (length - required.length)
    + required.reduce((sum, set) => sum + Math.min(...[...set].map(cp => byteLengths.get(cp))), 0);
  if (maxBytes !== null && maxBytes < minBytes) {
    throw new Error(`This policy needs at least ${minBytes} UTF-8 bytes. Increase the byte limit.`);
  }
  return { profile, length, count, maxBytes, pool, required, byteLengths };
}

export function utf8Length(cp) {
  if (!Number.isInteger(cp) || cp < 0 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
    throw new Error('The character set contains a non-scalar Unicode value.');
  }
  return cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
}

/** Uniform independent candidates, accepted only when all constraints hold. */
export function generatePassword(policy, randomWord = createRandomSource(), budget = { remaining: LIMITS.draws }) {
  for (let attempt = 0; attempt < LIMITS.attempts; attempt += 1) {
    if (budget.remaining < policy.length) break;
    budget.remaining -= policy.length;
    const points = Array.from({ length: policy.length }, () => policy.pool[randomBelow(policy.pool.length, randomWord)]);
    if (!policy.required.every(set => points.some(cp => set.has(cp)))) continue;
    const bytes = points.reduce((sum, cp) => sum + policy.byteLengths.get(cp), 0);
    if (policy.maxBytes !== null && bytes > policy.maxBytes) continue;
    return { value: String.fromCodePoint(...points), codePoints: points.length, bytes, points };
  }
  throw new Error('These constraints are too restrictive to sample reliably. Increase length or the byte limit, or require fewer sets.');
}

export function escapeCodePoints(value) {
  return Array.from(value, character => `\\u{${character.codePointAt(0).toString(16).toUpperCase()}}`).join('');
}

export function describeLength(value) {
  return { codePoints: Array.from(value).length, bytes: encoder.encode(value).length, utf16Units: value.length };
}
