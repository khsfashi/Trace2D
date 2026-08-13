from pathlib import Path
import tempfile
import unittest

import scripts.benchmark_b0_codex_windows_acl_wrapper as windows


class BenchmarkB1OwnerRunnerHelperTests(unittest.TestCase):
    def test_windows_wrapper_exposes_owner_runner_contract(self):
        self.assertEqual(windows.ISOLATION_TIMEOUT_SECONDS, 285.0)
        self.assertTrue(callable(windows.scrub_transient_codex_state))

    def test_scrub_removes_auth_and_codex_home_but_preserves_sandbox_log(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            codex_home = root / "trial" / "codex-home"
            sandbox = codex_home / ".sandbox"
            sandbox.mkdir(parents=True)
            (codex_home / "auth.json").write_text("secret", encoding="utf-8")
            (sandbox / "sandbox.log").write_text("diagnostic", encoding="utf-8")

            windows.scrub_transient_codex_state(root)

            self.assertFalse(codex_home.exists())
            logs = list((root / "codex-sandbox-logs").glob("*.log"))
            self.assertEqual(len(logs), 1)
            self.assertEqual(logs[0].read_text(encoding="utf-8"), "diagnostic")

    def test_owner_runner_reproduces_qualified_python_freeze_exactly(self):
        source = (
            Path(__file__)
            .with_name("run_benchmark_b1_codex_windows_acl_scored_cohort.py")
            .read_text(encoding="utf-8")
        )

        self.assertIn('"--no-deps",\n                    "-r",', source)
        self.assertIn('[str(python), "-m", "pip", "freeze", "--all"]', source)
        self.assertIn("order_only = not missing and not extra", source)


if __name__ == "__main__":
    unittest.main()
