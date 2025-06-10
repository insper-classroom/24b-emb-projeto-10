#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define SERVO_PIN        15
#define ECHO_PIN         6
#define TRIG_PIN         7
#define AUDIO_IN_PIN     27
#define AUDIO_OUT_PIN    28
#define LED_BLOCK_PIN    14

#define SAMPLE_RATE      8000
#define RECORD_TIME_S    3
#define AUDIO_SAMPLES   (SAMPLE_RATE * RECORD_TIME_S)

typedef struct {
    float distancia_cm;
} UltrassomMsg_t;

typedef struct {
    bool start_record;
} AudioMsg_t;

static QueueHandle_t qUltrassom;
static QueueHandle_t qAudio;

static void gpio_callback(uint gpio, uint32_t events) {
    static uint32_t start_us;
    static bool pulse_on = false;

    if (gpio == ECHO_PIN) {
        if ((events & GPIO_IRQ_EDGE_RISE) && !pulse_on) {
            start_us = time_us_32();
            pulse_on = true;
        }
        else if ((events & GPIO_IRQ_EDGE_FALL) && pulse_on) {
            uint32_t delta = time_us_32() - start_us;
            pulse_on = false;

            UltrassomMsg_t msg = {
                .distancia_cm = delta * 0.017015f
            };
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(qUltrassom, &msg, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

static void vTaskUltrassom(void *pvParameters) {
    UltrassomMsg_t rx;
    for (;;) {
        // envia trigger
        gpio_put(TRIG_PIN, 1);
        sleep_us(10);
        gpio_put(TRIG_PIN, 0);

        // espera resposta (timeout 100 ms)
        if (xQueueReceive(qUltrassom, &rx, pdMS_TO_TICKS(100))) {
            // coloca na fila de áudio se bloqueio
            if (rx.distancia_cm > 0 && rx.distancia_cm < 10.0f) {
                xQueueSend(qAudio, &(AudioMsg_t){ .start_record = true }, 0);
                gpio_put(LED_BLOCK_PIN, 1);
            } else {
                gpio_put(LED_BLOCK_PIN, 0);
            }
            // imprime sempre
            printf("Distância: %.2f cm\n", rx.distancia_cm);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void vTaskServo(void *pvParameters) {
    // configura PWM do servo
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 64.f);
    pwm_config_set_wrap(&cfg, 39062);
    pwm_init(slice, &cfg, true);

    int ang = 0, dir = 1;
    for (;;) {
        float pulse_ms = 1.0f + (ang / 180.0f);
        uint16_t lvl = (uint16_t)(pulse_ms * 1953.1f);
        pwm_set_gpio_level(SERVO_PIN, lvl);

        ang += 5 * dir;
        if (ang >= 180) dir = -1;
        if (ang <= 0)   dir =  1;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void vTaskAudio(void *pvParameters) {
    AudioMsg_t msg;
    uint8_t  buffer[AUDIO_SAMPLES];

    // init ADC
    adc_init();
    adc_gpio_init(AUDIO_IN_PIN);

    // init PWM saída áudio
    gpio_set_function(AUDIO_OUT_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(AUDIO_OUT_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, 255);
    pwm_config_set_clkdiv(&cfg, 1.0f);
    pwm_init(slice, &cfg, false);

    for (;;) {
        if (xQueueReceive(qAudio, &msg, portMAX_DELAY)) {
            if (msg.start_record) {
                printf("Gravando áudio...\n");
                adc_select_input(AUDIO_IN_PIN - 26);
                for (int i = 0; i < AUDIO_SAMPLES; i++) {
                    uint16_t s = adc_read();
                    buffer[i] = s >> 4;
                    sleep_us(1000000 / SAMPLE_RATE);
                }
                printf("Reproduzindo áudio...\n");
                pwm_set_enabled(slice, true);
                for (int i = 0; i < AUDIO_SAMPLES; i++) {
                    pwm_set_gpio_level(AUDIO_OUT_PIN, buffer[i]);
                    sleep_us(1000000 / SAMPLE_RATE);
                }
                pwm_set_enabled(slice, false);
            }
        }
    }
}

int main() {
    stdio_init_all();

    // filas
    qUltrassom = xQueueCreate(10, sizeof(UltrassomMsg_t));
    qAudio     = xQueueCreate(1,  sizeof(AudioMsg_t));

    // configura GPIOs
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true, gpio_callback);

    gpio_init(LED_BLOCK_PIN);
    gpio_set_dir(LED_BLOCK_PIN, GPIO_OUT);
    gpio_put(LED_BLOCK_PIN, 0);

    // cria tasks
    xTaskCreate(vTaskUltrassom, "Ultrassom", 256, NULL, 3, NULL);
    xTaskCreate(vTaskServo,     "Servo",     256, NULL, 2, NULL);
    xTaskCreate(vTaskAudio,     "Audio",     512, NULL, 1, NULL);

    // inicia scheduler
    vTaskStartScheduler();

    // nunca deve chegar aqui
    for (;;);
    return 0;
}
