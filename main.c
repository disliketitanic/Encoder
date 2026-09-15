#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"


// ============================================================
// GPIO configuration
// ============================================================

// CLOCK:
// Pico GP2 -> CD40109 -> CD4021
#define PIN_CLOCK   2

// PARALLEL/SERIAL CONTROL:
// Pico GP3 -> CD40109 -> CD4021
#define PIN_PARALLEL_SERIAL_CONTROL 3

// DATA:
// CD4021 -> 39 kOhm / 13 kOhm divider -> Pico GP4
#define PIN_DATA    4


// ============================================================
// Measurement settings
// ============================================================

// Time between two measurements.
// 10 ms gives approximately 100 measurements per second.
#define MEASUREMENT_PERIOD_MS 10


// ============================================================
// Encoder settings
// ============================================================

// Number of encoder positions during one full revolution.
#define ENCODER_COUNTS_PER_REVOLUTION 1024

// Linear distance corresponding to one full revolution.
//
// Temporary value.
// Replace this value later with the experimentally measured
// distance traveled during one complete revolution.
#define METERS_PER_REVOLUTION 1.000f


// ============================================================
// Wrap detection settings
// ============================================================

// Thresholds used to detect crossing of the
// 0 / 1023 encoder boundary.
//
// Forward:
//
//     1023 -> 0
//
// Backward:
//
//     0 -> 1023
//
#define ENCODER_WRAP_HIGH_THRESHOLD 900
#define ENCODER_WRAP_LOW_THRESHOLD  100


// ============================================================
// Generate one clock pulse
// ============================================================

void clock_pulse(void)
{
    // Set CLOCK high
    gpio_put(PIN_CLOCK, 1);
    sleep_us(10);

    // Set CLOCK low
    gpio_put(PIN_CLOCK, 0);
    sleep_us(10);
}


// ============================================================
// Load encoder data into CD4021 in parallel mode
// ============================================================

void parallel_load(void)
{
    // PARALLEL/SERIAL CONTROL = 1
    // Parallel load mode.
    gpio_put(PIN_PARALLEL_SERIAL_CONTROL, 1);

    sleep_us(10);

    clock_pulse();
}


// ============================================================
// Switch CD4021 to serial mode
// ============================================================

void serial_mode(void)
{
    // PARALLEL/SERIAL CONTROL = 0
    // Serial shift mode.
    gpio_put(PIN_PARALLEL_SERIAL_CONTROL, 0);

    sleep_us(10);
}


// ============================================================
// Read one bit from CD4021
// ============================================================

uint8_t read_data_bit(void)
{
    return gpio_get(PIN_DATA);
}


// ============================================================
// Read complete 16-bit CD4021 chain
// ============================================================

uint16_t read_shift_frame(void)
{
    uint16_t EncoderShiftRegisterFrame = 0;

    for (int i = 0; i < 16; i++)
    {
        uint8_t ShiftRegisterBit = read_data_bit();

        EncoderShiftRegisterFrame =
            (EncoderShiftRegisterFrame << 1) | ShiftRegisterBit;

        clock_pulse();
    }

    return EncoderShiftRegisterFrame;
}


// ============================================================
// Main
// ============================================================

int main(void)
{
    // Initialize USB Serial
    stdio_init_all();


    // ========================================================
    // CLOCK
    // ========================================================

    gpio_init(PIN_CLOCK);
    gpio_set_dir(PIN_CLOCK, GPIO_OUT);
    gpio_put(PIN_CLOCK, 0);


    // ========================================================
    // PARALLEL/SERIAL CONTROL
    // ========================================================

    gpio_init(PIN_PARALLEL_SERIAL_CONTROL);
    gpio_set_dir(PIN_PARALLEL_SERIAL_CONTROL, GPIO_OUT);
    gpio_put(PIN_PARALLEL_SERIAL_CONTROL, 1);


    // ========================================================
    // DATA
    // ========================================================

    gpio_init(PIN_DATA);
    gpio_set_dir(PIN_DATA, GPIO_IN);
    gpio_disable_pulls(PIN_DATA);


    // ========================================================
    // Startup delay
    // ========================================================

    sleep_ms(2000);


    // ========================================================
    // Initial encoder position
    // ========================================================

    // Read the current physical position of the encoder.
    parallel_load();
    serial_mode();

    uint16_t EncoderShiftRegisterFrame = read_shift_frame();

    uint16_t AbsoluteEncoderPosition =
        EncoderShiftRegisterFrame & 0x03FF;


    // --------------------------------------------------------
    // The current physical encoder position is treated
    // as the starting zero position.
    // --------------------------------------------------------

    uint16_t InitialAbsoluteEncoderPosition =
        AbsoluteEncoderPosition;

    int32_t RevolutionCount = 0;

    int64_t TotalEncoderPosition = 0;

    float LinearDistanceMeters = 0.0f;


    // --------------------------------------------------------
    // Previous encoder position is required to detect
    // crossing of the 0 / 1023 boundary.
    // --------------------------------------------------------

    uint16_t PreviousAbsoluteEncoderPosition =
        AbsoluteEncoderPosition;


    // ========================================================
    // Startup message
    // ========================================================

    printf("\r\n");
    printf("Encoder reader started.\r\n");
    printf(
        "Initial AbsoluteEncoderPosition = %u\r\n",
        InitialAbsoluteEncoderPosition
    );
    printf("Initial position = 0\r\n");
    printf("\r\n");


    // ========================================================
    // Main measurement loop
    // ========================================================

    while (true)
    {
        // ----------------------------------------------------
        // Step 1:
        // Load current encoder position into CD4021.
        // ----------------------------------------------------

        parallel_load();

        // Switch to serial shift mode.
        serial_mode();


        // ----------------------------------------------------
        // Step 2:
        // Read complete 16-bit shift register chain.
        // ----------------------------------------------------

        EncoderShiftRegisterFrame =
            read_shift_frame();


        // ----------------------------------------------------
        // Step 3:
        // Extract the 10 encoder bits.
        // ----------------------------------------------------

        AbsoluteEncoderPosition =
            EncoderShiftRegisterFrame & 0x03FF;


        // ----------------------------------------------------
        // Step 4:
        // Detect a complete revolution.
        // ----------------------------------------------------
        //
        // Forward:
        //
        //     1023 -> 0
        //
        // means one clockwise revolution.
        //
        // Backward:
        //
        //     0 -> 1023
        //
        // means one counter-clockwise revolution.
        // ----------------------------------------------------

        if (PreviousAbsoluteEncoderPosition >=
                ENCODER_WRAP_HIGH_THRESHOLD &&
            AbsoluteEncoderPosition <=
                ENCODER_WRAP_LOW_THRESHOLD)
        {
            // Crossed 1023 -> 0.
            // Clockwise movement.
            RevolutionCount++;
        }
        else if (PreviousAbsoluteEncoderPosition <=
                     ENCODER_WRAP_LOW_THRESHOLD &&
                 AbsoluteEncoderPosition >=
                     ENCODER_WRAP_HIGH_THRESHOLD)
        {
            // Crossed 0 -> 1023.
            // Counter-clockwise movement.
            RevolutionCount--;
        }


        // ----------------------------------------------------
        // Step 5:
        // Calculate total encoder displacement.
        //
        // Formula:
        //
        // TotalEncoderPosition =
        //     RevolutionCount * 1024
        //     + AbsoluteEncoderPosition
        //     - InitialAbsoluteEncoderPosition
        // ----------------------------------------------------

        TotalEncoderPosition =
            (int64_t)RevolutionCount *
                ENCODER_COUNTS_PER_REVOLUTION
            + AbsoluteEncoderPosition
            - InitialAbsoluteEncoderPosition;


        // ----------------------------------------------------
        // Step 6:
        // Convert encoder displacement to meters.
        //
        // Formula:
        //
        // LinearDistanceMeters =
        //     TotalEncoderPosition
        //     * METERS_PER_REVOLUTION
        //     / 1024
        // ----------------------------------------------------

        LinearDistanceMeters =
            ((float)TotalEncoderPosition *
             METERS_PER_REVOLUTION)
            / ENCODER_COUNTS_PER_REVOLUTION;


        // ----------------------------------------------------
        // Step 7:
        // Output current state.
        // ----------------------------------------------------

        printf(
            "AbsoluteEncoderPosition = %u    "
            "TotalEncoderPosition = %lld    "
            "RevolutionCount = %ld    "
            "LinearDistanceMeters = %.3f m\r\n",
            AbsoluteEncoderPosition,
            (long long)TotalEncoderPosition,
            (long)RevolutionCount,
            LinearDistanceMeters
        );


        // ----------------------------------------------------
        // Step 8:
        // Save current position for the next iteration.
        // ----------------------------------------------------

        PreviousAbsoluteEncoderPosition =
            AbsoluteEncoderPosition;


        // Wait before the next measurement.
        sleep_ms(MEASUREMENT_PERIOD_MS);
    }


    return 0;
}