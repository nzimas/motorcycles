// Motor:Cycles patcher core: turns the user's own stock Model:Cycles OS 1.13
// .syx into Motor:Cycles using a patch kit (motorcycles.mcpatch) that holds no
// Elektron code. Port of tools/apply_patchkit.py and the MIT-licensed mtlib
// (Elektron SysEx transport, ELE3 container, aPLib variant) from Modded-Cycles.
// Every stage is checked against the SHA-256 hashes recorded in the kit, so the
// output is byte-identical to the reference build or nothing is produced.
import CryptoKit
import Foundation

public enum PatchError: LocalizedError {
    case notStockFirmware
    case badKit(String)
    case corrupt(String)

    public var errorDescription: String? {
        switch self {
        case .notStockFirmware:
            return "This file is not the official Model:Cycles OS 1.13 firmware. Download it from elektron.se and try again."
        case .badKit(let why): return "The patch kit is damaged (\(why))."
        case .corrupt(let why): return "The conversion failed a safety check (\(why)). Nothing was written."
        }
    }
}

func sha256(_ bytes: [UInt8]) -> [UInt8] { Array(SHA256.hash(data: bytes)) }

// MARK: - Elektron OS-update SysEx transport

enum SysEx {
    static let elektron: [UInt8] = [0x00, 0x20, 0x3c]
    static let messageLength = 128, payloadOffset = 10, payloadEnd = 126, checksumOffset = 126
    static let bytesPerMessage = 101
    // product id -> (device byte, mask seed V, checksum base C0)
    static let products: [UInt8: (UInt8, Int, Int)] = [0x11: (0x0C, 0x3A, 0x22), 0x0F: (0x0A, 0x3C, 0x1E)]

    static func unpack7(_ p: ArraySlice<UInt8>) -> [UInt8] {
        var out: [UInt8] = []; out.reserveCapacity(p.count)
        let p = Array(p)
        var i = 0
        while i < p.count {
            let hi = Int(p[i])
            for k in 0..<7 where i + 1 + k < p.count {
                out.append(UInt8(((hi << (k + 1)) & 0x80) | Int(p[i + 1 + k])))
            }
            i += 8
        }
        return out
    }

    static func pack7(_ b: ArraySlice<UInt8>) -> [UInt8] {
        var out: [UInt8] = []
        let b = Array(b)
        var i = 0
        while i < b.count {
            let chunk = b[i..<min(i + 7, b.count)]
            var hi: UInt8 = 0
            for (k, v) in chunk.enumerated() { hi |= (v >> 7) << UInt8(6 - k) }
            out.append(hi)
            out.append(contentsOf: chunk.map { $0 & 0x7F })
            i += 7
        }
        return out
    }

    static func checksum(_ m: [UInt8], _ v: Int, _ c0: Int) -> UInt8 {
        var sum = 0
        for i in 8..<126 { sum += Int(m[i]) ^ ((v - i) & 0x3F) }
        return UInt8((c0 - sum) & 0x7F)
    }

    static func split(_ raw: [UInt8]) -> [[UInt8]] {
        var msgs: [[UInt8]] = []
        var i = 0
        while let a = raw[i...].firstIndex(of: 0xF0), let b = raw[a...].firstIndex(of: 0xF7) {
            msgs.append(Array(raw[a...b])); i = b + 1
            if i >= raw.count { break }
        }
        return msgs
    }

    static func u21(_ b: ArraySlice<UInt8>) -> Int {
        let b = Array(b); return (Int(b[0]) << 14) | (Int(b[1]) << 7) | Int(b[2])
    }

    static func p21(_ v: Int) -> [UInt8] { [UInt8((v >> 14) & 0x7F), UInt8((v >> 7) & 0x7F), UInt8(v & 0x7F)] }

    static func unwrap(_ raw: [UInt8]) throws -> (body: [UInt8], product: UInt8, startSeq: Int) {
        let msgs = split(raw)
        guard msgs.count >= 3 else { throw PatchError.notStockFirmware }
        let head = msgs[0], tail = msgs[msgs.count - 1], data = msgs[1..<(msgs.count - 1)]
        guard Array(head[1..<4]) == elektron, Array(tail[1..<4]) == elektron,
              let (_, v, c0) = products[head[4]],
              head[6] == 0x7f, head[7] == 0x01, tail[6] == 0x7f, tail[7] == 0x02 else { throw PatchError.notStockFirmware }
        let start = u21(head[9..<12]), count = u21(head[12..<15])
        guard count == data.count else { throw PatchError.notStockFirmware }
        var body: [UInt8] = []
        for m in data {
            guard m.count == messageLength, m[checksumOffset] == checksum(m, v, c0) else { throw PatchError.notStockFirmware }
            body += unpack7(m[payloadOffset..<payloadEnd])
        }
        return (body, head[4], start)
    }

    static func wrap(_ body: [UInt8], product: UInt8, startSeq: Int) throws -> [UInt8] {
        guard let (dev, v, c0) = products[product], body.count % bytesPerMessage == 0 else { throw PatchError.corrupt("transport") }
        let count = body.count / bytesPerMessage
        let pre: [UInt8] = [0xf0] + elektron + [product, 0x00]
        var out: [UInt8] = []
        out += pre + [0x7f, 0x01, dev] + p21(startSeq) + p21(count) + [0xf7]
        for n in 0..<count {
            let seq = startSeq + n
            var m = pre + [0x7e, 0x00, UInt8((seq >> 7) & 0x7F), UInt8(seq & 0x7F)]
            m += pack7(body[(n * bytesPerMessage)..<((n + 1) * bytesPerMessage)])
            m += [0, 0xf7]
            m[checksumOffset] = checksum(m, v, c0)
            out += m
        }
        out += pre + [0x7f, 0x02, dev] + p21(startSeq) + p21(count) + [0xf7]
        return out
    }
}

// MARK: - ELE3 container

struct Section { let index: Int, id: UInt32, off: Int, size: Int }

enum Container {
    static let preamble = 8, tableOffset = 0x20, entrySize = 16, align = 16, digestLength = 32
    static let keyAnchor: [UInt8] = [0xbe, 0xf9, 0xa3, 0xf7, 0xc6, 0x71, 0x78, 0xf2]

    static func be32(_ b: [UInt8], _ o: Int) -> Int {
        (Int(b[o]) << 24) | (Int(b[o + 1]) << 16) | (Int(b[o + 2]) << 8) | Int(b[o + 3])
    }
    static func put32(_ b: inout [UInt8], _ o: Int, _ v: Int) {
        b[o] = UInt8((v >> 24) & 0xff); b[o + 1] = UInt8((v >> 16) & 0xff); b[o + 2] = UInt8((v >> 8) & 0xff); b[o + 3] = UInt8(v & 0xff)
    }

    static func parse(_ stream: [UInt8]) throws -> (blob: [UInt8], sections: [Section]) {
        guard stream.count >= preamble + tableOffset else { throw PatchError.notStockFirmware }
        let declared = be32(stream, 0)
        guard preamble + declared <= stream.count else { throw PatchError.notStockFirmware }
        let blob = Array(stream[preamble..<(preamble + declared)])
        guard Array(blob[0..<4]) == Array("ELE3".utf8) else { throw PatchError.notStockFirmware }
        let n = be32(blob, 0x1C)
        guard (1...16).contains(n) else { throw PatchError.notStockFirmware }
        let sections = (0..<n).map { s -> Section in
            let e = tableOffset + s * entrySize
            return Section(index: s, id: UInt32(be32(blob, e)), off: be32(blob, e + 4), size: be32(blob, e + 8))
        }
        return (blob, sections)
    }

    static func contentChecksum(_ c: [UInt8]) -> Int {
        var acc: UInt32 = 0
        for k in 0..<(c.count / 4) { acc = acc &+ (UInt32(k + 1) ^ UInt32(be32(c, 4 * k))) }
        return Int(acc)
    }

    static func hmac(_ key: [UInt8], _ message: [UInt8]) -> [UInt8] {
        Array(HMAC<SHA256>.authenticationCode(for: message, using: SymmetricKey(data: key)))
    }

    static func findKey(_ buffers: [[UInt8]], message: [UInt8], expect: [UInt8]) -> [UInt8]? {
        for buf in buffers {
            var i = 0
            while i + keyAnchor.count <= buf.count {
                guard buf[i] == keyAnchor[0], Array(buf[i..<(i + keyAnchor.count)]) == keyAnchor else { i += 1; continue }
                let s = i + keyAnchor.count
                var p = s
                while p < buf.count && buf[p] >= 0x20 && buf[p] < 0x7F && p - s < 64 { p += 1 }
                if p > s && p < buf.count && buf[p] == 0 && p + 33 <= buf.count {
                    let text = Array(buf[s..<p]), const = Array(buf[(p + 1)..<(p + 33)])
                    let h = sha256(text), hr = sha256(Array(text.reversed()))
                    let key = (0..<32).map { h[$0] ^ hr[$0] ^ const[$0] }
                    if hmac(key, message) == expect { return key }
                }
                i += 1
            }
        }
        return nil
    }

    static func rebuild(blob: [UInt8], sections: [Section], replacing id: UInt32, with data: [UInt8], key: [UInt8]) -> [UInt8] {
        let first = sections.map(\.off).min()!
        var out = Array(blob[0..<first])
        var pos = first
        for sec in sections.sorted(by: { $0.off < $1.off }) {
            let bytes = sec.id == id ? data : Array(blob[sec.off..<(sec.off + sec.size)])
            pos = (pos + align - 1) & ~(align - 1)
            if out.count < pos { out += [UInt8](repeating: 0, count: pos - out.count) }
            if out.count < pos + bytes.count { out += [UInt8](repeating: 0, count: pos + bytes.count - out.count) }
            out.replaceSubrange(pos..<(pos + bytes.count), with: bytes)
            let e = tableOffset + sec.index * entrySize
            put32(&out, e + 4, pos); put32(&out, e + 8, bytes.count)
            pos += bytes.count
        }
        let hmacOff = (pos + 4 + align - 1) & ~(align - 1)
        if out.count < hmacOff { out += [UInt8](repeating: 0, count: hmacOff - out.count) }
        out = Array(out[0..<hmacOff]) + hmac(key, Array(out[0..<hmacOff]))
        return out
    }

    static func buildStream(_ blob: [UInt8], padTo: Int) -> [UInt8] {
        var stream = [UInt8](repeating: 0, count: preamble)
        put32(&stream, 0, blob.count); put32(&stream, 4, contentChecksum(blob))
        stream += blob
        if stream.count % padTo != 0 { stream += [UInt8](repeating: 0, count: padTo - stream.count % padTo) }
        return stream
    }
}

// MARK: - aPLib variant

enum APLib {
    static let header = 8, offsetBias = 767, reuseGamma = 2, farThreshold = 3328, endGamma = 0x1000002

    struct Op { let literal: Bool, pos: Int, off: Int, n: Int }

    static func depack(_ src: [UInt8]) throws -> (data: [UInt8], ops: [Op]) {
        guard src.count >= header else { throw PatchError.notStockFirmware }
        var ip = header, tag = 0
        func bit() throws -> Int {
            tag <<= 1
            if tag & 0xFF == 0 {
                guard ip < src.count else { throw PatchError.notStockFirmware }
                let by = Int(src[ip]); ip += 1
                tag = (by << 1) | 1
                return (by >> 7) & 1
            }
            return (tag >> 8) & 1
        }
        func gamma() throws -> Int {
            var v = 1
            while true {
                v = (v << 1) + (try bit())
                if try bit() == 1 { return v }
                if v > 0x02000000 { throw PatchError.notStockFirmware }
            }
        }
        var out: [UInt8] = []; out.reserveCapacity(2_000_000)
        var ops: [Op] = []; ops.reserveCapacity(400_000)
        var lastOff = 1
        while true {
            if try bit() == 1 {
                guard ip < src.count else { throw PatchError.notStockFirmware }
                ops.append(Op(literal: true, pos: out.count, off: 0, n: 1))
                out.append(src[ip]); ip += 1
                continue
            }
            let g = try gamma()
            var off: Int
            if g == reuseGamma { off = lastOff } else {
                guard ip < src.count else { throw PatchError.notStockFirmware }
                off = ((g << 8) + Int(src[ip])) & 0xFFFFFFFF; ip += 1
                if off == offsetBias { break }
                off -= offsetBias; lastOff = off
            }
            let sl = 2 * (try bit()) + (try bit())
            var l = sl != 0 ? sl : (try gamma()) + 2
            if off > farThreshold { l += 1 }
            let n = l + 1
            guard off > 0, off <= out.count else { throw PatchError.notStockFirmware }
            ops.append(Op(literal: false, pos: out.count, off: off, n: n))
            let s = out.count - off
            for k in 0..<n { out.append(out[s + k]) }
        }
        return (out, ops)
    }

    struct Writer {
        var o = [UInt8](repeating: 0, count: APLib.header)
        var tagPos = -1, tagBits = 0
        mutating func bit(_ v: Int) {
            if tagBits == 0 { tagPos = o.count; o.append(0); tagBits = 8 }
            if v != 0 { o[tagPos] |= UInt8(1 << (tagBits - 1)) }
            tagBits -= 1
        }
        mutating func gamma(_ v: Int) {
            let bitLength = Int.bitWidth - v.leadingZeroBitCount
            var i = bitLength - 2
            while i >= 0 { bit((v >> i) & 1); bit(i == 0 ? 1 : 0); i -= 1 }
        }
        mutating func literal(_ b: UInt8) { bit(1); o.append(b) }
        mutating func match(_ off: Int, _ n: Int, _ lastOff: Int) {
            bit(0)
            if off == lastOff { gamma(APLib.reuseGamma) } else {
                let raw = off + APLib.offsetBias
                gamma(raw >> 8); o.append(UInt8(raw & 0xFF))
            }
            let base = n - 1 - (off > APLib.farThreshold ? 1 : 0)
            if base <= 3 { bit(base >> 1); bit(base & 1) } else { bit(0); bit(0); gamma(base - 2) }
        }
        mutating func end() { bit(0); gamma(APLib.endGamma); o.append(0xFF) }
    }

    /// Re-emit the stock op stream over the patched image (ops touching a changed
    /// byte become literals), then greedily compress the appended tail.
    static func repackGrow(_ data: [UInt8], ops: [Op], dirty: [Bool], originalLength: Int) -> [UInt8] {
        var w = Writer()
        var lastOff = 1
        var prefix = [Int](repeating: 0, count: dirty.count + 1)    // dirty counts for range queries
        for i in 0..<dirty.count { prefix[i + 1] = prefix[i] + (dirty[i] ? 1 : 0) }
        func anyDirty(_ a: Int, _ b: Int) -> Bool { prefix[max(0, min(b, dirty.count))] - prefix[max(0, min(a, dirty.count))] > 0 }
        for op in ops {
            if op.literal { w.literal(data[op.pos]); continue }
            let end = op.pos + op.n, src = op.pos - op.off
            if anyDirty(op.pos, end) || anyDirty(src, src + op.n) {
                for k in op.pos..<end { w.literal(data[k]) }
            } else {
                w.match(op.off, op.n, lastOff); lastOff = op.off
            }
        }
        // Tail: greedy, matches of 4..0xFFFF within the appended part only.
        var table: [UInt32: Int] = [:]
        let n = data.count
        func key(_ i: Int) -> UInt32 { UInt32(data[i]) << 24 | UInt32(data[i + 1]) << 16 | UInt32(data[i + 2]) << 8 | UInt32(data[i + 3]) }
        var i = originalLength
        while i < n {
            var bestLen = 0, bestOff = 0
            if i + 4 <= n {
                let k = key(i)
                if let cand = table[k], i - cand <= 1 << 20 {
                    var length = 4
                    let limit = min(n - i, 0xFFFF)
                    while length < limit && data[cand + length] == data[i + length] { length += 1 }
                    bestLen = length; bestOff = i - cand
                }
                table[k] = i
            }
            if bestLen >= 4 {
                w.match(bestOff, bestLen, lastOff); lastOff = bestOff
                var k = i + 1
                while k < min(i + bestLen, n - 4 + 1) { table[key(k)] = k; k += 1 }
                i += bestLen
            } else {
                w.literal(data[i]); i += 1
            }
        }
        w.end()
        var body = w.o
        let streamLength = body.count - header
        Container.put32(&body, 0, streamLength)
        var sum: UInt32 = 0
        for b in body[header...] { sum = sum &+ UInt32(b) }
        Container.put32(&body, 4, Int(sum))
        return body
    }
}

// MARK: - Patch kit (format 2: two stages, see tools/make_patchkit.py)

struct Recipe {
    let ops: [(copy: Bool, src: Int, n: Int, bytes: ArraySlice<UInt8>)]
    func build(from base: [UInt8]) throws -> [UInt8] {
        var out: [UInt8] = []
        for op in ops {
            if op.copy {
                guard op.src + op.n <= base.count else { throw PatchError.badKit("copy range") }
                out += base[op.src..<(op.src + op.n)]
            } else { out += op.bytes }
        }
        return out
    }
}

public struct PatchKit {
    let section: UInt32
    let stockSyx: [UInt8], stockMain: [UInt8], baselineMain: [UInt8], baselinePacked: [UInt8], finalMain: [UInt8], output: [UInt8]
    let baselineLength: Int, finalLength: Int
    let stage1: Recipe, stage2: Recipe
    let baselineDirty: [(start: Int, length: Int)]

    static func recipe(_ k: [UInt8], _ p: inout Int) throws -> Recipe {
        guard p + 4 <= k.count else { throw PatchError.badKit("truncated") }
        let count = Container.be32(k, p); p += 4
        var ops: [(Bool, Int, Int, ArraySlice<UInt8>)] = []
        for _ in 0..<count {
            guard p < k.count else { throw PatchError.badKit("truncated") }
            if k[p] == 0 {
                guard p + 9 <= k.count else { throw PatchError.badKit("truncated") }
                ops.append((true, Container.be32(k, p + 1), Container.be32(k, p + 5), [])); p += 9
            } else {
                guard p + 5 <= k.count else { throw PatchError.badKit("truncated") }
                let n = Container.be32(k, p + 1)
                guard p + 5 + n <= k.count else { throw PatchError.badKit("truncated") }
                ops.append((false, 0, n, k[(p + 5)..<(p + 5 + n)])); p += 5 + n
            }
        }
        return Recipe(ops: ops.map { (copy: $0.0, src: $0.1, n: $0.2, bytes: $0.3) })
    }

    public init(_ data: Data) throws {
        let k = [UInt8](data)
        guard k.count >= 208, Array(k[0..<4]) == Array("MCPK".utf8) else { throw PatchError.badKit("header") }
        guard Container.be32(k, 4) >> 16 == 2 else { throw PatchError.badKit("version") }
        section = UInt32(Container.be32(k, 4) & 0xffff)
        let h = (0..<6).map { Array(k[(8 + 32 * $0)..<(40 + 32 * $0)]) }
        stockSyx = h[0]; stockMain = h[1]; baselineMain = h[2]; baselinePacked = h[3]; finalMain = h[4]; output = h[5]
        baselineLength = Container.be32(k, 200); finalLength = Container.be32(k, 204)
        var p = 208
        stage1 = try PatchKit.recipe(k, &p)
        stage2 = try PatchKit.recipe(k, &p)
        guard p + 4 <= k.count else { throw PatchError.badKit("truncated") }
        let count = Container.be32(k, p); p += 4
        guard p + 8 * count <= k.count else { throw PatchError.badKit("truncated") }
        baselineDirty = (0..<count).map { (start: Container.be32(k, p + 8 * $0), length: Container.be32(k, p + 8 * $0 + 4)) }
    }
}

public enum Patcher {
    public typealias Progress = (String) -> Void

    /// Stock Model:Cycles OS 1.13 .syx -> Motor:Cycles .syx, byte for byte the
    /// hardware-tested two-stage build (Modded-Cycles baseline, then our
    /// payload), or throws. Nothing is written unless every hash matches.
    public static func convert(stock: Data, kit: PatchKit, progress: Progress = { _ in }) throws -> Data {
        let raw = [UInt8](stock)
        progress("Checking your firmware…")
        guard sha256(raw) == kit.stockSyx else { throw PatchError.notStockFirmware }
        let (body, product, startSeq) = try SysEx.unwrap(raw)
        let (blob, sections) = try Container.parse(body)
        guard let main = sections.first(where: { $0.id == kit.section }) else { throw PatchError.notStockFirmware }
        progress("Unpacking the operating system…")
        let (stockImage, ops0) = try APLib.depack(Array(blob[main.off..<(main.off + main.size)]))
        guard sha256(stockImage) == kit.stockMain else { throw PatchError.notStockFirmware }
        progress("Applying the Modded-Cycles mods…")
        let baseline = try kit.stage1.build(from: stockImage)
        guard baseline.count == kit.baselineLength, sha256(baseline) == kit.baselineMain else { throw PatchError.corrupt("baseline image") }
        var dirty1 = [Bool](repeating: false, count: baseline.count)
        for r in kit.baselineDirty {
            guard r.start + r.length <= baseline.count else { throw PatchError.badKit("dirty range") }
            for i in r.start..<(r.start + r.length) { dirty1[i] = true }
        }
        let packed1 = APLib.repackGrow(baseline, ops: ops0, dirty: dirty1, originalLength: stockImage.count)
        guard sha256(packed1) == kit.baselinePacked else { throw PatchError.corrupt("baseline packing") }
        let (baselineAgain, ops1) = try APLib.depack(packed1)
        guard baselineAgain == baseline else { throw PatchError.corrupt("baseline round trip") }
        progress("Building Motor:Cycles…")
        let image = try kit.stage2.build(from: baseline)
        guard image.count == kit.finalLength, sha256(image) == kit.finalMain else { throw PatchError.corrupt("operating system image") }
        progress("Packing and signing…")
        var dirty2 = [Bool](repeating: true, count: image.count)
        for i in 0..<min(baseline.count, image.count) { dirty2[i] = baseline[i] != image[i] }
        let packed2 = APLib.repackGrow(image, ops: ops1, dirty: dirty2, originalLength: baseline.count)
        var plains: [[UInt8]] = []
        for s in sections { if let d = try? APLib.depack(Array(blob[s.off..<(s.off + s.size)])) { plains.append(d.data) } }
        guard let key = Container.findKey(plains, message: Array(blob[0..<(blob.count - 32)]), expect: Array(blob[(blob.count - 32)...])) else {
            throw PatchError.corrupt("signature key")
        }
        let rebuilt = Container.rebuild(blob: blob, sections: sections, replacing: kit.section, with: packed2, key: key)
        let out = try SysEx.wrap(Container.buildStream(rebuilt, padTo: SysEx.bytesPerMessage), product: product, startSeq: startSeq)
        guard sha256(out) == kit.output else { throw PatchError.corrupt("final package") }
        progress("Done")
        return Data(out)
    }
}
