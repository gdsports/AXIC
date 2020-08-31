/*
 * XInput/Xbox 360 on the USB device/peripheral (micro USB connector) port.
 * Various controllers on the USB host port such as Thrustermaster T.16000,
 * Logitech Extreme 3D Pro, and Sony DS4. GPIOs pins can be used for
 * buttons kind of like the Xbox Adaptive Controller. Just add 3.5 mm audio
 * jacks. The controllers connected to the USB host port and the GPIO
 * connected buttons can be used at the same time so it works kind of
 * like co-pilot mode. Many controllers -> 1 player.
 */

#include "USBHost_t36.h"
#include <XInput.h>
#include <Bounce2.h>

// Leave disabled(0) unless analog joysticks are connected to Teensy analog
// input pins.
#define ANALOG_JOYSTICKS  0
const int ADC_Max = 1023;
const int16_t JOY_MAX = 32767;
const int16_t JOY_MIN = -32768;

#define NUM_BUTTONS 11
const struct GPIO_BUTTONS {
  uint8_t gpio_pin;
  uint8_t xinput_button;
} GPIO_BUTTONS_TABLE[NUM_BUTTONS] = {
  {23, BUTTON_A},
  {22, BUTTON_B},
  {21, BUTTON_X},
  {20, BUTTON_Y},
  {6,  BUTTON_LB},
  {9,  BUTTON_RB},
  {7,  BUTTON_BACK},
  {10, BUTTON_START},
  {12, BUTTON_LOGO},
  {8,  BUTTON_L3},
  {11, BUTTON_R3}
};
#define NUM_DPAD 4
const uint8_t DPAD_PINS[NUM_DPAD] = {2, 3, 4, 5};  // Up, Down, Left, Right

USBHost myusb;
USBHub hub1(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
#define COUNT_JOYSTICKS 4
JoystickController joysticks[COUNT_JOYSTICKS](myusb);

USBDriver *drivers[] = {&hub1, &joysticks[0], &joysticks[1], &joysticks[2], &joysticks[3], &hid1, &hid2, &hid3, &hid4};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "joystick[0D]", "joystick[1D]", "joystick[2D]", "joystick[3D]",  "HID1", "HID2", "HID3", "HID4"};
bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&joysticks[0], &joysticks[1], &joysticks[2], &joysticks[3]};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"joystick[0H]", "joystick[1H]", "joystick[2H]", "joystick[3H]"};
bool hid_driver_active[CNT_DEVICES] = {false};

uint8_t joystick_left_trigger_value[COUNT_JOYSTICKS] = {0};
uint8_t joystick_right_trigger_value[COUNT_JOYSTICKS] = {0};
uint64_t joystick_full_notify_mask = (uint64_t) - 1;

// Convert direction pad numbers to (X,Y) values. X and Y range from 0..255.
// The direction pad returns 0..F where 0xF is the idle/centered position.
// X=128, Y=128 center position.
const struct dPad2dir {
  int16_t xDir;
  int16_t yDir;
} dPad2dir_table[16] = {
  {0, JOY_MAX},       /* 0 Up */
  {JOY_MAX, JOY_MAX}, /* 1 Up Right */
  {JOY_MAX, 0},       /* 2 Right */
  {JOY_MAX, JOY_MIN}, /* 3 Down Right */
  {0, JOY_MIN},       /* 4 Down */
  {JOY_MIN, JOY_MIN}, /* 5 Down Left */
  {JOY_MIN, 0},       /* 6 Left */
  {JOY_MIN, JOY_MAX}, /* 7 Up Left */
  {0, 0},             /* 8 Invalid */
  {0, 0},             /* 9 Invalid */
  {0, 0},             /* 10 Invalid */
  {0, 0},             /* 11 Invalid */
  {0, 0},             /* 12 Invalid */
  {0, 0},             /* 13 Invalid */
  {0, 0},             /* 14 Invalid */
  {0, 0}              /* 15 Centered */
};

// Convert direction pad encoded values 0..8 to 4 direction buttons.
const struct dPad2buttons {
  bool up, right, down, left;
} dPad2buttons_table[9] = {
/* Up     Right   Down    Left buttons */
  {true,  false,  false,  false}, /* 0 Up */
  {true,  true,   false,  false}, /* 1 Up Right */
  {false, true,   false,  false}, /* 2 Right */
  {false, true,   true,   false}, /* 3 Down Right */
  {false, false,  true,   false}, /* 4 Down */
  {false, false,  true,   true }, /* 5 Down Left */
  {false, false,  false,  true }, /* 6 Left */
  {true,  false,  false,  true }, /* 7 Up Left */
  {false, false,  false,  false}  /* 8 Centered */
};

Bounce * buttons = new Bounce[NUM_BUTTONS];
Bounce * dpad = new Bounce[NUM_DPAD];

//=============================================================================
// Setup
//=============================================================================
void setup()
{
  Serial1.begin(115200);
  Serial1.println("\n\nUSB Host Joystick Testing");
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].attach( GPIO_BUTTONS_TABLE[i].gpio_pin , INPUT_PULLUP  );  //setup the bounce instance for the current button
    buttons[i].interval(10);                              // interval in ms
  }
  for (int i = 0; i < NUM_DPAD; i++) {
    dpad[i].attach( DPAD_PINS[i] , INPUT_PULLUP  );       //setup the bounce instance for the current button
    dpad[i].interval(10);                                 // interval in ms
  }
  myusb.begin();
  XInput.setAutoSend(false);
  XInput.begin();
}

uint32_t update_buttons(uint32_t buttons, uint32_t buttons_old,
    const uint8_t *button_map, size_t button_map_size)
{
  for (uint8_t i = 0; i < button_map_size; i++) {
    uint8_t button_out;
    if (button_map == NULL) {
      button_out = i;
    }
    else {
      button_out = button_map[i];
    }
    if (button_out != 255) {
      uint32_t button_bit_mask = 1 << i;
      if ((buttons & button_bit_mask) && !(buttons_old & button_bit_mask)) {
        // button fell/press (0->1 transition)
        XInput.press(button_out);
      }
      else if (!(buttons & button_bit_mask) && (buttons_old & button_bit_mask)) {
        // button rose/release (1->0 transition)
        XInput.release(button_out);
      }
    }
  }
  return buttons;
}

// Process input events from Logitech Extreme 3D Pro flight control stick
//
// The Logitech Extreme 3D Pro joystick (also known as a flight stick)
// has a large X,Y,twist joystick with an 8-way hat switch on top.
// This maps the large X,Y axes to the gamepad right thumbstick and
// the hat switch to the gamepad left thumbstick. There are six
// buttons on the top of the stick and six on the base. The twist
// used to control the stick buttons. Each gamepad thumbstick is
// also a button. For example, clicking the right thumbstick enables
// stealth mode in Zelda:BOTW.
//
// Map LE3DP button numbers to NS gamepad buttons
//    LE3DP buttons
//    0 = front trigger
//    1 = side thumb rest button
//    2 = top large left
//    3 = top large right
//    4 = top small left
//    5 = top small right
//
// Button array (2 rows, 3 columns) on base
//
//    7 9 11
//    6 8 10
void handle_le3dp(int joystick_index)
{
  static const uint8_t BUTTON_MAP[12] = {
    BUTTON_A,             // Front trigger
    BUTTON_B,             // Side thumb trigger
    BUTTON_X,             // top large left
    BUTTON_Y,             // top large right
    BUTTON_LB,            // top small left
    BUTTON_RB,            // top small right
    BUTTON_BACK,
    BUTTON_START,
    255,
    BUTTON_LOGO,
    BUTTON_L3,
    BUTTON_R3,
  };

  uint64_t axis_mask = joysticks[joystick_index].axisChangedMask();
  static uint8_t twist_old = 0;

  for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
    if (axis_mask & 1) {
      int ax = joysticks[joystick_index].getAxis(i);
      switch (i) {
        case 0:
          // Big stick X axis
          XInput.setJoystickX(JOY_LEFT, map(ax, 0, 0x3FF, JOY_MIN, JOY_MAX));
          break;
        case 1:
          // Big stick Y axis
          XInput.setJoystickY(JOY_LEFT, map(ax, 0, 0x3FF, JOY_MAX, JOY_MIN));
          break;
        case 5:
          // Twist axis maps to right stick X (look left and right)
          XInput.setJoystickX(JOY_RIGHT, map(ax, 0, 0xFF, JOY_MIN, JOY_MAX));
          twist_old = ax;
          break;
        case 6:
          // Slider
          break;
        case 9:
          // direction pad (hat switch)
          // Convert direction to X,Y for right thumbstick
          // If the twist axis is not centered(128), let twist control
          // the X direction (look left and right). If the twist axis
          // is centered, the hat controls look left and right.
          if (twist_old == 128) {
            XInput.setJoystickX(JOY_RIGHT, dPad2dir_table[ax].xDir);
          }
          XInput.setJoystickY(JOY_RIGHT, dPad2dir_table[ax].yDir);
          break;
        default:
          break;
      }
    }
  }

  static uint32_t buttons_old = 0;
  buttons_old = update_buttons(joysticks[joystick_index].getButtons(),
      buttons_old, BUTTON_MAP, sizeof(BUTTON_MAP));
}

// Process input events from Thrustermaster T.16000 flight control stick
//
// Map T16K button numbers to NS gamepad buttons
// The Thrustmaster T.16000M ambidextrous joystick (also known as a flight stick)
// has a large X,Y,twist joystick with an 8-way hat switch on top.
// This function maps the large X,Y axes to the gamepad right thumbstick and
// the hat switch to the gamepad left thumbstick. There are four
// buttons on the top of the stick and 12 on the base. The twist
// used to control the stick buttons. Each gamepad thumbstick is
// also a button. For example, clicking the right thumbstick enables
// stealth mode in Zelda:BOTW.
//
//    Map T16K button numbers to NS gamepad buttons
//    T16K buttons
//    0 = trigger
//    1 = top center
//    2 = top left
//    3 = top right
//
//    Button array on base, left side
//
//    4
//    9 5
//      8 6
//        7
//
//    Button array on base, right side
//
//          10
//       11 15
//    12 14
//    13
void handle_t16k(int joystick_index)
{
  static const uint8_t BUTTON_MAP[16] = {
    BUTTON_A,             // Trigger
    BUTTON_B,             // Top center
    BUTTON_X,             // Top Left
    BUTTON_Y,             // Top Right
    BUTTON_LB,            // Base left 4
    BUTTON_RB,            // Base left 5
    BUTTON_BACK,          // Base left 6
    BUTTON_START,         // Base left 7
    255,                  // Base left 8
    BUTTON_LOGO,          // Base left 9
    BUTTON_L3,            // Base right 10
    BUTTON_R3,            // Base right 11
    255,                  // Base right 12
    255,                  // Base right 13
    255,                  // Base right 14
    255,                  // Base right 15
  };
  uint64_t axis_mask = joysticks[joystick_index].axisChangedMask();
  static uint8_t twist_old = 0;

  for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
    if (axis_mask & 1) {
      int ax = joysticks[joystick_index].getAxis(i);
      switch (i) {
        case 0:
          // Big stick X axis
          XInput.setJoystickX(JOY_LEFT, map(ax, 0, 0x3FFF, JOY_MIN, JOY_MAX));
          break;
        case 1:
          // Big stick Y axis
          XInput.setJoystickY(JOY_LEFT, map(ax, 0, 0x3FFF, JOY_MAX, JOY_MIN));
          break;
        case 5:
          // Twist axis maps to right stick X (look left and right)
          XInput.setJoystickX(JOY_RIGHT, map(ax, 0, 0xFF, JOY_MIN, JOY_MAX));
          twist_old = ax;
          break;
        case 6:
          // Slider
          break;
        case 9:
          // direction pad (hat switch)
          // Convert direction to X,Y for right thumbstick
          // If the twist axis is not centered(128), let twist control
          // the X direction (look left and right). If the twist axis
          // is centered, the hat controls look left and right.
          if (twist_old == 128) {
            XInput.setJoystickX(JOY_RIGHT, dPad2dir_table[ax].xDir);
          }
          XInput.setJoystickY(JOY_RIGHT, dPad2dir_table[ax].yDir);
          break;
        default:
          break;
      }
    }
  }

  static uint32_t buttons_old = 0;
  buttons_old = update_buttons(joysticks[joystick_index].getButtons(),
      buttons_old, BUTTON_MAP, sizeof(BUTTON_MAP));
}

void handle_gpio()
{
  for (int i = 0; i < NUM_BUTTONS; i++) {
    // Update the Bounce instance
    buttons[i].update();
    // Button fell means button pressed
    if ( buttons[i].fell() ) {
      XInput.press(GPIO_BUTTONS_TABLE[i].xinput_button);
    }
    else if ( buttons[i].rose() ) {
      XInput.release(GPIO_BUTTONS_TABLE[i].xinput_button);
    }
  }

  for (uint8_t i = 0; i < sizeof(DPAD_PINS); i++) {
    // Update the Bounce instance
    dpad[i].update();
    // Button fell means button pressed
    if ( dpad[i].fell() ) {
      XInput.setDpad((XInputControl)(i+DPAD_UP), true);
    }
    else if ( dpad[i].rose() ) {
      XInput.setDpad((XInputControl)(i+DPAD_UP), false);
    }
  }

  // If nothing is connected to the analog input pins, analogRead returns
  // random garbage. Enable only when joysticks are connected.
#if ANALOG_JOYSTICKS
  XInput.setTrigger(TRIGGER_LEFT,  map(analogRead(5), 0, ADC_Max, 0, 255));
  XInput.setTrigger(TRIGGER_RIGHT, map(analogRead(4), 0, ADC_Max, 0, 255));
  XInput.setJoystickY(JOY_LEFT,  map(analogRead(3), 0, ADC_Max, JOY_MAX, JOY_MIN));
  XInput.setJoystickX(JOY_LEFT,  map(analogRead(2), 0, ADC_Max, JOY_MIN, JOY_MAX));
  XInput.setJoystickY(JOY_RIGHT, map(analogRead(1), 0, ADC_Max, JOY_MAX, JOY_MIN));
  XInput.setJoystickX(JOY_RIGHT, map(analogRead(0), 0, ADC_Max, JOY_MIN, JOY_MAX));
#endif
}

void handle_xbox360u(int joystick_index)
{
  static const uint8_t BUTTON_MAP[16] = {
    255,            // DPAD UP
    255,            // DPAD DOWN
    255,            // DPAD LEFT
    255,            // DPAD RIGHT
    BUTTON_START,   //
    BUTTON_BACK,    //
    BUTTON_L3,      //
    BUTTON_R3,      //
    BUTTON_LB,      //
    BUTTON_RB,      //
    BUTTON_LOGO,    //
    255,            //
    BUTTON_A,       //
    BUTTON_B,       //
    BUTTON_X,       //
    BUTTON_Y,       //
  };
  uint64_t axis_mask = joysticks[joystick_index].axisChangedMask();

  for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
    if (axis_mask & 1) {
      int ax = joysticks[joystick_index].getAxis(i);
      switch (i) {
        case 0: // Left thumbstick X axis
          XInput.setJoystickX(JOY_LEFT, ax);
          break;
        case 1: // Left thumbstick Y axis
          XInput.setJoystickY(JOY_LEFT, ax);
          break;
        case 2: // Right thumbstick X axis
          XInput.setJoystickX(JOY_RIGHT, ax);
          break;
        case 3: // Right thumbstick Y axis
          XInput.setJoystickY(JOY_RIGHT, ax);
          break;
        case 4: // Left trigger
          XInput.setTrigger(TRIGGER_LEFT, ax);
          break;
        case 5: // Right trigger
          XInput.setTrigger(TRIGGER_RIGHT, ax);
          break;
        default:
          break;
      }
    }
  }

  static uint32_t buttons_old = 0;
  uint32_t buttons = joysticks[joystick_index].getButtons();
  buttons_old = update_buttons(buttons, buttons_old, BUTTON_MAP,
      sizeof(BUTTON_MAP));

  static uint8_t dpad_old = 0;
  uint8_t dpad_bits = buttons & 0xf;
  uint8_t dpad_change = dpad_bits ^ dpad_old;
  for (uint8_t i = 0; dpad_change != 0; i++, dpad_change >>= 1) {
    if (dpad_change & 1) {
      XInput.setDpad((XInputControl)(DPAD_UP+i), (dpad_bits & (1 << i)) != 0);
    }
  }
  dpad_old = dpad_bits;
}

void handle_xboxoneu(int joystick_index)
{
  static const uint8_t BUTTON_MAP[16] = {
    255,            // ?
    255,            // ?
    BUTTON_START,   // Menu
    BUTTON_BACK,    // View
    BUTTON_A,       // A
    BUTTON_B,       // B
    BUTTON_X,       // X
    BUTTON_Y,       // Y
    255,            // DPAD UP
    255,            // DPAD DOWN
    255,            // DPAD LEFT
    255,            // DPAD RIGHT
    BUTTON_LB,      // LB
    BUTTON_RB,      // RB
    BUTTON_L3,      // L3 (left stick button)
    BUTTON_R3,      // R3 (rigght stick button)
  };
  uint64_t axis_mask = joysticks[joystick_index].axisChangedMask();

  for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
    if (axis_mask & 1) {
      int ax = joysticks[joystick_index].getAxis(i);
      switch (i) {
        case 0: // Left thumbstick X axis
          XInput.setJoystickX(JOY_LEFT, ax);
          break;
        case 1: // Left thumbstick Y axis
          XInput.setJoystickY(JOY_LEFT, ax);
          break;
        case 2: // Right thumbstick X axis
          XInput.setJoystickX(JOY_RIGHT, ax);
          break;
        case 5: // Right thumbstick Y axis
          XInput.setJoystickY(JOY_RIGHT, ax);
          break;
        case 3: // Left trigger
          XInput.setTrigger(TRIGGER_LEFT, ax >> 2);
          break;
        case 4: // Right trigger
          XInput.setTrigger(TRIGGER_RIGHT, ax >> 2);
          break;
        default:
          break;
      }
    }
  }

  static uint32_t buttons_old = 0;
  uint32_t buttons = joysticks[joystick_index].getButtons();
  buttons_old = update_buttons(buttons, buttons_old, BUTTON_MAP,
      sizeof(BUTTON_MAP));

  static uint8_t dpad_old = 0;
  uint8_t dpad_bits = (buttons >> 8) & 0xf;
  uint8_t dpad_change = dpad_bits ^ dpad_old;
  for (uint8_t i = 0; dpad_change != 0; i++, dpad_change >>= 1) {
    if (dpad_change & 1) {
      XInput.setDpad((XInputControl)(DPAD_UP+i), (dpad_bits & (1 << i)) != 0);
    }
  }
  dpad_old = dpad_bits;
}

void handle_ps3(int joystick_index)
{
  static const uint8_t BUTTON_MAP[17] = {
    BUTTON_BACK,    // PS3 Select
    BUTTON_L3,      // PS3 Left stick button
    BUTTON_R3,      // PS3 Right stick button
    BUTTON_START,   // PS3 Start
    255,            // PS3 DPAD Up
    255,            // PS3 DPAD Right
    255,            // PS3 DPAD Down
    255,            // PS3 DPAD Left
    255,            // PS3 L2
    255,            // PS3 R2
    BUTTON_LB,      // PS3 L1
    BUTTON_RB,      // PS3 R1
    BUTTON_Y,       // PS3 Triangle
    BUTTON_B,       // PS3 Circle
    BUTTON_A,       // PS3 Cross
    BUTTON_X,       // PS3 Square
    BUTTON_LOGO,    // PS3 Logo
  };
  uint64_t axis_mask = joysticks[joystick_index].axisChangedMask();
  for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
    if (axis_mask & 1) {
      int ax = joysticks[joystick_index].getAxis(i);
      switch (i) {
        case 0: // Left thumbstick X axis
          XInput.setJoystickX(JOY_LEFT, map(ax, 0, 255, JOY_MIN, JOY_MAX));
          break;
        case 1: // Left thumbstick Y axis
          XInput.setJoystickY(JOY_LEFT, map(ax, 0, 255, JOY_MAX, JOY_MIN));
          break;
        case 2: // Right thumbstick X axis
          XInput.setJoystickX(JOY_RIGHT, map(ax, 0, 255, JOY_MIN, JOY_MAX));
          break;
        case 5: // Right thumbstick Y axis
          XInput.setJoystickY(JOY_RIGHT, map(ax, 0, 255, JOY_MAX, JOY_MIN));
          break;
        case 18: // Left trigger
          XInput.setTrigger(TRIGGER_LEFT, ax);
          break;
        case 19: // Right trigger
          XInput.setTrigger(TRIGGER_RIGHT, ax);
          break;
        default:
          break;
      }
    }
  }

  static uint32_t buttons_old = 0;
  uint32_t buttons = joysticks[joystick_index].getButtons();
  buttons_old = update_buttons(buttons, buttons_old, BUTTON_MAP,
      sizeof(BUTTON_MAP));

  static const XInputControl DPAD_REMAP[4] = {
    DPAD_UP,
    DPAD_RIGHT,
    DPAD_DOWN,
    DPAD_LEFT
  };
  static uint8_t dpad_old = 0;
  uint8_t dpad_bits = (buttons >> 4) & 0xf;
  uint8_t dpad_change = dpad_bits ^ dpad_old;
  for (uint8_t i = 0; dpad_change != 0; i++, dpad_change >>= 1) {
    if (dpad_change & 1) {
      XInput.setDpad(DPAD_REMAP[i], (dpad_bits & (1 << i)) != 0);
    }
  }
  dpad_old = dpad_bits;
}

void handle_ps4(int joystick_index)
{
  static const uint8_t BUTTON_MAP[13] = {
    BUTTON_X,       // PS4 Square
    BUTTON_A,       // PS4 Cross
    BUTTON_B,       // PS4 Circle
    BUTTON_Y,       // PS4 Triangle
    BUTTON_LB,      // PS4 L1
    BUTTON_RB,      // PS4 R1
    255,            // PS4 L2 Trigger
    255,            // PS4 R2 Trigger
    BUTTON_BACK,    // PS4 Share
    BUTTON_START,   // PS4 Options
    BUTTON_L3,      // PS4 Left stick button
    BUTTON_R3,      // PS4 Right stick button
    BUTTON_LOGO,    // PS4 Logo
  };
  uint64_t axis_mask = joysticks[joystick_index].axisChangedMask();

  for (uint8_t i = 0; axis_mask != 0; i++, axis_mask >>= 1) {
    if (axis_mask & 1) {
      int ax = joysticks[joystick_index].getAxis(i);
      switch (i) {
        case 0: // Left thumbstick X axis
          XInput.setJoystickX(JOY_LEFT, map(ax, 0, 255, JOY_MIN, JOY_MAX));
          break;
        case 1: // Left thumbstick Y axis
          XInput.setJoystickY(JOY_LEFT, map(ax, 0, 255, JOY_MAX, JOY_MIN));
          break;
        case 2: // Right thumbstick X axis
          XInput.setJoystickX(JOY_RIGHT, map(ax, 0, 255, JOY_MIN, JOY_MAX));
          break;
        case 3: // Left trigger
          XInput.setTrigger(TRIGGER_LEFT, ax);
          break;
        case 4: // Right trigger
          XInput.setTrigger(TRIGGER_RIGHT, ax);
          break;
        case 5: // Right thumbstick Y axis
          XInput.setJoystickY(JOY_RIGHT, map(ax, 0, 255, JOY_MAX, JOY_MIN));
          break;
        case 9: // DPAD
          if (0 <= ax && ax <= 8) {
            const struct dPad2buttons *element = &dPad2buttons_table[ax];
            XInput.setDpad(element->up, element->down, element->left, element->right);
          }
          break;
        default:
          break;
      }
    }
  }

  static uint32_t buttons_old = 0;
  uint32_t buttons = joysticks[joystick_index].getButtons();
  buttons_old = update_buttons(buttons, buttons_old, BUTTON_MAP,
      sizeof(BUTTON_MAP));
}

//=============================================================================
// loop
//=============================================================================
void loop()
{
  myusb.Task();
  PrintDeviceListChanges();

  for (int joystick_index = 0; joystick_index < COUNT_JOYSTICKS; joystick_index++) {
    if (joysticks[joystick_index].available()) {
      JoystickController::joytype_t joystickType = joysticks[joystick_index].joystickType();
      switch (joystickType) {
        case JoystickController::PS3:         // Sony Playstation 3
          handle_ps3(joystick_index);
          break;
        case JoystickController::PS4:         // Sony Playstation 4
          handle_ps4(joystick_index);
          break;
        case JoystickController::XBOXONE:     // Xbox One USB
          handle_xboxoneu(joystick_index);
          break;
        case JoystickController::XBOX360:     // Xbox 360 wireless
        case JoystickController::XBOX360USB:  // Xbox 360 USB
          handle_xbox360u(joystick_index);
          break;
        case JoystickController::EXTREME3D: // Logitech Extreme 3D Pro
          handle_le3dp(joystick_index);
          break;
        case JoystickController::T16000M: // Thrustmaster T.16000M
          handle_t16k(joystick_index);
          break;
        default:
          break;
      }
      joysticks[joystick_index].joystickDataClear();
    }
  }
  handle_gpio();
	XInput.send();
}

//=============================================================================
// Show when devices are added or removed
//=============================================================================
void PrintDeviceListChanges() {
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial1.printf("*** Device %s - disconnected ***\r\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial1.printf("*** Device %s %x:%x - connected ***\r\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) Serial1.printf("  manufacturer: %s\r\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) Serial1.printf("  product: %s\r\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) Serial1.printf("  Serial1: %s\r\n", psz);
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial1.printf("*** HID Device %s - disconnected ***\r\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial1.printf("*** HID Device %s %x:%x - connected ***\r\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial1.printf("  manufacturer: %s\r\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial1.printf("  product: %s\r\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial1.printf("  Serial1: %s\r\n", psz);
      }
    }
  }
}
