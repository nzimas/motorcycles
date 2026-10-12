// Renders an SVG to PNG: render-svg.swift IN.svg OUT.png SCALE
import AppKit
let src=URL(fileURLWithPath:CommandLine.arguments[1]),dst=URL(fileURLWithPath:CommandLine.arguments[2])
let img=NSImage(contentsOf:src)!;let scale=Double(CommandLine.arguments[3])!
let w=Int(img.size.width*scale),h=Int(img.size.height*scale)
let rep=NSBitmapImageRep(bitmapDataPlanes:nil,pixelsWide:w,pixelsHigh:h,bitsPerSample:8,samplesPerPixel:4,hasAlpha:true,isPlanar:false,colorSpaceName:.deviceRGB,bytesPerRow:0,bitsPerPixel:0)!
NSGraphicsContext.saveGraphicsState();NSGraphicsContext.current=NSGraphicsContext(bitmapImageRep:rep)
img.draw(in:NSRect(x:0,y:0,width:w,height:h));NSGraphicsContext.restoreGraphicsState()
try! rep.representation(using:.png,properties:[:])!.write(to:dst)
