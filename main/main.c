#include <stdio.h>
#include "pico/stdlib.h"

const int ECHO_PIN = 6;
const int TRIG_PIN = 7;

volatile bool echo_got = false;
volatile uint32_t start_us = 0;
volatile uint32_t end_us = 0;

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == ECHO_PIN) {
        if (gpio_get(ECHO_PIN)) {
            start_us = time_us_32();
            echo_got = false;
        } else {
            end_us = time_us_32();
            echo_got = true;
        }
    }
}

void send_trig_pulse() {
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);
}

int main() {
    stdio_init_all();

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true, gpio_callback);

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    sleep_ms(1000);  // aguarda inicialização do sensor

    while (true) {
        send_trig_pulse();
        sleep_ms(100); // tempo para leitura do echo

        if (echo_got && end_us > start_us) {
            uint32_t delta_t = end_us - start_us;
            float distancia_cm = (float)delta_t * 0.017015f;  // fator para cm
            printf("Distância: %.2f cm\n", distancia_cm);
            echo_got = false;
        }

        sleep_ms(200);  // intervalo entre medições
    }

    return 0;
}
