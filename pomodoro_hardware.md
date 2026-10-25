## Parts
| Part | Qty | Note |
|---|---|---|
| ESP8266 NodeMCU | 1 | Wi-Fi, 4 MB flash |
| 1.8" ST7735 TFT 128×160 | 1 | Portrait |
| Passive buzzer | 1 | 3.3–5 V type |
| BC547 (or 2N2222) NPN | 1 | Buzzer driver |
| 1 kΩ resistor | 1 | Transistor base |
| 1N4148 diode | 1 | Across the buzzer, protects the transistor |
| Tactile push buttons | 3 | MODE, OK, BACK |
| 10 kΩ resistor | 1 | Pull-up for D0 |
| Breadboard + jumpers | — | Perfboard later |

## Wiring
| Signal | NodeMCU pin | GPIO |
|---|---|---|
| TFT SCK | D5 | 14 |
| TFT SDA (MOSI) | D7 | 13 |
| TFT CS | D8 | 15 |
| TFT A0 / DC | D3 | 0 |
| TFT RESET | RST pin | — |
| TFT LED (backlight) | D4 (PWM dimming) | 2 |
| TFT VCC / GND | 3V3 / GND | — |
| Buzzer driver | D6 → 1 kΩ → BC547 base | 12 |
| MODE button | D1 → GND | 5 |
| OK / START button | D2 → GND | 4 |
| BACK button | D0 → GND, plus 10 kΩ to 3V3 | 16 |
| Spare | D9/D10 (RX/TX) | 3/1, kept free for serial debugging |

**Buzzer circuit:** 3V3 → buzzer + → buzzer − → BC547 collector. Emitter goes to GND, and the 1N4148 sits across the buzzer with its stripe toward 3V3.

## Button roles
- **MODE:** cycle presets 1 → 2 → 25 → 50 min. Long-press to mute.
- **OK:** start or pause. On the pet screen, it selects the care action.
- **BACK:** cancel the session (press twice to confirm), or go back a screen.

## Power (to decide)
- **USB only:** plug into a 5 V adapter or laptop. Simplest, with no extra parts.
- **Battery:** add a 3.7 V LiPo (1000–2000 mAh) + TP4056 charger with protection + a slide switch, fed through the NodeMCU's Vin. Expect several hours per charge with the backlight on, since Wi-Fi and the TFT draw about 80–120 mA.

## Pins left
Only RX/TX remain. That's why it's 3 buttons, not 4. A 4th button would go on A0.