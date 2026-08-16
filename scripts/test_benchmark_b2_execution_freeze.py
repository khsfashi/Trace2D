#!/usr/bin/env python3
from __future__ import annotations

import copy
import hashlib
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from scripts import benchmark_b2_execution_freeze as freeze


REPO_ROOT = Path(__file__).resolve().parents[1]


class B2ExecutionFreezeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = json.loads(
            (REPO_ROOT / "benchmarks/b2/execution-v1.json").read_text(encoding="utf-8")
        )

    def test_committed_execution_inputs_validate(self) -> None:
        freeze.validate_execution_data(self.contract, REPO_ROOT)

    def test_scored_observation_before_freeze_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.contract)
        mutated["scored_results_observed_before_execution_freeze"] = True
        with self.assertRaisesRegex(freeze.ExecutionFreezeError, "observed before execution inputs"):
            freeze.validate_execution_data(mutated, REPO_ROOT)

    def test_slot_reordering_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.contract)
        mutated["slots"][0], mutated["slots"][1] = mutated["slots"][1], mutated["slots"][0]
        with self.assertRaisesRegex(freeze.ExecutionFreezeError, "slot order changed"):
            freeze.validate_execution_data(mutated, REPO_ROOT)

    def test_retry_policy_relaxation_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.contract)
        mutated["execution_policy"]["automatic_retries_per_trial"] = 1
        with self.assertRaisesRegex(freeze.ExecutionFreezeError, "execution policy changed"):
            freeze.validate_execution_data(mutated, REPO_ROOT)

    def test_task_semantics_cannot_be_seeded_into_starter(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = REPO_ROOT / "benchmarks/b2/starters"
            destination = root / "benchmarks/b2/starters"
            destination.parent.mkdir(parents=True)
            shutil.copytree(source, destination)

            mutated = copy.deepcopy(self.contract)
            project = destination / "godot/project.godot"
            project.write_text(project.read_text(encoding="utf-8") + "\n# attack\n", encoding="utf-8")
            digest = hashlib.sha256(project.read_bytes()).hexdigest()
            mutated["lane_starters"]["godot.generic"]["files"]["project.godot"] = digest
            mutated["lane_starters"]["godot.agent"]["files"]["project.godot"] = digest

            with self.assertRaisesRegex(freeze.ExecutionFreezeError, "leaks frozen task token: attack"):
                freeze.validate_execution_data(mutated, root)


if __name__ == "__main__":
    unittest.main()
