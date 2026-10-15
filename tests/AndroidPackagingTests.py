"""Verify signing argument handling and malformed/misaligned ELF rejection without a device."""
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
loader = importlib.machinery.SourceFileLoader('verify_apk', str(ROOT / 'scripts/verify-android-apk'))
spec = importlib.util.spec_from_loader(loader.name, loader)
verifier = importlib.util.module_from_spec(spec)
loader.exec_module(verifier)


class AndroidPackagingTests(unittest.TestCase):
    def test_elf_alignment_and_truncation(self):
        with tempfile.TemporaryDirectory() as tmp:
            apk = Path(tmp) / 'app.apk'
            for bits in (1, 2):
                for alignment in (4096, 16384, 65536):
                    data = bytearray(128)
                    data[:6] = b'\x7fELF' + bytes((bits, 1))
                    if bits == 2:
                        struct.pack_into('<Q', data, 32, 64)
                        struct.pack_into('<HH', data, 54, 56, 1)
                        struct.pack_into('<Q', data, 112, alignment)
                    else:
                        struct.pack_into('<I', data, 28, 64)
                        struct.pack_into('<HH', data, 42, 32, 1)
                        struct.pack_into('<I', data, 92, alignment)
                    struct.pack_into('<I', data, 64, 1)
                    with zipfile.ZipFile(apk, 'w') as archive:
                        archive.writestr('lib/test/libtest.so', data)
                    if alignment < 16384:
                        with self.assertRaises(ValueError): verifier.verify(apk)
                    else:
                        verifier.verify(apk)
            with zipfile.ZipFile(apk, 'w') as archive:
                archive.writestr('lib/test/libtest.so', b'\x7fELF')
            with self.assertRaises((ValueError, IndexError, struct.error)):
                verifier.verify(apk)

    def test_signer_keeps_passwords_out_of_arguments(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            tools = root / 'sdk/build-tools/36.0.0'; tools.mkdir(parents=True)
            log = root / 'commands.jsonl'
            source = '''#!/usr/bin/env python3
import json, os, shutil, sys
with open(os.environ['LC_SIGN_TEST_LOG'], 'a') as out: out.write(json.dumps(sys.argv[1:])+'\\n')
if sys.argv[0].endswith('zipalign') and '-c' not in sys.argv: shutil.copyfile(sys.argv[-2], sys.argv[-1])
if sys.argv[0].endswith('apksigner') and sys.argv[1]=='sign': shutil.copyfile(sys.argv[-1], sys.argv[sys.argv.index('--out')+1])
'''
            for name in ('apksigner', 'zipalign'):
                tool = tools / name; tool.write_text(source); tool.chmod(0o755)
            apk = root / 'unsigned.apk'; apk.write_bytes(b'test APK')
            key = root / 'private.keystore'; key.write_bytes(b'test key')
            output = root / 'signed.apk'
            env = dict(os.environ, ANDROID_SDK_ROOT=str(root / 'sdk'), LC_ANDROID_KEYSTORE=str(key),
                       LC_ANDROID_KEY_ALIAS='test', LC_ANDROID_STORE_PASSWORD='secret-store-value',
                       LC_ANDROID_KEY_PASSWORD='secret-key-value', LC_SIGN_TEST_LOG=str(log))
            command = ['bash', str(ROOT / 'scripts/sign-android'), str(apk), str(output)]
            subprocess.run(command, env=env, check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), apk.read_bytes())
            arguments = log.read_text()
            self.assertNotIn('secret-store-value', arguments)
            self.assertNotIn('secret-key-value', arguments)
            rows = [json.loads(row) for row in arguments.splitlines()]
            self.assertEqual(len(rows), 4)
            self.assertEqual(rows[1][0], 'sign')
            self.assertIn('env:LC_ANDROID_STORE_PASSWORD', rows[1])
            self.assertIn('env:LC_ANDROID_KEY_PASSWORD', rows[1])
            self.assertEqual(rows[2][0], 'verify')
            del env['LC_ANDROID_KEY_PASSWORD']
            self.assertNotEqual(subprocess.run(command, env=env, capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
