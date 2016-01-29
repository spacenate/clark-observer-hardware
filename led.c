/* Name: led.c
 * Project: Olark Observer Hardware
 * Author: Nathan Sollenberger
 * Creation Date: 2016-01-03
 * Copyright: (c) 2016 Nathan Sollenberger
 * License: GPL
 */
#include "led.h"

/* These pins may be re-ordered, but if different pins are used,
   enablePWM must be updated as well. Be sure to avoid conflict
   with USB DATA- and DATA+ pins */
#define RED_PIN PB4
#define GREEN_PIN PB0
#define BLUE_PIN PB1
#define RED_OCP OCR1B
#define GREEN_OCP OCR0A
#define BLUE_OCP OCR0B

#define PULSE_MIN_BRIGHTNESS 140
#define PULSE_MAX_BRIGHTNESS 255
#define PULSE_PAUSE_DURATION 60
#define PULSE_INCREASE 1
#define PULSE_DECREASE -1
#define PULSE_PAUSE 0
#define FLASH_DURATION 30
#define RAINBOW_TRANSITION_DURATION 200
#define MOODLIGHT_FADE_DELAY 2

uint8_t fadePhase;
uint8_t fadeValue;

uint8_t colorMask[3];
uint8_t nextColor[3];

void initLED(void)
{
    DDRB |= _BV(RED_PIN) | _BV(BLUE_PIN) | _BV(GREEN_PIN);
}

void toggleLED(void)
{
    PORTB ^= _BV(RED_PIN) | _BV(BLUE_PIN) | _BV(GREEN_PIN);
}

void turnOffLED(void)
{
    PORTB &= ~( _BV(RED_PIN) | _BV(BLUE_PIN) | _BV(GREEN_PIN) );
}

void initTimers(void)
{
    TCCR0B |= _BV(CS02) | _BV(CS00);     // clock/1024 ~60Hz
    TCCR1 |= _BV(CS13) | _BV(CS11) | _BV(CS10); // clock/1024 ~60Hz
}

void enableIdleTimer(void)
{
    disablePWM();
    turnOffLED();
    enableFade();
    fadeValue = 0;
    fadePhase = 0;
    fadeFunction = &idleTimer;
}

void enablePulseEffect(uint8_t red, uint8_t green, uint8_t blue)
{
    enablePWM();
    enableFade();
    fadePhase = PULSE_INCREASE;
    fadeValue = PULSE_MIN_BRIGHTNESS;
    colorMask[0] = red;
    colorMask[1] = green;
    colorMask[2] = blue;
    fadeFunction = &pulseEffect;
}

void enableFlashEffect(uint8_t red, uint8_t green, uint8_t blue)
{
    disablePWM();
    turnOffLED();
    enableFade();
    // Turn on the LED the next time flashEffect is called
    fadeValue = FLASH_DURATION - 1;
    colorMask[0] = red;
    colorMask[1] = green;
    colorMask[2] = blue;
    fadeFunction = &flashEffect;
}

void enableMoodlightEffect()
{
    enablePWM();
    enableFade();
    fadePhase = 0;
    fadeValue = 0;
    nextColor[0] = colorMask[0];
    nextColor[1] = colorMask[1];
    nextColor[2] = colorMask[2];
    fadeFunction = &moodLightEffect;
}

void setPWMDutyCycle(uint8_t redCycle, uint8_t greenCycle, uint8_t blueCycle) {
    RED_OCP = redCycle;
    GREEN_OCP = greenCycle;
    BLUE_OCP = blueCycle;
}

void enablePWM(void)
{
    cli();
    // Fast PWM mode, clear on compare match
    TCCR0A |= _BV(WGM01) | _BV(WGM00) | _BV(COM0A1) | _BV(COM0B1);
    // PWM mode, clear on compare match with PB3 not connected
    GTCCR |= _BV(PWM1B) | _BV(COM1B1);
    setPWMDutyCycle(0, 0, 0);
    sei();
}

void disablePWM(void)
{
    TCCR0A = 0;
    GTCCR = 0;
}

void enableFade(void)
{
    cli();
    // Enable overflow interrupt
    TIMSK = _BV(TOIE1);
    sei();
}

void disableFade(void)
{
    TIMSK &= ~_BV(TOIE1);
}

ISR(TIMER1_OVF_vect)
{
    fadeTick = 1;
}

void pulseEffect(void)
{
static uint8_t pauseValue;

    if (fadePhase & (PULSE_INCREASE | PULSE_DECREASE)) {
        fadeValue += fadePhase;
        uint8_t redCycle = ((uint16_t)fadeValue * colorMask[0]) >> 8;
        uint8_t greenCycle = ((uint16_t)fadeValue * colorMask[1]) >> 8;
        uint8_t blueCycle = ((uint16_t)fadeValue * colorMask[2]) >> 8;
        setPWMDutyCycle(redCycle, greenCycle, blueCycle);
        // Detect min and max
        if (fadeValue == PULSE_MIN_BRIGHTNESS) {
            fadePhase = PULSE_PAUSE;
            pauseValue = 0;
        } else if (fadeValue == PULSE_MAX_BRIGHTNESS) {
            fadePhase = PULSE_DECREASE;
        }
    } else {
        pauseValue++;
        if (pauseValue == PULSE_PAUSE_DURATION)
            fadePhase = PULSE_INCREASE;
    }
}

void flashEffect(void)
{
    fadeValue++;
    if (fadeValue == FLASH_DURATION) {
        uint8_t redCycle = ((uint16_t)255 * colorMask[0]) >> 8;
        uint8_t greenCycle = ((uint16_t)255 * colorMask[1]) >> 8;
        uint8_t blueCycle = ((uint16_t)255 * colorMask[2]) >> 8;
        enablePWM();
        setPWMDutyCycle(redCycle, greenCycle, blueCycle);
    } else if (fadeValue == (FLASH_DURATION * 2)) {
        disablePWM();
        turnOffLED();
        fadeValue = 0;
    }
}

void idleTimer(void)
{
    fadeValue++;
    if (fadeValue == 255) {
        fadeValue = 0;
        fadePhase++;
        if (fadePhase == 1) {
            enableMoodlightEffect();
        }
    }
}

void rainbowEffect(void)
{
    if (fadeValue < 3) {
        fadeValue++;
        return;
    }
    fadeValue = 0;
    switch (fadePhase) {
        case 0:
            if (RED_OCP == 0xFE) // ff0000
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP+2, 0, 0);
            break;
        case 1:
            if (GREEN_OCP == 0x80) // ff8000
                fadePhase++;
            else
                setPWMDutyCycle(255, GREEN_OCP+1, 0);
            break;
        case 2:
            if (GREEN_OCP == 0xFE) // c0ff00
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP-1, GREEN_OCP+2, 0);
            break;
        case 3:
            if (RED_OCP == 0x80) // 80ff00
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP-1, 255, 0);
            break;
        case 4:
            if (RED_OCP == 0x00) // 00FF40
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP-2, 255, BLUE_OCP+1);
            break;
        case 5:
            if (BLUE_OCP == 0xFD) // 00FFFF
                fadePhase++;
            else
                setPWMDutyCycle(0, 255, BLUE_OCP+3);
            break;
        case 6:
            if (GREEN_OCP == 0x00) // 0000FF
                fadePhase++;
            else
                setPWMDutyCycle(0, GREEN_OCP-3, 255);
            break;
        case 7:
            if (RED_OCP == 0xFE) // FF00FF
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP+2, 0, 255);
            break;
        case 8:
            if (BLUE_OCP == 0x2D) // 00002D
                fadePhase++;
            else
                setPWMDutyCycle(255, 0, BLUE_OCP-3);
            break;
        case 9:
            if (BLUE_OCP == 0x00) // 000000
                fadePhase = 1;
            else
                setPWMDutyCycle(255, 0, BLUE_OCP-1);
            break;
        default:
            fadePhase = 0;
            break;
    }
}

uint8_t randomColor(void)
{
    // Take the upper byte
    return (uint8_t)(rand() >> 8);
}

void increaseColorSaturation(uint8_t *color)
{
uint8_t i;
uint8_t *min = &color[0];
uint8_t *max = &color[0];
    // Find the largest and smaller color values
    for (i = 1; i < 3; i++) {
        if (color[i] < *min) {
            min = &color[i];
        } else if (color[i] > *max) {
            max = &color[i];
        }
    }
    // If the difference is too small, the color is likely to be very washed out
    // Fix this by zeroing the smaller value
    if ((*max - *min) < 128) {
        *min = 0;
    }
    // Increase the highest color value even more, as well
    *max += (255 - *max) >> 1;
}

uint8_t calculateDistance(uint8_t start[3], uint8_t end[3])
{
uint16_t distance;
    // Euclidean distance = sqrt(a2-a1^2 + b2-b1^2 + c2-c1^2)
    // Divide by 4 to fit sum of squares in uint16_t
    distance = (end[0] - start[0]) * (end[0] - start[0]) >> 2;
    distance += (end[1] - start[1]) * (end[1] - start[1]) >> 2;
    distance += (end[2] - start[2]) * (end[2] - start[2]) >> 2;
    // Ranges determined by calculating the distance between two colors
    // [0,0,0] and [i,i,i] where i is an easily interpolated value.
    if (distance > 12096) {
        return 255;
    } else if (distance > 2976) {
        return 127;
    } else if (distance > 720) {
        return 63;
    } else {
        return 31;
    }
}

uint8_t interpolate(uint8_t start, uint8_t end, uint8_t distance, uint8_t step)
{
    // Linear interpolation = (A * ((distance + 1) - x) + B * x) / distance + 1
    // where 0 < x < distance
    switch (distance) {
        case 255:
            return ((uint16_t)start * (256-step) + end * step) >> 8;
        case 127:
            return ((uint16_t)start * (128-step) + end * step) >> 7;
        case 63:
            return ((uint16_t)start * (64-step) + end * step) >> 6;
        case 31:
            return ((uint16_t)start * (32-step) + end * step) >> 5;
        default:
            return end;
    }
}

void moodLightEffect(void)
{
static uint8_t i = 0;
    if (i < MOODLIGHT_FADE_DELAY) {
        i++;
        return;
    }
    i = 0;
    // If we've reached the end of the current phase, begin a new one
    if (fadeValue == fadePhase) {
        colorMask[0] = nextColor[0];
        colorMask[1] = nextColor[1];
        colorMask[2] = nextColor[2];
        nextColor[0] = randomColor();
        nextColor[1] = randomColor();
        nextColor[2] = randomColor();
        increaseColorSaturation(nextColor);
        // Store distance in fadePhase
        fadePhase = calculateDistance(colorMask, nextColor);
        // Store step in fadeValue
        fadeValue = 0;
    }
    fadeValue++;
    setPWMDutyCycle(
        interpolate(colorMask[0], nextColor[0], fadePhase, fadeValue),
        interpolate(colorMask[1], nextColor[1], fadePhase, fadeValue),
        interpolate(colorMask[2], nextColor[2], fadePhase, fadeValue)
    );
}
