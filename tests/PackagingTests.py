"""Exercise installer command arguments and removal guards without touching a device."""
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='localclipboard-packaging-')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.log = self.directory / 'commands.jsonl'
        self.env = dict(os.environ, LC_LANGUAGE='ru', LC_TEST_LOG=str(self.log))
        binaries = self.directory / 'bin'
        binaries.mkdir()
        for tool in ('adb', 'xcrun', 'dpkg-query', 'apt-get', 'sudo', 'systemctl', 'localclipboard'):
            executable = binaries / tool
            executable.write_text('#!/usr/bin/env python3\nimport json, os, sys\nwith open(os.environ["LC_TEST_LOG"], "a") as log:\n    log.write(json.dumps(sys.argv[1:]) + "\\n")\n')
            with executable.open('a') as output:
                output.write('if sys.argv[0].endswith("dpkg-query") and os.environ.get("LC_TEST_INSTALLED") == "yes": print("install ok installed")\n')
                output.write('if "pm" in sys.argv and os.environ.get("LC_TEST_INSTALLED") == "yes": print("package:/data/app/clipboard/base.apk")\n')
                output.write('if "pm" in sys.argv and os.environ.get("LC_TEST_PM_EXIT"): sys.exit(int(os.environ["LC_TEST_PM_EXIT"]))\n')
                output.write('if "install" in sys.argv and os.environ.get("LC_TEST_INSTALL_ERROR"):\n    print(os.environ["LC_TEST_INSTALL_ERROR"], file=sys.stderr)\n    sys.exit(1)\n')
                output.write('if "--json-output" in sys.argv:\n    path = sys.argv[sys.argv.index("--json-output") + 1]\n    with open(path, "w") as f: json.dump({"result": {"apps": [{"bundleIdentifier": "org.example.personalclipboard"}]}}, f)\n')
            executable.chmod(0o755)
        self.env['PATH'] = str(binaries) + os.pathsep + self.env['PATH']

    def script(self, platform, name):
        directory = self.directory / platform
        directory.mkdir(exist_ok=True)
        shutil.copy(ROOT / 'packaging' / platform / name, directory / name)
        shutil.copy(ROOT / 'packaging/common/language.sh', directory / 'language.sh')
        return directory / name

    def commands(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def test_windows_packaging_finds_nsis_and_preserves_paths_with_spaces(self):
        repository = self.directory / 'repository with spaces'
        for name in ('scripts', 'third_party/monocypher', 'third_party/zxing', 'packaging/windows', 'build/Release'):
            (repository / name).mkdir(parents=True)
        shutil.copy(ROOT / 'scripts/package', repository / 'scripts/package')
        (repository / 'CMakeLists.txt').write_text('project(LocalClipboard VERSION 2.2.0 LANGUAGES C CXX)\n')
        for name in ('third_party/monocypher/LICENCE.md', 'third_party/zxing/LICENSE',
                     'third_party/zxing/NOTICES.txt', 'packaging/windows/installer.nsi', 'build/Release/LocalClipboard.exe'):
            (repository / name).write_text('test fixture')
        binaries = self.directory / 'windows-tools'
        binaries.mkdir()
        # Use only these tools so a host-installed NSIS cannot hide lookup bugs.
        for name in ('bash', 'dirname', 'mkdir', 'sed', 'mktemp', 'rm', 'cp', 'find', 'printenv'):
            (binaries / name).symlink_to(shutil.which(name))
        (binaries / 'python3').symlink_to(sys.executable)
        def executable(path, code):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#!/usr/bin/env python3\n' + code)
            path.chmod(0o755)
        executable(binaries / 'cygpath',
                   'import os, sys\np = sys.argv[-1]\n'
                   'if p.startswith("C:\\\\"): p = os.environ["LC_TEST_C_DRIVE"] + "/" + p[3:].replace("\\\\", "/")\n'
                   'print(p)\n')
        executable(binaries / 'windeployqt', 'pass\n')
        executable(binaries / 'powershell.exe',
                   'import os\nfrom pathlib import Path\nPath(os.environ["LOCALCLIPBOARD_PACKAGE"]).write_bytes(b"zip")\n')
        drive = self.directory / 'drive c'
        crt = self.directory / 'redist/x64/Microsoft.VC.CRT'
        crt.mkdir(parents=True)
        (crt / 'msvcp140.dll').write_text('test runtime')
        base_env = dict(self.env, PATH=str(binaries), VCToolsRedistDir=str(crt.parent.parent),
                        VSCMD_ARG_TGT_ARCH='x64', LC_TEST_C_DRIVE=str(drive))
        for name in ('NSIS_EXE', 'ProgramW6432', 'ProgramFiles', 'ProgramFiles(x86)'):
            base_env.pop(name, None)
        cases = [
            ({}, binaries / 'makensis'),
            ({'NSIS_EXE': r'C:\custom NSIS\makensis.exe'}, drive / 'custom NSIS/makensis.exe'),
            ({'ProgramFiles': r'C:\Program Files'}, drive / 'Program Files/NSIS/makensis.exe'),
            ({'ProgramFiles(x86)': r'C:\Program Files (x86)'}, drive / 'Program Files (x86)/NSIS/makensis.exe'),
        ]
        for environment, compiler in cases:
            with self.subTest(environment=environment):
                executable(compiler, 'import json, os, sys\nfrom pathlib import Path\n'
                           'Path(os.environ["LC_TEST_LOG"]).write_text(json.dumps(sys.argv[1:]) + "\\n")\n'
                           'output = next(a.split("=", 1)[1] for a in sys.argv if a.startswith("-DOUTPUT_FILE="))\n'
                           'Path(output).write_bytes(b"installer")\n')
                result = subprocess.run(['bash', str(repository / 'scripts/package'), '-t', 'windows', '-b', str(repository / 'build')],
                                        env=dict(base_env, **environment), capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                arguments = self.commands()[0]
                self.assertEqual(len(arguments), 4)
                self.assertEqual(arguments[0], '-DAPP_VERSION=2.2.0')
                self.assertEqual(arguments[-1], str(repository / 'packaging/windows/installer.nsi'))
                self.assertEqual((repository / 'dist/LocalClipboard-windows-installer.exe').read_bytes(), b'installer')
                compiler.unlink()

    def test_android_installer_passes_language_and_serial_literally(self):
        script = self.script('android', 'install.sh')
        result = subprocess.run(['bash', str(script), 'device with spaces'], env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.commands(), [
            ['-s', 'device with spaces', 'get-state'],
            ['-s', 'device with spaces', 'shell', 'pm', 'path', 'org.localclipboard'],
            ['-s', 'device with spaces', 'install', '-r', 'LocalClipboard.apk'],
            ['-s', 'device with spaces', 'shell', 'am', 'start', '-n', 'org.localclipboard/.ClipboardActivity', '--es', 'localclipboard_language', 'ru']])

    def test_existing_android_installation_can_be_removed_in_installer(self):
        script = self.script('android', 'install.sh')
        self.env['LC_TEST_INSTALLED'] = 'yes'
        result = subprocess.run(['bash', str(script), 'my phone'], env=self.env,
                                input='2\nDELETE\n', capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = self.commands()
        self.assertEqual(commands[-1], ['-s', 'my phone', 'uninstall', 'org.localclipboard'])
        self.assertFalse(any('install' in command for command in commands))

    def test_android_removal_cancel_does_not_reinstall(self):
        script = self.script('android', 'install.sh')
        self.env['LC_TEST_INSTALLED'] = 'yes'
        result = subprocess.run(['bash', str(script)], env=self.env,
                                input='2\nno\n', capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.commands()), 2)  # Only device and installation queries.

    def test_android_reinstall_keeps_data(self):
        script = self.script('android', 'install.sh')
        self.env['LC_TEST_INSTALLED'] = 'yes'
        result = subprocess.run(['bash', str(script)], env=self.env,
                                input='1\n', capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(['install', '-r', 'LocalClipboard.apk'], self.commands())
        self.assertFalse(any('uninstall' in command for command in self.commands()))

    def test_android_first_install_accepts_missing_package_exit_status(self):
        script = self.script('android', 'install.sh')
        self.env['LC_TEST_PM_EXIT'] = '1'
        result = subprocess.run(['bash', str(script)], env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(['install', '-r', 'LocalClipboard.apk'], self.commands())

    def test_android_failed_upgrade_preserves_registration_and_reports_reason(self):
        script = self.script('android', 'install.sh')
        self.env['LC_TEST_INSTALLED'] = 'yes'
        for error, explanation in [('INSTALL_FAILED_UPDATE_INCOMPATIBLE', 'Подпись APK отличается'),
                                   ('INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES', 'Подпись APK отличается'),
                                   ('INSTALL_FAILED_VERSION_DOWNGRADE', 'Установлена более новая версия'),
                                   ('INSTALL_FAILED_USER_RESTRICTED', 'Установка запрещена'),
                                   ('INSTALL_FAILED_INSUFFICIENT_STORAGE', 'Недостаточно места'),
                                   ('INSTALL_FAILED_NO_MATCHING_ABIS', 'Для Galaxy S25')]:
            with self.subTest(error=error):
                self.env['LC_TEST_INSTALL_ERROR'] = error
                result = subprocess.run(['bash', str(script)], env=self.env, input='1\n', capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(error, result.stderr)
                self.assertIn(explanation, result.stderr)
                self.assertFalse(any('uninstall' in command for command in self.commands()))
                self.assertFalse(any('am' in command for command in self.commands()))

    @unittest.skipUnless(os.uname().sysname == 'Darwin', 'iOS installer host')
    def test_ios_installer_removes_without_qt_or_build(self):
        script = self.script('ios', 'install.sh')
        self.env['IOS_DEVICE_ID'] = 'my iPhone'
        self.env['IOS_BUNDLE_ID'] = 'org.example.personalclipboard'
        self.env.pop('QT_IOS_ROOT', None)
        result = subprocess.run(['bash', str(script)], env=self.env,
                                input='2\nDELETE\n', capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.commands()[-1], ['devicectl', 'device', 'uninstall', 'app', '--device', 'my iPhone', 'org.example.personalclipboard'])

    def test_linux_explicit_removal_never_installs_missing_app(self):
        script = self.script('linux', 'install.sh')
        self.env['LC_ACTION'] = 'uninstall'
        self.env['HOME'] = str(self.directory)
        self.env['XDG_CONFIG_HOME'] = str(self.directory / 'custom-config')
        result = subprocess.run(['bash', str(script)], env=self.env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(any('install' in command for command in self.commands()))

    def test_linux_installer_removes_package_but_keeps_profile(self):
        script = self.script('linux', 'install.sh')
        self.env['LC_TEST_INSTALLED'] = 'yes'
        self.env['HOME'] = str(self.directory)
        self.env['XDG_CONFIG_HOME'] = str(self.directory / 'custom-config')
        profile = self.directory / 'profile.json'
        profile.write_text('personal profile')
        config = Path(self.env['XDG_CONFIG_HOME']) / 'autostart'
        config.mkdir(parents=True)
        startup = config / 'localclipboard.desktop'
        startup.write_text('startup')
        result = subprocess.run(['bash', str(script)], env=self.env,
                                input='2\n', capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(['apt-get', 'remove', '-y', 'localclipboard'], self.commands())
        self.assertFalse(any('install' in command for command in self.commands()))
        self.assertFalse(startup.exists())
        self.assertEqual(profile.read_text(), 'personal profile')

    @unittest.skipUnless(shutil.which('PlistBuddy', path='/usr/libexec'), 'macOS package guard')
    def test_mac_package_removal_rejects_unrelated_or_linked_bundle(self):
        app = self.directory / 'LocalClipboard.app'
        (app / 'Contents').mkdir(parents=True)
        info = app / 'Contents/Info.plist'
        info.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'org.localclipboard'}))
        script = self.directory / 'remove-postinstall'
        source = (ROOT / 'packaging/macos/remove-postinstall').read_text()
        source = source.replace('app=/Applications/LocalClipboard.app', 'app="' + str(app) + '"')
        source = source.replace("legacy='/Applications/Uninstall LocalClipboard.app'", 'legacy="' + str(self.directory / 'Legacy.app') + '"')
        script.write_text(source)
        command = ['bash', str(script), '--dry-run']
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
        info.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'org.example.unrelated'}))
        self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
        info.unlink()
        target = self.directory / 'other.plist'
        target.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'org.localclipboard'}))
        info.symlink_to(target)
        self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
        self.assertTrue(target.exists())

    @unittest.skipUnless(shutil.which('PlistBuddy', path='/usr/libexec'), 'macOS removal guard')
    def test_mac_uninstaller_rejects_unrelated_apps_and_data(self):
        app = self.directory / "Clipboard with ' spaces.app"
        contents = app / 'Contents'
        contents.mkdir(parents=True)
        info = contents / 'Info.plist'
        info.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'org.localclipboard'}))
        data = str(Path.home() / 'Library/Application Support/LocalClipboard/LocalClipboard')
        command = ['bash', str(ROOT / 'packaging/macos/uninstall.sh'), '--dry-run', str(app), data, str(os.getpid()), 'ru', 'keep']
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('mode=keep', result.stdout)
        self.assertTrue(app.exists())
        invalid_data = command.copy()
        invalid_data[5] = str(self.directory)
        self.assertEqual(subprocess.run(invalid_data, capture_output=True).returncode, 2)
        info.write_bytes(plistlib.dumps({'CFBundleIdentifier': 'org.example.unrelated'}))
        self.assertEqual(subprocess.run(command, capture_output=True).returncode, 2)
        self.assertTrue(app.exists())


if __name__ == '__main__':
    unittest.main()
