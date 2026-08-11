#!/usr/bin/env python3
"""Behavior tests for the repository checker."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CHECKER = Path(__file__).with_name("check_repository.py")


class RepositoryCheckTests(unittest.TestCase):
    def run_checker(self, root: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(CHECKER), "--repo-root", str(root)],
            check=False,
            capture_output=True,
            text=True,
        )

    def test_clean_tree_with_angle_wrapped_space_link_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "docs").mkdir()
            (root / "docs" / "file name.md").write_text("# Method\n", encoding="utf-8")
            (root / "README.md").write_text(
                "[Method](<docs/file name.md>)\n", encoding="utf-8"
            )
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_git_worktree_pointer_is_not_public_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text("# Clean\n", encoding="utf-8")
            (root / ".git").write_text(
                "gitdir: ../repository/.git/worktrees/check\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("1 files clean", result.stdout)

    def test_generated_cmake_tree_is_ignored_by_marker_not_name(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text("# Clean\n", encoding="utf-8")
            build = root / "any-local-build-name"
            build.mkdir()
            (build / "CMakeCache.txt").write_text("generated", encoding="utf-8")
            (build / "generated.txt").write_text("generated", encoding="utf-8")
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_generated_pages_artifact_is_not_counted_as_source(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text("# Clean\n", encoding="utf-8")
            generated = root / "_site"
            generated.mkdir()
            (generated / "index.html").write_text(
                "generated", encoding="utf-8"
            )
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("1 files clean", result.stdout)

    def test_broken_and_escaping_links_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            root = base / "repo"
            root.mkdir()
            (base / "outside.md").write_text("# Outside\n", encoding="utf-8")
            (root / "README.md").write_text(
                "[Missing](missing.md)\n[Outside](../outside.md)\n", encoding="utf-8"
            )
            result = self.run_checker(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("broken link", result.stderr)
            self.assertIn("link escapes the repository", result.stderr)

    def test_cross_file_and_same_page_anchors_are_checked(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "report.md").write_text(
                "# Report\n\n## Addendum — what remains\n",
                encoding="utf-8",
            )
            (root / "README.md").write_text(
                "[Valid](report.md#addendum--what-remains)\n"
                "[Missing](report.md#missing-section)\n"
                "[Same-page](#missing-local-section)\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("report.md#missing-section", result.stderr)
            self.assertIn("#missing-local-section", result.stderr)
            self.assertNotIn("report.md#addendum--what-remains", result.stderr)

    def test_duplicate_heading_anchor_suffix_is_supported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "report.md").write_text(
                "# Report\n\n## Result\n\n## Result\n",
                encoding="utf-8",
            )
            (root / "README.md").write_text(
                "[Second result](report.md#result-1)\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_heading_inside_code_fence_is_not_an_anchor(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "report.md").write_text(
                "# Report\n\n```text\n## Not a heading\n```\n",
                encoding="utf-8",
            )
            (root / "README.md").write_text(
                "[Not rendered](report.md#not-a-heading)\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("broken anchor", result.stderr)

    def test_broken_image_path_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text(
                "![Missing figure](figures/missing.svg)\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("broken link", result.stderr)

    def test_image_metadata_is_detected_beyond_the_header(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            payload = b"\xff\xd8" + (b"x" * 5000) + b"Exif\x00\x00" + b"\xff\xd9"
            (root / "image.jpg").write_bytes(payload)
            result = self.run_checker(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("EXIF metadata", result.stderr)

    def test_ragged_csv_row_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "table.csv").write_text(
                "name,value,note\nreading,89,second reader,source identity\n",
                encoding="utf-8",
            )
            result = self.run_checker(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("table.csv:2", result.stderr)
            self.assertIn("4 fields; header has 3", result.stderr)


if __name__ == "__main__":
    unittest.main()
