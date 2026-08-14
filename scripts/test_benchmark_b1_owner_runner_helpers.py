from pathlib import Path
import sys
import unittest

SCRIPTS = Path(__file__).resolve().parent
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import benchmark_b0_codex_windows_acl_wrapper as windows
import run_benchmark_b1_codex_windows_acl_scored_cohort as owner


class BenchmarkB1OwnerRunnerHelperTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = SCRIPTS.parent

    def test_windows_wrapper_exposes_owner_runner_contract(self):
        self.assertEqual(windows.ISOLATION_TIMEOUT_SECONDS, 285.0)
        self.assertTrue(callable(windows.scrub_transient_codex_state))

    def test_owner_runner_keeps_qualification_freeze_and_adds_pinned_windows_runtime(self):
        qualification = owner._QUALIFIED_EXPECTED_PYTHON_FREEZE(self.repo_root)
        combined = owner.expected_owner_python_freeze(self.repo_root)
        self.assertNotIn("pywin32==312", qualification)
        self.assertIn("pywin32==312", combined)
        self.assertEqual(combined, sorted(combined, key=owner._distribution_name))
        self.assertEqual(
            owner.base.GODOT_AI_ENV_RECIPE,
            "qualified-freeze-no-deps-v1+win-cp312-x64-pywin32-312-7869c1f9",
        )
        self.assertIs(owner.base.expected_python_freeze, owner.expected_owner_python_freeze)
        self.assertIs(owner.base.ensure_godot_ai, owner.ensure_owner_godot_ai)

    def test_windows_runtime_lock_is_exact_and_hash_pinned(self):
        lock = self.repo_root / owner.GODOT_AI_WINDOWS_RUNTIME_FREEZE
        self.assertEqual(lock.read_text(encoding="utf-8"), "pywin32==312\n")
        self.assertEqual(
            owner.base.sha256_file(lock),
            "7869c1f9ee7d505696d5d2f181cb62a5156c88ceb01f4c462a615720c3d7be41",
        )
        self.assertEqual(owner.windows_runtime_freeze(self.repo_root), ["pywin32==312"])

    def test_unscored_transport_accepts_only_clean_input_token_overage(self):
        clean = {
            "status": "budget_exceeded",
            "return_code": 0,
            "timed_out": False,
            "human_interventions": 0,
            "budget": {"exceeded": ["input_tokens"]},
            "tool_metrics": {"tool_failures": 0},
        }
        self.assertTrue(owner._is_unscored_transport_input_budget_only(clean))

        for mutation in (
            {"status": "tool_transport_failure"},
            {"return_code": 1},
            {"timed_out": True},
            {"human_interventions": 1},
            {"budget": {"exceeded": ["output_tokens"]}},
            {"budget": {"exceeded": ["tool_calls"]}},
            {"tool_metrics": {"tool_failures": 1}},
        ):
            candidate = {
                **clean,
                **mutation,
            }
            self.assertFalse(owner._is_unscored_transport_input_budget_only(candidate))

    def test_owner_patch_changes_only_unscored_transport_verdict_not_scored_budget(self):
        self.assertIs(owner.base.run_godot_agent_preflight, owner.run_owner_godot_agent_preflight)
        source = (
            Path(__file__)
            .with_name("run_benchmark_b1_codex_windows_acl_scored_cohort_base.py")
            .read_text(encoding="utf-8")
        )
        self.assertIn(
            'env["TRACE2D_BENCH_MAX_INPUT_TOKENS"] = str(budget["max_input_tokens"])',
            source,
        )
        owner_source = (
            Path(__file__)
            .with_name("run_benchmark_b1_codex_windows_acl_scored_cohort.py")
            .read_text(encoding="utf-8")
        )
        self.assertNotIn('TRACE2D_BENCH_MAX_INPUT_TOKENS"] =', owner_source)
        self.assertIn('"scored_budget_unchanged": True', owner_source)

    def test_frozen_base_implementation_remains_no_deps_and_exact_freeze(self):
        source = (
            Path(__file__)
            .with_name("run_benchmark_b1_codex_windows_acl_scored_cohort_base.py")
            .read_text(encoding="utf-8")
        )
        self.assertIn('GODOT_AI_ENV_RECIPE = "qualified-freeze-no-deps-v1"', source)
        self.assertIn('expected_marker = f"{expected_hash}:{GODOT_AI_ENV_RECIPE}"', source)
        self.assertIn('"--no-deps",\n                    "-r",', source)
        self.assertIn('[str(python), "-m", "pip", "freeze"]', source)
        self.assertNotIn('[str(python), "-m", "pip", "freeze", "--all"]', source)
        self.assertIn("order_only = not missing and not extra", source)


if __name__ == "__main__":
    unittest.main()
