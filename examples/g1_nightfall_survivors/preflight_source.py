from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PRESENTATION = ROOT / "NightfallPresentationMeasured.cpp"
CONTRACT = ROOT / "NightfallLayoutContractMain.cpp"
PRODUCT = ROOT / "NightfallProduct.cpp"

STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"', re.DOTALL)
CORPUS_RE = re.compile(
    r"std::string\s+corpus\s*=\s*(?P<body>(?:\s*\"(?:\\.|[^\"\\])*\"\s*)+);",
    re.DOTALL,
)
PRODUCT_DEFINITIONS_RE = re.compile(
    r"constexpr\s+std::array<CharacterDefinition,.*?"
    r"(?P<body>.*?)"
    r"\[\[nodiscard\]\]\s+constexpr\s+std::uint32_t\s+Bit",
    re.DOTALL,
)


def cpp_string_value(token: str) -> str:
    # Product UI literals are UTF-8 source text. Only simple source escapes are
    # needed here; non-ASCII codepoints stay untouched.
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


def extract_product_definition_characters() -> set[str]:
    source = PRODUCT.read_text(encoding="utf-8")
    match = PRODUCT_DEFINITIONS_RE.search(source)
    if not match:
        raise RuntimeError("NightfallProduct.cpp: product definition tables not found")
    chars: set[str] = set()
    for token in STRING_RE.finditer(match.group("body")):
        chars.update(ch for ch in cpp_string_value(token.group(0)) if ord(ch) >= 0x80)
    return chars


def rendered_source_characters(source: str, corpus_span: tuple[int, int]) -> set[str]:
    # Remove the curated corpus itself and inspect every remaining non-ASCII
    # source literal. Character/stage/achievement definitions are handled as a
    # separate allowed set because both runtime and contract append them to the
    # atlas before the fixed revision is captured.
    stripped = source[: corpus_span[0]] + source[corpus_span[1] :]
    chars: set[str] = set()
    for token in STRING_RE.finditer(stripped):
        value = cpp_string_value(token.group(0))
        chars.update(ch for ch in value if ord(ch) >= 0x80)
    return chars


def check_file(path: Path, definition_chars: set[str]) -> tuple[set[str], set[str]]:
    source = path.read_text(encoding="utf-8")
    corpus, span = extract_corpus(source, path)
    corpus_chars = set(corpus)
    required = rendered_source_characters(source, span)
    missing = required - corpus_chars - definition_chars
    if missing:
        ordered = "".join(sorted(missing, key=ord))
        codepoints = ", ".join(f"U+{ord(ch):04X} {ch}" for ch in sorted(missing, key=ord))
        raise RuntimeError(
            f"{path.name}: UI glyphs missing from deterministic prewarm coverage: {ordered!r} ({codepoints})"
        )
    return corpus_chars | definition_chars, required


def main() -> int:
    try:
        definition_chars = extract_product_definition_characters()
        presentation_coverage, presentation_required = check_file(PRESENTATION, definition_chars)
        contract_coverage, contract_required = check_file(CONTRACT, definition_chars)

        missing_in_presentation = contract_required - presentation_coverage
        missing_in_contract = presentation_required - contract_coverage
        if missing_in_presentation or missing_in_contract:
            details: list[str] = []
            if missing_in_presentation:
                details.append(
                    "presentation coverage misses contract glyphs "
                    + repr("".join(sorted(missing_in_presentation, key=ord)))
                )
            if missing_in_contract:
                details.append(
                    "contract coverage misses presentation glyphs "
                    + repr("".join(sorted(missing_in_contract, key=ord)))
                )
            raise RuntimeError("; ".join(details))

        print(
            "Nightfall source preflight passed: deterministic UI glyph coverage is complete "
            f"(presentation={len(presentation_required)} non-ASCII glyphs, "
            f"contract={len(contract_required)}, definitions={len(definition_chars)})."
        )
        return 0
    except Exception as exc:
        print(f"Nightfall source preflight failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
