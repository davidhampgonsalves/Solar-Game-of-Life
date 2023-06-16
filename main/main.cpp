#include "bootloader_random.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "esp_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "ulp_main.h"
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gdew042t2.h"
#include <stdio.h>

#include <bitset>

// screen datasheet -
// https://thingpulse.com/wp-content/uploads/2019/07/GDEW042T2-V3.1-Specification_.pdf

#define DISPLAY_POWER_PIN GPIO_NUM_16
#define SCREEN_WIDTH 400
#define SCREEN_HEIGHT 300

// the previous generation needs to be written to deep sleep stable memory which is the limiting factor
// this size looks the best on my small screen but we could go smaller if we wanted since the bitset 
// uses 1 bit per boolean value.
#define WIDTH 50
#define HEIGHT 37

#define PIXEL_SIZE 400 / WIDTH

bool environment[WIDTH * HEIGHT] = {false};
RTC_NOINIT_ATTR static std::bitset<WIDTH * HEIGHT> previous;
RTC_NOINIT_ATTR int generation_count;

EpdSpi io;
Gdew042t2 display(io);

extern "C" {
void app_main();
}

int to_index(int x, int y) { return x + (y * (WIDTH - 1)); }

bool test_alive(bool env[WIDTH * HEIGHT], int x, int y) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
    return false;
  return env[to_index(x, y)];
}

void debug(bool env[WIDTH * HEIGHT]) {
  for (int x = 0; x < WIDTH; x++) {
    printf("\n");
    for (int y = 0; y < HEIGHT; y++) {
      if (test_alive(env, x, y))
        printf("*");
      else
        printf(".");
    }
  }
}

void init_environment() {
  bootloader_random_enable();
  unsigned char random_data[WIDTH * HEIGHT];
  esp_fill_random(random_data, sizeof(random_data));
  bootloader_random_disable();

  for (int i = 0; i < WIDTH * HEIGHT; i++) 
    environment[i] = random_data[i] & 0x01;
}

int count_living_neighbours(bool env[WIDTH * HEIGHT], int x, int y) {
  int count = 0;
  for (int y_delta = -1; y_delta <= 1; y_delta++)
    for (int x_delta = -1; x_delta <= 1; x_delta++) {
      if (x_delta == 0 && y_delta == 0) // don't count ourselves
        continue;
      if (count > 3) // early return b/c we don't care about counts > 3
        return count;
      if (test_alive(env, x + x_delta, y + y_delta))
        count++;
    }
  return count;
}

void draw(void) {
  if(generation_count % 100 == 0) // argegedon ever 100 generations to avoid loops
    init_environment();
  else {
    // restore environment from last run
    for (int i = 0; i < WIDTH * HEIGHT; i++)  
      environment[i] = previous[i];
  }

  bool next[WIDTH * HEIGHT] = {false};
  for (int x = 0; x < WIDTH; x++) {
    for (int y = 0; y < HEIGHT; y++) {
      int living_neighbour_count = count_living_neighbours(environment, x, y);
      bool is_alive = test_alive(environment, x, y);
      bool will_be_alive = false;

      if (is_alive &&
          (living_neighbour_count == 2 || living_neighbour_count == 3))
        will_be_alive = true;
      else if (!is_alive && living_neighbour_count == 3)
        will_be_alive = true;

      next[to_index(x, y)] = will_be_alive;
    }
  }

  // write the state back to the bitset for compact storage
  for (int i = 0; i < WIDTH * HEIGHT; i++)  
    previous[i] = next[i];

  gpio_reset_pin(DISPLAY_POWER_PIN);
  gpio_set_direction(DISPLAY_POWER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(DISPLAY_POWER_PIN, 1);

  vTaskDelay(100 / portTICK_RATE_MS);

  display.init();
  for (int x = 0; x < WIDTH; x++)
    for (int y = 0; y < HEIGHT; y++)
      if (test_alive(environment, x, y))
        display.fillRect(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE,
                         EPD_BLACK);

  display.setCursor(3, SCREEN_HEIGHT - (PIXEL_SIZE / 2) - 4);
  display.setTextColor(test_alive(environment, 0, HEIGHT-1) ? EPD_WHITE : EPD_BLACK);
  display.print(std::to_string(generation_count));

  display.update();
  gpio_set_level(DISPLAY_POWER_PIN, 0);

  generation_count++;
}

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

/* This function is called once after power-on reset, to load ULP program into
 * RTC memory and configure the ADC.
 */
static void init_ulp_program(void);

/* This function is called every time before going into deep sleep.
 * It starts the ULP program and resets measurement counter.
 */
static void start_ulp_program(void);

void app_main(void) {
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause != ESP_SLEEP_WAKEUP_ULP) {
    printf("Not ULP wakeup\n");
    init_ulp_program();
    generation_count = 1;
  } else {
    printf("Deep sleep wakeup\n");
    printf("ULP did %d measurements since last reset\n",
           ulp_sample_counter & UINT16_MAX);
    printf("Thresholds: high=%d\n", ulp_high_thr);
    ulp_last_result &= UINT16_MAX;

    draw();
  }
  printf("Entering deep sleep\n\n");
  start_ulp_program();
  ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
  esp_deep_sleep_start();
}

static void init_ulp_program(void) {
  esp_err_t err = ulp_load_binary(0, ulp_main_bin_start,
                                  (ulp_main_bin_end - ulp_main_bin_start) /
                                      sizeof(uint32_t));
  ESP_ERROR_CHECK(err);

  /* Configure ADC channel */
  /* Note: when changing channel here, also change 'adc_channel' constant
     in adc.S */
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
#if CONFIG_IDF_TARGET_ESP32
  adc1_config_width(ADC_WIDTH_BIT_12);
#elif CONFIG_IDF_TARGET_ESP32S2
  adc1_config_width(ADC_WIDTH_BIT_13);
#endif
  adc1_ulp_enable();

  /* Set the high threshold */
  ulp_high_thr = 3.2 * (4095 / 3.3); // about 3v

  /* Set ULP wake up period to 2 seconds */
  ulp_set_wakeup_period(0, 2000000);

  /* Disconnect GPIO12 and GPIO15 to remove current drain through
   * pullup/pulldown resistors.
   * GPIO12 may be pulled high to select flash voltage.
   */
  rtc_gpio_isolate(GPIO_NUM_12);
  rtc_gpio_isolate(GPIO_NUM_15);
  esp_deep_sleep_disable_rom_logging(); // suppress boot messages
}

static void start_ulp_program(void) {
  /* Reset sample counter */
  ulp_sample_counter = 0;

  esp_err_t err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
  ESP_ERROR_CHECK(err);
}