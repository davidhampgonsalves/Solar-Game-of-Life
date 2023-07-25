Solar powered, e-ink based version of [Conways Game of Life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life) which runs on an ESP-32 and uses the ULP processor to monitor its power level to only calculate the next iteration and update the display when we reach the given 3.3v threshold.

<p align="center">
  <img width="800" src="https://github.com/davidhampgonsalves/solar-game-of-life/assets/178893/fb31dc91-b645-4bbe-a6fb-8fa406ff8aac">
</p>

You can read more about it [here](https://davidhampgonsalves.com/solar-powered-conways-game-of-life/).

## Pins
* Voltage monitoring - 3.3v - IO34
* RST - switch - GND

### Display
* BUSY (Purple) - IO4
* RST (White) - IO0
* DC (Green) - IO2
* CS (Orange) - IO5
* CLK (Yellow) - IO18
* DIN (Blue) - IO23
* GND (Black) - GND
* 3.3V (Red) - 3.3

## Notes for Myself
  ### Switches
  Because I am using pin 0 for driving the displays RST line I need to disable it during booting or the board will go into download mode. Thus it needs to be in the off position on start.

  ### Resetting
  1. Turn off unit with left switch.
  2. Turn off display pin.
  3. Turn on power and then after a second turn on the display switch.
  4. If battery is above 3.3v the screen should refresh in ~10s.
    1. If not you need to ground the reset pin.
