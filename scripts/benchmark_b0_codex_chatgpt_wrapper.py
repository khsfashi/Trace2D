#!/usr/bin/env python3
"""ChatGPT-managed model selection shim for the frozen B0 Codex wrapper.

The core wrapper owns isolation, metrics, lane setup and result contracts. This
module changes only the provider-selectable model identity. Owner-local
qualification proved that the current ChatGPT account does not yet accept the
GPT-5.6 selector even though it is rolling out in Codex. GPT-5.5 is explicitly
listed as a ChatGPT/Codex CLI model and is available to the owner's account, so
B0 freezes that CLI selector before any scored matched-lane outcome exists.
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
