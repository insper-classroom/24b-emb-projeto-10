#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// === Pinos ===
#define ECHO_PIN 6
#define TRIG_PIN 7
#define SERVO_PIN 15

// === Variáveis do sensor ultrassônico ===
volatile bool echo_got = false;
volatile uint32_t start_us = 0;
volatile uint32_t end_us = 0;

bool bloqueado = false;

// === Interrupção do Echo ===
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

// Envia pulso de trigger
void send_trig_pulse() {
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);
}

// Setup do PWM no pino do servo
void setup_servo_pwm(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 64.f);      // Clock divisor
    pwm_config_set_wrap(&config, 39062);       // Frequência de 50Hz
    pwm_init(slice, &config, true);
}

// Define pulso do servo em milissegundos (1.0 a 2.0 normalmente)
void set_servo_pulse(uint pin, float pulse_ms) {
    uint16_t level = (uint16_t)(pulse_ms * 1953.1f); // 39062 / 20ms
    pwm_set_gpio_level(pin, level);
}

// === Função Principal ===
int main() {
    stdio_init_all();

    // Setup do sensor ultrassônico
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true, gpio_callback);

    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    // Setup do servo
    setup_servo_pwm(SERVO_PIN);
    sleep_ms(1000); // Aguarda estabilização

    // Começa rotacionando lentamente
    set_servo_pulse(SERVO_PIN, 1.7f);

    while (true) {
        send_trig_pulse();
        sleep_ms(100); // Espera resposta

        if (echo_got && end_us > start_us) {
            uint32_t delta_t = end_us - start_us;
            float distancia_cm = (float)delta_t * 0.017015f;
            echo_got = false;

            printf("Distância: %.2f cm\n", distancia_cm);

            if (distancia_cm < 10.0f) {
                if (!bloqueado) {
                    set_servo_pulse(SERVO_PIN, 1.5f); // parar
                    printf("BLOCK\n");
                    bloqueado = true;
                }
            } else {
                set_servo_pulse(SERVO_PIN, 1.7f); // rotação lenta
                bloqueado = false;
            }
        }

        sleep_ms(200); // Intervalo entre medições
    }

    return 0;
}
