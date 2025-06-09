#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// Pinos
#define ECHO_PIN 6
#define TRIG_PIN 7
#define SERVO_PIN 15

// Variáveis do sensor ultrassônico
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

// PWM para servo
void setup_servo_pwm(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 64.f); // divisor de clock
    pwm_config_set_wrap(&config, 39062);  // 50 Hz
    pwm_init(slice, &config, true);
}

void set_servo_pulse(uint pin, float pulse_ms) {
    uint16_t level = (uint16_t)(pulse_ms * 1953.1f); // 39062 / 20ms
    pwm_set_gpio_level(pin, level);
}

int main() {
    stdio_init_all();

    // Inicia pinos do ultrassônico
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true, gpio_callback);

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    // Inicia servo
    setup_servo_pwm(SERVO_PIN);

    sleep_ms(1000); // aguarda inicialização

    while (true) {
        send_trig_pulse();
        sleep_ms(100); // tempo para o echo voltar

        if (echo_got && end_us > start_us) {
            uint32_t delta_t = end_us - start_us;
            float distancia_cm = (float)delta_t * 0.017015f;
            printf("Distância: %.2f cm\n", distancia_cm);
            echo_got = false;

            // Controle do servo com base na distância
            if (distancia_cm < 10.0f) {
                set_servo_pulse(SERVO_PIN, 2.0f); // gira em um sentido
            } else if (distancia_cm > 20.0f) {
                set_servo_pulse(SERVO_PIN, 1.0f); // gira em outro sentido
            } else {
                set_servo_pulse(SERVO_PIN, 1.5f); // parada
            }
        }

        sleep_ms(200); // intervalo entre medições
    }

    return 0;
}
