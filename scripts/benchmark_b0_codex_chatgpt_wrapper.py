#!/usr/bin/env python3
"""ChatGPT-managed model selection shim for the frozen B0 Codex wrapper.

The core wrapper owns isolation, metrics, lane setup and result contracts. This
module changes only the provider-selectable model identity. ChatGPT-signed-in
Codex does not expose the dated API snapshot that the first B0 attempt tried,
so the benchmark freezes the documented Codex model identifier instead of
pretending that an unavailable API snapshot was executed.
"""
from __future__ import annotations

import benchmark_b0_codex_wrapper as core

MODEL_ID = "gpt-5.6-sol"
MODEL_REVISION = "gpt-5.6-sol"
PROVIDER_REVISION_POLICY = "chatgpt_managed_identifier_no_dated_snapshot"


def configure() -> None:
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
