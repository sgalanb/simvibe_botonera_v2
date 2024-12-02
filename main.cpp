#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

#include "hardware/gpio.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

void hid_task(void);
void gpio_init_buttons(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  gpio_init_buttons();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task(); // tinyusb device task
    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    board_led_write(true);  // Turn LED on when USB is mounted
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    board_led_write(false);  // Turn LED off when USB is unmounted
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

#define NUM_BUTTONS 6
const uint8_t button_pins[NUM_BUTTONS] = {15, 14, 13, 12, 11, 10};

void gpio_init_buttons(void) {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    gpio_init(button_pins[i]);
    gpio_set_dir(button_pins[i], GPIO_IN);
    gpio_pull_up(button_pins[i]); // Enable internal pull-up resistor
  }
}

uint32_t read_buttons(void) {
  uint32_t buttons_state = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!gpio_get(button_pins[i])) { // Button pressed when GPIO reads LOW (due to pull-up)
      buttons_state |= (1 << i); // Set the corresponding bit
    }
  }
  
  return buttons_state;
}

struct gamepad_report_custom_t {
    uint32_t buttons;
};

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // Skip if HID is not ready yet
  if (!tud_hid_ready()) {
    board_led_write(false);  // LED off if HID not ready
    return;
  }
  // Only handle gamepad report
  if (report_id != REPORT_ID_GAMEPAD) return;
 
  gamepad_report_custom_t report = {1}; // Initialize all fields to 0
  report.buttons = btn & 0x3F; // Only use first 6 bits, rest will be 0

  // Send the HID report
  bool success = tud_hid_report(report_id, &report, sizeof(report));

  // Visual feedback for report sending
  if (success) {
    // Quick double blink for successful send
    board_led_write(true);
    board_delay(50);
    board_led_write(false);
  } else {
    // Slow blink for failed send
    board_led_write(true);
    board_delay(2000);
    board_led_write(false);
  }
}

void hid_task(void)
{
    // Poll every 10ms
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    uint32_t buttons_states = read_buttons();

    // Check if gpio is working
    // if (buttons_states) {
    //     board_led_write(true);  // Turn LED on when any button is pressed
    // } else {
    //     board_led_write(false);  // Turn LED off when no buttons are pressed
    // }

    if (tud_suspended() && buttons_states)
    {
        tud_remote_wakeup();
    }
    else
    {
        send_hid_report(REPORT_ID_GAMEPAD, buttons_states);
    }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
// This do matter because it is used to send data to the host
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_type;
    
    // Only handle gamepad report
    if (report_id != REPORT_ID_GAMEPAD) return 0;
    
    // Get current button states (only first 6 buttons)
    uint32_t buttons_states = read_buttons() & 0x3F; // Mask to only keep first 6 bits
    
    gamepad_report_custom_t report = {1}; // Initialize all fields to 0
    report.buttons = buttons_states; // The remaining bits will be 0
    
    // Copy to the buffer
    uint16_t length = MIN(reqlen, sizeof(report));
    memcpy(buffer, &report, length);

    return length;
}


// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
// I think this doesn't matter because it is used to receive data from the host
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}