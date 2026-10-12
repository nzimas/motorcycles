// Motor:Cycles patcher for macOS: load your own stock Model:Cycles OS 1.13
// firmware, convert, save the Motor:Cycles .syx, install it with Elektron Transfer.
import AppKit
import MotorCyclesKit
import SwiftUI
import UniformTypeIdentifiers

let appVersion = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"

@MainActor
final class Model: ObservableObject {
    enum Phase: Equatable { case empty, ready, converting, done, failed }
    @Published var phase: Phase = .empty
    @Published var stockName = ""
    @Published var status = ""
    var stock: Data?
    var output: Data?

    func load(_ url: URL) {
        output = nil
        do {
            let data = try Data(contentsOf: url)
            stock = data; stockName = url.lastPathComponent
            phase = .ready; status = "Ready to convert."
        } catch {
            fail("Could not read that file.")
        }
    }

    func convert() {
        guard let stock else { return }
        phase = .converting; status = "Working…"
        Task.detached(priority: .userInitiated) {
            do {
                guard let url = Bundle.main.url(forResource: "motorcycles", withExtension: "mcpatch") else {
                    throw PatchError.badKit("missing from the app")
                }
                let kit = try PatchKit(try Data(contentsOf: url))
                let out = try Patcher.convert(stock: stock, kit: kit) { step in
                    Task { @MainActor in self.status = step }
                }
                await MainActor.run { self.output = out; self.phase = .done; self.status = "Motor:Cycles is ready." }
            } catch {
                await MainActor.run { self.fail(error.localizedDescription) }
            }
        }
    }

    func save() {
        guard let output else { return }
        let panel = NSSavePanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "syx") ?? .data]
        panel.nameFieldStringValue = "Motor-Cycles_\(appVersion).syx"
        panel.canCreateDirectories = true
        if panel.runModal() == .OK, let url = panel.url {
            do { try output.write(to: url); status = "Saved. Install it with Elektron Transfer." }
            catch { fail("Could not save the file.") }
        }
    }

    func choose() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "syx") ?? .data]
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url { load(url) }
    }

    private func fail(_ message: String) { phase = .failed; status = message }
}

struct DropZone: View {
    @ObservedObject var model: Model
    @State private var targeted = false

    var body: some View {
        let loaded = model.phase != .empty
        VStack(spacing: 10) {
            Image(systemName: loaded ? "checkmark.circle.fill" : "square.and.arrow.down")
                .font(.system(size: 30, weight: .light))
                .foregroundStyle(loaded ? Color.accentColor : .secondary)
            if loaded {
                Text(model.stockName).font(.system(.body, design: .monospaced)).lineLimit(1).truncationMode(.middle)
                Button("Choose another file…") { model.choose() }.buttonStyle(.link)
            } else {
                Text("Drop the stock Model:Cycles OS 1.13 firmware here").multilineTextAlignment(.center)
                Button("Choose File…") { model.choose() }.buttonStyle(.link)
            }
        }
        .frame(maxWidth: .infinity, minHeight: 150)
        .padding(16)
        .background(RoundedRectangle(cornerRadius: 14).fill(Color.primary.opacity(targeted ? 0.08 : 0.04)))
        .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(style: StrokeStyle(lineWidth: 1.5, dash: [6, 5]))
            .foregroundStyle(targeted ? Color.accentColor : Color.secondary.opacity(0.5)))
        .onDrop(of: [.fileURL], isTargeted: $targeted) { providers in
            guard let provider = providers.first else { return false }
            _ = provider.loadObject(ofClass: URL.self) { url, _ in
                if let url { Task { @MainActor in model.load(url) } }
            }
            return true
        }
    }
}

struct ContentView: View {
    @StateObject private var model = Model()

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Motor:Cycles").font(.system(size: 30, weight: .bold, design: .rounded))
                Text("Rhythmic noise firmware for the Elektron Model:Cycles")
                    .font(.callout).foregroundStyle(.secondary)
            }
            DropZone(model: model)
            HStack(spacing: 12) {
                Button {
                    model.convert()
                } label: {
                    Text("Convert").frame(maxWidth: .infinity)
                }
                .controlSize(.large)
                .buttonStyle(.borderedProminent)
                .disabled(model.phase != .ready && model.phase != .failed || model.stock == nil)

                Button {
                    model.save()
                } label: {
                    Text("Save…").frame(maxWidth: .infinity)
                }
                .controlSize(.large)
                .buttonStyle(.bordered)
                .disabled(model.phase != .done)
            }
            HStack(spacing: 8) {
                if model.phase == .converting { ProgressView().controlSize(.small) }
                Text(model.status.isEmpty ? "Your file never leaves your Mac." : model.status)
                    .font(.callout)
                    .foregroundStyle(model.phase == .failed ? Color.red : .secondary)
            }
            .frame(minHeight: 20)
            Divider()
            Text("Then send the saved file to your Model:Cycles with Elektron Transfer. Installing custom firmware may void your warranty and is at your own risk.")
                .font(.footnote).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(28)
        .frame(width: 460)
    }
}

struct MotorCyclesApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    var body: some Scene {
        WindowGroup("Motor:Cycles") { ContentView() }
            .windowResizability(.contentSize)
            .commands { CommandGroup(replacing: .newItem) {} }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

MotorCyclesApp.main()
