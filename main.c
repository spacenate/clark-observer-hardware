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
#define CUSTOM_RX_CONFIRM 0x22

volatile uint8_t fadeTick;
void (* fadeFunction)(void);
uint8_t fadeValue;
uint8_t fadePhase;
uint8_t pauseValue;

#define BREATHE_INCREASE 2     //  0b10
#define BREATHE_DECREASE -1    // -0b01
#define BREATHE_PAUSE 0        //  0b00
#define BREATHE_MIN 25
#define BREATHE_MAX 145
#define BREATHE_PAUSE_DURATION 140

#define FLASH_DURATION 40

uint8_t currentStatus;
uint8_t newStatus;

/* ---------------------- Interface with V-USB driver ---------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t *request = (void *)data;
static uchar outputBuffer[2];
uint8_t msgLen = 0;

    switch (request->bRequest) {
        case CUSTOM_RX_STATUS_OFF:
        case CUSTOM_RX_STATUS_AVAIL:
        case CUSTOM_RX_STATUS_UNAVAIL:
        case CUSTOM_RX_STATUS_MAXCHATS:
        case CUSTOM_RX_STATUS_UNREAD:
            newStatus = 1;
            currentStatus = request->bRequest;
            outputBuffer[0] = CUSTOM_RX_CONFIRM;
            outputBuffer[1] = currentStatus;
            msgLen = 2;
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

void toggleLED(void)
{
    PORTB ^= (1 << PB1);
}

void turnOffLED(void)
{
    PORTB |= (1 << PB1);
}

void enablePWM(void)
{
    cli();
    TCCR0A |= (1 << WGM01 | 1 << WGM00);  // Fast PWM mode
    TCCR0A |= (1 << COM0B1 | 1 << COM0B0); // Inverting mode
    TCCR0B |= (1 << CS02 | 1 << CS00);   // clock/1024 ~60Hz
    OCR0B = 0;
    sei();
}

void setPWMDutyCycle(uint8_t dutyCycle) {
    OCR0B = dutyCycle;
}

void disablePWM(void)
{
    TCCR0A = 0;
}

void enableFade(void)
{
    cli();
    TCCR1 |= (1 << CS13 | 1 << CS11 | 1 << CS10); // clock/1024 ~60Hz
    TIMSK = (1 << TOIE1); // enable overflow interrupt
    sei();
}

void disableFade(void)
{
    TIMSK &= ~(1 << TOIE1);
}

ISR(TIMER1_OVF_vect)
{
    fadeTick = 1;
}

void breatheEffect(void)
{
    if (fadePhase & (BREATHE_INCREASE | BREATHE_DECREASE)) {
        fadeValue += fadePhase;
        setPWMDutyCycle(fadeValue);
        /* Detect MAX and MIN */
        if (fadeValue == BREATHE_MIN) {
            fadePhase = BREATHE_PAUSE;
            pauseValue = 0;
        } else if (fadeValue == BREATHE_MAX) {
            fadePhase = BREATHE_DECREASE;
        }
    } else {
        pauseValue++;
        if (pauseValue == BREATHE_PAUSE_DURATION)
            fadePhase = BREATHE_INCREASE;
    }
}

void flashEffect(void)
{
    fadeValue++;
    if (fadeValue == FLASH_DURATION) {
        fadeValue = 0;
        toggleLED();
    }
}

/* --------------------------------- Main ---------------------------------- */

int main(void)
{
uchar i;

    DDRB |= (1 << PB1);     /* led output, active LOW */
    PORTB |= (1 << PB1);

    usbInit();
    usbDeviceDisconnect();
    for (i=0;i<20;i++) {    /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();
    wdt_enable(WDTO_1S);
    sei();
    for(;;){
        wdt_reset();
        usbPoll();
        if (newStatus == 1) {
            newStatus = 0;
            switch (currentStatus) {
                case CUSTOM_RX_STATUS_OFF:
                    disablePWM();
                    disableFade();
                    turnOffLED();
                    break;
                case CUSTOM_RX_STATUS_AVAIL:
                    enablePWM();
                    enableFade();
                    fadePhase = BREATHE_INCREASE;
                    fadeValue = BREATHE_MIN;
                    fadeFunction = &breatheEffect;
                    break;
                case CUSTOM_RX_STATUS_UNAVAIL:
                    enablePWM();
                    disableFade();
                    setPWMDutyCycle(25);
                    // breatheOff
                    break;
                case CUSTOM_RX_STATUS_MAXCHATS:
                    enablePWM();
                    disableFade();
                    setPWMDutyCycle(125);
                    // fast breathe
                    break;
                case CUSTOM_RX_STATUS_UNREAD:
                    disablePWM();
                    enableFade();
                    fadePhase = BREATHE_INCREASE;
                    fadeValue = BREATHE_MIN;
                    fadeFunction = &flashEffect;
                    break;
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
