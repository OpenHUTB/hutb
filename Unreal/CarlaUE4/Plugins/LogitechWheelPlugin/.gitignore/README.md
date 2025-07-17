# LogitechWheelPlugin
A plugin for UE4 that allows you to use a number of Logitech steering wheels and joysticks as an input device. 

Supported Devices
G27, Driving Force GT, G25, Momo Racing, Momo Force, Driving Force Pro, Driving Force, Nascar Racing Wheel, Formula Force, Formula Force GP, Force 3D Pro, Extreme 3D Pro, Freedom 24, Attack 3, Force 3D, Strike Force 3D, G940 Joystick, G940 Throttle, G940 Pedals, Rumblepad, Rumblepad 2, Cordless Rumblepad 2, Cordless Gamepad, Dual Action Gamepad, Precision Gamepad 2, Chillstream, G29, G920

Blueprint Library: You can use the plugin completely through Blueprints. No C++ Required. A few of the BP functions include...
Get Wheel State
Play various force feedback effects (Centering Spring, Collision, Bumpy Road, Slippery Road, Dirt Road, Car Airborne, etc)
Play LEDs
Get Shifter Mode
Is Wheel Connected
Is Button Triggered, Released

Input Keys: I added input keys for the following inputs, which are kind of obsolete now that 4.15 has generic USB input keys for wheels.
Axis
Wheel
Throttle Pedal
Brake Pedal
Clutch Pedal
D-Pad (POV1)
Action
Face Buttons
Bumpers
Special Buttons
Stick Buttons
Xbox Button

KNOWN ISSUES:
Broken Functions - WheelButtonIsPressed, WheelGetFriendlyProductName

Pedals will output a value of 0.5 until the wheel/pedals receive any kind of input.

When playing in the editor, you must shutdown and initialize the wheel OnBeginPlay. This is not necessary when playing in standalone mode or a packaged game.
