/* Name: main.c
 * Project: Olark Observer Hardware
 * Author: Nathan Sollenberger
 * Creation Date: 2016-01-03
 * Copyright: (c) 2016 Nathan Sollenberger
 * License: GPL
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbdrv.h"

#define CUSTOM_RX_STATUS_OFF 0x00
#define CUSTOM_RX_STATUS_AVAIL 0x01
#define CUSTOM_RX_STATUS_UNAVAIL 0x02
#define CUSTOM_RX_STATUS_MAXCHATS 0x03
#define CUSTOM_RX_STATUS_UNREAD 0x04
#define CUSTOM_RX_RAINBOW 0x45
#define CUSTOM_RX_CONFIRM 0x22

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
uint16_t fadeValue;
uint8_t pauseValue;
uint8_t ledMask[3];

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
        case CUSTOM_RX_STATUS_OFF:
        case CUSTOM_RX_STATUS_AVAIL:
        case CUSTOM_RX_STATUS_UNAVAIL:
        case CUSTOM_RX_STATUS_MAXCHATS:
        case CUSTOM_RX_STATUS_UNREAD:
        case CUSTOM_RX_RAINBOW:
            newStatus = 1;
            currentStatus = request->bRequest;
            outputBuffer[0] = CUSTOM_RX_CONFIRM;
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

void enablePWM(void)
{
    cli();
    TCCR0A |= _BV(WGM01) | _BV(WGM00) | _BV(COM0A1) | _BV(COM0B1);   // Fast PWM mode, clear on compare match
    GTCCR |= _BV(PWM1B) | _BV(COM1B1);   // PWM mode, clear on compare match with PB3 not connected
    setPWMDutyCycle(0, 0, 0);
    sei();
}

void setPWMDutyCycle(uint8_t redCycle, uint8_t greenCycle, uint8_t blueCycle) {
    RED_OCP = redCycle;
    GREEN_OCP = greenCycle;
    BLUE_OCP = blueCycle;
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
        uint8_t redCycle = ((uint16_t)fadeValue * ledMask[0]) >> 8;
        uint8_t greenCycle = ((uint16_t)fadeValue * ledMask[1]) >> 8;
        uint8_t blueCycle = ((uint16_t)fadeValue * ledMask[2]) >> 8;
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
        uint8_t redCycle = ((uint16_t)255 * ledMask[0]) >> 8;
        uint8_t greenCycle = ((uint16_t)255 * ledMask[1]) >> 8;
        uint8_t blueCycle = ((uint16_t)255 * ledMask[2]) >> 8;
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
    if (fadeValue == 3000) {
        newStatus = 1;
        currentStatus = CUSTOM_RX_RAINBOW;
    }
}

void rainbowEffect(void)
{
    if (fadeValue < 2) {
        fadeValue++;
        return;
    }
    fadeValue = 0;
    switch (fadePhase) {
        case 0:
            if (RED_OCP == 0xFF) // ff0000
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP+1, 0, 0);
        case 1:
            if (GREEN_OCP == 0x80) // ff8000
                fadePhase++;
            else
                setPWMDutyCycle(256, GREEN_OCP+1, 0);
            break;
        case 2:
            if (GREEN_OCP == 0xFF) // c0ff00
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP-1, GREEN_OCP+2, 0);
            break;
        case 3:
            if (RED_OCP == 0x80) // 80ff00
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP-1, 256, 0);
            break;
        case 4:
            if (RED_OCP == 0x00) // 00FF80
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP-2, 256, BLUE_OCP+2);
            break;
        case 5:
            if (BLUE_OCP == 0xFF) // 00FFFF
                fadePhase++;
            else
                setPWMDutyCycle(0, 256, BLUE_OCP+2);
            break;
        case 6:
            if (GREEN_OCP == 0x01) // 0000FF
                fadePhase++;
            else
                setPWMDutyCycle(0, GREEN_OCP-3, 256);
            break;
        case 7:
            if (RED_OCP == 0x80) // 8000FF
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP+2, 0, 256);
            break;
        case 8:
            if (RED_OCP == 0xFF) // FF00FF
                fadePhase++;
            else
                setPWMDutyCycle(RED_OCP+2, 0, 256);
            break;
        case 9:
            if (BLUE_OCP == 0x01) // 000000
                fadePhase = 1;
            else
                setPWMDutyCycle(0, 0, BLUE_OCP-3);
            break;
        default:
            fadePhase = 0;
            break;
    }
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
    fadeFunction = &idleTimer;
    for(;;){
        wdt_reset();
        usbPoll();
        if (newStatus == 1) {
            newStatus = 0;
            switch (currentStatus) {
                case CUSTOM_RX_STATUS_OFF:
                    disablePWM();
                    turnOffLED();
                    enableFade();
                    fadeValue = 0;
                    fadeFunction = &idleTimer;
                    break;
                case CUSTOM_RX_STATUS_AVAIL:
                    enablePWM();
                    enableFade();
                    fadePhase = PULSE_INCREASE;
                    fadeValue = PULSE_MIN_BRIGHTNESS;
                    ledMask[0] = 30;
                    ledMask[1] = 255;
                    ledMask[2] = 10;
                    fadeFunction = &pulseEffect;
                    break;
                case CUSTOM_RX_STATUS_UNAVAIL:
                    enablePWM();
                    enableFade();
                    fadePhase = PULSE_INCREASE;
                    fadeValue = PULSE_MIN_BRIGHTNESS;
                    ledMask[0] = 255;
                    ledMask[1] = 20;
                    ledMask[2] = 10;
                    fadeFunction = &pulseEffect;
                    // pulseOff
                    break;
                case CUSTOM_RX_STATUS_MAXCHATS:
                    enablePWM();
                    enableFade();
                    fadePhase = PULSE_INCREASE;
                    fadeValue = PULSE_MIN_BRIGHTNESS;
                    ledMask[0] = 170;
                    ledMask[1] = 170;
                    ledMask[2] = 170;
                    fadeFunction = &pulseEffect;
                    // fast pulse
                    break;
                case CUSTOM_RX_STATUS_UNREAD:
                    disablePWM();
                    turnOffLED();
                    enableFade();
                    fadeValue = FLASH_DURATION - 1; /* LED will turn on next time flashEffect is called */
                    ledMask[0] = 255;
                    ledMask[1] = 0;
                    ledMask[2] = 0;
                    fadeFunction = &flashEffect;
                    break;
                case CUSTOM_RX_RAINBOW:
                    enablePWM();
                    enableFade();
                    fadePhase = 0;
                    fadeValue = 0;
                    fadeFunction = &rainbowEffect;
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
