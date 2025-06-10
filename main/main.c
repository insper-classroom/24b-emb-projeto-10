#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/irq.h"

#define SERVO_PIN 15
#define ECHO_PIN 6
#define TRIG_PIN 7
#define AUDIO_IN_PIN 27
#define AUDIO_OUT_PIN 28

#define SAMPLE_RATE 8000
#define RECORD_TIME_SECONDS 3
#define AUDIO_SAMPLES (SAMPLE_RATE * RECORD_TIME_SECONDS)

char audio[AUDIO_SAMPLES];
int wav_position = 0;

// === Ultrassônico globals ===
volatile bool echo_got = false;
volatile uint32_t start_us = 0;
volatile uint32_t end_us = 0;
bool bloqueado = false;

// === PWM Servo Setup ===
void setup_servo_pwm(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 64.f);
    pwm_config_set_wrap(&config, 39062); // 50Hz
    pwm_init(slice, &config, true);
}

void set_servo_angle(uint pin, float angle_deg) {
    float pulse_ms = 1.0f + (angle_deg / 180.0f); // 1ms a 2ms
    uint16_t level = (uint16_t)(pulse_ms * 1953.1f);
    pwm_set_gpio_level(pin, level);
}

// === Ultrassônico ===
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

float medir_distancia_cm() {
    send_trig_pulse();
    sleep_ms(100);
    if (echo_got && end_us > start_us) {
        uint32_t delta_t = end_us - start_us;
        echo_got = false;
        return delta_t * 0.017015f;
    }
    return -1.0f;
}

// === Gravação e Reprodução ===
void adc_record_audio() {
    adc_select_input(AUDIO_IN_PIN - 26);
    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        uint16_t sample = adc_read();
        audio[i] = sample >> 4; // Escala para 8 bits
        sleep_us(1000000 / SAMPLE_RATE);
    }
}

void pwm_play_audio() {
    gpio_set_function(AUDIO_OUT_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(AUDIO_OUT_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255); // 8-bit sample
    pwm_config_set_clkdiv(&config, 1.0f);
    pwm_init(slice, &config, true);
    pwm_set_enabled(slice, true);

    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        pwm_set_gpio_level(AUDIO_OUT_PIN, audio[i]);
        sleep_us(1000000 / SAMPLE_RATE);
    }

    pwm_set_enabled(slice, false);
}

// === MAIN ===
int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(AUDIO_IN_PIN);

    // Setup ultrassônico
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, gpio_callback);
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    // Setup servo
    setup_servo_pwm(SERVO_PIN);
    sleep_ms(1000);

    int ang = 0;
    int dir = 1;
    bool ja_gravou = false;

    while (true) {
        float dist = medir_distancia_cm();
        if (dist > 0) {
            printf("Distância: %.2f cm\n", dist);
        }

        if (dist > 0 && dist < 10.0f) {
            bloqueado = true;
        } else {
            bloqueado = false;
            ja_gravou = false;
        }

        if (!bloqueado) {
            set_servo_angle(SERVO_PIN, ang);
            ang += 5 * dir;
            if (ang >= 180) dir = -1;
            else if (ang <= 0) dir = 1;
        }

        if (bloqueado && !ja_gravou) {
            printf("Objeto detectado! Gravando...\n");
            adc_record_audio();
            printf("Reproduzindo...\n");
            pwm_play_audio();
            ja_gravou = true;
        }

        sleep_ms(20);
    }

    return 0;
}
