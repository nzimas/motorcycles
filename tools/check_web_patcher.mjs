// Checks the browser patcher (docs/patcher.js) in Node: stock OS 1.13 .syx +
// docs/motorcycles.mcpatch must give exactly the expected Motor:Cycles .syx.
// Usage: node tools/check_web_patcher.mjs STOCK.syx EXPECTED.syx
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { readKit, convert, outputHash } from '../docs/patcher.js';
const [stock, expected] = process.argv.slice(2).map(p => new Uint8Array(readFileSync(p)));
const kit = readKit(new Uint8Array(readFileSync(new URL('../docs/motorcycles.mcpatch', import.meta.url))));
const t = Date.now();
const out = await convert(stock, kit, s => console.log(' ', s));
const same = out.length === expected.length && out.every((v, i) => v === expected[i]);
console.log(same ? 'IDENTICAL' : 'DIFFERENT', createHash('sha256').update(out).digest('hex'), `kit expects ${outputHash(kit)}`, `${Date.now() - t} ms`);
let refused = false;
try { const bad = stock.slice(); bad[5000] ^= 1; await convert(bad, kit); } catch (e) { refused = /official/.test(e.message); }
console.log(refused ? 'altered input refused' : 'ALTERED INPUT NOT REFUSED');
process.exit(same && refused ? 0 : 1);
