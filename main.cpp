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
#define NUM_ENCODER_BUTTONS 5
#define NUM_ENCODERS 5

const uint8_t button_pins[NUM_BUTTONS] = {15, 14, 13, 12, 11, 10};
const uint8_t encoder_buttons_pins[NUM_ENCODER_BUTTONS] = {7, 4, 1, 26, 20};

const uint8_t encoder_a_pins[NUM_ENCODERS] = {9, 6, 3, 28, 22};
const uint8_t encoder_b_pins[NUM_ENCODERS] = {8, 5, 2, 27, 21};

static uint8_t prev_encoder_states[NUM_ENCODERS] = {0};

void gpio_init_buttons(void) {
    // Initialize regular buttons
    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]); // Enable internal pull-up resistor
    }
    
    // Initialize encoder buttons
    for (int i = 0; i < NUM_ENCODER_BUTTONS; i++) {
        gpio_init(encoder_buttons_pins[i]);
        gpio_set_dir(encoder_buttons_pins[i], GPIO_IN);
        gpio_pull_up(encoder_buttons_pins[i]); // Enable internal pull-up resistor
    }

    // New: Initialize encoder pins
    for (int i = 0; i < NUM_ENCODERS; i++) {
        // Initialize A pins
        gpio_init(encoder_a_pins[i]);
        gpio_set_dir(encoder_a_pins[i], GPIO_IN);
        gpio_pull_up(encoder_a_pins[i]);
        
        // Initialize B pins
        gpio_init(encoder_b_pins[i]);
        gpio_set_dir(encoder_b_pins[i], GPIO_IN);
        gpio_pull_up(encoder_b_pins[i]);
    }
}

uint32_t read_buttons(void) {
    uint32_t buttons_state = 0;
    
    // Read regular buttons (bits 0-5)
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!gpio_get(button_pins[i])) { // Button pressed when GPIO reads LOW (due to pull-up)
            buttons_state |= (1 << i); // Set the corresponding bit
        }
    }
    
    // Read encoder buttons (bits 6-10)
    for (int i = 0; i < NUM_ENCODER_BUTTONS; i++) {
        if (!gpio_get(encoder_buttons_pins[i])) { // Button pressed when GPIO reads LOW (due to pull-up)
            buttons_state |= (1 << (i + NUM_BUTTONS)); // Set the corresponding bit after regular buttons
        }
    }
    
    // New: Read encoders (bits 11-20)
    for (int i = 0; i < NUM_ENCODERS; i++) {
        uint8_t a_state = !gpio_get(encoder_a_pins[i]);  // Inverted due to pull-up
        uint8_t b_state = !gpio_get(encoder_b_pins[i]);  // Inverted due to pull-up
        uint8_t current_state = (a_state << 1) | b_state;
        uint8_t prev_state = prev_encoder_states[i];
        
        // Check rotation direction based on state change
        if (prev_state != current_state) {
            // Calculate rotation direction
            if ((prev_state == 0b00 && current_state == 0b01) ||
                (prev_state == 0b01 && current_state == 0b11) ||
                (prev_state == 0b11 && current_state == 0b10) ||
                (prev_state == 0b10 && current_state == 0b00)) {
                // Clockwise rotation - set first encoder button
                buttons_state |= (1 << (11 + i * 2));
            }
            else if ((prev_state == 0b00 && current_state == 0b10) ||
                     (prev_state == 0b10 && current_state == 0b11) ||
                     (prev_state == 0b11 && current_state == 0b01) ||
                     (prev_state == 0b01 && current_state == 0b00)) {
                // Counter-clockwise rotation - set second encoder button
                buttons_state |= (1 << (12 + i * 2));
            }
            
            prev_encoder_states[i] = current_state;
        }
    }
    
    return buttons_state;
}

struct __attribute__((packed)) gamepad_report_custom_t {
    uint8_t reportId;   // Report ID
    uint8_t buttons[3]; // 3 bytes for 21 buttons + 3 padding bits
};

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
    if (!tud_hid_ready()) {
        return;
    }
    if (report_id != REPORT_ID_GAMEPAD) return;
 
    gamepad_report_custom_t report = {0};
    report.reportId = report_id;
    
    // Split the 24 bits into 3 bytes
    report.buttons[0] = (btn & 0xFF);         // First 8 bits
    report.buttons[1] = (btn >> 8) & 0xFF;    // Next 8 bits
    report.buttons[2] = (btn >> 16) & 0xFF;   // Last 8 bits

    // Send the report
    tud_hid_report(report_id, &report.buttons[0], sizeof(report.buttons));
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
// This matters because it is used to send data to the host
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    if (report_id != REPORT_ID_GAMEPAD) return 0;
    
    uint32_t buttons_states = read_buttons() & 0x1FFFFF; // Mask to keep first 21 bits
    
    buffer[0] = (buttons_states & 0xFF);
    buffer[1] = (buttons_states >> 8) & 0xFF;
    buffer[2] = (buttons_states >> 16) & 0xFF;
    
    return 3; // Return the number of bytes in the report
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