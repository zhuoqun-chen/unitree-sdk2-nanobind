"""Smoke tests: run against the stub SDK build (USE_STUB_SDK=ON).

pip install . -C cmake.define.USE_STUB_SDK=ON
pytest tests/
"""

from __future__ import annotations

import warnings

import numpy as np
import pytest

with warnings.catch_warnings():
    warnings.simplefilter("ignore", RuntimeWarning)  # stub-build import warning
    import unitree_sdk2_bind as u
    from unitree_sdk2_bind.comms import MotorCommand, UnitreeRobotComms


def test_is_stub_build():
    # The test suite only makes sense on the hardware-free build.
    assert u.is_stub() is True
    assert u.MOTOR_COUNT == 35
    assert u.__version__


def test_crc32_matches_known_vector():
    # CRC-32 (poly 0x04c11db7, init 0xFFFFFFFF) over four zero bytes.
    val = u.crc32(b"\x00\x00\x00\x00")
    ref = 0xFFFFFFFF
    poly = 0x04C11DB7
    for _ in range(32):
        ref = (
            ((ref << 1) ^ poly) & 0xFFFFFFFF
            if ref & 0x80000000
            else (ref << 1) & 0xFFFFFFFF
        )
    assert val == ref


def test_lowcmd_set_motors_bulk():
    cmd = u.LowCmd()
    cmd.mode_pr = 0
    cmd.mode_machine = 4
    n = 29
    q = np.arange(n, dtype=np.float32)
    z = np.zeros(n, dtype=np.float32)
    kp = np.full(n, 60.0, dtype=np.float32)
    # Should not raise; fills the first 29 of 35 slots.
    cmd.set_motors(q, z, kp, z, z, mode=1, num_motors=n)
    cmd.crc = 12345
    assert cmd.crc == 12345


def test_lowcmd_set_motors_rejects_short_array():
    cmd = u.LowCmd()
    short = np.zeros(5, dtype=np.float32)
    with pytest.raises(ValueError):
        cmd.set_motors(short, short, short, short, short, num_motors=29)


def test_subscriber_delivers_synthetic_state():
    u.init_channel(0, "")
    sub = u.LowStateSubscriber("rt/lowstate")
    sub.start(10)
    ls = sub.latest()
    assert ls is not None
    assert ls.mode_machine == 4  # stub_fill
    assert ls.tick >= 1
    q = np.asarray(ls.q)
    assert q.shape == (35,)
    quat = np.asarray(ls.quaternion)
    np.testing.assert_allclose(quat, [1.0, 0.0, 0.0, 0.0])  # identity, wxyz
    assert len(ls.wireless_remote) == 40
    sub.close()


def test_publisher_write_roundtrip():
    u.init_channel(0, "")
    pub = u.LowCmdPublisher("rt/lowcmd")
    cmd = u.LowCmd()
    n = 29
    z = np.zeros(n, dtype=np.float32)
    cmd.set_motors(z, z, z, z, z, num_motors=n)
    pub.write(cmd)  # stub drops it; just must not raise and must fill CRC
    assert cmd.crc != 0
    pub.close()


def test_unitree_robot_comms_full_cycle():
    comms = UnitreeRobotComms(
        nic="",
        num_motors=29,
        wire_hz=1000.0,
        joint_names=tuple(f"j{i}" for i in range(29)),
    )
    comms.start()
    state = comms.read_state()
    assert state.q.shape == (29,)
    assert state.quat.shape == (4,)
    # wxyz identity -> xyzw identity
    np.testing.assert_allclose(state.quat, [0.0, 0.0, 0.0, 1.0])
    assert state.gyro.shape == (3,)
    assert isinstance(state.remote, dict)

    n = 29
    action = MotorCommand(
        q=np.zeros(n, np.float32),
        dq=np.zeros(n, np.float32),
        kp=np.full(n, 60.0, np.float32),
        kd=np.full(n, 1.0, np.float32),
        tau=np.zeros(n, np.float32),
        joint_names=comms.joint_names,
    )
    comms.write_command(action)
    comms.damp()
    comms.stop()
