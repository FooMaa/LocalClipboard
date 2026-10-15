import SwiftUI
import AppKit

typealias InstallerState<Value> = State<Value>

enum MaintenanceAction: String { case install, remove }

struct InstallerView: View {
    @InstallerState private var language = Locale.preferredLanguages.first?.hasPrefix("ru") == true ? "ru" : "en"
    @InstallerState private var action = MaintenanceAction.install
    @InstallerState private var working = false
    @InstallerState private var message = ""
    @InstallerState private var installed = FileManager.default.fileExists(atPath: "/Applications/LocalClipboard.app/Contents/Info.plist")
    private func text(_ english: String, _ russian: String) -> String { language == "ru" ? russian : english }
    private func quoteShell(_ value: String) -> String { "'" + value.replacingOccurrences(of: "'", with: "'\\''") + "'" }
    private func quoteAppleScript(_ value: String) -> String {
        "\"" + value.replacingOccurrences(of: "\\", with: "\\\\").replacingOccurrences(of: "\"", with: "\\\"") + "\""
    }
    private func performAction() {
        guard !working, let resources = Bundle.main.resourceURL else { return }
        let package = resources.appendingPathComponent(action == .install ? "Install.pkg" : "Remove.pkg")
        guard FileManager.default.fileExists(atPath: package.path) else {
            message = text("Installer payload is missing.", "В установщике отсутствует пакет."); return
        }
        let command = "/usr/sbin/installer -pkg " + quoteShell(package.path) + " -target /"
        let source = "do shell script " + quoteAppleScript(command) + " with administrator privileges"
        let completion = action == .install
            ? text("Installed. Open LocalClipboard to register or continue with your profile.", "Установлено. Откройте LocalClipboard для регистрации или работы с вашим профилем.")
            : text("Removed. Your profile and received files were kept.", "Приложение удалено. Профиль и полученные файлы сохранены.")
        let opening = action == .install
        let selectedLanguage = language
        working = true; message = ""
        DispatchQueue.global(qos: .userInitiated).async {
            let process = Process()
            process.executableURL = URL(fileURLWithPath: "/usr/bin/osascript")
            process.arguments = ["-e", source]
            let errors = Pipe(); process.standardError = errors
            process.standardOutput = FileHandle.nullDevice
            var failure = ""
            do {
                try process.run()
                let data = errors.fileHandleForReading.readDataToEndOfFile()
                process.waitUntilExit()
                if process.terminationStatus != 0 { failure = String(decoding: data, as: UTF8.self) }
            } catch { failure = error.localizedDescription }
            DispatchQueue.main.async {
                working = false
                message = failure.isEmpty ? completion : failure
                installed = FileManager.default.fileExists(atPath: "/Applications/LocalClipboard.app/Contents/Info.plist")
                if !installed { action = .install }
                if failure.isEmpty && opening {
                    let configuration = NSWorkspace.OpenConfiguration()
                    configuration.arguments = ["--language", selectedLanguage]
                    NSWorkspace.shared.openApplication(at: URL(fileURLWithPath: "/Applications/LocalClipboard.app"), configuration: configuration)
                }
            }
        }
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 22) {
            HStack(spacing: 16) {
                Image(nsImage: NSApp.applicationIconImage).resizable().frame(width: 56, height: 56)
                VStack(alignment: .leading, spacing: 5) {
                    Text("LocalClipboard").font(.system(size: 26, weight: .semibold))
                    Text(text("Your space. Your devices.", "Твоё пространство. Твои устройства.")).foregroundColor(.secondary)
                }
                Spacer()
            }
            Picker("Language / Язык", selection: $language) {
                Text("Русский").tag("ru")
                Text("English").tag("en")
            }.pickerStyle(.segmented).disabled(working)
            Divider()
            ScrollView {
              VStack(alignment: .leading, spacing: 18) {
            Text(text("Choose an action", "Выберите действие")).font(.title2.weight(.semibold))
            Picker(text("Action", "Действие"), selection: $action) {
                Text(installed ? text("Reinstall LocalClipboard", "Переустановить LocalClipboard") : text("Install LocalClipboard", "Установить LocalClipboard"))
                    .tag(MaintenanceAction.install)
                Text(text("Uninstall LocalClipboard", "Удалить LocalClipboard")).tag(MaintenanceAction.remove).disabled(!installed)
            }.pickerStyle(.radioGroup).labelsHidden().disabled(working)
            Text(action == .install
                ? text("Install the application in Applications. Your existing profile and files are preserved.", "Установить приложение в «Программы». Существующий профиль и файлы сохранятся.")
                : text("Remove the application and automatic startup. Your profile and files stay on this Mac for a future reinstall.", "Удалить приложение и автозапуск. Профиль и файлы останутся на Mac для повторной установки."))
                .foregroundColor(.secondary).fixedSize(horizontal: false, vertical: true)
            if !message.isEmpty { Text(message).textSelection(.enabled).fixedSize(horizontal: false, vertical: true) }
              }.frame(maxWidth: .infinity, alignment: .leading)
            }
            HStack {
                if working {
                    ProgressView().controlSize(.small)
                    Text(text("Please wait…", "Подождите…")).foregroundColor(.secondary)
                }
                Spacer()
            }.frame(height: 20)
            HStack {
                if installed {
                    Button(text("Open application", "Открыть приложение")) {
                        NSWorkspace.shared.open(URL(fileURLWithPath: "/Applications/LocalClipboard.app"))
                    }.disabled(working)
                }
                Spacer()
                Button(action == .install ? text("Install", "Установить") : text("Uninstall", "Удалить"), action: performAction)
                    .keyboardShortcut(.defaultAction).disabled(working || (action == .remove && !installed))
            }
        }
        .padding(32).frame(width: 560, height: 510)
        .preferredColorScheme(.dark)
    }
}

@main struct LocalClipboardInstaller: App {
    var body: some Scene {
        WindowGroup("LocalClipboard") { InstallerView() }.windowResizability(.contentSize)
    }
}
