#!/usr/bin/env python3
"""Keep the retired project Page a narrow redirect to the account site."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
DESTINATION = "https://ferazambuja.github.io/imaging/"


def main() -> int:
    expected = {"index.html", "404.html"}
    actual = {path.name for path in SITE.iterdir() if path.is_file()}
    if actual != expected:
        raise SystemExit(f"redirect site must contain only {sorted(expected)}; got {sorted(actual)}")

    for name in sorted(expected):
        text = (SITE / name).read_text(encoding="utf-8")
        if DESTINATION not in text:
            raise SystemExit(f"{name}: missing destination {DESTINATION}")
        if 'name="robots" content="noindex,follow"' not in text:
            raise SystemExit(f"{name}: redirect must not compete with the canonical site")

    index = (SITE / "index.html").read_text(encoding="utf-8")
    for fragment in ("study-sfr", "study-gamut", "study-spectral", "study-flat"):
        if fragment not in index:
            raise SystemExit(f"index.html: missing legacy fragment mapping {fragment}")
    print("pages redirect: destination and four legacy fragments verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
