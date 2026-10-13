// Motor:Cycles patcher core for the browser (and Node 20+): turns the user's own
// stock Model:Cycles OS 1.13 .syx into Motor:Cycles using the patch kit
// (motorcycles.mcpatch), which holds no Elektron code. Port of
// tools/apply_patchkit.py and the MIT-licensed mtlib (Elektron SysEx transport,
// ELE3 container, aPLib variant) from Modded-Cycles. Every stage is checked
// against the SHA-256 hashes recorded in the kit, so the output is
// byte-identical to the hardware-tested build or nothing is produced.

export class PatchError extends Error {}
const notStock = () => new PatchError('This file is not the official Model:Cycles OS 1.13 firmware. Download it from elektron.se and try again.');
const badKit = why => new PatchError(`The patch kit is damaged (${why}).`);
const corrupt = why => new PatchError(`The conversion failed a safety check (${why}). Nothing was produced.`);

const subtle = globalThis.crypto.subtle;
async function sha256(bytes) { return new Uint8Array(await subtle.digest('SHA-256', bytes)); }
async function hmac(key, message) {
  const k = await subtle.importKey('raw', key, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return new Uint8Array(await subtle.sign('HMAC', k, message));
}
const equal = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const be32 = (b, o) => ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0;
const put32 = (b, o, v) => { b[o] = (v >>> 24) & 255; b[o + 1] = (v >>> 16) & 255; b[o + 2] = (v >>> 8) & 255; b[o + 3] = v & 255; };

class Bytes {                                   // growable byte buffer
  constructor(n = 1 << 16) { this.a = new Uint8Array(n); this.n = 0; }
  reserve(k) { if (this.n + k > this.a.length) { let m = this.a.length * 2; while (m < this.n + k) m *= 2; const b = new Uint8Array(m); b.set(this.a.subarray(0, this.n)); this.a = b; } }
  push(v) { this.reserve(1); this.a[this.n++] = v; }
  append(s) { this.reserve(s.length); this.a.set(s, this.n); this.n += s.length; }
  zeros(k) { this.reserve(k); this.a.fill(0, this.n, this.n + k); this.n += k; }
  bytes() { return this.a.slice(0, this.n); }
}

// ---- Elektron OS-update SysEx transport -------------------------------------
const ELEKTRON = [0x00, 0x20, 0x3c];
const MSG_LEN = 128, PAYLOAD = 10, PAYLOAD_END = 126, CHECKSUM = 126, BYTES_PER_MSG = 101;
const PRODUCTS = { 0x11: [0x0c, 0x3a, 0x22], 0x0f: [0x0a, 0x3c, 0x1e] };   // device byte, mask seed, checksum base
function checksum(m, v, c0) { let s = 0; for (let i = 8; i < 126; i++) s += m[i] ^ ((v - i) & 0x3f); return (c0 - s) & 0x7f; }
const u21 = (b, o) => (b[o] << 14) | (b[o + 1] << 7) | b[o + 2];
const p21 = v => [(v >> 14) & 0x7f, (v >> 7) & 0x7f, v & 0x7f];
function unwrap(raw) {
  const msgs = [];
  for (let i = 0; i < raw.length;) {
    const a = raw.indexOf(0xf0, i); if (a < 0) break;
    const b = raw.indexOf(0xf7, a); if (b < 0) break;
    msgs.push(raw.subarray(a, b + 1)); i = b + 1;
  }
  if (msgs.length < 3) throw notStock();
  const head = msgs[0], tail = msgs[msgs.length - 1], data = msgs.slice(1, -1);
  const same = (m, o, x) => x.every((v, i) => m[o + i] === v);
  const product = PRODUCTS[head[4]];
  if (!same(head, 1, ELEKTRON) || !same(tail, 1, ELEKTRON) || !product || head[6] !== 0x7f || head[7] !== 1 || tail[6] !== 0x7f || tail[7] !== 2) throw notStock();
  const [, v, c0] = product, start = u21(head, 9);
  if (u21(head, 12) !== data.length) throw notStock();
  const body = new Bytes(data.length * 112);
  for (const m of data) {
    if (m.length !== MSG_LEN || m[CHECKSUM] !== checksum(m, v, c0)) throw notStock();
    for (let i = PAYLOAD; i < PAYLOAD_END; i += 8) {
      const hi = m[i];
      for (let k = 0; k < 7 && i + 1 + k < PAYLOAD_END; k++) body.push(((hi << (k + 1)) & 0x80) | m[i + 1 + k]);
    }
  }
  return { body: body.bytes(), product: head[4], start };
}
function wrap(body, productId, start) {
  const product = PRODUCTS[productId];
  if (!product || body.length % BYTES_PER_MSG) throw corrupt('transport');
  const [dev, v, c0] = product, count = body.length / BYTES_PER_MSG, pre = [0xf0, ...ELEKTRON, productId, 0];
  const out = new Bytes(count * MSG_LEN + 64);
  out.append([...pre, 0x7f, 1, dev, ...p21(start), ...p21(count), 0xf7]);
  for (let n = 0; n < count; n++) {
    const seq = start + n, m = [...pre, 0x7e, 0, (seq >> 7) & 0x7f, seq & 0x7f];
    for (let i = n * BYTES_PER_MSG; i < (n + 1) * BYTES_PER_MSG; i += 7) {
      let hi = 0; const chunk = [];
      for (let k = 0; k < 7 && i + k < (n + 1) * BYTES_PER_MSG; k++) { hi |= (body[i + k] >> 7) << (6 - k); chunk.push(body[i + k] & 0x7f); }
      m.push(hi, ...chunk);
    }
    m.push(0, 0xf7); m[CHECKSUM] = checksum(m, v, c0);
    out.append(m);
  }
  out.append([...pre, 0x7f, 2, dev, ...p21(start), ...p21(count), 0xf7]);
  return out.bytes();
}

// ---- ELE3 container ----------------------------------------------------------
const PREAMBLE = 8, TABLE = 0x20, ENTRY = 16, ALIGN = 16;
const KEY_ANCHOR = [0xbe, 0xf9, 0xa3, 0xf7, 0xc6, 0x71, 0x78, 0xf2];
function parse(stream) {
  if (stream.length < PREAMBLE + TABLE) throw notStock();
  const declared = be32(stream, 0);
  if (PREAMBLE + declared > stream.length) throw notStock();
  const blob = stream.subarray(PREAMBLE, PREAMBLE + declared);
  if (String.fromCharCode(...blob.subarray(0, 4)) !== 'ELE3') throw notStock();
  const n = be32(blob, 0x1c);
  if (n < 1 || n > 16) throw notStock();
  const sections = [];
  for (let s = 0; s < n; s++) { const e = TABLE + s * ENTRY; sections.push({ index: s, id: be32(blob, e), off: be32(blob, e + 4), size: be32(blob, e + 8) }); }
  return { blob, sections };
}
async function findKey(buffers, message, expect) {
  for (const buf of buffers) {
    for (let i = buf.indexOf(KEY_ANCHOR[0]); i >= 0 && i + 8 <= buf.length; i = buf.indexOf(KEY_ANCHOR[0], i + 1)) {
      if (!KEY_ANCHOR.every((v, k) => buf[i + k] === v)) continue;
      const s = i + 8; let p = s;
      while (p < buf.length && buf[p] >= 0x20 && buf[p] < 0x7f && p - s < 64) p++;
      if (p > s && p < buf.length && buf[p] === 0 && p + 33 <= buf.length) {
        const text = buf.slice(s, p), constant = buf.subarray(p + 1, p + 33);
        const h = await sha256(text), hr = await sha256(text.slice().reverse());
        const key = new Uint8Array(32).map((_, k) => h[k] ^ hr[k] ^ constant[k]);
        if (equal(await hmac(key, message), expect)) return key;
      }
    }
  }
  return null;
}
async function rebuild(blob, sections, id, data, key) {
  const first = Math.min(...sections.map(s => s.off));
  const out = new Bytes(blob.length + data.length);
  out.append(blob.subarray(0, first));
  let pos = first;
  for (const sec of [...sections].sort((a, b) => a.off - b.off)) {
    const bytes = sec.id === id ? data : blob.subarray(sec.off, sec.off + sec.size);
    pos = (pos + ALIGN - 1) & ~(ALIGN - 1);
    if (out.n < pos) out.zeros(pos - out.n);
    if (out.n < pos + bytes.length) out.zeros(pos + bytes.length - out.n);
    out.a.set(bytes, pos);
    const e = TABLE + sec.index * ENTRY;
    put32(out.a, e + 4, pos); put32(out.a, e + 8, bytes.length);
    pos += bytes.length;
  }
  const hmacOff = (pos + 4 + ALIGN - 1) & ~(ALIGN - 1);
  if (out.n < hmacOff) out.zeros(hmacOff - out.n);
  out.n = hmacOff;
  const head = out.bytes();
  const result = new Uint8Array(hmacOff + 32);
  result.set(head); result.set(await hmac(key, head), hmacOff);
  return result;
}
function buildStream(blob, padTo) {
  let acc = 0;
  for (let k = 0; k < (blob.length >> 2); k++) acc = (acc + ((k + 1) ^ be32(blob, 4 * k))) >>> 0;
  let length = PREAMBLE + blob.length;
  if (length % padTo) length += padTo - (length % padTo);
  const stream = new Uint8Array(length);
  put32(stream, 0, blob.length); put32(stream, 4, acc); stream.set(blob, PREAMBLE);
  return stream;
}

// ---- aPLib variant ---------------------------------------------------------------
const HEADER = 8, OFFSET_BIAS = 767, REUSE_GAMMA = 2, FAR = 3328, END_GAMMA = 0x1000002;
function depack(src) {
  if (src.length < HEADER) throw notStock();
  let ip = HEADER, tag = 0;
  const bit = () => {
    tag <<= 1;
    if ((tag & 0xff) === 0) {
      if (ip >= src.length) throw notStock();
      const by = src[ip++]; tag = (by << 1) | 1; return (by >> 7) & 1;
    }
    return (tag >> 8) & 1;
  };
  const gamma = () => {
    let v = 1;
    for (;;) { v = (v << 1) + bit(); if (bit()) return v; if (v > 0x02000000) throw notStock(); }
  };
  const out = new Bytes(2_000_000);
  // ops as parallel arrays: pos, off (0 = literal), n
  let cap = 1 << 19, count = 0, P = new Int32Array(cap), O = new Int32Array(cap), N = new Int32Array(cap);
  const op = (pos, off, n) => {
    if (count === cap) { cap *= 2; for (const [k, a] of [['P', P], ['O', O], ['N', N]]) { const b = new Int32Array(cap); b.set(a); if (k === 'P') P = b; else if (k === 'O') O = b; else N = b; } }
    P[count] = pos; O[count] = off; N[count] = n; count++;
  };
  let lastOff = 1;
  for (;;) {
    if (bit()) { if (ip >= src.length) throw notStock(); op(out.n, 0, 1); out.push(src[ip++]); continue; }
    const g = gamma();
    let off;
    if (g === REUSE_GAMMA) off = lastOff;
    else {
      if (ip >= src.length) throw notStock();
      off = ((g << 8) + src[ip++]) >>> 0;
      if (off === OFFSET_BIAS) break;
      off -= OFFSET_BIAS; lastOff = off;
    }
    const sl = 2 * bit() + bit();
    let l = sl !== 0 ? sl : gamma() + 2;
    if (off > FAR) l += 1;
    const n = l + 1;
    if (off <= 0 || off > out.n) throw notStock();
    op(out.n, off, n);
    out.reserve(n);
    for (let k = 0, s = out.n - off; k < n; k++) out.a[out.n++] = out.a[s + k];
  }
  return { data: out.bytes(), ops: { P, O, N, count } };
}
class Writer {
  constructor() { this.o = new Bytes(1 << 20); this.o.zeros(HEADER); this.tagPos = -1; this.tagBits = 0; }
  bit(v) {
    if (this.tagBits === 0) { this.tagPos = this.o.n; this.o.push(0); this.tagBits = 8; }
    if (v) this.o.a[this.tagPos] |= 1 << (this.tagBits - 1);
    this.tagBits--;
  }
  gamma(v) { for (let i = 31 - Math.clz32(v) - 1; i >= 0; i--) { this.bit((v >>> i) & 1); this.bit(i === 0 ? 1 : 0); } }
  literal(b) { this.bit(1); this.o.push(b); }
  match(off, n, lastOff) {
    this.bit(0);
    if (off === lastOff) this.gamma(REUSE_GAMMA);
    else { const raw = off + OFFSET_BIAS; this.gamma(raw >>> 8); this.o.push(raw & 0xff); }
    const base = n - 1 - (off > FAR ? 1 : 0);
    if (base <= 3) { this.bit(base >> 1); this.bit(base & 1); } else { this.bit(0); this.bit(0); this.gamma(base - 2); }
  }
  end() { this.bit(0); this.gamma(END_GAMMA); this.o.push(0xff); }
}
// Re-emit the stock op stream over the patched image (ops touching a changed
// byte become literals), then greedily compress the appended tail.
function repackGrow(data, ops, dirty, originalLength) {
  const w = new Writer();
  let lastOff = 1;
  const prefix = new Int32Array(dirty.length + 1);
  for (let i = 0; i < dirty.length; i++) prefix[i + 1] = prefix[i] + dirty[i];
  const clamp = x => Math.max(0, Math.min(x, dirty.length));
  const anyDirty = (a, b) => prefix[clamp(b)] - prefix[clamp(a)] > 0;
  const { P, O, N, count } = ops;
  for (let i = 0; i < count; i++) {
    const pos = P[i], off = O[i], n = N[i];
    if (off === 0) { w.literal(data[pos]); continue; }
    const end = pos + n, src = pos - off;
    if (anyDirty(pos, end) || anyDirty(src, src + n)) { for (let k = pos; k < end; k++) w.literal(data[k]); }
    else { w.match(off, n, lastOff); lastOff = off; }
  }
  const table = new Map(), n = data.length;
  const key = i => ((data[i] << 24) | (data[i + 1] << 16) | (data[i + 2] << 8) | data[i + 3]) >>> 0;
  for (let i = originalLength; i < n;) {
    let bestLen = 0, bestOff = 0;
    if (i + 4 <= n) {
      const k = key(i), cand = table.get(k);
      if (cand !== undefined && i - cand <= 1 << 20) {
        let length = 4; const limit = Math.min(n - i, 0xffff);
        while (length < limit && data[cand + length] === data[i + length]) length++;
        bestLen = length; bestOff = i - cand;
      }
      table.set(k, i);
    }
    if (bestLen >= 4) {
      w.match(bestOff, bestLen, lastOff); lastOff = bestOff;
      for (let k = i + 1; k < Math.min(i + bestLen, n - 4 + 1); k++) table.set(key(k), k);
      i += bestLen;
    } else { w.literal(data[i]); i++; }
  }
  w.end();
  const body = w.o.bytes();
  put32(body, 0, body.length - HEADER);
  let sum = 0; for (let i = HEADER; i < body.length; i++) sum = (sum + body[i]) >>> 0;
  put32(body, 4, sum);
  return body;
}

// ---- Patch kit (format 2: two stages, see tools/make_patchkit.py) ---------------
function recipe(k, p) {
  if (p + 4 > k.length) throw badKit('truncated');
  const count = be32(k, p); p += 4; const ops = [];
  for (let i = 0; i < count; i++) {
    if (p >= k.length) throw badKit('truncated');
    if (k[p] === 0) { if (p + 9 > k.length) throw badKit('truncated'); ops.push({ copy: true, src: be32(k, p + 1), n: be32(k, p + 5) }); p += 9; }
    else {
      if (p + 5 > k.length) throw badKit('truncated');
      const n = be32(k, p + 1); if (p + 5 + n > k.length) throw badKit('truncated');
      ops.push({ copy: false, bytes: k.subarray(p + 5, p + 5 + n) }); p += 5 + n;
    }
  }
  return [ops, p];
}
function build(ops, base) {
  const out = new Bytes(base.length + 65536);
  for (const op of ops) {
    if (op.copy) { if (op.src + op.n > base.length) throw badKit('copy range'); out.append(base.subarray(op.src, op.src + op.n)); }
    else out.append(op.bytes);
  }
  return out.bytes();
}
export function readKit(k) {
  if (k.length < 208 || String.fromCharCode(...k.subarray(0, 4)) !== 'MCPK') throw badKit('header');
  if (be32(k, 4) >>> 16 !== 2) throw badKit('version');
  const kit = { section: be32(k, 4) & 0xffff, hashes: [0, 1, 2, 3, 4, 5].map(i => k.slice(8 + 32 * i, 40 + 32 * i)),
                baselineLength: be32(k, 200), finalLength: be32(k, 204) };
  let p = 208;
  [kit.stage1, p] = recipe(k, p);
  [kit.stage2, p] = recipe(k, p);
  if (p + 4 > k.length) throw badKit('truncated');
  const count = be32(k, p); p += 4;
  if (p + 8 * count > k.length) throw badKit('truncated');
  kit.dirty = Array.from({ length: count }, (_, i) => [be32(k, p + 8 * i), be32(k, p + 8 * i + 4)]);
  return kit;
}
export const outputHash = kit => [...kit.hashes[5]].map(b => b.toString(16).padStart(2, '0')).join('');

const pause = () => new Promise(r => setTimeout(r, 0));
// Stock Model:Cycles OS 1.13 .syx -> Motor:Cycles .syx, byte for byte the
// hardware-tested two-stage build (Modded-Cycles baseline, then our payload),
// or throws. progress(text) is called between stages.
export async function convert(raw, kit, progress = () => {}) {
  const [hStock, hStockMain, hBaseline, hBaselinePacked, hFinal, hOutput] = kit.hashes;
  progress('Checking your firmware…'); await pause();
  if (!equal(await sha256(raw), hStock)) throw notStock();
  const { body, product, start } = unwrap(raw);
  const { blob, sections } = parse(body);
  const main = sections.find(s => s.id === kit.section);
  if (!main) throw notStock();
  progress('Unpacking the operating system…'); await pause();
  const { data: stock, ops: ops0 } = depack(blob.subarray(main.off, main.off + main.size));
  if (!equal(await sha256(stock), hStockMain)) throw notStock();
  progress('Applying the Modded-Cycles mods…'); await pause();
  const baseline = build(kit.stage1, stock);
  if (baseline.length !== kit.baselineLength || !equal(await sha256(baseline), hBaseline)) throw corrupt('baseline image');
  const dirty1 = new Uint8Array(baseline.length);
  for (const [a, n] of kit.dirty) { if (a + n > baseline.length) throw badKit('dirty range'); dirty1.fill(1, a, a + n); }
  const packed1 = repackGrow(baseline, ops0, dirty1, stock.length);
  if (!equal(await sha256(packed1), hBaselinePacked)) throw corrupt('baseline packing');
  const { data: again, ops: ops1 } = depack(packed1);
  if (!equal(again, baseline)) throw corrupt('baseline round trip');
  progress('Building Motor:Cycles…'); await pause();
  const image = build(kit.stage2, baseline);
  if (image.length !== kit.finalLength || !equal(await sha256(image), hFinal)) throw corrupt('operating system image');
  progress('Packing and signing…'); await pause();
  const dirty2 = new Uint8Array(image.length).fill(1);
  for (let i = 0; i < Math.min(baseline.length, image.length); i++) dirty2[i] = baseline[i] !== image[i] ? 1 : 0;
  const packed2 = repackGrow(image, ops1, dirty2, baseline.length);
  const plains = [];
  for (const s of sections) { try { plains.push(depack(blob.subarray(s.off, s.off + s.size)).data); } catch { /* not packed */ } }
  const key = await findKey(plains, blob.subarray(0, blob.length - 32), blob.subarray(blob.length - 32));
  if (!key) throw corrupt('signature key');
  const rebuilt = await rebuild(blob, sections, kit.section, packed2, key);
  const out = wrap(buildStream(rebuilt, BYTES_PER_MSG), product, start);
  if (!equal(await sha256(out), hOutput)) throw corrupt('final package');
  progress('Done');
  return out;
}
