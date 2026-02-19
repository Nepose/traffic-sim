/*
 * hal_stm32.c - STM32 HAL implementation for the traffic light controller
 *
 * Compile this file only for STM32 targets (not included in the desktop
 * CMakeLists.txt). Add it to your STM32CubeIDE / STM32CubeMX project
 * alongside the rest of the src/ files.
 *
 *
 * Initial GPIO assignment
 * -----------------------
 * Light outputs (4 roads x 4 states = 16 pins), all push-pull, initially low:
 *
 *   GPIOA: North and South
 *     PA0  North RED         PA4  South RED
 *     PA1  North YELLOW      PA5  South YELLOW
 *     PA2  North GREEN       PA6  South GREEN
 *     PA3  North GREEN_ARROW PA7  South GREEN_ARROW
 *
 *   GPIOB: East and West
 *     PB0  East  RED         PB4  West  RED
 *     PB1  East  YELLOW      PB5  West  YELLOW
 *     PB2  East  GREEN       PB6  West  GREEN
 *     PB3  East  GREEN_ARROW PB7  West  GREEN_ARROW
 *
 * Sensor inputs (4 roads x 3 lanes = 12 pins), pull-down, active high:
 *
 *   GPIOC:
 *     PC0  North LEFT    PC1  North STRAIGHT    PC2  North RIGHT
 *     PC3  South LEFT    PC4  South STRAIGHT    PC5  South RIGHT
 *     PC6  East  LEFT    PC7  East  STRAIGHT    PC8  East  RIGHT
 *     PC9  West  LEFT    PC10 West  STRAIGHT    PC11 West  RIGHT
 *
 * Timer setup
 * -----------
 *
 * Configure TIM2 in CubeMX for update interrupts at your desired step
 * interval (e.g. 2 s). In stm32xx_it.c:
 *
 *   void TIM2_IRQHandler(void) { HAL_TIM_IRQHandler(&htim2); }
 *
 * In your main or a dedicated callback override:
 *
 *   void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *       if (htim->Instance == TIM2) stm32_traffic_step();
 *   }
 *
 * Call stm32_traffic_init() once after MX_GPIO_Init() and MX_TIM2_Init().
 */

#include "hal.h"
#include "simulation.h"

/* STM32 HAL header - path depends on your MCU family. */
#include "stm32f4xx_hal.h"   /* change to stm32f1xx_hal.h, stm32l4xx_hal.h, etc. */

/*
 * GPIO lookup tables
 */

/* Light outputs: [road][LightState] -> (port, pin) */
static GPIO_TypeDef * const LIGHT_PORT[ROAD_COUNT][4] = {
    [ROAD_NORTH] = { GPIOA, GPIOA, GPIOA, GPIOA },
    [ROAD_SOUTH] = { GPIOA, GPIOA, GPIOA, GPIOA },
    [ROAD_EAST]  = { GPIOB, GPIOB, GPIOB, GPIOB },
    [ROAD_WEST]  = { GPIOB, GPIOB, GPIOB, GPIOB },
};

static const uint16_t LIGHT_PIN[ROAD_COUNT][4] = {
    [ROAD_NORTH] = { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3 },
    [ROAD_SOUTH] = { GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7 },
    [ROAD_EAST]  = { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3 },
    [ROAD_WEST]  = { GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7 },
};

/* Sensor inputs: [road][lane] -> (port, pin) */
static GPIO_TypeDef * const SENSE_PORT[ROAD_COUNT][LANES_PER_ROAD] = {
    [ROAD_NORTH] = { GPIOC, GPIOC, GPIOC },
    [ROAD_SOUTH] = { GPIOC, GPIOC, GPIOC },
    [ROAD_EAST]  = { GPIOC, GPIOC, GPIOC },
    [ROAD_WEST]  = { GPIOC, GPIOC, GPIOC },
};

static const uint16_t SENSE_PIN[ROAD_COUNT][LANES_PER_ROAD] = {
    [ROAD_NORTH] = { GPIO_PIN_0,  GPIO_PIN_1,  GPIO_PIN_2  },
    [ROAD_SOUTH] = { GPIO_PIN_3,  GPIO_PIN_4,  GPIO_PIN_5  },
    [ROAD_EAST]  = { GPIO_PIN_6,  GPIO_PIN_7,  GPIO_PIN_8  },
    [ROAD_WEST]  = { GPIO_PIN_9,  GPIO_PIN_10, GPIO_PIN_11 },
};

/*
 * EmbeddedHAL callbacks
 */

static bool stm32_sense_lane(RoadDir road, Lane lane) {
    return HAL_GPIO_ReadPin(SENSE_PORT[road][lane],
                            SENSE_PIN[road][lane]) == GPIO_PIN_SET;
}

static void stm32_set_light(RoadDir road, LightState state) {
    /* De-assert all four outputs for this road, then assert the active one.
     * This guarantees no two outputs are ever high at the same time,
     * regardless of the order individual writes take effect. */
    for (int s = 0; s < 4; s++) {
        HAL_GPIO_WritePin(LIGHT_PORT[road][s],
                          LIGHT_PIN[road][s],
                          GPIO_PIN_RESET);
    }
    HAL_GPIO_WritePin(LIGHT_PORT[road][state],
                      LIGHT_PIN[road][state],
                      GPIO_PIN_SET);
}

static const EmbeddedHAL HAL_STM32 = {
    .sense_lane = stm32_sense_lane,
    .set_light  = stm32_set_light,
};

/*
 * Lifecycle
 */

/* Declared extern in stm32xx_it.c or wherever the timer callback lives. */
extern TIM_HandleTypeDef htim2;

static SimulationContext sim_ctx;

void stm32_traffic_init(void) {
    simulation_init(&sim_ctx);
    HAL_TIM_Base_Start_IT(&htim2);
}

void stm32_traffic_step(void) {
    simulation_tick(&sim_ctx, &HAL_STM32);
}
