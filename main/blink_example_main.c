/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "example";

static uint8_t s_led_state = 0;

static led_strip_handle_t led_strips[CONFIG_LED_STRING_COUNT];

#define BLINK_GPIO 27  // GPIO 27 for single LED output
#define RAINBOW_PIXELS_PER_CYCLE 10.0f  // Number of pixels per full rainbow cycle
#define RAINBOW_SECONDS_PER_PIXEL 0.25f  // Seconds for pattern to advance one pixel (4 pixels per second)

/* Pattern Enable Macros */
#define ENABLE_RAINBOW_PATTERN          1
#define ENABLE_WINTERY_TWINKLE_PATTERN  1
#define ENABLE_CHRISTMAS_LIGHTS_PATTERN 1

/* Pattern duration in milliseconds */
#define PATTERN_DURATION_MS 300000  // 5 minutes

/* Pattern types */
typedef enum
{
  RAINBOW_PATTERN,
  WINTERY_TWINKLE_PATTERN,
  CHRISTMAS_LIGHTS_PATTERN,
  PATTERN_COUNT
} pattern_type;

/* System states */
typedef enum
{
  STARTUP,
  DAY,
  NIGHT_ON,
  NIGHT_OFF,
  TEST_MODE
} states_type;

/* Pattern management variables */
static pattern_type enabledPatterns[PATTERN_COUNT];
static int numEnabledPatterns = 0;
static int currentPatternIndex = 0;
static bool manualControl = false;
static TickType_t patternStartTime = 0;
static states_type state = NIGHT_ON;  /* Default to NIGHT_ON until day/night logic implemented */
static bool patternInterrupted = false;
static SemaphoreHandle_t patternMutex = NULL;

/* UART configuration for serial commands */
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024

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

/* Helper function to turn off all LEDs */
static void all_leds_off( void )
{
  for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
  {
    led_strip_clear( led_strips[i] );
    led_strip_refresh( led_strips[i] );
  }
}

/* Pattern transition helper */
static void pattern_transition( void )
{
  all_leds_off();
  vTaskDelay( pdMS_TO_TICKS( 100 ) );  /* Brief delay for clean transition */
}

/* Check if pattern should be interrupted */
static bool check_pattern_interrupt( void )
{
  bool interrupted = false;
  if( patternMutex != NULL && xSemaphoreTake( patternMutex, 0 ) == pdTRUE )
  {
    interrupted = patternInterrupted;
    xSemaphoreGive( patternMutex );
  }
  return interrupted;
}

/* Rainbow pattern function - smoothly sequences through colors moving down the string */
static void rainbow_pattern( void )
{
  /* Check for interrupt */
  if( check_pattern_interrupt() )
  {
    return;
  }

  /* Get current time in milliseconds */
  TickType_t current_tick = xTaskGetTickCount();
  float time_seconds = (float)( current_tick * portTICK_PERIOD_MS ) / 1000.0f;
  
  /* Update all LED strings */
  for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
  {
    int led_count = get_led_count( i );
    
    /* Ensure we set all LEDs in the string - explicitly set every pixel */
    for( int j = 0; j < led_count; j++ )
    {
      /* Calculate hue for this pixel position */
      /* Pattern moves down the string at 4 seconds per pixel */
      /* Phase offset: each pixel is offset by its position to create the moving wave effect */
      float phase_offset = (float)j * ( 360.0f / RAINBOW_PIXELS_PER_CYCLE );
      /* Calculate speed: full cycle time = pixels_per_cycle * seconds_per_pixel */
      float cycle_time = RAINBOW_PIXELS_PER_CYCLE * RAINBOW_SECONDS_PER_PIXEL;
      float time_hue = fmodf( ( time_seconds / cycle_time ) * 360.0f, 360.0f );
      
      /* Normalize phase_offset to 0-360 range before subtracting */
      float normalized_phase = fmodf( phase_offset, 360.0f );
      float hue = fmodf( time_hue - normalized_phase + 360.0f, 360.0f );
      
      /* Convert HSV to RGB */
      /* Saturation = 1.0 (full color), Value = 0.15 (15% brightness) */
      uint8_t r, g, b;
      hsv_to_rgb( hue, 1.0f, 0.15f, &r, &g, &b );
      
      /* Set pixel color - swap R and G for GRB color order (LED strip expects GRB, not RGB) */
      led_strip_set_pixel( led_strips[i], j, g, r, b );
    }
    
    /* Refresh the strip to send data for all LEDs */
    led_strip_refresh( led_strips[i] );
  }
}

/* Wintery twinkle pattern - matches Arduino sparkle pattern exactly */
static void wintery_twinkle_pattern( void )
{
  /* Check for interrupt */
  if( check_pattern_interrupt() )
  {
    return;
  }

  /* Static variables to track state between calls */
  static TickType_t last_change_ms = 0;
  static TickType_t last_color_change_ms = 0;
  static uint8_t current_hue = 160;  /* Blue hue (wintery theme) */
  static uint8_t current_sat = 200;
  static uint8_t current_val = 150;  /* Value from Arduino sparkle call */
  static int sat_dir = 0;
  
  /* Static arrays to track RGB values for proper fading (since we can't read back) */
  #define MAX_LEDS_PER_STRING 300
  static uint8_t pixel_r[CONFIG_LED_STRING_COUNT][MAX_LEDS_PER_STRING] = {0};
  static uint8_t pixel_g[CONFIG_LED_STRING_COUNT][MAX_LEDS_PER_STRING] = {0};
  static uint8_t pixel_b[CONFIG_LED_STRING_COUNT][MAX_LEDS_PER_STRING] = {0};
  
  /* Configuration parameters (matching Arduino sparkle exactly) */
  const int num_on_pct = 5;  /* 5% of LEDs on at a time */
  const bool change_color = true;
  const int percent_white = 20;  /* 20% chance of white */
  const bool random_color = false;  /* Sequential color change, not random */
  const unsigned long color_change_period_ms = 100;  /* Change color every 100ms */
  const unsigned long change_rate_ms = 20;  /* Add new pixels every 20ms */
  const unsigned long fade_ms = 1600;  /* Fade time in ms */
  const unsigned long refresh_rate_ms = CONFIG_LED_STRING_UPDATE_PERIOD_MS;
  
  TickType_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
  
  /* Calculate decay percentage for fade effect (matching Arduino formula) */
  int decay_pct = 255;
  if( fade_ms != 0 )
  {
    decay_pct = (int)(( 255L * (long)refresh_rate_ms * 6L ) / fade_ms );
    if( decay_pct > 255 )
    {
      decay_pct = 255;
    }
    if( decay_pct <= 0 )
    {
      decay_pct = 1;
    }
  }

  /* Update all LED strings */
  for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
  {
    int led_count = get_led_count( i );
    if( led_count > MAX_LEDS_PER_STRING )
    {
      led_count = MAX_LEDS_PER_STRING;
    }
    
    /* FIRST: Fade all pixels every frame (matching fadeToBlackBy behavior) */
    if( fade_ms == 0 )
    {
      /* No fade - clear all */
      for( int j = 0; j < led_count; j++ )
      {
        pixel_r[i][j] = 0;
        pixel_g[i][j] = 0;
        pixel_b[i][j] = 0;
      }
    }
    else
    {
      /* Fade existing pixels using fadeToBlackBy equivalent: new = old * (255 - decay_pct) / 255 */
      for( int j = 0; j < led_count; j++ )
      {
        pixel_r[i][j] = ( (uint16_t)pixel_r[i][j] * ( 255 - decay_pct ) ) / 255;
        pixel_g[i][j] = ( (uint16_t)pixel_g[i][j] * ( 255 - decay_pct ) ) / 255;
        pixel_b[i][j] = ( (uint16_t)pixel_b[i][j] * ( 255 - decay_pct ) ) / 255;
      }
    }
    
    /* SECOND: Turn on some NEW pixels at FULL brightness */
    if( ( now_ms >= ( last_change_ms + change_rate_ms ) ) ||
        ( now_ms < last_change_ms ) )
    {
      last_change_ms = now_ms;
      
      int num_on = ( num_on_pct * led_count ) / 100;
      if( num_on < 1 )
      {
        num_on = 1;
      }
      
      for( int k = 0; k < num_on; k++ )
      {
        int pixel = rand() % led_count;
        
        /* Use CURRENT global color (not per-pixel color) */
        uint8_t temp_hue = current_hue;
        uint8_t temp_sat = current_sat;
        uint8_t temp_val = current_val;
        
        /* 20% chance of white (saturation = 0) */
        if( ( rand() % 100 ) < percent_white )
        {
          temp_sat = 0;
        }
        
        /* Convert HSV to RGB at FULL brightness */
        uint8_t r, g, b;
        hsv_to_rgb( (float)temp_hue, temp_sat / 255.0f, temp_val / 255.0f, &r, &g, &b );
        
        /* Set pixel to FULL brightness (will fade on next frames) */
        pixel_r[i][pixel] = r;
        pixel_g[i][pixel] = g;
        pixel_b[i][pixel] = b;
      }
      
      /* THIRD: Change the GLOBAL color over time */
      if( now_ms > ( last_color_change_ms + color_change_period_ms ) )
      {
        if( change_color )
        {
          if( random_color )
          {
            current_hue = rand() % 256;
            current_sat = rand() % 256;
          }
          else
          {
            /* Sequential color change: hue += 3 */
            current_hue += 3;
            
            /* Saturation oscillation */
            if( sat_dir == 0 )
            {
              if( current_sat < 255 )
              {
                current_sat += 1;
              }
              else
              {
                current_sat -= 1;
                sat_dir = 1;
              }
            }
            else
            {
              int min_sat = 0;
              if( percent_white > 0 )
              {
                min_sat = 150;  /* Minimum saturation when white is enabled */
              }
              if( current_sat > min_sat )
              {
                current_sat -= 1;
              }
              else
              {
                current_sat += 1;
                sat_dir = 0;
              }
            }
          }
        }
        last_color_change_ms = now_ms;
      }
    }
    
    /* FINALLY: Write all pixels to the LED strip */
    for( int j = 0; j < led_count; j++ )
    {
      /* GRB order for WS2812 */
      led_strip_set_pixel( led_strips[i], j, pixel_g[i][j], pixel_r[i][j], pixel_b[i][j] );
    }
    
    /* Refresh the strip */
    led_strip_refresh( led_strips[i] );
  }
}

/* Christmas lights pattern - classic colors spaced 6 pixels apart */
static void christmas_lights_pattern( void )
{
  /* Check for interrupt */
  if( check_pattern_interrupt() )
  {
    return;
  }

  /* Get current time for color rotation */
  TickType_t current_tick = xTaskGetTickCount();
  float time_seconds = (float)( current_tick * portTICK_PERIOD_MS ) / 1000.0f;
  
  /* Rotate colors every 2 seconds */
  int color_offset = (int)( time_seconds / 2.0f ) % 5;
  
  /* Classic Christmas colors: Red, Green, Blue, Yellow, White */
  uint8_t colors[5][3] = {
    { 38, 0, 0 },    /* Red at 15% brightness (GRB) */
    { 0, 38, 0 },    /* Green at 15% brightness (GRB) */
    { 0, 0, 38 },    /* Blue at 15% brightness (GRB) */
    { 38, 38, 0 },   /* Yellow at 15% brightness (GRB) */
    { 38, 38, 38 }   /* White at 15% brightness (GRB) */
  };
  
  /* Update all LED strings */
  for( int i = 0; i < CONFIG_LED_STRING_COUNT; i++ )
  {
    int led_count = get_led_count( i );
    
    for( int j = 0; j < led_count; j++ )
    {
      /* Bulbs spaced 6 pixels apart (pixel 0, 6, 12, 18, etc.) */
      if( ( j % 6 ) == 0 )  /* Every 6th pixel (0-indexed: 0, 6, 12, 18...) */
      {
        /* Calculate which color this bulb should be */
        int bulb_index = j / 6;
        int color_index = ( bulb_index + color_offset ) % 5;
        
        led_strip_set_pixel( led_strips[i], j, 
                            colors[color_index][0], 
                            colors[color_index][1], 
                            colors[color_index][2] );
      }
      else
      {
        /* Blank space between bulbs */
        led_strip_set_pixel( led_strips[i], j, 0, 0, 0 );
      }
    }
    
    /* Refresh the strip */
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

/* Get pattern name string */
static const char* get_pattern_name( pattern_type pattern )
{
  switch( pattern )
  {
    case RAINBOW_PATTERN:
      return "Rainbow";
    case WINTERY_TWINKLE_PATTERN:
      return "Wintery Twinkle";
    case CHRISTMAS_LIGHTS_PATTERN:
      return "Christmas Lights";
    default:
      return "Unknown";
  }
}

/* Initialize pattern list based on enable flags */
static void init_pattern_list( void )
{
  numEnabledPatterns = 0;
  
#if ENABLE_RAINBOW_PATTERN
  enabledPatterns[numEnabledPatterns++] = RAINBOW_PATTERN;
#endif
#if ENABLE_WINTERY_TWINKLE_PATTERN
  enabledPatterns[numEnabledPatterns++] = WINTERY_TWINKLE_PATTERN;
#endif
#if ENABLE_CHRISTMAS_LIGHTS_PATTERN
  enabledPatterns[numEnabledPatterns++] = CHRISTMAS_LIGHTS_PATTERN;
#endif
  
  ESP_LOGI( TAG, "Initialized %d enabled patterns", numEnabledPatterns );
}

/* Run a specific pattern function once */
static void run_pattern( pattern_type pattern )
{
  /* Call appropriate pattern function */
  switch( pattern )
  {
    case RAINBOW_PATTERN:
      rainbow_pattern();
      break;
    case WINTERY_TWINKLE_PATTERN:
      wintery_twinkle_pattern();
      break;
    case CHRISTMAS_LIGHTS_PATTERN:
      christmas_lights_pattern();
      break;
    default:
      ESP_LOGE( TAG, "Unknown pattern type: %d", pattern );
      return;
  }
}

/* Serial command handler task */
static void serial_command_task( void *pvParameters )
{
  char command;
  
  ESP_LOGI( TAG, "Serial command task started" );
  
  /* Print help menu */
  ESP_LOGI( TAG, "Interactive Pattern Control Enabled:" );
  ESP_LOGI( TAG, "Pattern Control:" );
  ESP_LOGI( TAG, "  n/N - Next pattern" );
  ESP_LOGI( TAG, "  p/P - Previous pattern" );
  ESP_LOGI( TAG, "  0-9 - Jump to pattern number" );
  ESP_LOGI( TAG, "  a/A - Auto mode (sequential)" );
  ESP_LOGI( TAG, "System Control:" );
  ESP_LOGI( TAG, "  t/T - Test mode (placeholder)" );
  ESP_LOGI( TAG, "  r/R - Resume normal (placeholder)" );
  ESP_LOGI( TAG, "  s/S - Show status" );
  ESP_LOGI( TAG, "  l/L - List all patterns" );
  ESP_LOGI( TAG, "  h/H/? - Show help" );
  
  while( 1 )
  {
    /* Read one character from UART (non-blocking) */
    uint8_t byte;
    int len = uart_read_bytes( UART_NUM, &byte, 1, pdMS_TO_TICKS( 100 ) );
    
    if( len > 0 )
    {
      command = (char)byte;
      
      /* Skip whitespace */
      if( command == ' ' || command == '\n' || command == '\r' || command == '\t' )
      {
        continue;
      }
      
      /* Process command */
      switch( command )
      {
          case 'n':
          case 'N':
            /* Next pattern */
            if( numEnabledPatterns > 0 )
            {
              if( xSemaphoreTake( patternMutex, portMAX_DELAY ) == pdTRUE )
              {
                currentPatternIndex = ( currentPatternIndex + 1 ) % numEnabledPatterns;
                patternInterrupted = true;
                manualControl = true;
                patternStartTime = xTaskGetTickCount();
                xSemaphoreGive( patternMutex );
                
                ESP_LOGI( TAG, "Next pattern: %d - %s", 
                         currentPatternIndex, 
                         get_pattern_name( enabledPatterns[currentPatternIndex] ) );
              }
            }
            break;
            
          case 'p':
          case 'P':
            /* Previous pattern */
            if( numEnabledPatterns > 0 )
            {
              if( xSemaphoreTake( patternMutex, portMAX_DELAY ) == pdTRUE )
              {
                currentPatternIndex = ( currentPatternIndex - 1 + numEnabledPatterns ) % numEnabledPatterns;
                patternInterrupted = true;
                manualControl = true;
                patternStartTime = xTaskGetTickCount();
                xSemaphoreGive( patternMutex );
                
                ESP_LOGI( TAG, "Previous pattern: %d - %s", 
                         currentPatternIndex, 
                         get_pattern_name( enabledPatterns[currentPatternIndex] ) );
              }
            }
            break;
            
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            /* Jump to pattern number */
            {
              int pattern_num = command - '0';
              
              /* Try to read additional digits with timeout */
              TickType_t digit_timeout = xTaskGetTickCount() + pdMS_TO_TICKS( 1000 );
              while( xTaskGetTickCount() < digit_timeout )
              {
                uint8_t next_byte;
                int next_len = uart_read_bytes( UART_NUM, &next_byte, 1, pdMS_TO_TICKS( 100 ) );
                if( next_len > 0 )
                {
                  char next_char = (char)next_byte;
                  if( next_char >= '0' && next_char <= '9' )
                  {
                    pattern_num = pattern_num * 10 + ( next_char - '0' );
                  }
                  else if( next_char == ' ' || next_char == '\n' || next_char == '\r' || next_char == '\t' )
                  {
                    /* Whitespace - finish reading number */
                    break;
                  }
                  else
                  {
                    /* Non-digit, non-whitespace - finish reading number */
                    break;
                  }
                }
                else
                {
                  /* No more data available */
                  break;
                }
              }
              
              if( pattern_num < numEnabledPatterns )
              {
                if( xSemaphoreTake( patternMutex, portMAX_DELAY ) == pdTRUE )
                {
                  currentPatternIndex = pattern_num;
                  patternInterrupted = true;
                  manualControl = true;
                  patternStartTime = xTaskGetTickCount();
                  xSemaphoreGive( patternMutex );
                  
                  ESP_LOGI( TAG, "Jump to pattern: %d - %s", 
                           currentPatternIndex, 
                           get_pattern_name( enabledPatterns[currentPatternIndex] ) );
                }
              }
              else
              {
                ESP_LOGW( TAG, "Pattern %d out of range (0-%d)", pattern_num, numEnabledPatterns - 1 );
              }
            }
            break;
            
          case 'a':
          case 'A':
            /* Auto mode */
            if( xSemaphoreTake( patternMutex, portMAX_DELAY ) == pdTRUE )
            {
              manualControl = false;
              currentPatternIndex = 0;
              xSemaphoreGive( patternMutex );
              ESP_LOGI( TAG, "Auto mode enabled" );
            }
            break;
            
          case 's':
          case 'S':
            /* Status */
            {
              ESP_LOGI( TAG, "Status:" );
              ESP_LOGI( TAG, "  Pattern: %d/%d - %s", 
                       currentPatternIndex, 
                       numEnabledPatterns - 1,
                       get_pattern_name( enabledPatterns[currentPatternIndex] ) );
              ESP_LOGI( TAG, "  Manual: %s", manualControl ? "ON" : "OFF" );
              ESP_LOGI( TAG, "  State: %d", state );
            }
            break;
            
          case 'l':
          case 'L':
            /* List patterns */
            {
              ESP_LOGI( TAG, "Enabled patterns (%d):", numEnabledPatterns );
              for( int j = 0; j < numEnabledPatterns; j++ )
              {
                ESP_LOGI( TAG, "  %d: %s", j, get_pattern_name( enabledPatterns[j] ) );
              }
            }
            break;
            
          case 'h':
          case 'H':
          case '?':
            /* Help */
            ESP_LOGI( TAG, "Pattern Control:" );
            ESP_LOGI( TAG, "  n/N - Next pattern" );
            ESP_LOGI( TAG, "  p/P - Previous pattern" );
            ESP_LOGI( TAG, "  0-9 - Jump to pattern number" );
            ESP_LOGI( TAG, "  a/A - Auto mode (sequential)" );
            ESP_LOGI( TAG, "System Control:" );
            ESP_LOGI( TAG, "  t/T - Test mode (placeholder)" );
            ESP_LOGI( TAG, "  r/R - Resume normal (placeholder)" );
            ESP_LOGI( TAG, "  s/S - Show status" );
            ESP_LOGI( TAG, "  l/L - List patterns" );
            ESP_LOGI( TAG, "  h/H/? - This help" );
            break;
            
          case 't':
          case 'T':
            /* Test mode (placeholder) */
            ESP_LOGI( TAG, "Test mode (placeholder - not implemented)" );
            break;
            
          case 'r':
          case 'R':
            /* Resume normal (placeholder) */
            ESP_LOGI( TAG, "Resume normal (placeholder - not implemented)" );
            break;
            
          default:
            /* Ignore unknown commands */
            break;
        }
    }
    
    /* Small delay to prevent CPU spinning */
    vTaskDelay( pdMS_TO_TICKS( 10 ) );
  }
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
        ESP_LOGI( TAG, "Configuring string %d: GPIO %d, %d LEDs (max_leds will be %d)", i, gpio_num, led_count, led_count );
        led_strip_config_t strip_config = {
            .strip_gpio_num = gpio_num,
            .max_leds = led_count,  /* Ensure max_leds matches the configured LED count */
        };

#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
        led_strip_rmt_config_t rmt_config = {
            .resolution_hz = 10 * 1000 * 1000, // 10MHz
            .flags.with_dma = false,  /* ESP32 RMT doesn't support DMA, use memory blocks */
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
        /* Initialize all LEDs to black/off */
        led_strip_clear(led_strips[i]);
        
        /* Explicitly set all pixels to ensure they're initialized */
        for( int init_j = 0; init_j < led_count; init_j++ )
        {
          led_strip_set_pixel( led_strips[i], init_j, 0, 0, 0 );
        }
        led_strip_refresh( led_strips[i] );
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

  /* Create mutex for pattern control */
  patternMutex = xSemaphoreCreateMutex();
  if( patternMutex == NULL )
  {
    ESP_LOGE( TAG, "Failed to create pattern mutex" );
    return;
  }

  /* Initialize random number generator */
  srand( (unsigned int)xTaskGetTickCount() );

  /* Initialize pattern list */
  init_pattern_list();
  
  if( numEnabledPatterns == 0 )
  {
    ESP_LOGE( TAG, "No patterns enabled!" );
    return;
  }

  /* Configure the peripheral according to the LED type */
  configure_led();
  
  /* Configure single LED on GPIO 27 */
  configure_single_led();

  /* Configure UART for reading serial commands
   * Note: ESP-IDF console uses UART_NUM_0, but we can still read from it
   * by installing the UART driver with proper configuration */
  uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };
  
  /* Install UART driver with receive buffer
   * This allows reading while console output still works */
  ESP_ERROR_CHECK( uart_driver_install( UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0 ) );
  ESP_ERROR_CHECK( uart_param_config( UART_NUM, &uart_config ) );
  /* Use default pins (already configured by console) */
  ESP_ERROR_CHECK( uart_set_pin( UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE ) );

  /* Create serial command handler task */
  xTaskCreate( serial_command_task, "serial_cmd", 4096, NULL, 5, NULL );

  TickType_t last_single_led_toggle = xTaskGetTickCount();
  patternStartTime = xTaskGetTickCount();
  
  while (1)
  {
    /* Check state - only run patterns in NIGHT_ON or TEST_MODE */
    if( state != NIGHT_ON && state != TEST_MODE )
    {
      /* Day or NIGHT_OFF - turn off LEDs */
      all_leds_off();
      /* Placeholder: GPIO control for relay will be added later */
    }
    else
    {
      /* Run patterns */
      if( numEnabledPatterns > 0 )
      {
        /* Check if pattern duration expired */
        TickType_t current_tick = xTaskGetTickCount();
        TickType_t elapsed = current_tick - patternStartTime;
        TickType_t duration_ticks = pdMS_TO_TICKS( PATTERN_DURATION_MS );
        
        bool advance_pattern = false;
        
        if( xSemaphoreTake( patternMutex, 0 ) == pdTRUE )
        {
          if( !manualControl )
          {
            /* Auto mode - advance when duration expires */
            if( elapsed >= duration_ticks )
            {
              advance_pattern = true;
            }
          }
          else
          {
            /* Manual mode - check for timeout (30 seconds) */
            if( elapsed >= pdMS_TO_TICKS( 30000 ) )
            {
              manualControl = false;
              currentPatternIndex = 0;
              advance_pattern = true;
            }
          }
          xSemaphoreGive( patternMutex );
        }
        
        if( advance_pattern )
        {
          if( xSemaphoreTake( patternMutex, portMAX_DELAY ) == pdTRUE )
          {
            currentPatternIndex = ( currentPatternIndex + 1 ) % numEnabledPatterns;
            patternStartTime = xTaskGetTickCount();
            patternInterrupted = false;  /* Clear interrupt when advancing */
            xSemaphoreGive( patternMutex );
            
            /* Pattern transition */
            pattern_transition();
          }
        }
        
        /* Clear interrupt flag at start of pattern run */
        if( xSemaphoreTake( patternMutex, 0 ) == pdTRUE )
        {
          patternInterrupted = false;
          pattern_type current_pattern = enabledPatterns[currentPatternIndex];
          xSemaphoreGive( patternMutex );
          
          /* Run current pattern */
          run_pattern( current_pattern );
        }
      }
    }
    
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
    vTaskDelay( pdMS_TO_TICKS( CONFIG_LED_STRING_UPDATE_PERIOD_MS ) );
  }
}
