import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

test('published authentication corpus metadata describes the exact public test strings', () => {
  const lines = readFileSync(new URL('../docs/assets/authentication-cases.jsonl', import.meta.url), 'utf8').trim().split('\n');
  const ids = new Set();
  for (const line of lines) {
    const record = JSON.parse(line);
    assert.ok(!ids.has(record.id), `duplicate id ${record.id}`);
    ids.add(record.id);
    assert.equal([...record.value].length, record.code_points, record.id);
    assert.equal(Buffer.byteLength(record.value, 'utf8'), record.utf8_bytes, record.id);
    assert.equal(record.value.normalize('NFC') !== record.value, record.nfc_changed, record.id);
    if (record.nfc) assert.equal(record.value.normalize('NFC'), record.nfc, record.id);
  }
});
