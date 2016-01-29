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
#include "led.c"

#define CUSTOM_RQ_STATUS_IDLE 0x00
#define CUSTOM_RQ_STATUS_AVAIL 0x01
#define CUSTOM_RQ_STATUS_AWAY 0x02
#define CUSTOM_RQ_STATUS_MAXCHATS 0x03
#define CUSTOM_RQ_STATUS_UNREAD 0x04
#define CUSTOM_RQ_MOODLIGHT 0x45
#define CUSTOM_RQ_CONFIRM 0x22

uint8_t status;
uint8_t newStatusRequested;
volatile uint8_t fadeTick;
void (* fadeFunction)(void);

/* ---------------------- Interface with V-USB driver ---------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t *request = (void *)data;
static uchar outputBuffer[1];
uint8_t msgLen = 0;

    switch (request->bRequest) {
        case CUSTOM_RQ_STATUS_IDLE:
        case CUSTOM_RQ_STATUS_AVAIL:
        case CUSTOM_RQ_STATUS_AWAY:
        case CUSTOM_RQ_STATUS_MAXCHATS:
        case CUSTOM_RQ_STATUS_UNREAD:
        case CUSTOM_RQ_MOODLIGHT:
            newStatusRequested = 1;
            status = request->bRequest;
            outputBuffer[0] = CUSTOM_RQ_CONFIRM;
            msgLen = 1;
            break;
        default:
            break;
    }
    usbMsgPtr = outputBuffer;
    return msgLen;
}

// Calibrate internal oscillator - called by V-USB after device reset
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

/* --------------------------------- Main ---------------------------------- */

int main(void)
{
uint8_t i;

    // LED init
    initTimers();
    initLED();
    turnOffLED();
    // USB driver init
    usbInit();
    usbDeviceDisconnect();
    for (i=0;i<20;i++) {    /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();
    wdt_enable(WDTO_1S);
    sei();
    // Enable idle timer upon starting
    enableIdleTimer();
    for (;;) {
        wdt_reset();
        usbPoll();
        if (newStatusRequested == 1) {
            newStatusRequested = 0;
            switch (status) {
                case CUSTOM_RQ_STATUS_IDLE:
                    enableIdleTimer();
                    break;
                case CUSTOM_RQ_STATUS_AVAIL:
                    enablePulseEffect(30, 255, 10);
                    break;
                case CUSTOM_RQ_STATUS_AWAY:
                    enablePulseEffect(255, 20, 10);
                    break;
                case CUSTOM_RQ_STATUS_MAXCHATS:
                    enablePulseEffect(170, 170, 170);
                    break;
                case CUSTOM_RQ_STATUS_UNREAD:
                    enableFlashEffect(255, 0, 0);
                    break;
                case CUSTOM_RQ_MOODLIGHT:
                    enableMoodlightEffect();
                default:
                    break;
            }
        }
        // The tick that drives all LED functions
        if (fadeTick == 1) {
            fadeTick = 0;
            fadeFunction();
        }
    }
    return 0;
}
