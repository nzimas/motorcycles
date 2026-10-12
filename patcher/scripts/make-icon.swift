// Renders the Motor:Cycles app icon (1024x1024 PNG): argv[1] = output path.
import AppKit
let size = 1024.0
let image = NSImage(size: NSSize(width: size, height: size))
image.lockFocus()
let rect = NSRect(x: 0, y: 0, width: size, height: size).insetBy(dx: 100, dy: 100)
let path = NSBezierPath(roundedRect: rect, xRadius: 185, yRadius: 185)
NSGradient(starting: NSColor(calibratedRed: 0.10, green: 0.10, blue: 0.11, alpha: 1),
           ending: NSColor(calibratedRed: 0.02, green: 0.02, blue: 0.02, alpha: 1))!.draw(in: path, angle: -90)
// Six bars: a rhythmic pulse, the loudest in signal red.
let heights: [CGFloat] = [0.30, 0.62, 0.42, 0.88, 0.50, 0.70]
let barWidth = rect.width / 11
for (i, h) in heights.enumerated() {
    let x = rect.minX + barWidth * (1 + CGFloat(i) * 1.6)
    let bar = NSRect(x: x, y: rect.minY + rect.height * 0.18, width: barWidth, height: rect.height * 0.64 * h)
    (i == 3 ? NSColor(calibratedRed: 0.93, green: 0.16, blue: 0.13, alpha: 1) : NSColor(white: 0.92, alpha: 1)).setFill()
    NSBezierPath(roundedRect: bar, xRadius: barWidth / 2, yRadius: barWidth / 2).fill()
}
image.unlockFocus()
let rep = NSBitmapImageRep(data: image.tiffRepresentation!)!
try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
