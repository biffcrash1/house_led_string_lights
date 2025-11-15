/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "example";

static uint8_t s_led_state = 0;

static led_strip_handle_t led_strips[CONFIG_LED_STRING_COUNT];

#define BLINK_GPIO 27  // GPIO 27 for single LED output

static int get_led_count( int string_index )
{
  int led_count;
  switch( string_index )
  {
    case 0:
      led_count = CONFIG_LED_STRING_1_LED_COUNT;
      break;
#if CONFIG_LED_STRING_COUNT >= 2
    case 1:
      led_count = CONFIG_LED_STRING_2_LED_COUNT;
      break;
#endif
#if CONFIG_LED_STRING_COUNT >= 3
    case 2:
      led_count = CONFIG_LED_STRING_3_LED_COUNT;
      break;
#endif
#if CONFIG_LED_STRING_COUNT >= 4
    case 3:
      led_count = CONFIG_LED_STRING_4_LED_COUNT;
      break;
#endif
#if CONFIG_LED_STRING_COUNT >= 5
    case 4:
      led_count = CONFIG_LED_STRING_5_LED_COUNT;
      break;
#endif
#if CONFIG_LED_STRING_COUNT >= 6
    case 5:
      led_count = CONFIG_LED_STRING_6_LED_COUNT;
      break;
#endif
    default:
      led_count = 0;
      break;
  }
  return led_count;
}

static void blink_led(void)
{
  /* If the addressable LED is enabled */
  if( s_led_state )
  {
    /* Set all LED strings to the same color */
    for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
    {
      int led_count = get_led_count( i );
      /* Set all LEDs in the string using RGB from 0 (0%) to 255 (100%) for each color */
      for( int j = 0; j < led_count; j++ )
      {
        if( j >= led_count )
        {
          ESP_LOGE( TAG, "ERROR: Attempting to access index %d but led_count is %d for string %d", j, led_count, i );
          break;
        }
        led_strip_set_pixel( led_strips[i], j, 127, 127, 127 );
      }
      /* Refresh the strip to send data */
      led_strip_refresh( led_strips[i] );
    }
  }
  else
  {
    /* Set all LED strings off */
    for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
    {
      led_strip_clear( led_strips[i] );
    }
  }
}

static void configure_single_led(void)
{
  /* Configure GPIO 27 as output for single LED */
  gpio_reset_pin( BLINK_GPIO );
  gpio_set_direction( BLINK_GPIO, GPIO_MODE_OUTPUT );
  ESP_LOGI( TAG, "Configured GPIO %d for single LED", BLINK_GPIO );
}

static void blink_single_led( uint8_t state )
{
  gpio_set_level( BLINK_GPIO, state );
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Configuring %d LED strings!", CONFIG_LED_STRING_COUNT);

    /* Configure each LED string */
    for (int i = 0; i < CONFIG_LED_STRING_COUNT; i++) {
        int gpio_num, led_count;
        
        /* Get the configuration for this string */
        switch(i) {
            case 0:
                gpio_num = CONFIG_LED_STRING_1_GPIO;
                led_count = CONFIG_LED_STRING_1_LED_COUNT;
                break;
#if CONFIG_LED_STRING_COUNT >= 2
            case 1:
                gpio_num = CONFIG_LED_STRING_2_GPIO;
                led_count = CONFIG_LED_STRING_2_LED_COUNT;
                break;
#endif
#if CONFIG_LED_STRING_COUNT >= 3
            case 2:
                gpio_num = CONFIG_LED_STRING_3_GPIO;
                led_count = CONFIG_LED_STRING_3_LED_COUNT;
                break;
#endif
#if CONFIG_LED_STRING_COUNT >= 4
            case 3:
                gpio_num = CONFIG_LED_STRING_4_GPIO;
                led_count = CONFIG_LED_STRING_4_LED_COUNT;
                break;
#endif
#if CONFIG_LED_STRING_COUNT >= 5
            case 4:
                gpio_num = CONFIG_LED_STRING_5_GPIO;
                led_count = CONFIG_LED_STRING_5_LED_COUNT;
                break;
#endif
#if CONFIG_LED_STRING_COUNT >= 6
            case 5:
                gpio_num = CONFIG_LED_STRING_6_GPIO;
                led_count = CONFIG_LED_STRING_6_LED_COUNT;
                break;
#endif
            default:
                ESP_LOGE(TAG, "Invalid string index %d", i);
                continue;
        }

        /* LED strip initialization with the GPIO and pixels number*/
        ESP_LOGI( TAG, "Configuring string %d: GPIO %d, %d LEDs", i, gpio_num, led_count );
        led_strip_config_t strip_config = {
            .strip_gpio_num = gpio_num,
            .max_leds = led_count,
        };

#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
        led_strip_rmt_config_t rmt_config = {
            .resolution_hz = 10 * 1000 * 1000, // 10MHz
            .flags.with_dma = false,
        };
        ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strips[i]));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
        led_strip_spi_config_t spi_config = {
            .spi_bus = SPI2_HOST,
            .flags.with_dma = true,
        };
        ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strips[i]));
#else
#error "unsupported LED strip backend"
#endif
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strips[i]);
    }
}

void app_main(void)
{
  /* Print all LED string configurations */
  ESP_LOGI( TAG, "LED String Count: %d", CONFIG_LED_STRING_COUNT );
  ESP_LOGI( TAG, "String 1: GPIO %d, LEDs %d", CONFIG_LED_STRING_1_GPIO, CONFIG_LED_STRING_1_LED_COUNT );
#if CONFIG_LED_STRING_COUNT >= 2
  ESP_LOGI( TAG, "String 2: GPIO %d, LEDs %d", CONFIG_LED_STRING_2_GPIO, CONFIG_LED_STRING_2_LED_COUNT );
#endif
#if CONFIG_LED_STRING_COUNT >= 3
  ESP_LOGI( TAG, "String 3: GPIO %d, LEDs %d", CONFIG_LED_STRING_3_GPIO, CONFIG_LED_STRING_3_LED_COUNT );
#endif
#if CONFIG_LED_STRING_COUNT >= 4
  ESP_LOGI( TAG, "String 4: GPIO %d, LEDs %d", CONFIG_LED_STRING_4_GPIO, CONFIG_LED_STRING_4_LED_COUNT );
#endif
#if CONFIG_LED_STRING_COUNT >= 5
  ESP_LOGI( TAG, "String 5: GPIO %d, LEDs %d", CONFIG_LED_STRING_5_GPIO, CONFIG_LED_STRING_5_LED_COUNT );
#endif
#if CONFIG_LED_STRING_COUNT >= 6
  ESP_LOGI( TAG, "String 6: GPIO %d, LEDs %d", CONFIG_LED_STRING_6_GPIO, CONFIG_LED_STRING_6_LED_COUNT );
#endif

  /* Configure the peripheral according to the LED type */
  configure_led();
  
  /* Configure single LED on GPIO 27 */
  configure_single_led();

  TickType_t last_single_led_toggle = xTaskGetTickCount();
  
  while (1)
  {
    /* Update LED strips at configured period */
    blink_led();
    
    /* Check if it's time to toggle the single LED (every 1000ms) */
    TickType_t current_tick = xTaskGetTickCount();
    if( ( current_tick - last_single_led_toggle ) >= ( 1000 / portTICK_PERIOD_MS ) )
    {
      /* Toggle the single LED state */
      s_led_state = !s_led_state;
      blink_single_led( s_led_state );
      last_single_led_toggle = current_tick;
    }
    
    /* Delay for LED strip update period */
    vTaskDelay( CONFIG_LED_STRING_UPDATE_PERIOD_MS / portTICK_PERIOD_MS );
  }
}
