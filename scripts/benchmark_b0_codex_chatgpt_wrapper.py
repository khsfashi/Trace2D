#!/usr/bin/env python3
"""ChatGPT-managed model selection shim for the frozen B0 Codex wrapper.

The core wrapper owns isolation, metrics, lane setup and result contracts. This
module changes only the provider-selectable model identity. For ChatGPT-signed-
in Codex, the official CLI documentation selects GPT-5.6 with `-m gpt-5.6` and
shows that selection as GPT-5.6 Sol in the model UI. B0 therefore freezes the
CLI selector actually exposed to the owner instead of an API-only dated model
snapshot or a guessed provider-internal identifier.
"""
from __future__ import annotations

import benchmark_b0_codex_wrapper as core

MODEL_ID = "gpt-5.6"
MODEL_REVISION = "gpt-5.6"
PROVIDER_REVISION_POLICY = "chatgpt_codex_cli_selector_no_dated_snapshot"


def configure() -> None:
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
