# Mechanical Ocean firmware

Arduino firmware for the 5 × 5 kinetic installation described in the thesis
*Mechanical Ocean* by Mingde Liu.

## Run the sketch

Open `Wave1_Shenzhen/Wave1_Shenzhen.ino` in Arduino IDE with the
`sea_data.h` header in the same folder. Select **Arduino UNO R4 WiFi** and
install the **Adafruit PWM Servo Driver Library** and its dependencies.

The sketch uses analog inputs A0 and A1 for the sound sensors and a 100 kHz
I²C bus for the PCA9685 boards. PCA1 at 0x40 drives 13 servos on channels
0–12. PCA2 at 0x41 drives 12 servos on channels 0–11. D8 connects to both
output-enable inputs. Use USB-C to power the Uno, its 5 V logic rail for
PCA9685 VCC, and a separate 5 V servo supply for both V+ rails. The supplies
and boards share a ground.

On power-up the sketch checks for both boards, centres the lifts, starts the
44-second exhibition loop and calibrates the sound sensors. Keep the area
quiet during the eight-second calibration. The stored data play offline.

Use the serial monitor at 115200 baud with Newline for bench tests. Send
`m` to centre the lifts, then `w` for full playback or `f` for exhibition
playback. Send `a` to calibrate and arm the sound inputs, `s` to hold,
or `x` to turn off PWM signals.

## Source data

`data/shenzhen_2026.csv` contains 168 hourly wave-model records from 22–28
October 2026 UTC. The grid point is 22.375°N, 114.45836°E, south of the
Dapeng Peninsula. `data/source_manifest.json` records the source, bounds,
selection and API request.

Météo-France MFWAM output was accessed through the [Open-Meteo Marine
Weather API](https://open-meteo.com/en/docs/marine-weather-api), using
[E.U. Copernicus Marine Service Information](https://doi.org/10.48670/moi-00017).
The firmware maps significant wave height, mean wave period and mean
wave direction to pulse amplitude, oscillation period and phase across the
tiles. See the thesis for the mapping and its limitations.
