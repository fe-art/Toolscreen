#!/usr/bin/env python3
"""Validate translation keys against the codebase.

Two rules:
  1. Every key passed to tr/trc/tr_ref as a string literal must exist in every
     lang/*.json locale (excluding langs.json).
  2. Every key in lang/en.json must appear as a string literal somewhere in
     src/, directly or via an intermediate struct field, switch, data table.

Exits 0 clean, 1 on validation errors, 2 on usage error.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

SOURCE_EXTS = {".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hh", ".hxx", ".inl"}
SOURCE_DIRS = ["src"]
LANG_DIR = "lang"

# Index of available locales, not a translation file itself.
LANGS_INDEX_FILE = "langs.json"

# en.json must exist; other locales are optional.
PRIMARY_LOCALE = "en"

TR_LITERAL_RE = re.compile(r'\btr(?:_ref|c)?\s*\(\s*"((?:[^"\\]|\\.)*)"')

# Dynamic tr*() sites (non-literal first arg).
TR_ANY_RE = re.compile(r"\btr(?:_ref|c)?\s*\(")

# Picks up literals in comments too. Fine: we only care whether the key
# appears anywhere as a literal. Does not parse C++ raw strings R"(...)".
ANY_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')

_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "0": "\0"}


def iter_source_files(root: Path):
    for src_dir in SOURCE_DIRS:
        base = root / src_dir
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.suffix.lower() in SOURCE_EXTS and path.is_file():
                yield path


def unescape_cpp_literal(raw: str) -> str:
    """Decode C++ string-literal escapes (ASCII keys only)."""
    out = []
    i = 0
    while i < len(raw):
        c = raw[i]
        if c == "\\" and i + 1 < len(raw):
            nxt = raw[i + 1]
            out.append(_ESCAPES.get(nxt, nxt))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def scan_codebase(root: Path):
    """Scan src/ and return:
        tr_call_keys  -> dict[key -> list[(file, line)]]
        all_literals  -> set[str]
        dynamic_sites -> list[(file, line, snippet)]
    """
    tr_call_keys: dict[str, list[tuple[str, int]]] = {}
    all_literals: set[str] = set()
    dynamic_sites: list[tuple[str, int, str]] = []

    for path in iter_source_files(root):
        rel = path.relative_to(root).as_posix()
        # i18n declarations contain every key as a literal; would self-match.
        if rel.endswith("src/common/i18n.h") or rel.endswith("src/common/i18n.cpp"):
            continue

        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue

        # Catches keys reached indirectly (struct fields, tables, switches).
        for m in ANY_LITERAL_RE.finditer(text):
            all_literals.add(unescape_cpp_literal(m.group(1)))

        # Line-by-line for file:line reporting.
        for lineno, line in enumerate(text.splitlines(), start=1):
            if "tr" not in line:
                continue

            for m in TR_LITERAL_RE.finditer(line):
                key = unescape_cpp_literal(m.group(1))
                tr_call_keys.setdefault(key, []).append((rel, lineno))

            for m in TR_ANY_RE.finditer(line):
                after = line[m.end():].lstrip()
                if not after.startswith('"'):
                    snippet = line.strip()
                    if len(snippet) > 160:
                        snippet = snippet[:157] + "..."
                    dynamic_sites.append((rel, lineno, snippet))

    return tr_call_keys, all_literals, dynamic_sites


def load_translations(root: Path):
    """Return dict[locale -> dict[key -> value]]."""
    lang_dir = root / LANG_DIR
    if not lang_dir.is_dir():
        raise SystemExit(f"error: {lang_dir} does not exist")

    translations: dict[str, dict] = {}
    for path in sorted(lang_dir.glob("*.json")):
        if path.name == LANGS_INDEX_FILE:
            continue
        locale = path.stem
        with path.open("r", encoding="utf-8") as f:
            try:
                data = json.load(f)
            except json.JSONDecodeError as e:
                raise SystemExit(f"error: {path}: invalid JSON: {e}")
        if not isinstance(data, dict):
            raise SystemExit(f"error: {path}: expected a JSON object at top level")
        for k, v in data.items():
            if not isinstance(v, str):
                raise SystemExit(
                    f"error: {path}: key '{k}' has non-string value "
                    f"({type(v).__name__}); nested objects are not supported"
                )
        translations[locale] = data

    if PRIMARY_LOCALE not in translations:
        raise SystemExit(
            f"error: primary locale '{PRIMARY_LOCALE}.json' not found in {lang_dir}"
        )

    return translations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        default=str(Path(__file__).resolve().parent.parent),
        help="Repository root (default: parent of this script's directory)",
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    if not root.is_dir():
        print(f"error: root '{root}' is not a directory", file=sys.stderr)
        return 2

    translations = load_translations(root)
    tr_call_keys, all_literals, dynamic_sites = scan_codebase(root)

    locale_bits = " ".join(f"{loc}={len(d)}" for loc, d in sorted(translations.items()))
    print(
        f"translations: {locale_bits} | "
        f"tr*() static={len(tr_call_keys)} dynamic={len(dynamic_sites)} | "
        f"literals={len(all_literals)}"
    )

    has_errors = False

    # Info only, does not set has_errors.
    if dynamic_sites:
        print(f"\ndynamic tr*() sites ({len(dynamic_sites)}):")
        for file, line, snippet in dynamic_sites:
            print(f"  {file}:{line}  {snippet}")

    for locale in sorted(translations.keys()):
        missing = sorted(set(tr_call_keys) - set(translations[locale].keys()))
        if not missing:
            continue
        has_errors = True
        width = max(len(k) for k in missing)
        print(f"\nmissing in lang/{locale}.json ({len(missing)}):")
        for key in missing:
            f, ln = tr_call_keys[key][0]
            print(f"  {key.ljust(width)}  {f}:{ln}")

    for locale in sorted(translations.keys()):
        unused = sorted(set(translations[locale]) - all_literals)
        if not unused:
            continue
        has_errors = True
        print(f"\nunused in lang/{locale}.json ({len(unused)}):")
        for key in unused:
            print(f"  {key}")

    print(f"\n{'FAIL' if has_errors else 'OK'}")
    return 1 if has_errors else 0


if __name__ == "__main__":
    sys.exit(main())
