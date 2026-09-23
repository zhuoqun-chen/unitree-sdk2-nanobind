"""READ-ONLY G1 LowState probe: subscribes, never publishes.

Answers "is the robot actually sending LowState, and how fast?" without
commanding anything. Deliberately never constructs a LowCmdPublisher and never
calls release_mode(), so it cannot drive a joint. The companion
g1_lowlevel_smoke.py DOES command; this one does not.

    python examples/g1_lowstate_probe.py --nic eth0

Secure the robot on a gantry or lay it down first. Joining the low-level DDS
domain is widely reported to make the G1 hand over control and drop into
damping, and a free-standing robot would collapse. Gantry or lying down, always.

If init_channel fails with "<nic>: does not match an available interface",
the interface almost certainly has no IPv4 address; CycloneDDS reports that
as an unavailable interface, which reads like a wrong NIC name. Check
`ip -br addr` and assign an address on the robot's subnet before retrying.
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
    ap.add_argument("--topic", default="rt/lowstate")
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument(
        "--check-mode",
        action="store_true",
        help="also query MotionSwitcherClient.check_mode() (read-only RPC)",
    )
    args = ap.parse_args()

    if u.is_stub():
        raise SystemExit(
            "refusing to run: this is a STUB build, so every reading below "
            "would be synthetic. Rebuild without USE_STUB_SDK to probe a robot."
        )

    print(
        f"binding {u.__version__}  is_stub={u.is_stub()}  MOTOR_COUNT={u.MOTOR_COUNT}"
    )
    u.init_channel(args.domain, args.nic)

    if args.check_mode:
        # Read-only: reports whether a high-level mode still holds the robot.
        # Never release_mode() here; that is a state change, not a probe.
        msc = u.MotionSwitcherClient()
        msc.init(5.0)
        form, name = msc.check_mode()
        print(f"check_mode -> form={form!r} name={name!r}")

    sub = u.LowStateSubscriber(args.topic)
    sub.start(10)
    print(f"subscribed to {args.topic!r}, sampling for {args.seconds}s ...")

    t0 = time.perf_counter()
    first_at: float | None = None
    ticks: set[int] = set()
    polls = 0
    last = None

    while time.perf_counter() - t0 < args.seconds:
        state = sub.latest()
        polls += 1
        if state is not None:
            if first_at is None:
                first_at = time.perf_counter() - t0
            # tick is the robot's own counter, so distinct ticks measure the
            # publish rate rather than how fast we happened to poll.
            ticks.add(state.tick)
            last = state
        time.sleep(0.0005)

    sub.close()
    elapsed = time.perf_counter() - t0
    print(f"\nhas()={sub.has()}  polls={polls}  distinct ticks={len(ticks)}")

    if last is None:
        raise SystemExit(
            f"NO LowState in {elapsed:.1f}s. Check --nic/--domain/--topic, that the "
            "robot is powered and on the wire, and confirm it is publishing at all "
            "(a healthy G1 multicasts ~1 kHz LowState on udp port 7401; tcpdump it)."
        )

    assert first_at is not None
    print(f"first frame after {first_at * 1000:.0f} ms")
    print(f"update rate ~{len(ticks) / elapsed:.1f} Hz (distinct ticks / wall time)")
    print(f"mode_machine={last.mode_machine}  tick={last.tick}  crc={last.crc}")

    np.set_printoptions(precision=3, suppress=True, linewidth=100)
    q = np.asarray(last.q, dtype=np.float32)
    dq = np.asarray(last.dq, dtype=np.float32)
    print(f"q.shape={q.shape}, G1 29-DoF occupies the first {NUM_MOTORS}")
    print(f"q[:{NUM_MOTORS}]  = {q[:NUM_MOTORS]}")
    print(f"dq[:{NUM_MOTORS}] = {dq[:NUM_MOTORS]}")
    print(f"q[{NUM_MOTORS}:]   = {q[NUM_MOTORS:]}  (unused wire slots)")
    print(f"quaternion (WIRE order, wxyz) = {np.asarray(last.quaternion)}")
    print(f"rpy   = {np.asarray(last.rpy)}")
    print(f"gyro  = {np.asarray(last.gyroscope)}")
    print(f"accel = {np.asarray(last.accelerometer)}")

    if not q[:NUM_MOTORS].any() and not dq[:NUM_MOTORS].any():
        print("\nWARNING: q and dq are all zero; that is not a real robot pose.")


if __name__ == "__main__":
    main()
