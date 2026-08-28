from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PRESENTATION = ROOT / "NightfallPresentationMeasured.cpp"
CONTRACT = ROOT / "NightfallLayoutContractMain.cpp"

STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"', re.DOTALL)
CORPUS_RE = re.compile(
    r"std::string\s+corpus\s*=\s*(?P<body>(?:\s*\"(?:\\.|[^\"\\])*\"\s*)+);",
    re.DOTALL,
)


def cpp_string_value(token: str) -> str:
    # The product UI literals are UTF-8 source text. We only need simple C/C++
    # escape handling here; non-ASCII characters remain untouched.
    body = token[1:-1]
    replacements = {
        r"\n": "\n",
        r"\r": "\r",
        r"\t": "\t",
        r'\"': '"',
        r"\\": "\\",
    }
    for old, new in replacements.items():
        body = body.replace(old, new)
    return body


def extract_corpus(source: str, path: Path) -> tuple[str, tuple[int, int]]:
    match = CORPUS_RE.search(source)
    if not match:
        raise RuntimeError(f"{path.name}: std::string corpus block not found")
    value = "".join(cpp_string_value(token.group(0)) for token in STRING_RE.finditer(match.group("body")))
    return value, match.span()


def rendered_source_characters(source: str, corpus_span: tuple[int, int]) -> set[str]:
    # Remove the manually curated corpus itself, then inspect source literals.
    # ASCII-only diagnostics/file paths do not affect this check; non-ASCII UI
    # glyphs do. Product definition strings are appended to the atlas at runtime
    # and are therefore checked separately by the headless contract.
    stripped = source[: corpus_span[0]] + source[corpus_span[1] :]
    chars: set[str] = set()
    for token in STRING_RE.finditer(stripped):
        value = cpp_string_value(token.group(0))
        chars.update(ch for ch in value if ord(ch) >= 0x80)
    return chars


def check_file(path: Path) -> tuple[set[str], set[str]]:
    source = path.read_text(encoding="utf-8")
    corpus, span = extract_corpus(source, path)
    corpus_chars = set(corpus)
    required = rendered_source_characters(source, span)
    missing = required - corpus_chars
    if missing:
        ordered = "".join(sorted(missing, key=ord))
        codepoints = ", ".join(f"U+{ord(ch):04X} {ch}" for ch in sorted(missing, key=ord))
        raise RuntimeError(
            f"{path.name}: UI glyphs missing from deterministic prewarm corpus: {ordered!r} ({codepoints})"
        )
    return corpus_chars, required


def main() -> int:
    try:
        presentation_corpus, presentation_required = check_file(PRESENTATION)
        contract_corpus, contract_required = check_file(CONTRACT)

        missing_in_presentation = contract_required - presentation_corpus
        missing_in_contract = presentation_required - contract_corpus
        if missing_in_presentation or missing_in_contract:
            details: list[str] = []
            if missing_in_presentation:
                details.append(
                    "presentation corpus misses contract glyphs "
                    + repr("".join(sorted(missing_in_presentation, key=ord)))
                )
            if missing_in_contract:
                details.append(
                    "contract corpus misses presentation glyphs "
                    + repr("".join(sorted(missing_in_contract, key=ord)))
                )
            raise RuntimeError("; ".join(details))

        print(
            "Nightfall source preflight passed: deterministic UI glyph coverage is complete "
            f"(presentation={len(presentation_required)} non-ASCII glyphs, "
            f"contract={len(contract_required)})."
        )
        return 0
    except Exception as exc:
        print(f"Nightfall source preflight failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
