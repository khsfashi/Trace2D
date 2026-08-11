#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import unittest
from pathlib import Path
from unittest import mock

import qualify_benchmark_b0_windows_acl_isolation as probe


class WindowsAclIsolationProbeTests(unittest.TestCase):
    def test_parse_sid_extracts_windows_sid(self) -> None:
        text = '"HOST\\CodexSandboxOffline","S-1-5-21-111-222-333-444"\n'
        self.assertEqual(probe.parse_sid(text), "S-1-5-21-111-222-333-444")

    def test_redact_removes_random_canary(self) -> None:
        secret = "TRACE2D-B0-ACL-DENY-secret"
        self.assertEqual(probe.redact(f"before {secret} after", secret), "before <REDACTED_RANDOM_CANARY> after")

    def test_direct_sandbox_uses_explicit_profile_and_cd(self) -> None:
        completed = subprocess.CompletedProcess(args=[], returncode=0, stdout="ok", stderr="")
        with mock.patch.object(probe.core, "capture", return_value=completed) as capture:
            result = probe.run_codex_sandbox(
                "codex.cmd",
                profile=":workspace",
                cwd=Path("C:/candidate"),
                command=["cmd.exe", "/c", "echo ok"],
                timeout=42.0,
            )
        self.assertIs(result, completed)
        args = capture.call_args.args[1]
        self.assertEqual(args[:4], ["sandbox", "windows", "--permissions-profile", ":workspace"])
        self.assertIn("--cd", args)
        self.assertIn("--", args)
        self.assertEqual(capture.call_args.kwargs["timeout"], 42.0)

    def test_process_record_redacts_canary(self) -> None:
        secret = "TRACE2D-B0-ACL-DENY-secret"
        completed = subprocess.CompletedProcess(
            args=[],
            returncode=1,
            stdout=f"stdout {secret}",
            stderr=f"stderr {secret}",
        )
        record = probe.process_record(completed, secret=secret)
        self.assertNotIn(secret, record["stdout"])
        self.assertNotIn(secret, record["stderr"])
        self.assertEqual(record["return_code"], 1)


if __name__ == "__main__":
    unittest.main()
