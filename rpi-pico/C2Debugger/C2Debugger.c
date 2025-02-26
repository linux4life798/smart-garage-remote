#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/error.h"

/* IO map from faultier.h */
/* https://github.com/hextreeio/faultier/blob/main/faultier/faultier.h */

#define DISPLAY_SDA 18
#define DISPLAY_SCL 19

#define PIN_GATE 0
#define PIN_MUX0 1
#define PIN_MUX1 2
#define PIN_MUX2 3
#define PIN_EXT0 4
#define PIN_EXT1 27

#define PIN_MUX_MEASURE 26
#define PIN_CB_MEASURE 28

#define PIN_LED0 22
#define PIN_LED1 21
#define PIN_LED2 20

#define PIN_IO_BASE 5

// ADC channels
#define ADC_MUX_PIN 26
#define ADC_EXT_PIN 27
#define ADC_CB_PIN 28
#define ADC_MUX 0
#define ADC_EXT 1
#define ADC_CB 2

#define SWD_CLK 15
#define SWD_IO 16
#define SWD_RST 17

#define PIO_IRQ_TRIGGERED 0
#define PIO_IRQ_GLITCHED 1

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

/////////////////

// C2 registers
#define DEVICEID 0x00
#define REVID 0x01
#define FPCTL 0x02

// - Note that sleep_us(1) has jitter of about 200ns.
// - Note that using only internal pull-up takes about 12us for the signal to
//   fully rise to the max (about 2.8V  with pull-up), once put into input mode.
//   It takes about 5us to reach 1.65V.
// - Doing back-to-back gpio set 1 and then set 0, without a sleep_us delay
//   results in a very small almost sinusoidal signal at about 32MHz, IIRC.
static const uint64_t PULLUP_RAISE_DELAY_US = 20;

void c2_clock_pulse(void) {
    gpio_set_pulls(SWD_CLK, true, false);
    sleep_us(PULLUP_RAISE_DELAY_US);

    // C2CK low for 80ns-5μs
    gpio_put(SWD_CLK, 0);
    gpio_set_dir(SWD_CLK, GPIO_OUT);
    sleep_us(1);
    // C2CK high for >120ns
    gpio_put(SWD_CLK, 1); 
    sleep_us(1);
    gpio_set_dir(SWD_CLK, GPIO_IN);
}

void c2_reset(void) {
    // Ensure C2D is seen as disabled.
    gpio_set_dir(SWD_IO, GPIO_IN);
    gpio_set_dir(SWD_CLK, GPIO_IN);
    // Ensure the signal was high, before sending the low pulse.
    gpio_set_pulls(SWD_CLK, true, false);
    sleep_us(PULLUP_RAISE_DELAY_US);

    // Hold C2CK low for >20μs for reset
    gpio_put(SWD_CLK, 0);
    gpio_set_dir(SWD_CLK, GPIO_OUT);
    gpio_disable_pulls(SWD_CLK);
    sleep_us(25);
    // Return C2CK high
    gpio_put(SWD_CLK, 1);
    sleep_us(3);
    gpio_set_dir(SWD_CLK, GPIO_IN);
}

void c2_write_addr(uint8_t addr) {
    // START bit
    c2_clock_pulse();
    
    // INS field (11b for Address Write)
    gpio_set_dir(SWD_IO, GPIO_OUT);
    gpio_put(SWD_IO, 1);
    c2_clock_pulse();
    gpio_put(SWD_IO, 1);
    c2_clock_pulse();
    
    // Send address LSB first
    for(int i = 0; i < 8; i++) {
        gpio_put(SWD_IO, addr & 0x01);
        c2_clock_pulse();
        addr >>= 1;
    }
    
    // STOP bit
    gpio_set_dir(SWD_IO, GPIO_IN);
    c2_clock_pulse();
}

void c2_write_data(uint8_t data) {
    // START bit
    c2_clock_pulse();
    
    // INS field (01b for Data Write)
    gpio_set_dir(SWD_IO, GPIO_OUT);
    gpio_put(SWD_IO, 1);
    c2_clock_pulse();
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    
    // LENGTH field (00b for 1 byte)
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    
    // Send data LSB first
    for(int i = 0; i < 8; i++) {
        gpio_put(SWD_IO, data & 0x01);
        c2_clock_pulse();
        data >>= 1;
    }
    
    // Wait for ready (1 bit after any number of 0s)
    gpio_set_dir(SWD_IO, GPIO_IN);
    while(gpio_get(SWD_IO) == 0) {
        c2_clock_pulse();
    }
    
    // STOP bit
    c2_clock_pulse();
}

uint8_t c2_read_data(void) {
    uint8_t data = 0;
    
    // START bit
    c2_clock_pulse();
    
    // INS field (00b for Data Read)
    gpio_set_dir(SWD_IO, GPIO_OUT);
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    
    // LENGTH field (00b for 1 byte)
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    gpio_put(SWD_IO, 0);
    c2_clock_pulse();
    
    // Wait for ready
    gpio_set_dir(SWD_IO, GPIO_IN);
    while(gpio_get(SWD_IO) == 0) {
        c2_clock_pulse();
    }
    
    // Read 8 bits LSB first
    for(int i = 0; i < 8; i++) {
        c2_clock_pulse();
        data >>= 1;
        if(gpio_get(SWD_IO)) {
            data |= 0x80;
        }
    }
    
    // STOP bit
    c2_clock_pulse();
    
    return data;
}

void c2_disable() {
    // gpio_disable_pulls(SWD_CLK);
    // gpio_disable_pulls(SWD_IO);
    gpio_set_dir(SWD_CLK, false);
    gpio_set_dir(SWD_IO, false);
}

enum pico_error_codes c2_init(void) {
    // Initialize pins
    gpio_init(SWD_CLK);
    gpio_init(SWD_IO);
    gpio_disable_pulls(SWD_CLK);
    gpio_disable_pulls(SWD_IO);
    
    gpio_set_dir(SWD_CLK, false);
    gpio_set_dir(SWD_IO, false);
    gpio_put(SWD_CLK, true);
    
    // Reset device
    // c2_reset();
    
    // Initialize programming interface
    // c2_write_addr(FPCTL);
    // c2_write_data(0x02);
    // c2_write_data(0x04);
    // c2_write_data(0x01);
    // sleep_ms(20);
    return PICO_OK;
}

/////////////

enum pico_error_codes faultier_init() {
    gpio_init(PIN_LED0);
    gpio_init(PIN_LED1);
    gpio_init(PIN_LED2);
    gpio_init(PIN_MUX0);
    gpio_init(PIN_MUX1);
    gpio_init(PIN_MUX2);
    gpio_init(PIN_GATE);
    // gpio_init(SWD_CLK);
    // gpio_init(SWD_IO);
    gpio_init(SWD_RST);
    
    gpio_set_dir(PIN_LED0, true);
    gpio_set_dir(PIN_LED1, true);
    gpio_set_dir(PIN_LED2, true);
    gpio_set_dir(PIN_MUX0, true);
    gpio_set_dir(PIN_MUX1, true);
    gpio_set_dir(PIN_MUX2, true);
    gpio_set_dir(PIN_GATE, true);

    // gpio_set_dir(SWD_CLK, false);
    // gpio_set_dir(SWD_IO, false);
    // gpio_set_dir(SWD_RST, false);
    // gpio_disable_pulls(SWD_CLK);
    // gpio_disable_pulls(SWD_IO);
    // gpio_disable_pulls(SWD_RST);
    
    gpio_put(PIN_LED0, 0);
    gpio_put(PIN_LED1, 0);
    gpio_put(PIN_LED2, 0);
    gpio_put(PIN_MUX0, 0);
    gpio_put(PIN_MUX1, 0);
    gpio_put(PIN_MUX2, 0);
    gpio_put(PIN_GATE, 0);
    return PICO_OK;
}

void set_leds(uint8_t led_state) {
    gpio_put(PICO_DEFAULT_LED_PIN, !!(led_state & (1<<0)));
    gpio_put(PIN_LED0, !!(led_state & (1<<1)));
    gpio_put(PIN_LED1, !!(led_state & (1<<2)));
    gpio_put(PIN_LED2, !!(led_state & (1<<3)));
}

void update_leds(void) {
    static uint8_t led_state = 0;
    set_leds(led_state);
    // sleep_ms(LED_DELAY_MS);
    // if ((led_state % 50) == 0) {
    //     printf("Hello!\n");
    // }
    led_state++;
}

void fundamentals_test() {
    // gpio_set_dir(SWD_CLK, GPIO_OUT);
    // gpio_set_dir(SWD_IO, GPIO_OUT);
    // gpio_set_pulls(SWD_CLK, true, false);
    // gpio_set_pulls(SWD_IO, true, false);
    // gpio_set_dir(SWD_CLK, false);
    // gpio_set_dir(SWD_IO, false);
    // gpio_put(SWD_CLK, false);
    // gpio_put(SWD_IO, false);

    while(true) {
        c2_reset();
        c2_read_data();
        // gpio_put(SWD_CLK, true);
        // gpio_put(SWD_IO, true);
        // gpio_set_dir(SWD_CLK, false);
        sleep_ms(10);

        // sleep_ns(50);
        // gpio_put(SWD_IO, false);
        // gpio_set_dir(SWD_CLK, true);
        // gpio_put(SWD_CLK, false);
        // sleep_us(100);
        // sleep_ns(50);
        // sleep_us(1000);
        // gpio_put(SWD_CLK, false);
        // sleep_us(1000);
    }
}

void c2ddebug_test() {
    // Reset device
    c2_reset();
    
    // Read Device ID directly
    uint8_t device_id = c2_read_data();

    while(true) {
        update_leds();
        printf("First Device ID: 0x%02X\n", device_id);

        // Reset device
        c2_reset();
        c2_write_addr(DEVICEID);
        uint8_t new_device_id = c2_read_data();
        c2_write_addr(REVID);
        uint8_t new_rev_id = c2_read_data();
        printf("New Device ID:   0x%02X\n", new_device_id);
        printf("New Revision ID: 0x%02X\n", new_rev_id);
        sleep_ms(500);
        puts("");
    }
}


int main()
{
    hard_assert(c2_init() == PICO_OK);
    hard_assert(faultier_init() == PICO_OK);
    stdio_init_all();

    // fundamentals_test();
    c2ddebug_test();
}
