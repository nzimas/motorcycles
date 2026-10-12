// Command-line Motor:Cycles patcher: mcpatch STOCK.syx KIT.mcpatch OUT.syx
import Foundation
import MotorCyclesKit

let args = CommandLine.arguments
guard args.count == 4 else {
    FileHandle.standardError.write("usage: mcpatch STOCK.syx motorcycles.mcpatch OUT.syx\n".data(using: .utf8)!)
    exit(2)
}
do {
    let kit = try PatchKit(try Data(contentsOf: URL(fileURLWithPath: args[2])))
    let out = try Patcher.convert(stock: try Data(contentsOf: URL(fileURLWithPath: args[1])), kit: kit) { print($0) }
    try out.write(to: URL(fileURLWithPath: args[3]))
} catch {
    FileHandle.standardError.write("error: \(error.localizedDescription)\n".data(using: .utf8)!)
    exit(1)
}
