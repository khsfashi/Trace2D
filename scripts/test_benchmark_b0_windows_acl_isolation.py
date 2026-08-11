#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import tempfile
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
        self.assertEqual(
            probe.redact(f"before {secret} after", secret),
            "before <REDACTED_RANDOM_CANARY> after",
        )

    def test_direct_sandbox_uses_platform_native_surface_profile_and_cd(self) -> None:
        completed = subprocess.CompletedProcess(args=[], returncode=0, stdout="ok", stderr="")
        with mock.patch.object(probe.core, "capture", return_value=completed) as capture:
            result = probe.run_codex_sandbox(
                "codex.cmd",
                profile=":workspace",
                cwd=Path("candidate"),
                command=["cmd.exe", "/d", "/c", "workspace-write.cmd"],
                timeout=42.0,
            )
        self.assertIs(result, completed)
        args = capture.call_args.args[1]
        self.assertEqual(args[:3], ["sandbox", "--permission-profile", ":workspace"])
        self.assertNotIn("windows", args)
        self.assertIn("--cd", args)
        self.assertIn("--", args)
        self.assertEqual(args[-4:], ["cmd.exe", "/d", "/c", "workspace-write.cmd"])
        self.assertNotIn("/s", args)
        self.assertEqual(capture.call_args.kwargs["timeout"], 42.0)

    def test_probe_commands_are_workspace_files_not_freeform_c_strings(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "workspace"
            workspace.mkdir()
            canary = root / "held-out" / "CANARY.txt"
            canary.parent.mkdir()
            canary.write_text("secret", encoding="utf-8")

            write_script, read_script = probe.write_probe_command_files(workspace, canary)

            self.assertEqual(write_script.name, "workspace-write.cmd")
            self.assertEqual(read_script.name, "held-out-read.cmd")
            self.assertIn("ACL_WORKSPACE_OK", write_script.read_text(encoding="utf-8"))
            self.assertIn(str(canary), read_script.read_text(encoding="utf-8"))
            self.assertIn("type", read_script.read_text(encoding="utf-8").lower())

            cmd = "cmd.exe"
            write_argv = [cmd, "/d", "/c", write_script.name]
            read_argv = [cmd, "/d", "/c", read_script.name]
            self.assertEqual(len(write_argv), 4)
            self.assertEqual(len(read_argv), 4)
            self.assertNotIn("echo ACL_WORKSPACE_OK", " ".join(write_argv))
            self.assertNotIn(str(canary), " ".join(read_argv))

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
