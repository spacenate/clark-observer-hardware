/* Name: main.c
 * Project: Olark Observer Hardware
 * Author: Nathan Sollenberger
 * Creation Date: 2016-01-03
 * Copyright: (c) 2016 Nathan Sollenberger
 * License: GPL
 */
#include <stdlib.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbdrv.h"

#define CUSTOM_RQ_STATUS_OFF 0x00
#define CUSTOM_RQ_STATUS_AVAIL 0x01
#define CUSTOM_RQ_STATUS_AWAY 0x02
#define CUSTOM_RQ_STATUS_MAXCHATS 0x03
#define CUSTOM_RQ_STATUS_UNREAD 0x04
#define CUSTOM_RQ_RAINBOW 0x45
#define CUSTOM_RQ_CONFIRM 0x22

/* These pins may be re-ordered, but if different pins are used,
   enablePWM must be updated as well. Be sure to avoid conflict
   with USB DATA- and DATA+ pins */
#define RED_PIN PB4
#define GREEN_PIN PB0
#define BLUE_PIN PB1
#define RED_OCP OCR1B
#define GREEN_OCP OCR0A
#define BLUE_OCP OCR0B

void (* fadeFunction)(void);
volatile uint8_t fadeTick;
uint8_t fadePhase;
uint8_t fadeValue;
uint8_t pauseValue;
uint8_t colorMask[3];
uint8_t nextColor[3];

#define PULSE_MIN_BRIGHTNESS 140
#define PULSE_MAX_BRIGHTNESS 255
#define PULSE_PAUSE_DURATION 60
#define PULSE_INCREASE 1
#define PULSE_DECREASE -1
#define PULSE_PAUSE 0

#define FLASH_DURATION 30

#define RAINBOW_TRANSITION_DURATION 200

uint8_t currentStatus;
uint8_t newStatus;

/* ---------------------- Interface with V-USB driver ---------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t *request = (void *)data;
static uchar outputBuffer[1];
uint8_t msgLen = 0;

    switch (request->bRequest) {
        case CUSTOM_RQ_STATUS_OFF:
        case CUSTOM_RQ_STATUS_AVAIL:
        case CUSTOM_RQ_STATUS_AWAY:
        case CUSTOM_RQ_STATUS_MAXCHATS:
        case CUSTOM_RQ_STATUS_UNREAD:
        case CUSTOM_RQ_RAINBOW:
            newStatus = 1;
            currentStatus = request->bRequest;
            outputBuffer[0] = CUSTOM_RQ_CONFIRM;
            msgLen = 1;
            break;

        case 40:
            outputBuffer[0] = fadePhase;
            msgLen = 1;
            break;
        case 41:
            outputBuffer[0] = fadeValue;
            msgLen = 1;
            break;
        case 50:
            outputBuffer[0] = RED_OCP;
            msgLen = 1;
            break;

        default:
            break;
    }
    usbMsgPtr = outputBuffer;
    return msgLen;
}

/* --------------------- Calibrate internal oscillator --------------------- */

#define abs(x) ((x) > 0 ? (x) : (-x))
// Called by V-USB after device reset
void usbEventResetReady(void)
{
int frameLength, targetLength = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
int bestDeviation = 9999;
uchar trialCal, bestCal, step, region;
    // do a binary search in regions 0-127 and 128-255 to get optimum OSCCAL
    for(region = 0; region <= 1; region++) {
        frameLength = 0;
        trialCal = (region == 0) ? 0 : 128;
        for(step = 64; step > 0; step >>= 1) {
            if(frameLength < targetLength) // true for initial iteration
                trialCal += step; // frequency too low
            else
                trialCal -= step; // frequency too high
            OSCCAL = trialCal;
            frameLength = usbMeasureFrameLength();
            if(abs(frameLength-targetLength) < bestDeviation) {
                bestCal = trialCal; // new optimum found
                bestDeviation = abs(frameLength -targetLength);
            }
        }
    }
    OSCCAL = bestCal;
}

/* ---------------------------- LED functions ------------------------------ */

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

void setPWMDutyCycle(uint8_t redCycle, uint8_t greenCycle, uint8_t blueCycle) {
    RED_OCP = redCycle;
    GREEN_OCP = greenCycle;
    BLUE_OCP = blueCycle;
}

void enablePWM(void)
{
    cli();
    TCCR0A |= _BV(WGM01) | _BV(WGM00) | _BV(COM0A1) | _BV(COM0B1);   // Fast PWM mode, clear on compare match
    GTCCR |= _BV(PWM1B) | _BV(COM1B1);   // PWM mode, clear on compare match with PB3 not connected
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
    TIMSK = _BV(TOIE1); // Enable overflow interrupt
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
    if (fadePhase & (PULSE_INCREASE | PULSE_DECREASE)) {
        fadeValue += fadePhase;
        uint8_t redCycle = ((uint16_t)fadeValue * colorMask[0]) >> 8;
        uint8_t greenCycle = ((uint16_t)fadeValue * colorMask[1]) >> 8;
        uint8_t blueCycle = ((uint16_t)fadeValue * colorMask[2]) >> 8;
        setPWMDutyCycle(redCycle, greenCycle, blueCycle);
        /* Detect MAX and MIN */
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
        if (fadePhase == 1) { // 11
            newStatus = 1;
            currentStatus = CUSTOM_RQ_RAINBOW;
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
    return (uint8_t)(rand() >> 8);
}

uint8_t calculateDistance(uint8_t start[3], uint8_t end[3])
{
uint16_t distance;
    distance = (end[0] - start[0]) * (end[0] - start[0]) >> 2;
    distance += (end[1] - start[1]) * (end[1] - start[1]) >> 2;
    distance += (end[2] - start[2]) * (end[2] - start[2]) >> 2;
    if (distance > 12096) {
        return 255;
    } else if (distance > 2976) {
        return 127;
    } else if (distance > 720) {
        return 63;
    } else if (distance > 168) {
        return 31;
    } else {
        return 15;
    }
}

uint8_t interpolate(uint8_t start, uint8_t end, uint8_t distance, uint8_t step)
{
    switch (distance) {
        case 255:
            return ((uint16_t)start * (256-step) + end * step) >> 8;
        case 127:
            return ((uint16_t)start * (128-step) + end * step) >> 7;
        case 63:
            return ((uint16_t)start * (64-step) + end * step) >> 6;
        case 31:
            return ((uint16_t)start * (32-step) + end * step) >> 5;
        case 15:
            return ((uint16_t)start * (16-step) + end * step) >> 4;
        default:
            return end;
    }
}

void increaseNextColorSaturation(void)
{
uint8_t i;
uint8_t *min = &nextColor[0];
uint8_t *max = &nextColor[0];

    for (i = 1; i < 3; i++) {
        if (nextColor[i] < *min) {
            min = &nextColor[i];
        } else if (nextColor[i] > *max) {
            max = &nextColor[i];
        }
    }
    if ((*max - *min) < 128) {
        *min = 0;
    }
}

void moodLightEffect(void)
{
static uint8_t i = 0;
    if (i < 4) {
        i++;
        return;
    }
    i = 0;
    if (fadeValue == fadePhase) {
        colorMask[0] = nextColor[0];
        colorMask[1] = nextColor[1];
        colorMask[2] = nextColor[2];
        nextColor[0] = randomColor();
        nextColor[1] = randomColor();
        nextColor[2] = randomColor();
        increaseNextColorSaturation();
        // Store distance in fadePhase
        fadePhase = calculateDistance(colorMask, nextColor);
        // Store step in fadeValue
        fadeValue = 0;
    }
    fadeValue++;
    setPWMDutyCycle(interpolate(colorMask[0], nextColor[0], fadePhase, fadeValue), interpolate(colorMask[1], nextColor[1], fadePhase, fadeValue),interpolate(colorMask[2], nextColor[2], fadePhase, fadeValue));

}

/* --------------------------------- Main ---------------------------------- */

int main(void)
{
uchar i;

    initTimers();
    initLED();
    turnOffLED();
    usbInit();
    usbDeviceDisconnect();
    for (i=0;i<20;i++) {    /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();
    wdt_enable(WDTO_1S);
    sei();
    enableFade();
    fadeValue = 0;
    fadePhase = 0;
    fadeFunction = &idleTimer;
    for(;;){
        wdt_reset();
        usbPoll();
        if (newStatus == 1) {
            newStatus = 0;
            switch (currentStatus) {
                case CUSTOM_RQ_STATUS_OFF:
                    disablePWM();
                    turnOffLED();
                    enableFade();
                    fadeValue = 0;
                    fadePhase = 0;
                    fadeFunction = &idleTimer;
                    break;
                case CUSTOM_RQ_STATUS_AVAIL:
                    enablePWM();
                    enableFade();
                    fadePhase = PULSE_INCREASE;
                    fadeValue = PULSE_MIN_BRIGHTNESS;
                    colorMask[0] = 30;
                    colorMask[1] = 255;
                    colorMask[2] = 10;
                    fadeFunction = &pulseEffect;
                    break;
                case CUSTOM_RQ_STATUS_AWAY:
                    enablePWM();
                    enableFade();
                    fadePhase = PULSE_INCREASE;
                    fadeValue = PULSE_MIN_BRIGHTNESS;
                    colorMask[0] = 255;
                    colorMask[1] = 20;
                    colorMask[2] = 10;
                    fadeFunction = &pulseEffect;
                    // pulseOff
                    break;
                case CUSTOM_RQ_STATUS_MAXCHATS:
                    enablePWM();
                    enableFade();
                    fadePhase = PULSE_INCREASE;
                    fadeValue = PULSE_MIN_BRIGHTNESS;
                    colorMask[0] = 170;
                    colorMask[1] = 170;
                    colorMask[2] = 170;
                    fadeFunction = &pulseEffect;
                    // fast pulse
                    break;
                case CUSTOM_RQ_STATUS_UNREAD:
                    disablePWM();
                    turnOffLED();
                    enableFade();
                    fadeValue = FLASH_DURATION - 1; /* LED will turn on next time flashEffect is called */
                    colorMask[0] = 255;
                    colorMask[1] = 0;
                    colorMask[2] = 0;
                    fadeFunction = &flashEffect;
                    break;
                case CUSTOM_RQ_RAINBOW:
                    enablePWM();
                    enableFade();
                    fadePhase = 0;
                    fadeValue = 0;
                    nextColor[0] = 0;
                    nextColor[1] = 0;
                    nextColor[2] = 0;
                    fadeFunction = &moodLightEffect;
                default:
                    break;
            }
        }

        if (fadeTick == 1) {
            fadeTick = 0;
            fadeFunction();
        }
    }
    return 0;
}
