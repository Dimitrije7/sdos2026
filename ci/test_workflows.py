#!/usr/bin/env python3
"""Tests for the workflow validator, including its deliberately broken fixture."""

from __future__ import annotations

import contextlib
import io
import unittest
from pathlib import Path

from ci import validate_workflows as subject

FIXTURE = Path(__file__).resolve().parent / "fixtures" / "workflows" / "unpinned.yml"
MANIFEST = subject.ROOT / "manifest" / "versions.json"


class WorkflowTests(unittest.TestCase):
    def test_repository_workflows_pass(self) -> None:
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(subject.main([]), 0)

    def test_fixture_reports_every_violation(self) -> None:
        problems = "\n".join(subject.check_workflow(FIXTURE, subject.pinned_actions(MANIFEST)))
        self.assertIn("actions/checkout@v4 is not pinned to a full commit SHA", problems)
        self.assertIn("differs from the manifest pin", problems)
        self.assertIn("no top-level permissions block", problems)

    def test_gateway_requires_always_reporting_gate(self) -> None:
        self.assertEqual(subject.check_gateway(subject.ROOT / ".github" / "workflows" / "ci.yml"), [])
        self.assertTrue(subject.check_gateway(FIXTURE))


if __name__ == "__main__":
    unittest.main()
