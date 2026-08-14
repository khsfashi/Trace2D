import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

MODULE_PATH = Path(__file__).with_name("materialize_benchmark_b1_frozen_bytes.py")
SPEC = importlib.util.spec_from_file_location("materialize_b1", MODULE_PATH)
materialize_b1 = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(materialize_b1)


class MaterializeBenchmarkB1FrozenBytesTests(unittest.TestCase):
    def init_repo(self, root: Path, content: bytes) -> tuple[Path, Path]:
        subprocess.run(["git", "init"], cwd=root, check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["git", "config", "user.email", "test@example.com"], cwd=root, check=True)
        subprocess.run(["git", "config", "user.name", "Trace2D Test"], cwd=root, check=True)
        target = root / "benchmarks/b0/profile.json"
        target.parent.mkdir(parents=True)
        target.write_bytes(content)
        manifest = root / "benchmarks/b1/freeze-manifest.json"
        manifest.parent.mkdir(parents=True)
        manifest.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "benchmark_id": "trace2d-b1",
                    "state": "frozen",
                    "algorithm": "sha256",
                    "files": [
                        {
                            "path": "benchmarks/b0/profile.json",
                            "sha256": hashlib.sha256(content).hexdigest(),
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        subprocess.run(["git", "add", "."], cwd=root, check=True)
        subprocess.run(["git", "commit", "-m", "fixture"], cwd=root, check=True, stdout=subprocess.DEVNULL)
        return target, manifest

    def test_repairs_crlf_worktree_from_matching_lf_git_blob(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target, manifest = self.init_repo(root, b'{\n  "value": 1\n}\n')
            target.write_bytes(b'{\r\n  "value": 1\r\n}\r\n')
            result = materialize_b1.materialize(root, manifest)
            self.assertEqual(result["repaired_count"], 1)
            self.assertEqual(target.read_bytes(), b'{\n  "value": 1\n}\n')

    def test_check_only_rejects_stale_worktree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target, manifest = self.init_repo(root, b"a\nb\n")
            target.write_bytes(b"a\r\nb\r\n")
            with self.assertRaises(materialize_b1.MaterializeError):
                materialize_b1.materialize(root, manifest, check_only=True)

    def test_refuses_to_hide_repository_blob_drift(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target, manifest = self.init_repo(root, b"frozen\n")
            target.write_bytes(b"changed\n")
            subprocess.run(["git", "add", str(target)], cwd=root, check=True)
            subprocess.run(["git", "commit", "-m", "drift"], cwd=root, check=True, stdout=subprocess.DEVNULL)
            with self.assertRaises(materialize_b1.MaterializeError):
                materialize_b1.materialize(root, manifest)


if __name__ == "__main__":
    unittest.main()
