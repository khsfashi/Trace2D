#!/usr/bin/env python3
from __future__ import annotations

import unittest

import benchmark_b0_codex_chatgpt_wrapper as chatgpt_wrapper
import benchmark_b0_codex_wrapper as core


class ChatGptCodexWrapperTests(unittest.TestCase):
    def test_frozen_chatgpt_model_selection_is_exact(self) -> None:
        self.assertEqual(chatgpt_wrapper.MODEL_ID, "gpt-5.6-sol")
        self.assertEqual(chatgpt_wrapper.MODEL_REVISION, "gpt-5.6-sol")
        self.assertEqual(
            chatgpt_wrapper.PROVIDER_REVISION_POLICY,
            "chatgpt_managed_identifier_no_dated_snapshot",
        )

    def test_configure_changes_only_provider_model_identity(self) -> None:
        original_id = core.MODEL_ID
        original_revision = core.MODEL_REVISION
        try:
            chatgpt_wrapper.configure()
            self.assertEqual(core.MODEL_ID, "gpt-5.6-sol")
            self.assertEqual(core.MODEL_REVISION, "gpt-5.6-sol")
            self.assertEqual(core.AGENT_ID, "openai-codex-cli@0.144.6")
            self.assertEqual(core.REASONING_EFFORT, "high")
        finally:
            core.MODEL_ID = original_id
            core.MODEL_REVISION = original_revision


if __name__ == "__main__":
    unittest.main()
