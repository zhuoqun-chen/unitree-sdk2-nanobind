"""UnitreeRobotComms: a RobotComms adapter backed by unitree-sdk2-bind.

Implements the start/read_state/write_command/damp/stop contract on top of the
compiled core: the DDS round-trip runs in C++ via ``unitree_sdk2_bind._core``
and the per-joint command is filled in one ``set_motors`` call.

Baked-in G1 / unitree_hg conventions:
  - Fill the first ``num_motors`` (29) slots; ``mode_pr = 0``; per-motor ``mode = 1``.
  - ``mode_machine`` is learned from the first LowState and echoed back.
  - CRC is computed last, by the publisher, before each write.
  - Release the high-level motion service before publishing ``rt/lowcmd``.
  - Rate decoupling: ``write_command`` updates a held target at policy rate; a
    background thread re-streams it to the wire at ``wire_hz`` (~200 Hz).
  - Quaternion wire order is wxyz; ``read_state`` returns xyzw.

Usage (``joint_names`` are the 29 G1 joint names in wire order)::

    from unitree_sdk2_bind.comms import UnitreeRobotComms
    comms = UnitreeRobotComms(nic="eth0", joint_names=joint_names)
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Any

import numpy as np

from . import _core


@dataclass
class MotorCommand:
    q: np.ndarray
    dq: np.ndarray
    kp: np.ndarray
    kd: np.ndarray
    tau: np.ndarray
    joint_names: tuple[str, ...]


@dataclass
class RobotState:
    q: np.ndarray
    dq: np.ndarray
    quat: np.ndarray
    gyro: np.ndarray
    accel: np.ndarray
    joint_names: tuple[str, ...] = ()
    rpy: np.ndarray | None = None
    remote: dict[str, float] = field(default_factory=dict)
    stamp_s: float = 0.0


# Unitree wireless-remote button bit map (keySwitch uint16 at bytes [2:4]).
_REMOTE_BITS = {
    "R1": 0, "L1": 1, "start": 2, "select": 3, "R2": 4, "L2": 5, "F1": 6,
    "F2": 7, "A": 8, "B": 9, "X": 10, "Y": 11, "up": 12, "right": 13,
    "down": 14, "left": 15,
}  # fmt: skip


def _parse_remote(remote: bytes | None) -> dict[str, float]:
    if remote is None or len(remote) < 4:
        return {}
    keys = int(remote[2]) | (int(remote[3]) << 8)
    return {name: float((keys >> bit) & 1) for name, bit in _REMOTE_BITS.items()}


def _f32(a: Any) -> np.ndarray:
    return np.ascontiguousarray(a, dtype=np.float32)


class UnitreeRobotComms:
    """RobotComms over unitree-sdk2-bind. Implements start/read_state/
    write_command/damp/stop."""

    def __init__(
        self,
        nic: str,
        num_motors: int = 29,
        domain_id: int = 0,
        cmd_topic: str = "rt/lowcmd",
        state_topic: str = "rt/lowstate",
        wire_hz: float = 200.0,
        release_motion_service: bool = True,
        damp_kd: float = 8.0,
        joint_names: tuple[str, ...] | list[str] | None = None,
        first_state_timeout_s: float = 5.0,
    ) -> None:
        self.nic = nic
        self.num_motors = num_motors
        self.domain_id = domain_id
        self.cmd_topic = cmd_topic
        self.state_topic = state_topic
        self.wire_dt = 1.0 / wire_hz
        self.release_motion_service = release_motion_service
        self.damp_kd = damp_kd
        self.first_state_timeout_s = first_state_timeout_s
        self.joint_names = (
            tuple(joint_names)
            if joint_names
            else tuple(f"joint_{i}" for i in range(num_motors))
        )
        self._pub: Any = None
        self._sub: Any = None
        self._mode_machine = 0
        self._latest_cmd: Any = None
        self._wire_thread: threading.Thread | None = None
        self._running = False
        self._lock = threading.Lock()

    # -- lifecycle ----------------------------------------------------------
    def start(self) -> None:
        _core.init_channel(self.domain_id, self.nic or "")

        if self.release_motion_service:
            self._release_motion()

        self._pub = _core.LowCmdPublisher(self.cmd_topic)
        self._sub = _core.LowStateSubscriber(self.state_topic)
        self._sub.start(10)

        # Learn mode_machine from the first LowState; never invent it.
        t0 = time.monotonic()
        while self._sub.latest() is None:
            if time.monotonic() - t0 > self.first_state_timeout_s:
                raise RuntimeError(
                    f"no LowState within {self.first_state_timeout_s}s; check "
                    "nic/domain_id and that the robot is powered and on the wire."
                )
            time.sleep(0.01)
        self._mode_machine = int(self._sub.latest().mode_machine)

        self._running = True
        self._wire_thread = threading.Thread(
            target=self._wire_loop, name="lowcmd_wire", daemon=True
        )
        self._wire_thread.start()

    def _release_motion(self) -> None:
        msc = _core.MotionSwitcherClient()
        msc.init(5.0)
        _form, name = msc.check_mode()
        while name:
            msc.release_mode()
            _form, name = msc.check_mode()
            time.sleep(1.0)

    def stop(self) -> None:
        self._running = False
        if self._wire_thread is not None:
            self._wire_thread.join(timeout=1.0)
        if self._sub is not None:
            self._sub.close()
        if self._pub is not None:
            self._pub.close()

    # -- io -----------------------------------------------------------------
    def read_state(self) -> RobotState:
        ls = self._sub.latest() if self._sub is not None else None
        if ls is None:
            raise RuntimeError("read_state() before start()/first LowState")
        n = self.num_motors
        wxyz = np.asarray(ls.quaternion, dtype=np.float32)
        quat = np.array([wxyz[1], wxyz[2], wxyz[3], wxyz[0]], dtype=np.float32)
        return RobotState(
            q=np.asarray(ls.q, dtype=np.float32)[:n],
            dq=np.asarray(ls.dq, dtype=np.float32)[:n],
            quat=quat,
            gyro=np.asarray(ls.gyroscope, dtype=np.float32),
            accel=np.asarray(ls.accelerometer, dtype=np.float32),
            joint_names=self.joint_names,
            rpy=np.asarray(ls.rpy, dtype=np.float32),
            remote=_parse_remote(ls.wireless_remote),
            stamp_s=time.monotonic(),
        )

    def _build_lowcmd(self, cmd: MotorCommand) -> Any:
        lc = _core.LowCmd()
        lc.mode_pr = 0
        lc.mode_machine = self._mode_machine
        lc.set_motors(
            _f32(cmd.q),
            _f32(cmd.dq),
            _f32(cmd.kp),
            _f32(cmd.kd),
            _f32(cmd.tau),
            mode=1,
            num_motors=self.num_motors,
        )
        return lc

    def write_command(self, cmd: MotorCommand) -> None:
        lc = self._build_lowcmd(cmd)
        with self._lock:
            self._latest_cmd = lc  # re-streamed by the wire thread

    def damp(self) -> None:
        """kp=0, kd>0 on every motor: the safe stop. Hold position, send now."""
        n = self.num_motors
        ls = self._sub.latest() if self._sub is not None else None
        q_now = (
            np.asarray(ls.q, dtype=np.float32)[:n].copy()
            if ls is not None
            else np.zeros(n, dtype=np.float32)
        )
        zeros = np.zeros(n, dtype=np.float32)
        lc = self._build_lowcmd(
            MotorCommand(
                q=q_now,
                dq=zeros.copy(),
                kp=zeros.copy(),
                kd=np.full(n, self.damp_kd, dtype=np.float32),
                tau=zeros.copy(),
                joint_names=self.joint_names,
            )
        )
        with self._lock:
            self._latest_cmd = lc
        if self._pub is not None:
            self._pub.write(lc)  # immediate, don't wait for the wire tick

    def _wire_loop(self) -> None:
        while self._running:
            with self._lock:
                cmd = self._latest_cmd
            if cmd is not None and self._pub is not None:
                self._pub.write(cmd)
            time.sleep(self.wire_dt)
