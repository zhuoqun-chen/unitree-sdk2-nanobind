"""Type stubs for the compiled ``unitree_sdk2_bind._core`` extension."""

from __future__ import annotations

import numpy as np
import numpy.typing as npt

MOTOR_COUNT: int
__is_stub__: bool

def init_channel(domain_id: int = 0, interface: str = "") -> None:
    """Initialize the DDS channel factory. Call once before pub/sub."""

def crc32(data: bytes) -> int:
    """Unitree CRC-32 over a little-endian uint32 buffer."""

class LowCmd:
    """unitree_hg LowCmd_: a low-level G1 command."""

    mode_pr: int
    mode_machine: int
    crc: int
    def __init__(self) -> None: ...
    def set_motors(
        self,
        q: npt.NDArray[np.float32],
        dq: npt.NDArray[np.float32],
        kp: npt.NDArray[np.float32],
        kd: npt.NDArray[np.float32],
        tau: npt.NDArray[np.float32],
        mode: int = 1,
        num_motors: int = -1,
    ) -> None:
        """Bulk-fill the first ``num_motors`` slots (num_motors<0 uses len(q))."""

class LowState:
    """unitree_hg LowState_: low-level G1 feedback."""

    mode_machine: int
    tick: int
    crc: int
    @property
    def q(self) -> npt.NDArray[np.float32]: ...
    @property
    def dq(self) -> npt.NDArray[np.float32]: ...
    @property
    def tau_est(self) -> npt.NDArray[np.float32]: ...
    @property
    def motor_mode(self) -> npt.NDArray[np.uint8]:
        """Per-motor enable flag: 0 disabled, 1 enabled."""
    @property
    def quaternion(self) -> npt.NDArray[np.float32]:
        """Base orientation, wire order (w, x, y, z)."""
    @property
    def gyroscope(self) -> npt.NDArray[np.float32]: ...
    @property
    def accelerometer(self) -> npt.NDArray[np.float32]: ...
    @property
    def rpy(self) -> npt.NDArray[np.float32]: ...
    @property
    def wireless_remote(self) -> bytes:
        """Raw 40-byte wireless-remote frame."""
    def __init__(self) -> None: ...

class LowCmdPublisher:
    def __init__(self, topic: str = "rt/lowcmd") -> None: ...
    def write(self, cmd: LowCmd, compute_crc: bool = True) -> None: ...
    def close(self) -> None: ...

class LowStateSubscriber:
    def __init__(self, topic: str = "rt/lowstate") -> None: ...
    def start(self, queue_len: int = 10) -> None: ...
    def latest(self) -> LowState | None: ...
    def has(self) -> bool: ...
    def close(self) -> None: ...

class MotionSwitcherClient:
    def __init__(self) -> None: ...
    def init(self, timeout: float = 5.0) -> None: ...
    def check_mode(self) -> tuple[str, str]: ...
    def release_mode(self) -> int: ...
    def select_mode(self, name: str) -> int:
        """Load a high-level service by name (e.g. ``"ai"``). MOVES THE ROBOT."""

class LocoStateClient:
    """Read-only view of the loco service's FSM. Getters only; cannot command."""

    def __init__(self) -> None: ...
    def init(self, timeout: float = 5.0) -> None: ...
    def fsm_id(self) -> int:
        """Current FSM id; see ``unitree_sdk2_bind.FSM_IDS``."""
    def fsm_mode(self) -> int: ...

class AiModeSetupClient:
    """Bring the robot up under a loaded high-level service. MOVES THE ROBOT.

    The software equivalent of the remote's L2+B / L2+UP / R1+X. Only works
    while a service is loaded; every call fails rc 3104 after release_mode().
    """

    def __init__(self) -> None: ...
    def init(self, timeout: float = 5.0) -> None: ...
    def damp(self) -> int:
        """FSM 1. Keep the remote's L2+B as an independent damp path."""
    def stand_up(self) -> int:
        """FSM 4. MOVES THE ROBOT."""
    def start(self) -> int:
        """FSM 500, operating stance. MOVES THE ROBOT."""
    def zero_torque(self) -> int:
        """FSM 0."""
    def set_fsm_id(self, fsm_id: int) -> int:
        """Raw SetFsmId for states without a named helper (2 squat, 3 sit)."""
