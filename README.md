# Arduino XInput Converter for Teensy 3.6

The Arduino XInput Converter (AXIC) for Teensy 3.6 uses the [Arduino XInput
Library](https://github.com/dmadison/ArduinoXInput) on the USB device port and
the Teensy USBHost_t36 library on the USB host port. AXIC translates axis and
button inputs from joysticks and gamepads into XInput (XBox 360) outputs. For
example, plug a PS4 DS4 controller into the Teensy 3.6 USB host port then plug
the Teensy 3.6 into a PC. The PC sees an XBox 360 controller, not the DS4.

There is a similar project for Nintendo Switch at https://github.com/gdsports/NSGadget_Teensy.

The Teensy 3.6 acts as a Teensy-in-the-middle between one or more game
controllers and the XInput system. This can be used for co-pilot mode as well
as simulator control panel/button boxes (car, truck, tractor, etc).

AXIC supports buttons and analog joysticks on the Teensy 3.6 pins so building a custom
controller is easy. A gamepad can be connected to the USB host port at the same
time. For example, connect to two foot pedals via GPIO pins while using a
gamepad on the USB host port.

AXIC does not do console controller authentication so it cannot be used with
consoles. However, since it emulates an XBox 360 controller it may be usable
with controller converters such as Titan Two or MayFlash. I verified a
Teensy 3.6 running this project works on a Sony PS4 when connected using
a MayFlash Magic S Pro.

```
PS4 -- MayFlash Magic S Pro -- Teensy 3.6 -- Thrustmaster T.16000M Flight Stick
```

AXIC supports the following gamepads.

* Dual Shock 3
* Dual Shock 4
* Xbox 360
* XBox One

AXIC supports the following USB HID joysticks.

* Logitech Extreme 3D Pro
* Thrustmaster T.16000M FCS
* Dragon Rise

The T.16000M can be configured for left or right hand users.

The big stick X and Y axes on the Logitech and Thrustmaster joysticks are
mapped to the left thumbstick. The big stick twist/Z axis is mapped to the
right thumbstick X axis for look left/right. The hat switches are mapped to the
right gamepad thumbstick for look up/down/left/right.

The Dragon Rise joystick controller is frequently included in low cost
arcade button and stick kits. It suports up to 12 buttons and 1 8-way
stick.

This project is based on the Arduino XInput libraries. The Arduino IDE,
Teensyduino, and the following libraries must be installed and working to use
this project. This project modifies Teensyduino USBHost_t36 files by
overwriting them.

## Install

Install the [Arduino IDE 1.8.13](https://www.arduino.cc/en/Main/Software) and
[Teensyduino 1.53](https://www.pjrc.com/teensy/td_download.html). I highly
recommend extracting the IDE zip or tar in a separate directory from the
default Arduino directory.

On a Linux system, the following instructions will install the IDE into
`~/axic/arduino-1.8.13`. Creating the portable directory ensures the sketches and
libraries are stored separately from the default Arduino sketches and
libraries.

```
cd
mkdir axic
cd axic
tar xf ~/Downloads/arduino-1.8.13-linux64.tar.xz
cd arduino-1.8.13
mkdir portable
```
Run the Teensyduino installer. Make sure to install in the arduino-1.8.13
directory created above.

Use the IDE Library Manager to install the XInput library by David Madison. For
more details see https://github.com/dmadison/ArduinoXInput.

Follow the instructions at https://github.com/dmadison/ArduinoXInput_Teensy
to merge in support for XInput on the Teensy USB device interface.

Copy the files in this repo's hardware directory to the arduino-1.8.13/hardware
directory. This will overwrite Teensyduino files to add XInput to the
USBHost_t36 library. The library supports XBox 360 wireless but this adds
support for XBox 360 USB.

Start the IDE with all changes.

```
cd ~/axic/arduino-1.8.13
./arduino&
```

## examples/XInputAC

Teensy 3.6 USB XInput gamepad pass through and conversion.

Select "XInput" from the "Tools > USB Type" menu.

This example contains proof of concept code for various modes of operation. All
USB controllers are active so the result is a kind of co-pilot mode. The XInput
system (for example, Windows) sees a single gamepad but one or more controllers
connected to the USB host port may be in use. For example, one person is
driving with one controller while the other is aiming and shooting using a
different controller.

### Teensy 3 pin out with button and axis assignments

For a graphic Teensy 3.6 pin out diagram see the [following](https://www.pjrc.com/teensy/teensy36.html).

|Function       |Pin    |Pin    |Function   |
|---------------|-------|-------|-----------|
|               |GND    |Vin    |           |
|RX1            |0      |GND    |           |
|TX1            |1      |3.3V   |           |
|DPad Up        |2      |23/A9  |A          |
|DPad Right     |3      |22/A8  |B          |
|DPad Down      |4      |21/A7  |X          |
|DPad Left      |5      |20/A6  |Y          |
|LB             |6      |19/A5  |Left trigger|
|BACK           |7      |18/A4  |Right trigger|
|L3,LSB         |8      |17/A3  |Left X axis|
|RB             |9      |16/A2  |Left Y axis|
|START          |10     |15/A1  |Right X axis|
|R3,RSB         |11     |14/A0  |Right Y axis|
|LOGO           |12     |13     |           |

Serial1 (RX1/TX1) pins are available for debug output.

Note 1: The XInput thumbsticks are clickable so they also count as buttons. See
L3/R3. Also known as LSB = Left Stick Button and RSB = Right Stick Button

### Use flight control stick as a gamepad

Use one hand to control left and right sticks.
The hat switch at the top of the stick is mapped to the right thumbstick.
The big stick twist axis is also mapped to the right thumbstick X axis so it
can be used to look left and right.
A nice feature of the Thrustmaster T.16000M is it can be configured for left hands using a
screwdriver. See the [manual](http://ts.thrustmaster.com/download/accessories/manuals/T16000M/T16000M-User_manual.pdf) for details.

### Gamepad macro recorder
Maybe someday use to record and play macros. Plug in an XBox 360 game
controller and record the axis and button events to a file on the micro SD
card.

### Button mapping from micro SD card
Maybe someday load button mappings from files on the microSD card.
