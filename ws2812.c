#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#include "ws2812.pio.h"

#define IS_RGBW false
#define NUM_PIXELS 25
#define WS2812_PIN 7

#define LED_RED_PIN 13
#define BTN_A_PIN 5
#define BTN_B_PIN 6

#define DEBOUNCE_DELAY_US 30000 // Debounce delay (em microsegundos)
volatile int counter = 0; // Contador global

// Matrizes para cada número de 0 a 9
bool numbers[10][NUM_PIXELS] = {
    { // Número 0
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 1
        0, 1, 1, 1, 0,
        0, 0, 1, 0, 0,
        0, 0, 1, 0, 0,
        0, 1, 1, 0, 0,
        0, 0, 1, 0, 0
    },
    { // Número 2
        0, 1, 1, 1, 0,
        0, 1, 0, 0, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 3
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 4
        0, 1, 0, 0, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 0, 1, 0
    },
    { // Número 5
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 1, 0, 0, 0,
        0, 1, 1, 1, 0
    },
    { // Número 6
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 1, 0, 0, 0,
        0, 1, 1, 1, 0
    },
    { // Número 7
        0, 1, 0, 0, 0,
        0, 0, 0, 1, 0,
        0, 1, 0, 0, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 8
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0
    },
    { // Número 9
        0, 1, 1, 1, 0,
        0, 0, 0, 1, 0,
        0, 1, 1, 1, 0,
        0, 1, 0, 1, 0,
        0, 1, 1, 1, 0
    }
};

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void display_number(bool *buffer, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = urgb_u32(r, g, b);
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(buffer[i] ? color : 0);
    }
}

void setup() {
    // Configuração do LED vermelho do LED RGB
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);

    // Configuração dos botões
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
}

// Função de interrupção única para ambos os botões
void button_callback(uint gpio, uint32_t events) {

    static volatile uint64_t last_interrupt_time_a = 0;
    static volatile uint64_t last_interrupt_time_b = 0;
    uint64_t current_time = time_us_64();

    if (gpio == BTN_A_PIN) { // Verifica se foi o botão A
        if (current_time - last_interrupt_time_a > DEBOUNCE_DELAY_US) {
            counter = (counter + 1) % 10;
            last_interrupt_time_a = current_time;
        }
    } else if (gpio == BTN_B_PIN) { // Verifica se foi o botão B
        if (current_time - last_interrupt_time_b > DEBOUNCE_DELAY_US) {
            counter = (counter - 1 + 10) % 10;
            last_interrupt_time_b = current_time;
        }
    }
}

// Thread para piscar o LED vermelho, aguardar 100 ms e delisgar o LED
void led_red_blink() {
    while (1) {
        gpio_put(LED_RED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_RED_PIN, 0);
        sleep_ms(100);
    }
}

int main() {
    stdio_init_all();
    setup();

    // Inicia a thread para o LED vermelho no núcleo 1
    multicore_launch_core1(led_red_blink);

    // Configuração inicial do PIO e LEDs
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Registra um único callback para ambos os botões
    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_RISE, true, &button_callback);
    gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_RISE, true, &button_callback);


    while (1) {
        // Combinação de valores          R,  G,  B      Ex: 20, 0, 0 - LEDs na cor vermelha com 20% de intensidade
        display_number(numbers[counter], 0, 0, 200);
        sleep_ms(100);
    }

    return 0;
}
