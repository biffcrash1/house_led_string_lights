/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <math.h>
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
#define RAINBOW_PIXELS_PER_CYCLE 10.0f  // Number of pixels per full rainbow cycle
#define RAINBOW_SECONDS_PER_PIXEL 4.0f  // Seconds for pattern to advance one pixel

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

/* Convert HSV to RGB */
static void hsv_to_rgb( float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b )
{
  int i;
  float f, p, q, t;
  
  if( s == 0.0f )
  {
    /* Grayscale */
    *r = *g = *b = (uint8_t)( v * 255.0f );
    return;
  }
  
  h /= 60.0f;  /* Sector 0 to 5 */
  i = (int)floorf( h );
  f = h - i;  /* Fractional part of h */
  p = v * ( 1.0f - s );
  q = v * ( 1.0f - s * f );
  t = v * ( 1.0f - s * ( 1.0f - f ) );
  
  switch( i )
  {
    case 0:
      *r = (uint8_t)( v * 255.0f );
      *g = (uint8_t)( t * 255.0f );
      *b = (uint8_t)( p * 255.0f );
      break;
    case 1:
      *r = (uint8_t)( q * 255.0f );
      *g = (uint8_t)( v * 255.0f );
      *b = (uint8_t)( p * 255.0f );
      break;
    case 2:
      *r = (uint8_t)( p * 255.0f );
      *g = (uint8_t)( v * 255.0f );
      *b = (uint8_t)( t * 255.0f );
      break;
    case 3:
      *r = (uint8_t)( p * 255.0f );
      *g = (uint8_t)( q * 255.0f );
      *b = (uint8_t)( v * 255.0f );
      break;
    case 4:
      *r = (uint8_t)( t * 255.0f );
      *g = (uint8_t)( p * 255.0f );
      *b = (uint8_t)( v * 255.0f );
      break;
    default:  /* case 5 */
      *r = (uint8_t)( v * 255.0f );
      *g = (uint8_t)( p * 255.0f );
      *b = (uint8_t)( q * 255.0f );
      break;
  }
}

/* Rainbow pattern function - smoothly sequences through colors moving down the string */
static void rainbow_pattern( void )
{
  /* Get current time in milliseconds */
  TickType_t current_tick = xTaskGetTickCount();
  float time_seconds = (float)( current_tick * portTICK_PERIOD_MS ) / 1000.0f;
  
  /* Update all LED strings */
  for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
  {
    int led_count = get_led_count( i );
    
    for( int j = 0; j < led_count; j++ )
    {
      /* Calculate hue for this pixel position */
      /* Pattern moves down the string at 4 seconds per pixel */
      /* Phase offset: each pixel is offset by its position to create the moving wave effect */
      float phase_offset = (float)j * ( 360.0f / RAINBOW_PIXELS_PER_CYCLE );
      /* Calculate speed: full cycle time = pixels_per_cycle * seconds_per_pixel */
      float cycle_time = RAINBOW_PIXELS_PER_CYCLE * RAINBOW_SECONDS_PER_PIXEL;
      float time_hue = fmodf( ( time_seconds / cycle_time ) * 360.0f, 360.0f );
      float hue = fmodf( time_hue - phase_offset + 360.0f, 360.0f );
      
      /* Convert HSV to RGB */
      /* Saturation = 1.0 (full color), Value = 0.15 (15% brightness) */
      uint8_t r, g, b;
      hsv_to_rgb( hue, 1.0f, 0.15f, &r, &g, &b );
      
      /* Set pixel color */
      led_strip_set_pixel( led_strips[i], j, r, g, b );
    }
    
    /* Refresh the strip to send data */
    led_strip_refresh( led_strips[i] );
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
    /* Update LED strips with rainbow pattern at configured period */
    rainbow_pattern();
    
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
