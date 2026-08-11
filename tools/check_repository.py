#!/usr/bin/env python3
"""Validate repository links, image metadata, and CSV structure."""

from __future__ import annotations

import argparse
import csv
import io
import os
import re
import sys
from pathlib import Path
from urllib.parse import unquote

LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^#{1,6}\s+(.+?)\s*$")
SKIP_DIRS = {".git", "__pycache__", ".pytest_cache", "_site"}

def candidate_files(root: Path) -> list[Path]:
    """Return publishable files while pruning generated CMake trees.

    Build directories are identified by their own ``CMakeCache.txt`` marker,
    not by a short list of directory names. This keeps local builds out of the
    publication scan without creating a blind spot for a source directory such
    as ``tools``.
    """

    files: list[Path] = []
    for directory, dirnames, filenames in os.walk(root):
        current = Path(directory)
        dirnames[:] = sorted(
            name
            for name in dirnames
            if name not in SKIP_DIRS
            and not (current / name / "CMakeCache.txt").is_file()
        )
        for name in sorted(filenames):
            if name in SKIP_DIRS:
                continue
            path = current / name
            if path.is_file():
                files.append(path)
    return files


def markdown_anchors(path: Path) -> set[str]:
    """Return GitHub-style anchors for the headings in one Markdown file."""

    anchors: set[str] = set()
    duplicate_counts: dict[str, int] = {}
    in_fence = False
    fence_character = ""
    fence_length = 0
    for line in path.read_text(encoding="utf-8").splitlines():
        fence = re.match(r"^\s*(`{3,}|~{3,})", line)
        if fence:
            marker = fence.group(1)
            if not in_fence:
                in_fence = True
                fence_character = marker[0]
                fence_length = len(marker)
            elif marker[0] == fence_character and len(marker) >= fence_length:
                in_fence = False
            continue
        if in_fence:
            continue
        match = HEADING_RE.match(line)
        if not match:
            continue
        heading = match.group(1).strip()
        heading = re.sub(r"\s+#+\s*$", "", heading)
        heading = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", heading)
        heading = re.sub(r"<[^>]+>", "", heading)
        heading = heading.replace("`", "").replace("*", "").replace("_", "")
        slug = heading.lower()
        slug = re.sub(r"[^\w\-\s]", "", slug, flags=re.UNICODE)
        slug = re.sub(r"\s", "-", slug)
        if not slug:
            continue
        duplicate_index = duplicate_counts.get(slug, 0)
        duplicate_counts[slug] = duplicate_index + 1
        anchors.add(slug if duplicate_index == 0 else f"{slug}-{duplicate_index}")
    return anchors


def scan_links(root: Path, files: list[Path]) -> list[str]:
    failures = []
    anchor_cache: dict[Path, set[str]] = {}
    for path in files:
        if path.suffix != ".md":
            continue
        for raw_target in LINK_RE.findall(path.read_text(encoding="utf-8")):
            target = raw_target.strip()
            if target.startswith("<") and ">" in target:
                target = target[1:target.index(">")]
            else:
                target = target.split(maxsplit=1)[0]
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            path_part, separator, raw_fragment = target.partition("#")
            local_target = unquote(path_part.split("?", 1)[0])
            resolved = (
                path.resolve()
                if not local_target
                else (path.parent / local_target).resolve()
            )
            if not resolved.exists():
                failures.append(f"{path.relative_to(root)}: broken link -> {target}")
            elif root.resolve() not in resolved.parents and resolved != root.resolve():
                failures.append(f"{path.relative_to(root)}: link escapes the repository -> {target}")
            elif separator and raw_fragment and resolved.suffix.lower() == ".md":
                fragment = unquote(raw_fragment).lower()
                if resolved not in anchor_cache:
                    anchor_cache[resolved] = markdown_anchors(resolved)
                if fragment not in anchor_cache[resolved]:
                    failures.append(
                        f"{path.relative_to(root)}: broken anchor -> {target}"
                    )
    return failures


def scan_image_metadata(root: Path, files: list[Path]) -> list[str]:
    failures = []
    for path in files:
        if path.suffix.lower() not in {".jpg", ".jpeg"}:
            continue
        blob = path.read_bytes()
        signatures = (
            (b"Exif\x00\x00", "EXIF"),
            (b"http://ns.adobe.com/xap/1.0/", "XMP"),
            (b"Photoshop 3.0", "Photoshop/IPTC"),
        )
        for signature, label in signatures:
            if signature in blob:
                failures.append(
                    f"{path.relative_to(root)}: retains {label} metadata; strip it before publishing"
                )
    return failures


def scan_csv_structure(root: Path, files: list[Path]) -> list[str]:
    """Reject ragged public tables instead of silently shifting their fields."""

    failures = []
    for path in files:
        if path.suffix.lower() != ".csv":
            continue
        try:
            rows = list(
                csv.reader(
                    io.StringIO(path.read_text(encoding="utf-8")), strict=True
                )
            )
        except (UnicodeDecodeError, csv.Error) as error:
            failures.append(f"{path.relative_to(root)}: invalid CSV ({error})")
            continue
        if not rows or not rows[0]:
            failures.append(f"{path.relative_to(root)}: CSV has no header")
            continue
        width = len(rows[0])
        for line_number, row in enumerate(rows[1:], start=2):
            if len(row) != width:
                failures.append(
                    f"{path.relative_to(root)}:{line_number}: CSV row has "
                    f"{len(row)} fields; header has {width}"
                )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.repo_root.resolve()

    files = candidate_files(root)
    if not files:
        print("repository check: no files found to check", file=sys.stderr)
        return 1

    failures = (
        scan_links(root, files)
        + scan_image_metadata(root, files)
        + scan_csv_structure(root, files)
    )
    if failures:
        for failure in failures:
            print(f"repository check: {failure}", file=sys.stderr)
        return 1

    print(f"repository check: {len(files)} files clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
