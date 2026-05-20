# Project 1 | Server Room Monitoring System

This guide walks through the development of a server room temperature monitor on an STM32 microcontroller. The project is intentionally designed to demonstrate skills we can put on a CV and defend in a job interview, not just list. Every decision has a reason, and knowing those reasons is the path to get hired.

---

## The Project

A NUCLEO-F401RE reads temperature from a BMP280 sensor every second over I2C, streams readings over UART, and activates an alert LED when temperature exceeds a configurable threshold. FreeRTOS manages two independent tasks. The codebase has unit tests, MISRA C static analysis, a Docker build environment, and a GitHub Actions CI pipeline.

---

## Milestones

| # | Milestone | What we build |
|---|-----------|---------------|
| 1 | First Setup | UART boot message + LED heartbeat |
| 2 | Sensor driver | BMP280 over I2C, temperature over UART |
| 3 | RTOS | FreeRTOS tasks and inter-task queue |
| 4 | Quality | Unit tests (Unity) + static analysis (MISRA C) |
| 5 | DevOps | Docker build environment + GitHub Actions CI |

---

## Milestone 1 - First Setup: Getting the Board Talking

### Goal

By the end of this milestone the board sends a boot message over UART and blinks the onboard LED. No sensor yet. This may feel trivial, but it is not.

### Why start here

On any real embedded project, the first thing we want to do is establish a communication channel with the board. Before we have a sensor, before we have an RTOS, we need to know that:

1. The toolchain is installed and working
2. The board is alive and executing our code
3. We can observe what is happening

We start with exactly two peripherals: UART and a GPIO output (the onboard LED). Both are ideal for a first milestone because they require zero external hardware (the LED is already on the board, and UART routes through the ST-Link USB bridge), and they are fast to configure in CubeMX. Together they prove two things: the MCU can produce serial output and it can drive a digital output. If either fails, we know immediately where to look.

UART gives us a fast, low-overhead information channel. It is not a substitute for a debugger. A JTAG/SWD debugger with breakpoints and memory inspection is always the right tool for tracking down bugs. But for observing system behavior at runtime (boot sequence, sensor values, task activity), UART output is immediate, lightweight, and works without halting the CPU.

> **Interview angle:** "What is the first thing you do when starting a new embedded project?" The correct answer is: prove the board boots and establish a channel to observe runtime behavior. On the NUCLEO we do that with two peripherals: UART to send a boot message and a GPIO output to blink an LED. Both require zero external hardware, both take minutes to configure, and together they confirm the toolchain, the clock tree, and the code are all working before we touch anything more complex.

---

### Hardware choice

The right MCU for this project needs: a hardware I2C peripheral (for the BMP280 sensor), a hardware UART peripheral (for streaming readings), enough timers to avoid conflicts between the HAL and FreeRTOS, and enough CPU headroom to run an RTOS comfortably.

**This guide uses the STM32F401RE on a NUCLEO-F401RE as the reference example.** It satisfies all of those requirements: ARM Cortex-M4 at up to 84 MHz, hardware FPU, I2C, UART, and multiple independent timers. If you are using a different MCU or development board, the architecture and reasoning are identical; only the specific pin names and peripheral numbers will differ. Consult your board's schematic and reference manual for the equivalents.

The NUCLEO board is not the final hardware. Evaluation boards and sensor modules exist to prototype and validate firmware on the target MCU before committing to a custom PCB (or while it's being designed), not for building products. That is exactly how they are used in professional embedded development: bring up the software on the eval board, prove it works, then hand off to hardware for the custom design.

The practical advantages of any NUCLEO-style development board for this phase:

- It has an ST-Link debugger built in; we can flash and debug over USB without buying separate hardware
- The ST-Link also exposes a virtual COM port over the same USB cable, so UART output is visible immediately on our laptop with any serial terminal
- It eliminates hardware variables while we develop firmware; any bug we find is a software bug, not a PCB routing issue

> **Interview angle:** MCU selection is driven by peripheral requirements, not familiarity. For this system we need I2C, UART, and enough timers to support both the HAL and an RTOS without conflicts. We use an evaluation board because that is the standard way to validate firmware before a custom PCB exists.

---

### Toolchain: what we are installing and why

We need four things:

**gcc-arm-none-eabi** is the cross-compiler. "Cross" means it runs on our laptop but produces code for the ARM target. `arm-none-eabi` means ARM architecture, no operating system (bare metal), using the Embedded ABI.

**CMake + Ninja** is the build system. We will not use IDE project files. Reasons: IDE project files are not readable in code review, they do not reproduce reliably on other machines, and they cannot be used in a CI pipeline. CMake is the industry standard for C/C++ builds.

**STM32CubeMX** is a GUI tool from ST that generates the peripheral initialization code (clocks, GPIO, UART, I2C) and the CMake project structure. It is used in industry because writing clock configuration registers by hand is error-prone and undifferentiated work.

**A serial terminal** (e.g. minicom, screen, or MobaXterm) to read UART output from the board.

> **Interview angle:** We chose CMake over IDE-generated project files deliberately, because CMake integrates with CI systems and is toolchain-agnostic. CubeMX generates the repetitive setup code for us automatically, so we can focus on writing the actual application logic.

---

### STM32CubeMX: what to configure and why

> **Note:** The specific pin names (PA2, PA3, PA5) and timer (TIM11) below are specific to the NUCLEO-F401RE. If you are using a different board, check your board's schematic for the UART pins connected to the ST-Link bridge, the pin driving the onboard LED, and which timers are available for the HAL timebase. We learned all of this in the Embedded Roadmap.

Open CubeMX and configure:

**USART2 on PA2/PA3 at 115200 baud, 8N1.** PA2 and PA3 are connected to the ST-Link UART bridge on the NUCLEO board; this is the virtual COM port. 115200 is fast enough for debug output and slow enough to be reliable.

**PA5 as GPIO Output.** PA5 is the onboard LED (LD2). Using the onboard LED means we need zero external hardware for the first milestone.

**TIM11 as HAL timebase source** (instead of SysTick). This is not obvious. The default HAL timebase is SysTick. Later, FreeRTOS will also use SysTick as its tick source. Two things cannot share the same hardware timer. Moving HAL's timebase to TIM11 now avoids a conflict we would otherwise only discover in Milestone 3.

> **Interview angle:** Proactively moving the HAL timebase shows we understand how FreeRTOS and the HAL interact at the hardware level, and that we planned the architecture before writing code.

**Generate CMake project** (not Makefile, not IDE project).

---

### The HAL: why use it

The STM32 HAL (Hardware Abstraction Layer) is a library from ST that wraps peripheral register access behind C functions like `HAL_UART_Transmit()` and `HAL_GPIO_TogglePin()`.

The alternative is writing directly to hardware registers:
```c
// Bare metal: toggle PA5
GPIOA->ODR ^= (1 << 5);

// HAL equivalent
HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
```

For this project, HAL is the right choice because:
- It handles the non-trivial initialization details (clock gating, alternate function mapping, etc.)
- It is portable across the STM32 family
- It is what ST-based industrial code uses in practice, because it's faster and reliable

We give up some performance and some transparency in exchange for development speed and maintainability. That trade-off is appropriate for a system where the bottleneck is I2C communication at 100 kHz, not GPIO toggle speed.

> **Interview angle:** We can explain the HAL trade-off clearly. We know what is happening at the register level, but we chose HAL deliberately for known reasons. This is different from "I used HAL because that's what the tutorial used".

---

### Writing the code

**The boot message:**

```c
const char *boot_msg = "Server Room Monitor - Boot OK\r\n";
HAL_UART_Transmit(&huart2, (uint8_t *)boot_msg, strlen(boot_msg), HAL_MAX_DELAY);
```

`HAL_UART_Transmit` is blocking: it waits until all bytes are sent before returning. This is fine for a one-time boot message. For continuous streaming later, we will want to reconsider. `HAL_MAX_DELAY` means wait forever if the peripheral is busy. Acceptable here, but problematic in an RTOS context.

**The LED blink in the main loop:**

```c
while (1) {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(500);
}
```

`HAL_Delay` uses the HAL timebase (now TIM11). It blocks the CPU. In a single-task system with no concurrency requirements this is perfectly adequate. In Milestone 3 it will be replaced by `osDelay`, which yields the CPU to other tasks.

---

### What we can now defend in an interview

- Why we first use an evaluation board and sensor modules
- Why we chose this board
- What a cross-compiler is and why we need one
- Why CMake instead of IDE project files
- What the HAL is and why use it
- Why we moved the HAL timebase to TIM11
- The difference between blocking and non-blocking UART
- Why establishing UART communication is the first step on any embedded project

---

## Milestone 2 - Sensor Driver: Reading the BMP280

### Goal

By the end of this milestone the board reads temperature from the BMP280 sensor over I2C and prints the readings over UART. We now have a system that does something real.

### Why now

Milestone 1 proved three things: the toolchain works, the board boots, and we can observe runtime behavior over UART. That foundation was not optional. Without a reliable way to see what is happening, adding a sensor would just add confusion. We would not know whether a problem was in the sensor code, the I2C peripheral, or the UART output.

Now that we have that channel, adding the sensor is the natural next step.

### What is I2C and why we use it

The BMP280 supports both I2C and SPI. We use I2C because it requires only two wires, most breakout boards include the required pull-up resistors, and 100 kHz is more than fast enough for a sensor we read once per second.

> **Interview angle:** I2C vs SPI is a standard question. I2C needs fewer wires and is simpler to connect, but it is slower and does not support full-duplex transfers. SPI is faster and full-duplex, but uses more pins. For a low-speed sensor polled once per second, I2C is the right choice.

### The BMP280 sensor

The BMP280 is a barometric pressure and temperature sensor from Bosch. It is accurate, well documented, available on cheap breakout boards, and used in real products. We use it for temperature only; we configure the pressure oversampling to minimum and ignore the pressure output.

Two details about the BMP280 that matter for the driver:

**Calibration data.** The sensor stores individual calibration coefficients in its non-volatile memory. Every unit from the factory has slightly different values. We must read those coefficients on startup and use them in the compensation formula. A driver that skips this step will produce wrong readings on every physical unit.

**The compensation formula.** The BMP280 gives us a raw ADC value, not a temperature in degrees. The datasheet provides a specific integer formula to convert the raw value using the calibration coefficients. We implement that formula exactly as specified.

> **Interview angle:** "Walk me through how you would write a driver for a new sensor." The answer has two parts: first, read the datasheet to understand the startup sequence, including any calibration data the sensor needs on initialization, instead of search tutorials; second, implement the conversion from raw ADC values to engineering units. For the BMP280 that means reading factory calibration coefficients and applying the compensation formula the datasheet specifies. The key point is that you read the datasheet first and understand what the sensor actually gives you before writing a single line of code.

### Why we write our own driver

In industry it is common for teams to maintain their own drivers for sensors and other components, versioned as git submodules inside the firmware repository. This gives the team full control over the code: no unexpected changes from an upstream library, no dependencies that cannot be audited, and no constraints on how the driver is structured internally.

We follow the same practice here. Writing the driver ourselves gives us two things a third-party library cannot.

First, we understand every line of it and can answer questions about it in an interview.

Second, we can structure it so the core logic has no HAL dependencies. The compensation formula is pure arithmetic. It does not call any HAL function. That makes it directly testable on a laptop in Milestone 4, without hardware. A third-party library written as a monolith cannot be separated this way.

### Driver structure

We split the driver across `bmp280.h` and `bmp280.c`.

The calibration data structure holds the three temperature coefficients read from the sensor:

```c
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
} BMP280_CalibData;
```

The public interface:

```c
HAL_StatusTypeDef BMP280_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef BMP280_ReadCalibration(I2C_HandleTypeDef *hi2c, BMP280_CalibData *calib);
HAL_StatusTypeDef BMP280_ReadTemperature(I2C_HandleTypeDef *hi2c, const BMP280_CalibData *calib, int32_t *temp_x100);
```

`BMP280_Init` configures the sensor into normal mode with temperature and pressure oversampling. `BMP280_ReadCalibration` reads the six calibration bytes and parses them into the struct. `BMP280_ReadTemperature` reads the raw ADC bytes and calls the compensation formula.

Temperature is returned as `int32_t temp_x100`: an integer representing degrees Celsius multiplied by 100. A value of 2508 means 25.08 C. This avoids floating-point arithmetic in the driver itself.

The compensation formula is isolated in its own function with no HAL dependencies:

```c
int32_t BMP280_CompensateTemp(const BMP280_CalibData *calib, int32_t adc_T) {
    int32_t var1 = ((adc_T >> 3) - ((int32_t)calib->dig_T1 << 1));
    var1 = (var1 * (int32_t)calib->dig_T2) >> 11;

    int32_t var2 = ((adc_T >> 4) - (int32_t)calib->dig_T1);
    var2 = (((var2 * var2) >> 12) * (int32_t)calib->dig_T3) >> 14;

    return ((var1 + var2) * 5 + 128) >> 8;
}
```

This is the Bosch formula from the BMP280 datasheet. The bit shifts and integer arithmetic are not arbitrary; they are the fixed-point equivalent of the floating-point formula, chosen to avoid overflow on a 32-bit MCU.

> **Note:** The I2C address used here (`0x77 << 1`) depends on how the SDO pin is wired on your breakout board. If the sensor does not respond, try `0x76 << 1` instead.

### What we can now defend in an interview

- When to use I2C vs SPI and how to make that decision
- How to approach writing a driver from a datasheet
- Why sensor calibration data matters and what happens if you ignore it
- When to write your own driver vs using a third-party library
- Why embedded drivers often use integer arithmetic instead of floating point
- How to structure a driver so it can be tested without hardware

---

## Milestone 3 - RTOS: Adding FreeRTOS

### Goal

By the end of this milestone the system runs two independent FreeRTOS tasks. A sensor task reads the BMP280 every second and puts the temperature on a queue. A logger task receives from the queue, prints the reading over UART, and controls the alert LED based on the threshold.

### Why now

At the end of Milestone 2 the system works correctly, but it has a structural problem. Everything runs in a single while(1) loop with blocking calls:

```c
while (1) {
    BMP280_ReadTemperature(&hi2c1, &bmp280_calib, &temp_x100);
    HAL_UART_Transmit(...);
    HAL_Delay(1000);
}
```

`HAL_Delay(1000)` stops the CPU completely for one second. In a system with a single responsibility that is acceptable. But as soon as we add a second requirement, the blocking loop breaks down. If the alert LED needs to react within 100 ms and we are inside a 1000 ms delay, the LED is late by up to 900 ms.

An RTOS solves this by running each responsibility as an independent task. Each task has its own stack and its own loop. The scheduler switches between them, and each one runs as if it were the only thing on the CPU.

> **Interview angle:** "When do you need an RTOS?" When you have two or more things with different timing requirements that should not block each other. A superloop with blocking delays couples everything together. An RTOS decouples tasks so each one can meet its own timing requirements independently.

### Why FreeRTOS

FreeRTOS is the most widely deployed RTOS in embedded systems. It is open source, well documented, supported by CubeMX, and used in production across industrial, medical, and consumer products. It is not the only option, but it is the one most likely to appear in a job interview.

We use the CMSIS-RTOS v2 API (`osDelay`, `osMessageQueuePut`, `osMessageQueueGet`) rather than the raw FreeRTOS API (`vTaskDelay`, `xQueueSend`). CMSIS-RTOS v2 is a standard wrapper that sits on top of FreeRTOS. It makes the application code independent of the underlying RTOS kernel.

> **Interview angle:** "What is the difference between the FreeRTOS API and the CMSIS-RTOS API?" FreeRTOS functions use the `x`/`v` prefix (`xQueueSend`, `vTaskDelay`). CMSIS-RTOS v2 wraps them behind `os`-prefixed functions (`osMessageQueuePut`, `osDelay`) that are portable across RTOS kernels. Using CMSIS-RTOS avoids tying the application code to one specific kernel.

### The SysTick conflict: already solved

FreeRTOS uses SysTick as its tick source. The HAL also uses SysTick for `HAL_Delay` and peripheral timeouts by default. Two things cannot share one hardware timer.

We moved the HAL timebase to TIM11 in Milestone 1. This is exactly why we did it then and not now. Had we left it on SysTick, Milestone 3 would start with a system that compiles and flashes but behaves incorrectly in ways that are hard to diagnose.

### The design: two tasks, one queue

We use two tasks connected by a single queue.

**SensorTask** runs on a fixed 1000 ms interval. It reads the BMP280 and puts the result on the queue. It does not care what happens to the data after that.

**LoggerTask** blocks indefinitely waiting for data on the queue. When data arrives, it formats and prints the reading over UART, then checks the threshold and updates the alert LED.

The queue decouples the two tasks. The sensor task does not know what the logger task will do with the data. The logger task does not know how or when the reading was taken. Each task has exactly one responsibility.

> **Interview angle:** "Why a queue and not a global variable?" A global variable shared between two tasks is a race condition. Even reading a 32-bit integer is not guaranteed to be atomic on all ARM Cortex-M configurations. A FreeRTOS queue is thread-safe by design; it handles the mutual exclusion internally. It is the correct primitive for passing data between tasks.

### The task implementation

```c
#define TEMP_ALERT_THRESHOLD_x100  3000   /* 30.00 C */

void SensorTask(void *argument) {
    (void)argument;
    int32_t temp_x100 = 0;

    for (;;) {
        BMP280_ReadTemperature(&hi2c1, &bmp280_calib, &temp_x100);
        osMessageQueuePut(temp_queue, &temp_x100, 0, 0);
        osDelay(1000);
    }
}

void LoggerTask(void *argument) {
    (void)argument;
    int32_t temp_x100 = 0;
    char msg[64];

    for (;;) {
        osMessageQueueGet(temp_queue, &temp_x100, NULL, osWaitForever);

        (void)snprintf(msg, sizeof(msg), "[TEMP] %ld.%02ld C\r\n",
                (long)(temp_x100 / 100), (long)(temp_x100 % 100));
        HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);

        if (temp_x100 >= TEMP_ALERT_THRESHOLD_x100) {
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
        }
    }
}
```

`osDelay(1000)` yields the CPU to other runnable tasks for 1000 ms. `HAL_Delay(1000)` would spin the CPU without yielding. This is the fundamental behavioral difference between the two.

`osMessageQueueGet` with `osWaitForever` puts the logger task to sleep until data is available. It consumes zero CPU while waiting.

Note the temperature formatting: we print `temp_x100 / 100` as the integer part and `temp_x100 % 100` as the decimal part. No floating-point formatting is needed in the UART output, which keeps the code simpler and avoids pulling in the `printf` floating-point library.

### What we can now defend in an interview

- Why a superloop with blocking delays does not scale
- What an RTOS gives us that a superloop does not
- Why we chose FreeRTOS and why we use the CMSIS-RTOS API
- The SysTick conflict and why we resolved it in Milestone 1 rather than now
- Why a queue is safer than a global variable for inter-task communication
- The difference between `osDelay` and `HAL_Delay`
- How the two-task design separates concerns

---

## Milestone 4 - Quality: Unit Tests and Static Analysis

### Goal

By the end of this milestone the project has a suite of unit tests for the BMP280 driver and a static analysis pass using cppcheck with MISRA C rules. Both run from the command line and produce a clear pass or fail result.

### Why now

We have a working, tested-on-hardware system. Before we automate the build in Milestone 5, we need something worth automating. A CI pipeline that only checks "does it compile" is not very valuable. One that also runs logic tests and checks code quality is.

There is a second reason to add the tests before the pipeline. If we add tests and CI at the same time and something fails, we cannot tell whether the problem is in the test logic or in the pipeline configuration. Separating the two milestones keeps failures traceable.

### The challenge of testing embedded code

Our code runs on a microcontroller. The BMP280 is physically connected over I2C. The HAL reads real hardware registers. How do we run tests on a laptop where there is no MCU and no sensor?

The answer is to separate the code into two categories.

**Pure logic:** the compensation formula and the calibration parser. These functions take inputs, do arithmetic, and return outputs. They have no HAL calls and no hardware dependencies. They can run on any machine.

**Hardware-dependent code:** anything that calls the HAL directly (`HAL_I2C_Mem_Read`, `HAL_UART_Transmit`, etc.). This code cannot run on a laptop as-is.

We test the pure logic directly. For the hardware-dependent paths we use stubs: simple replacement functions that behave like the HAL without touching any hardware.

This split was not accidental. When we structured the driver in Milestone 2, we isolated `BMP280_CompensateTemp` and `BMP280_ParseCalibration` as standalone functions with no HAL dependencies. We did that precisely so they could be tested here.

> **Interview angle:** "How do you test embedded code without the hardware?" You split the code into pure logic and hardware-dependent code. The pure logic is tested directly. The hardware-dependent code is tested through stubs that replace the HAL. This separation needs to be designed in from the start; it is hard to add later.

### Two test suites

We have two test files.

**`test_bmp280_pure.c`** tests the functions that have no hardware dependency. These link directly against our driver source with a native C compiler. No stubs needed.

```c
void test_CompensateTemp_returns_expected_value(void) {
    BMP280_CalibData calib = {
        .dig_T1 = 27504,
        .dig_T2 = 26435,
        .dig_T3 = -1000
    };
    int32_t adc_T = 519888;

    int32_t result = BMP280_CompensateTemp(&calib, adc_T);

    TEST_ASSERT_INT32_WITHIN(50, 2508, result);
}
```

The calibration values and raw ADC input come from the BMP280 datasheet example. The expected output (2508, meaning 25.08 C) is the value the datasheet says the formula should produce. This test verifies our implementation of the Bosch formula is correct.

**`test_bmp280_i2c.c`** tests the HAL-dependent functions using the I2C stub. The stub intercepts the HAL calls and returns pre-programmed data:

```c
void test_ReadCalibration_parses_i2c_data_correctly(void) {
    uint8_t raw[6] = { 0x70, 0x6B, 0x43, 0x67, 0xD0, 0x01 };
    HAL_I2C_Stub_SetReadData(raw, sizeof(raw));

    BMP280_CalibData calib;
    HAL_StatusTypeDef ret = BMP280_ReadCalibration(&hi2c_stub, &calib);

    TEST_ASSERT_EQUAL(HAL_OK, ret);
    TEST_ASSERT_EQUAL_UINT16(0x6B70, calib.dig_T1);
}
```

We also test the error path: what happens when the I2C read returns `HAL_ERROR`. This is easy with a stub; it would require physically breaking the wiring to test on real hardware.

### The HAL stub

The stub lives in `tests/stubs/hal_i2c_stub.c`. It replaces the real HAL I2C functions at link time:

```c
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize,
                                    uint8_t *pData, uint16_t Size, uint32_t Timeout) {
    (void)hi2c; (void)DevAddress; (void)MemAddress;
    (void)MemAddSize; (void)Timeout;
    memcpy(pData, stub_buf, Size);
    return stub_ret;
}
```

The test controls what data the stub returns and whether it reports success or failure. This lets us test every code path in the driver, including the ones that only trigger on hardware errors.

### Unity test framework

Unity is a test framework designed for embedded systems. It has no external dependencies, runs on any platform, and is itself written following MISRA C guidelines. A passing run looks like:

```
test_bmp280_pure.c:17:test_ParseCalibration_correctly_parses_little_endian:PASS
test_bmp280_pure.c:31:test_CompensateTemp_returns_expected_value:PASS
-----------------------
2 Tests 0 Failures 0 Ignored
OK
```

A non-zero exit code on failure is what lets a CI pipeline detect test failures automatically.

### Static analysis with cppcheck and MISRA C

MISRA C is a set of coding guidelines originally written for automotive safety-critical software. It restricts or bans C language features that are known to cause bugs: implicit type conversions, unreachable code, uninitialized variables, and similar issues.

We do not claim full MISRA C compliance. Full compliance is a formal process used in certified projects (IEC 61508, ISO 26262). What we do is use cppcheck to enforce a practical subset of MISRA C rules. This is a common and legitimate approach for projects that want the safety benefits without the certification overhead.

```bash
cppcheck --enable=warning,style,performance,portability \
         --addon=misra \
         --error-exitcode=1 \
         --suppressions-list=cppcheck-suppressions.txt \
         -I Core/Inc \
         Core/Src/bmp280.c Core/Src/app_tasks.c Core/Src/main.c
```

`--error-exitcode=1` makes cppcheck return a non-zero exit code when it finds issues. That is what CI pipelines check. `--suppressions-list` points to a file that lists known, justified deviations. Any suppression in that file should have a comment explaining why it is acceptable.

> **Interview angle:** "What is MISRA C and why do you use it?" MISRA C is a set of coding rules that restricts the dangerous parts of C. We use a subset enforced by a tool like cppcheck. The point is not compliance for its own sake; it is catching a class of bugs that are hard to find in code review and that tend to appear in production under edge cases, so our code is safer.

### What we can now defend in an interview

- Why we added tests before the CI pipeline, not at the same time
- How to test embedded code on a host machine
- The difference between pure logic tests and stub-based tests
- What Unity is and why it is appropriate for embedded systems
- How the HAL stub works and what it lets us test that hardware cannot
- What MISRA C is and the difference between full compliance and a practical subset
- Why `--error-exitcode=1` matters for CI integration

---

## Milestone 5 - DevOps: Docker and GitHub Actions

### Goal

By the end of this milestone the project builds inside a Docker container and a GitHub Actions pipeline runs automatically on every push: it builds the firmware, runs the unit tests, and runs the static analysis.

### Why now

Milestones 1 through 4 produced a working, tested, statically-analyzed codebase. Now we automate it.

The value of CI is not the automation itself. The value is that it runs on a clean machine with a fixed toolchain version on every change. A developer joining the project six months from now gets the same build result. A reviewer looking at a pull request knows whether the tests passed before reading a single line of code.

Without Docker, CI pipelines have a well-known failure mode: the build works today, but when the CI runner is updated or the toolchain package version changes, something breaks. Docker eliminates this by capturing the exact environment.

> **Interview angle:** "Why use Docker for an embedded build?" Embedded toolchains have breaking changes between versions. A project that builds with `gcc-arm-none-eabi` 10.3 might not build cleanly with 12.2. Docker pins the toolchain version. The build that works today will work in a year, on any machine, because the environment does not change.

### The Dockerfile

```dockerfile
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    cmake \
    ninja-build \
    build-essential \
    cppcheck \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
```

We use `ubuntu:22.04` as the base image because its package versions are pinned to the Ubuntu 22.04 LTS release. `--no-install-recommends` keeps the image small by skipping packages that are not strictly required. `rm -rf /var/lib/apt/lists/*` clears the package index cache after installation to reduce the final image size.

The image installs everything needed to build the firmware and run the tests:

- `gcc-arm-none-eabi`, `binutils-arm-none-eabi`, `libnewlib-arm-none-eabi` for cross-compilation
- `cmake` and `ninja-build` for the build system
- `build-essential` for compiling the native unit tests on the host
- `cppcheck` and `python3` for static analysis (the MISRA addon requires Python)

### The GitHub Actions pipeline

The pipeline is in `.github/workflows/ci.yml`. It runs on every push to `main` and on every pull request. It has three independent jobs.

**Firmware** builds the STM32 binary inside the Docker container:

```yaml
- name: Build firmware
  run: |
    docker run --rm -v "$PWD":/workspace -w /workspace stm32-monitor-build \
      cmake --preset Debug
    docker run --rm -v "$PWD":/workspace -w /workspace stm32-monitor-build \
      cmake --build --preset Debug
```

**Tests** builds and runs the unit test suite and publishes the results as a check on the pull request:

```yaml
- name: Build and run tests
  run: |
    docker run --rm -v "$PWD":/workspace -w /workspace stm32-monitor-build bash -c \
      "cmake -G Ninja -B tests/build -S tests \
       && cmake --build tests/build \
       && cd tests/build && ctest --output-junit test-results.xml"

- name: Publish test results
  uses: EnricoMi/publish-unit-test-result-action@v2
  if: always()
  with:
    files: tests/build/test-results.xml
```

**Static analysis** runs cppcheck with the MISRA addon:

```yaml
- name: Run cppcheck with MISRA
  run: |
    docker run --rm -v "$PWD":/workspace -w /workspace stm32-monitor-build \
      cppcheck --enable=warning,style,performance,portability \
        --addon=misra \
        --error-exitcode=1 \
        --suppressions-list=cppcheck-suppressions.txt \
        -I Core/Inc \
        Core/Src/bmp280.c Core/Src/app_tasks.c Core/Src/main.c
```

The three jobs are separate deliberately. If the firmware fails to build, we still want to know whether the unit tests pass (they compile and run on the host, not the target). Separate jobs give us that information. A pull request page shows three independent status checks, not one combined result.

The Docker image is built once per job using `docker/build-push-action` and cached by GitHub Actions with `cache-from: type=gha`. Subsequent runs reuse the cached layers and only rebuild the layers that changed, which keeps pipeline times short.

### The complete picture

At this point the project is complete. Every push to the repository triggers:

1. A firmware build for the STM32 target
2. A unit test run on the host with results published to the pull request
3. A MISRA C static analysis check

The codebase covers the full stack of a professional embedded firmware project: hardware driver, RTOS integration, host-based unit tests, static analysis, a reproducible build environment, and automated CI. Not every employer uses exactly these tools, but every employer who builds embedded systems will recognize this as a serious approach.

### What we can now defend in an interview

- What Docker is and why it matters for embedded builds specifically
- How toolchain version pinning prevents build drift over time
- What each job in the CI pipeline does and why the three jobs are separate
- How the Docker layer cache works in GitHub Actions
- Why a CI pipeline that only checks compilation is not enough
- The complete architecture of the project from MCU peripheral to CI pipeline
