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
#define PIN_PAR_SER 3

// DATA:
// CD4021 -> 39 kOhm / 13 kOhm divider -> Pico GP4
#define PIN_DATA    4


// ============================================================
// Measurement settings
// ============================================================

// Time between two measurements.
// 10 ms gives approximately 100 measurements per second.
#define SAMPLE_PERIOD_MS 10


// ============================================================
// Generate one clock pulse for CD4021
// ============================================================

void clock_pulse(void)
{
    // Set CLOCK high
    gpio_put(PIN_CLOCK, 1);

    // Keep CLOCK high for 10 microseconds
    sleep_us(10);

    // Set CLOCK low
    gpio_put(PIN_CLOCK, 0);

    // Keep CLOCK low for 10 microseconds
    sleep_us(10);
}


// ============================================================
// Load encoder data into CD4021 in parallel mode
// ============================================================

void parallel_load(void)
{
    // PARALLEL/SERIAL CONTROL = 1
    //
    // According to the CD4021B datasheet:
    // 1 = parallel load
    // 0 = serial shift
    gpio_put(PIN_PAR_SER, 1);

    // Small delay before the clock pulse
    sleep_us(10);

    // Transfer the parallel encoder inputs into the
    // internal shift register of the CD4021 chain.
    clock_pulse();
}


// ============================================================
// Switch CD4021 to serial shift mode
// ============================================================

void serial_mode(void)
{
    // PARALLEL/SERIAL CONTROL = 0
    //
    // After this the data can be shifted out
    // bit by bit using the CLOCK signal.
    gpio_put(PIN_PAR_SER, 0);

    // Small delay after changing the control signal
    sleep_us(10);
}


// ============================================================
// Read one bit from CD4021
// ============================================================

uint8_t read_data_bit(void)
{
    // Read the current logic level on Pico GP4.
    //
    // GP4 receives the CD4021 output through
    // the 39 kOhm / 13 kOhm voltage divider.
    return gpio_get(PIN_DATA);
}


// ============================================================
// Read the complete 16-bit CD4021 shift chain
// ============================================================

uint16_t read_shift_frame(void)
{
    // Variable that will contain all 16 received bits.
    uint16_t frame = 0;


    // The system contains two 8-bit CD4021 registers:
    //
    // U1 = 8 bits
    // U2 = 8 bits
    //
    // Total:
    // 8 + 8 = 16 bits
    for (int i = 0; i < 16; i++)
    {
        // Read the current bit before generating
        // the next clock pulse.
        uint8_t bit = read_data_bit();


        // Shift the previously received bits one position left
        // and append the new bit at bit 0.
        //
        // Example:
        //
        // frame = 1010
        // bit   = 1
        //
        // result = 10101
        frame = (frame << 1) | bit;


        // Move the CD4021 shift registers to the next bit.
        clock_pulse();
    }


    // Return the complete 16-bit frame.
    return frame;
}


// ============================================================
// Main program
// ============================================================

int main(void)
{
    // Initialize the standard I/O system.
    //
    // In our project this is used for sending text
    // from Pico to the computer through USB.
    stdio_init_all();


    // ========================================================
    // CLOCK GPIO
    // ========================================================

    // Initialize GP2
    gpio_init(PIN_CLOCK);

    // GP2 is an output
    gpio_set_dir(PIN_CLOCK, GPIO_OUT);

    // Initial CLOCK state = LOW
    gpio_put(PIN_CLOCK, 0);


    // ========================================================
    // PARALLEL/SERIAL CONTROL GPIO
    // ========================================================

    // Initialize GP3
    gpio_init(PIN_PAR_SER);

    // GP3 is an output
    gpio_set_dir(PIN_PAR_SER, GPIO_OUT);

    // Initial state = parallel mode
    gpio_put(PIN_PAR_SER, 1);


    // ========================================================
    // DATA GPIO
    // ========================================================

    // Initialize GP4
    gpio_init(PIN_DATA);

    // GP4 is an input
    gpio_set_dir(PIN_DATA, GPIO_IN);

    // Disable Pico internal pull-up/pull-down resistors.
    //
    // The DATA signal already has an external voltage divider
    // connected to the CD4021 output.
    gpio_disable_pulls(PIN_DATA);


    // ========================================================
    // Startup delay
    // ========================================================

    // Give the system some time to initialize
    // before starting the measurement loop.
    sleep_ms(2000);


    // ========================================================
    // Main measurement loop
    // ========================================================

    while (true)
    {
        // ----------------------------------------------------
        // Step 1:
        // Load the current 10-bit encoder position
        // into the two CD4021 registers.
        // ----------------------------------------------------

        parallel_load();


        // ----------------------------------------------------
        // Step 2:
        // Switch the CD4021 chain to serial mode.
        // ----------------------------------------------------

        serial_mode();


        // ----------------------------------------------------
        // Step 3:
        // Read all 16 bits from the two CD4021 registers.
        // ----------------------------------------------------

        uint16_t frame = read_shift_frame();


        // ----------------------------------------------------
        // Step 4:
        // Extract the 10 bits used by the encoder.
        //
        // 0x03FF in binary:
        //
        // 0000001111111111
        //
        // Therefore only the lowest 10 bits remain.
        // ----------------------------------------------------

        uint16_t raw = frame & 0x03FF;


        // ----------------------------------------------------
        // Step 5:
        // Print the received data to the Serial Monitor.
        //
        // FRAME:
        // complete 16-bit frame in hexadecimal format.
        //
        // RAW:
        // 10-bit encoder value in decimal format.
        // ----------------------------------------------------

        printf(
            "FRAME = 0x%04X    RAW = %u\r\n",
            frame,
            raw
        );


        // ----------------------------------------------------
        // Step 6:
        // Wait before the next measurement.
        //
        // SAMPLE_PERIOD_MS = 10 ms
        // approximately 100 measurements per second.
        // ----------------------------------------------------

        sleep_ms(SAMPLE_PERIOD_MS);
    }


    return 0;
}