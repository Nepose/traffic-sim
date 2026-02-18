# Traffic simulation

An example of an intelligent crossroad traffic simulation system in C.

Given that there's a sample crossroad consisting of N-S and W-E road, and a queue of cars for each of the lanes being separated by light, the app returns the suggested correct queue of escaping car.


## Adaptive weighted scheduler

Instead of a fixed timer, each phase gets a priority score:

`priority(phase) = \sum{ queueLength(road) \cdot (1 + avgWaitTime(road)) }`

(Yeah, it's me falling in love with LaTeX)

Phase selection each cycle:
1. Compute priority for each candidate phase (NS, EW, plus arrow phases if turns queued)
2. Select the highest-priority compatible phase
3. Run it for ceil(pressure / factor) steps (clamped to [MIN, MAX])
4. Transition through YELLOW before switching

# Step semantics

On each step:
1. For every road currently on green, dequeue its first vehicle → add to leftVehicles
2. Decrement the phase's remaining green steps
3. If remaining steps = 0 → controller picks next phase (possibly via YELLOW)

Example: vehicle1 (south) + vehicle2 (north) both leave in step 1 because North+South are simultaneously green.


