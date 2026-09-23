"""Minimal G1 low-level loop over the raw binding (no adapter).

Real robot:
    python examples/g1_lowlevel_smoke.py --nic eth0
Stub build (no hardware): runs one synthetic step and exits.
"""

from __future__ import annotations

import argparse
import time

import numpy as np

import unitree_sdk2_bind as u

NUM_MOTORS = 29


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--nic", default="", help="network interface, e.g. eth0")
    ap.add_argument("--domain", type=int, default=0)
    ap.add_argument("--steps", type=int, default=200)
    ap.add_argument("--hz", type=float, default=50.0)
    args = ap.parse_args()

    u.init_channel(args.domain, args.nic)

    # Release the onboard high-level controller so the policy owns rt/lowcmd.
    msc = u.MotionSwitcherClient()
    msc.init(5.0)
    _form, name = msc.check_mode()
    while name:
        msc.release_mode()
        _form, name = msc.check_mode()
        time.sleep(1.0)

    sub = u.LowStateSubscriber("rt/lowstate")
    sub.start(10)
    while sub.latest() is None:
        time.sleep(0.01)
    mode_machine = sub.latest().mode_machine
    print(f"connected: mode_machine={mode_machine}, is_stub={u.is_stub()}")

    pub = u.LowCmdPublisher("rt/lowcmd")

    kp = np.full(NUM_MOTORS, 60.0, dtype=np.float32)
    kd = np.full(NUM_MOTORS, 1.0, dtype=np.float32)
    zeros = np.zeros(NUM_MOTORS, dtype=np.float32)
    dt = 1.0 / args.hz

    for _ in range(args.steps):
        ls = sub.latest()
        q_meas = np.asarray(ls.q, dtype=np.float32)[:NUM_MOTORS]

        # Trivial "hold current pose" command (replace with a policy).
        cmd = u.LowCmd()
        cmd.mode_pr = 0
        cmd.mode_machine = mode_machine
        cmd.set_motors(q_meas, zeros, kp, kd, zeros, mode=1, num_motors=NUM_MOTORS)
        pub.write(cmd)  # CRC filled by the publisher

        if u.is_stub():
            print("stub build: one step, exiting.")
            break
        time.sleep(dt)

    pub.close()
    sub.close()


if __name__ == "__main__":
    main()
