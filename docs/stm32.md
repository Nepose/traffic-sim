## Embedded deployment (STM32)

The simulation core (`road`, `traffic_light`, `controller`, `intersection`) is fully platform-agnostic. Deploying on an STM32 requires only two additional files and a timer.

### Deployment modes

| Mode | Entry point | Input | Output |
|------|-------------|-------|--------|
| Desktop | `main.c` | stdin line protocol | stdout |
| Embedded | `hal_stm32.c` + timer ISR | GPIO sensors | GPIO lights |

### HAL interface (`include/hal.h`)

```c
typedef struct {
    bool (*sense_lane)(RoadDir road, Lane lane); /* read one sensor       */
    void (*set_light) (RoadDir road, LightState state); /* drive one light */
} EmbeddedHAL;
```

That is the entire porting surface. Swap in any implementation of these two functions to target a different MCU or sensor type.

### Embedded tick loop (`simulation.c`)

`SimulationContext` holds the intersection state, per-lane edge-detection flags, and a vehicle ID counter. `simulation_tick()` is the function called by the timer ISR every N seconds:

1. Poll all 12 lane sensors; enqueue one new vehicle per rising edge.
2. Call `intersection_step()` to advance the controller and lights.
3. Call `hal->set_light()` for each road to update the physical outputs.

Vehicles detected by sensor are added via `intersection_add_vehicle_by_lane()` rather than `intersection_add_vehicle()`. Since sensors know which lane is occupied but not the driver's destination, the destination is stored as `ROAD_NONE`; the controller and departure logic never read it.

### STM32 usage (`hal_stm32.c`)

Call once after CubeMX peripheral init:

```c
stm32_traffic_init();   /* initialises intersection + starts TIM2 */
```

Wire the timer callback:

```c
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) stm32_traffic_step();
}
```

`stm32_traffic_step()` calls `simulation_tick()` with the STM32 HAL struct. The timer period determines the real-time duration of one simulation step (e.g. 2 s/step gives `MIN_GREEN_STEPS x 2 s = 4 s` minimum green).

### GPIO assignment

**Light outputs** - 16 pins, push-pull, active high (one pin per state per road):

| Road | RED | YELLOW | GREEN | GREEN_ARROW |
|------|-----|--------|-------|-------------|
| North | PA0 | PA1 | PA2 | PA3 |
| South | PA4 | PA5 | PA6 | PA7 |
| East | PB0 | PB1 | PB2 | PB3 |
| West | PB4 | PB5 | PB6 | PB7 |

**Sensor inputs** - 12 pins, pull-down, active high (one pin per lane per road):

| Road | LANE_LEFT | LANE_STRAIGHT | LANE_RIGHT |
|------|-----------|---------------|------------|
| North | PC0 | PC1 | PC2 |
| South | PC3 | PC4 | PC5 |
| East | PC6 | PC7 | PC8 |
| West | PC9 | PC10 | PC11 |

Pin assignments are defined as lookup tables in `hal_stm32.c` and can be
changed without touching any other file.

---
