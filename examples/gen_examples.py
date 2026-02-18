#!/usr/bin/env python3
"""
Generate large, complex traffic simulation examples.
Run once from the project root:
    python3 examples/gen_examples.py
"""
import json, random, pathlib

OUT = pathlib.Path(__file__).parent
rng = random.Random(42)          # fixed seed → reproducible examples

# ── movement pools ──────────────────────────────────────────────────────────
NS_STRAIGHT = [("north","south"), ("south","north")]
EW_STRAIGHT = [("east","west"),  ("west","east")]
NS_LEFT     = [("north","east"), ("south","west")]
EW_LEFT     = [("east","south"), ("west","north")]
NS_RIGHT    = [("north","west"), ("south","east")]
EW_RIGHT    = [("east","north"), ("west","south")]
ALL_MOVES   = NS_STRAIGHT + EW_STRAIGHT + NS_LEFT + EW_LEFT + NS_RIGHT + EW_RIGHT

# ── helpers ─────────────────────────────────────────────────────────────────
_ctr = 0
def vid(tag="v"):
    global _ctr; _ctr += 1
    return f"{tag}_{_ctr:05d}"

def av(start, end, tag="v"):
    return {"type":"addVehicle","vehicleId":vid(tag),"startRoad":start,"endRoad":end}

def add_batch(cmds, pool, n, tag="v"):
    for _ in range(n):
        s, e = rng.choice(pool)
        cmds.append(av(s, e, tag))

STEP = {"type": "step"}

def save(name, meta, commands):
    doc = {"_scenario": name, "_description": meta, "commands": commands}
    p = OUT / name
    p.write_text(json.dumps(doc, indent=2))
    n_add  = sum(1 for c in commands if c["type"] == "addVehicle")
    n_step = sum(1 for c in commands if c["type"] == "step")
    print(f"  {name}: {n_step} steps, {n_add} vehicles")


# ════════════════════════════════════════════════════════════════════════════
# 06 — Morning Rush
#
# Two-phase rush: heavy N↔S commuter flow for the first 150 steps, then an
# E↔W surge kicks in as cross-city traffic picks up.  Left-turners trickle
# in throughout; because the main lanes are always busy they accumulate wait
# time until the scoring formula finally forces an arrow phase.
# ════════════════════════════════════════════════════════════════════════════
def gen_morning_rush():
    cmds = []

    # Phase 1: NS dominant (steps 1-150)
    for i in range(1, 151):
        add_batch(cmds, NS_STRAIGHT, 3, "ns")       # 3 NS straight arrivals
        if i % 8  == 0: add_batch(cmds, EW_STRAIGHT, 1, "ew")   # occasional EW
        if i % 20 == 0: add_batch(cmds, NS_LEFT,     2, "nl")   # NS left burst
        if i % 35 == 0: add_batch(cmds, EW_LEFT,     1, "el")   # EW left trickle
        cmds.append(STEP)

    # Phase 2: EW surge (steps 151-300)
    for i in range(151, 301):
        add_batch(cmds, EW_STRAIGHT, 3, "ew2")      # EW now dominant
        if i % 6  == 0: add_batch(cmds, NS_STRAIGHT, 1, "ns2")
        if i % 18 == 0: add_batch(cmds, EW_LEFT,     2, "el2")
        if i % 30 == 0: add_batch(cmds, NS_LEFT,     1, "nl2")
        if i % 50 == 0: add_batch(cmds, NS_RIGHT + EW_RIGHT, 2, "rt")
        cmds.append(STEP)

    return cmds

# ════════════════════════════════════════════════════════════════════════════
# 07 — All-Directions Chaos
#
# Continuous random arrivals from every road and every movement type for
# 400 steps.  Arrival rate varies sinusoidally to simulate traffic fluctuation.
# All four phases will be forced to compete throughout.
# ════════════════════════════════════════════════════════════════════════════
import math
def gen_chaos():
    cmds = []
    for i in range(1, 401):
        # base rate oscillates between 1 and 6 vehicles per step
        rate = max(1, round(3 + 3 * math.sin(i * math.pi / 40)))
        for _ in range(rate):
            s, e = rng.choice(ALL_MOVES)
            cmds.append(av(s, e, "ch"))
        # every 5 steps inject a left-turn burst to keep arrow phases alive
        if i % 5 == 0:
            s, e = rng.choice(NS_LEFT + EW_LEFT)
            cmds.append(av(s, e, "lt"))
        cmds.append(STEP)
    return cmds

# ════════════════════════════════════════════════════════════════════════════
# 08 — Wave Attack
#
# Traffic arrives in tight waves every 25 steps, alternating between NS and
# EW.  Each wave dumps 40 vehicles at once, creating a large burst that the
# controller must absorb (clamped to MAX_GREEN_STEPS=8 per phase decision).
# Left-turn waves appear every 75 steps, competing with the lingering queues.
# ════════════════════════════════════════════════════════════════════════════
def gen_waves():
    cmds = []
    WAVE_EVERY = 25
    for i in range(1, 501):
        if i % WAVE_EVERY == 1:
            wave_num = (i // WAVE_EVERY) % 4
            if wave_num == 0:
                add_batch(cmds, NS_STRAIGHT, 40, "wns")
            elif wave_num == 1:
                add_batch(cmds, EW_STRAIGHT, 40, "wew")
            elif wave_num == 2:
                add_batch(cmds, NS_STRAIGHT + EW_STRAIGHT, 30, "wmix")
                add_batch(cmds, NS_RIGHT + EW_RIGHT, 10, "wrt")
            else:
                add_batch(cmds, NS_LEFT + EW_LEFT, 20, "wlt")
                add_batch(cmds, NS_STRAIGHT, 10, "wns2")
                add_batch(cmds, EW_STRAIGHT, 10, "wew2")
        cmds.append(STEP)
    return cmds

# ════════════════════════════════════════════════════════════════════════════
# 09 — Left-Turn Siege
#
# Heavy steady straight traffic on all axes throughout.  Every 10 steps a
# cluster of left-turners joins the queue.  Because the straight lanes are
# always busy, each left-turner must accumulate enough wait-time score to
# finally beat the straight-lane pressure — demonstrating the anti-starvation
# guarantee under sustained load.
# ════════════════════════════════════════════════════════════════════════════
def gen_left_siege():
    cmds = []
    for i in range(1, 351):
        # Constant straight pressure: 2 NS + 2 EW every step
        add_batch(cmds, NS_STRAIGHT, 2, "st")
        add_batch(cmds, EW_STRAIGHT, 2, "st")
        # Burst of left-turners every 10 steps (all four roads)
        if i % 10 == 0:
            add_batch(cmds, NS_LEFT, 3, "lt")
            add_batch(cmds, EW_LEFT, 3, "lt")
        # Occasional right-turn noise
        if i % 17 == 0:
            add_batch(cmds, NS_RIGHT + EW_RIGHT, 2, "rt")
        cmds.append(STEP)
    return cmds


# ── main ────────────────────────────────────────────────────────────────────
print("Generating examples…")
save("06_morning_rush.json",
     "Two-phase commuter rush: NS-dominant for 150 steps, EW surge for 150 more. "
     "Left-turners trickle throughout and must fight for arrow phases.",
     gen_morning_rush())

save("07_all_directions_chaos.json",
     "400 steps of sinusoidally varying random arrivals from every road and movement "
     "type. All four phases compete dynamically throughout.",
     gen_chaos())

save("08_wave_attack.json",
     "500 steps. Traffic arrives in waves of 40 vehicles every 25 steps, alternating "
     "NS / EW / mixed / arrow. Controller must repeatedly absorb large bursts.",
     gen_waves())

save("09_left_turn_siege.json",
     "350 steps of constant straight-lane pressure (4 vehicles/step) with left-turn "
     "clusters every 10 steps. Demonstrates anti-starvation guarantee under load.",
     gen_left_siege())

print("Done.")
