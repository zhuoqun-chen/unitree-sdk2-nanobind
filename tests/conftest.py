"""Refuse to run this suite against a real (non-stub) build.

tests/ is stub-only by design, and two of its tests COMMAND the robot:
``test_unitree_robot_comms_full_cycle`` releases the motion service and then
streams q=0 at kp=60 on every joint, and ``test_publisher_write_roundtrip``
writes to rt/lowcmd. On a stub build both are dropped harmlessly. On a real
build they reach whatever robot the DDS interface resolves to, which is not
necessarily the interface you expected -- ``nic=""`` lets CycloneDDS choose.

A real robot snapping to zero position at kp=60 is not an acceptable outcome of
running `pytest`, so fail loudly here rather than rely on the interface lottery.
"""

from __future__ import annotations

import warnings

import pytest

with warnings.catch_warnings():
    warnings.simplefilter("ignore", RuntimeWarning)  # stub-build import warning
    import unitree_sdk2_bind as u


def pytest_configure(config: pytest.Config) -> None:
    if not u.is_stub():
        pytest.exit(
            "REFUSING to run tests/ against a REAL build: this suite writes to "
            "rt/lowcmd (q=0 at kp=60) and releases the motion service.\n"
            f"  imported from: {u.__file__}\n"
            "If that path is inside a local .venv/ rather than the wheel you "
            "just built, an editable install is shadowing it: `uv run "
            "--no-project` still puts ./.venv and ./src on sys.path. Remove "
            "the stray .venv and re-run. Otherwise rebuild hardware-free with "
            "`make test` or `-C cmake.define.USE_STUB_SDK=ON`.",
            returncode=2,
        )
