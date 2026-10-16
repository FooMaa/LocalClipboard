"""Regression checks for bilingual README validation, without touching repository files."""
from pathlib import Path
import runpy
import tempfile
import unittest

CHECK = runpy.run_path(str(Path(__file__).resolve().parents[1] / 'scripts/check-readmes'))['check']


class ReadmeTests(unittest.TestCase):
    def check_document(self, english, russian):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'README.md'
            path.write_text('# Guide\n\n' + english + '\n\n## Русская версия\n\n'
                            '### Руководство\n\n' + russian + '\n', encoding='utf-8')
            return CHECK(path)

    def test_translated_comments_and_local_anchors_are_allowed(self):
        self.assertEqual(self.check_document(
            'Run `tool` and [read](#guide).\n\n```sh\ntool --check # Check\n```',
            'Запустите `tool` и [прочитайте](#руководство).\n\n```sh\ntool --check # Проверка\n```'), [])

    def test_missing_technical_details_are_reported_in_either_language(self):
        for detail in ('`--dry-run`', '[docs](https://example.com/docs)', '```sh\ntool --check\n```'):
            for missing_language in ('English', 'Russian'):
                with self.subTest(detail=detail, language=missing_language):
                    english = 'Use this option.'
                    russian = 'Используйте этот параметр.'
                    if missing_language == 'English':
                        russian += '\n\n' + detail
                    else:
                        english += '\n\n' + detail
                    errors = self.check_document(english, russian)
                    self.assertTrue(any(error.startswith(missing_language + ' version is missing') for error in errors))

    def test_untranslated_paragraph_is_reported(self):
        errors = self.check_document('Instructions.', 'Перевод.\n\nThis whole paragraph was accidentally left without any translation.')
        self.assertTrue(any('English paragraph' in error for error in errors))

    def test_broken_local_links_are_reported(self):
        errors = self.check_document('[Guide](missing.md)', '[Руководство](missing.md)')
        self.assertIn('broken local link: missing.md', errors)

    def test_missing_or_duplicate_russian_sections_are_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'README.md'
            for content in ('# Guide', '# Guide\n## Русская версия\n## Русская версия'):
                path.write_text(content, encoding='utf-8')
                self.assertTrue(CHECK(path))


if __name__ == '__main__':
    unittest.main()
