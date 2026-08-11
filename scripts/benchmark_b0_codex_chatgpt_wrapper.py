#!/usr/bin/env python3
"""ChatGPT-managed model selection shim for the frozen B0 Codex wrapper.

The core wrapper owns isolation, metrics, lane setup and result contracts. This
module changes only the provider-selectable model identity. Owner-local
qualification proved that the current ChatGPT account does not accept the
attempted GPT-5.6 selector, while a later model-only preflight completed
successfully with GPT-5.5. B0 therefore freezes the proven ``gpt-5.5`` CLI
selector before any scored matched-lane outcome exists, without claiming a
hidden dated provider snapshot.
"""
from __future__ import annotations

import benchmark_b0_codex_wrapper as core

MODEL_ID = "gpt-5.5"
MODEL_REVISION = "gpt-5.5"
PROVIDER_REVISION_POLICY = "chatgpt_codex_cli_selector_no_dated_snapshot"


def configure() -> None:
    core.MODEL_ID = MODEL_ID
    core.MODEL_REVISION = MODEL_REVISION


def main() -> int:
    configure()
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
