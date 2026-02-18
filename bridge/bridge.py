#!/usr/bin/env python3
"""JSON <-> line-protocol bridge for the traffic_sim C binary.

Usage:
    python3 bridge/bridge.py input.json output.json

The binary is located automatically at ../build/traffic_sim relative to this
script. Override by setting the TRAFFIC_SIM environment variable.
"""

import json
import os
import subprocess
import sys


def find_binary():
    env = os.environ.get("TRAFFIC_SIM")
    if env:
        return env
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(script_dir, "..", "build", "traffic_sim"))


def run(input_path, output_path):
    with open(input_path) as f:
        data = json.load(f)

    commands = data.get("commands", [])

    lines = []
    step_count = 0
    for cmd in commands:
        t = cmd["type"]
        if t == "addVehicle":
            lines.append(
                f"addVehicle {cmd['vehicleId']} {cmd['startRoad']} {cmd['endRoad']}\n"
            )
        elif t == "step":
            lines.append("step\n")
            step_count += 1

    proc = subprocess.run(
        [find_binary()],
        input="".join(lines),
        capture_output=True,
        text=True,
    )

    if proc.returncode != 0:
        print(f"Simulator error:\n{proc.stderr}", file=sys.stderr)
        sys.exit(1)

    output_lines = proc.stdout.splitlines()
    if len(output_lines) != step_count:
        print(
            f"Expected {step_count} output lines, got {len(output_lines)}",
            file=sys.stderr,
        )
        sys.exit(1)

    step_statuses = []
    for line in output_lines:
        step_statuses.append({"leftVehicles": line.split() if line.strip() else []})

    with open(output_path, "w") as f:
        json.dump({"stepStatuses": step_statuses}, f, indent=2)
        f.write("\n")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.json output.json", file=sys.stderr)
        sys.exit(1)
    run(sys.argv[1], sys.argv[2])
