#!/usr/bin/env python3
"""Fail-closed B2 Agent execution classification for future evidence layers.

Consumed B2 records are immutable. This helper exists for future harnesses and
read-only diagnostics so an upstream Agent transport/process failure cannot be
misreported as a deterministic candidate implementation failure merely because
a downstream verifier also fails on an uncreated workspace artifact.
"""
from __future__ import annotations

from typing import Any


COMPLETED = "completed"
TRANSPORT_FAILURE = "agent_transport_failure"
TIMEOUT = "agent_timeout"
IDENTITY_FAILURE = "agent_identity_failure"
RESULT_FAILURE = "agent_result_failure"
BUDGET_FAILURE = "agent_budget_failure"


def classify_agent_execution(
    *,
    agent_result: Any,
    agent_identity_ok: bool,
    process_timed_out: bool = False,
) -> dict[str, Any]:
    """Classify whether deterministic candidate verification is authoritative.

    Precedence is intentional:
    1. process timeout;
    2. missing/invalid Agent result or identity;
    3. provider/CLI transport completion;
    4. recorded budget policy;
    5. completed Agent turn.

    Only the final state is eligible to be classified by the deterministic
    candidate verifier. This function does not mutate any retained record.
    """
    if process_timed_out:
        return _result(TIMEOUT, "infrastructure", False, "agent_process_timed_out")

    if not isinstance(agent_result, dict):
        return _result(RESULT_FAILURE, "infrastructure", False, "agent_result_missing_or_invalid")

    if not agent_identity_ok:
        return _result(IDENTITY_FAILURE, "infrastructure", False, "agent_identity_invalid")

    status = agent_result.get("status")
    wrapper = agent_result.get("wrapper") if isinstance(agent_result.get("wrapper"), dict) else {}
    return_code = wrapper.get("process_return_code")
    turn_completed = wrapper.get("turn_completed")
    budget_ok = wrapper.get("budget_ok")

    # The B1/B2 Codex wrappers use this exact status when the provider/CLI turn
    # did not complete. Prefer explicit process/turn evidence as well so future
    # wrapper status names cannot accidentally let transport failure fall through.
    if status == "tool_transport_failure" or return_code not in (None, 0) or turn_completed is False:
        return _result(
            TRANSPORT_FAILURE,
            "infrastructure",
            False,
            "agent_turn_did_not_complete",
            agent_status=status,
            process_return_code=return_code,
            turn_completed=turn_completed,
            budget_ok=budget_ok,
        )

    if status == "budget_exceeded" or budget_ok is False:
        return _result(
            BUDGET_FAILURE,
            "budget",
            False,
            "agent_turn_completed_over_budget",
            agent_status=status,
            process_return_code=return_code,
            turn_completed=turn_completed,
            budget_ok=budget_ok,
        )

    if status != "completed":
        return _result(
            RESULT_FAILURE,
            "infrastructure",
            False,
            "agent_result_status_not_completed",
            agent_status=status,
            process_return_code=return_code,
            turn_completed=turn_completed,
            budget_ok=budget_ok,
        )

    # A completed B2 Codex result is expected to contain positive turn evidence.
    # Missing legacy fields remain tolerated; explicit negative evidence never is.
    return _result(
        COMPLETED,
        "none",
        True,
        "agent_turn_completed",
        agent_status=status,
        process_return_code=return_code,
        turn_completed=turn_completed,
        budget_ok=budget_ok,
    )


def classify_retained_record(record: dict[str, Any]) -> dict[str, Any]:
    """Return a non-mutating execution classification for a retained B2 record."""
    process_timed_out = False
    raw_status = str(record.get("status", "unknown"))
    if raw_status in {"timeout", "agent_timeout"}:
        process_timed_out = True
    result = classify_agent_execution(
        agent_result=record.get("agent_result"),
        agent_identity_ok=record.get("agent_identity_ok") is True,
        process_timed_out=process_timed_out,
    )
    return {
        "raw_status": raw_status,
        **result,
        "raw_record_unchanged": True,
    }


def _result(
    status: str,
    domain: str,
    deterministic_verifier_authoritative: bool,
    reason: str,
    **evidence: Any,
) -> dict[str, Any]:
    return {
        "status": status,
        "failure_domain": domain,
        "deterministic_verifier_authoritative": deterministic_verifier_authoritative,
        "reason": reason,
        "agent_evidence": evidence,
    }
