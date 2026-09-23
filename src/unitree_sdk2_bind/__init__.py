"""unitree-sdk2-bind: nanobind bindings for the Unitree SDK2 unitree_hg (G1) API.

The compiled ``_core`` extension links the C++ ``unitree_sdk2`` (which bundles its
own CycloneDDS), so there is **no** ``cyclonedds`` Python dependency. Built with
the CPython Stable ABI (abi3): one ``cp312-abi3`` wheel runs on 3.12, 3.13, 3.14 and later.

Low-level API (see ``_core``):
    init_channel(domain_id, interface)   -- bring up the DDS channel factory
    LowCmd / LowState                    -- unitree_hg messages, ndarray accessors
    LowCmdPublisher / LowStateSubscriber -- typed pub/sub (auto-CRC, cached state)
    MotionSwitcherClient                 -- release / load the onboard service
    LocoStateClient                      -- read-only loco FSM state (see FSM_IDS)
    AiModeSetupClient                    -- drive the loco FSM for bring-up

High-level helper:
    unitree_sdk2_bind.comms.UnitreeRobotComms -- a RobotComms adapter over the core
"""

from __future__ import annotations

import warnings
from importlib.metadata import PackageNotFoundError
from importlib.metadata import version as _pkg_version

from ._core import (
    MOTOR_COUNT,
    AiModeSetupClient,
    LocoStateClient,
    LowCmd,
    LowCmdPublisher,
    LowState,
    LowStateSubscriber,
    MotionSwitcherClient,
    __is_stub__,
    crc32,
    init_channel,
)

# G1 loco-service FSM ids, read off SetFsmId() in the SDK's g1_loco_client.hpp.
# These are G1-specific: H1 and R1 number their states differently.
FSM_IDS = {
    0: "zero_torque",
    1: "damp",
    2: "squat",
    3: "sit",
    4: "stand_up",
    500: "start",
}

try:
    __version__ = _pkg_version("unitree-sdk2-bind")
except PackageNotFoundError:  # running from a source tree without install metadata
    __version__ = "0.1.0+local"


def is_stub() -> bool:
    """True if the extension was built against the stub SDK (no hardware/DDS)."""
    return bool(__is_stub__)


if __is_stub__:
    warnings.warn(
        "unitree_sdk2_bind was built against the STUB SDK (no hardware/DDS): "
        "publishers drop writes and subscribers return synthetic data. Rebuild "
        "against the real unitree_sdk2 for hardware.",
        RuntimeWarning,
        stacklevel=2,
    )

__all__ = [
    "FSM_IDS",
    "MOTOR_COUNT",
    "AiModeSetupClient",
    "LocoStateClient",
    "LowCmd",
    "LowCmdPublisher",
    "LowState",
    "LowStateSubscriber",
    "MotionSwitcherClient",
    "__version__",
    "crc32",
    "init_channel",
    "is_stub",
]
