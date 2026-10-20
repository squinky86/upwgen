const encoder = new TextEncoder();
const CATEGORY_NAMES = Object.freeze({
  Lu: 'Uppercase letter', Ll: 'Lowercase letter', Lt: 'Titlecase letter', Lm: 'Modifier letter', Lo: 'Other letter',
  Mn: 'Nonspacing mark', Mc: 'Spacing combining mark', Me: 'Enclosing mark',
  Nd: 'Decimal number', Nl: 'Letter number', No: 'Other number',
  Pc: 'Connector punctuation', Pd: 'Dash punctuation', Ps: 'Open punctuation', Pe: 'Close punctuation',
  Pi: 'Initial punctuation', Pf: 'Final punctuation', Po: 'Other punctuation',
  Sm: 'Math symbol', Sc: 'Currency symbol', Sk: 'Modifier symbol', So: 'Other symbol',
  Zs: 'Space separator', Zl: 'Line separator', Zp: 'Paragraph separator',
  Cc: 'Control', Cf: 'Format', Cs: 'Surrogate', Co: 'Private use', Cn: 'Unassigned',
});
const HANGUL_L = ['G', 'GG', 'N', 'D', 'DD', 'R', 'M', 'B', 'BB', 'S', 'SS', '', 'J', 'JJ', 'C', 'K', 'T', 'P', 'H'];
const HANGUL_V = ['A', 'AE', 'YA', 'YAE', 'EO', 'E', 'YEO', 'YE', 'O', 'WA', 'WAE', 'OE', 'YO', 'U', 'WEO', 'WE', 'WI', 'YU', 'EU', 'YI', 'I'];
const HANGUL_T = ['', 'G', 'GG', 'GS', 'N', 'NJ', 'NH', 'D', 'L', 'LG', 'LM', 'LB', 'LS', 'LT', 'LP', 'LH', 'M', 'B', 'BS', 'S', 'SS', 'NG', 'J', 'C', 'K', 'T', 'P', 'H'];

export function inRange(cp, ranges) {
  let low = 0;
  let high = ranges.length - 1;
  while (low <= high) {
    const mid = (low + high) >>> 1;
    const row = ranges[mid];
    if (cp < row[0]) high = mid - 1;
    else if (cp > row[1]) low = mid + 1;
    else return row;
  }
  return null;
}

export function codePointLabel(cp) {
  return `U+${cp.toString(16).toUpperCase().padStart(4, '0')}`;
}

export function createAnalyzer(data) {
  const characters = new Map(data.characters.map(row => [row[0], row]));
  const nameRanges = [...data.nameRanges].sort((a, b) => a[0] - b[0]);
  const scripts = [...data.scripts].sort((a, b) => a[0] - b[0]);
  const ignorables = [...data.defaultIgnorable].sort((a, b) => a[0] - b[0]);
  return cp => {
    const exact = characters.get(cp);
    const range = exact ? null : inRange(cp, nameRanges);
    let name = exact?.[1] ?? 'UNASSIGNED';
    const category = exact?.[2] ?? range?.[3] ?? 'Cn';
    if (range) {
      if (range[2] === 'HANGUL SYLLABLE') {
        const index = cp - 0xac00;
        name = `HANGUL SYLLABLE ${HANGUL_L[Math.floor(index / 588)]}${HANGUL_V[Math.floor((index % 588) / 28)]}${HANGUL_T[index % 28]}`;
      } else if (range[2] === 'PRIVATE USE' || range[2] === 'SURROGATE') {
        name = range[2];
      } else {
        name = `${range[2]}-${cp.toString(16).toUpperCase()}`;
      }
    }
    const character = String.fromCodePoint(cp);
    const ignorable = Boolean(inRange(cp, ignorables));
    const combining = category.startsWith('M');
    const separator = category.startsWith('Z');
    const control = category === 'Cc' || category === 'Cf';
    const surrogate = category === 'Cs';
    const unassigned = category === 'Cn';
    const noncharacter = (cp >= 0xfdd0 && cp <= 0xfdef) || ((cp & 0xffff) >= 0xfffe);
    const blank = cp === 0x2800;
    if (noncharacter) name = 'NONCHARACTER';
    const glyph = surrogate ? 'INVALID' : unassigned ? 'N/A' : ignorable ? 'HIDDEN' : blank ? 'BLANK' :
      control ? 'CTRL' : separator ? (cp === 0x20 ? '␠' : 'SPACE') : combining ? `◌${character}` : character;
    const flags = [];
    if (ignorable) flags.push('Default ignorable');
    if (combining) flags.push('Combining mark');
    if (separator) flags.push('Whitespace');
    if (control) flags.push('Control / format');
    if (surrogate) flags.push('Lone surrogate');
    if (noncharacter) flags.push('Noncharacter');
    else if (unassigned) flags.push('Unassigned');
    if (blank) flags.push('Blank glyph');
    if (category === 'Co') flags.push('Private use');
    const nfc = character.normalize('NFC');
    const utf8Bytes = [...encoder.encode(character)].map(byte => byte.toString(16).toUpperCase().padStart(2, '0')).join(' ');
    return {
      cp, character, code: codePointLabel(cp), name, category, categoryName: CATEGORY_NAMES[category] ?? category,
      script: (inRange(cp, scripts)?.[2] ?? 'Unknown').replaceAll('_', ' '),
      combiningClass: exact?.[3] ?? 0, bidiClass: exact?.[4] ?? '',
      glyph, flags, utf8Bytes, utf16Units: character.length,
      nfcChanged: nfc !== character, nfc: Array.from(nfc, ch => codePointLabel(ch.codePointAt(0))).join(' '),
    };
  };
}

export function analyzeText(value, lookup, maxRows = 256, segmenter = globalThis.Intl?.Segmenter) {
  const rows = [];
  let codePoints = 0;
  for (const character of value) {
    if (rows.length < maxRows) rows.push(lookup(character.codePointAt(0)));
    codePoints += 1;
  }
  let graphemes = null;
  if (segmenter) {
    graphemes = 0;
    for (const unused of new segmenter(undefined, { granularity: 'grapheme' }).segment(value)) {
      void unused;
      graphemes += 1;
    }
  }
  const normalization = ['NFC', 'NFD', 'NFKC', 'NFKD'].map(form => {
    const normalized = value.normalize(form);
    return { form, changed: normalized !== value, codePoints: Array.from(normalized).length, bytes: encoder.encode(normalized).length };
  });
  return {
    rows, codePoints, graphemes, bytes: encoder.encode(value).length, utf16Units: value.length,
    truncated: Math.max(0, codePoints - rows.length), normalization,
    hasLoneSurrogate: typeof value.isWellFormed === 'function'
      ? !value.isWellFormed() : /[\uD800-\uDBFF](?![\uDC00-\uDFFF])|(?<![\uD800-\uDBFF])[\uDC00-\uDFFF]/u.test(value),
  };
}
