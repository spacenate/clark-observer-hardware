/* Name: led.h
 * Project: Olark Observer Hardware
 * Author: Nathan Sollenberger
 * Creation Date: 2016-01-03
 * Copyright: (c) 2016 Nathan Sollenberger
 * License: GPL
 */

uint8_t status;
uint8_t newStatusRequested;
volatile uint8_t fadeTick;
void (* fadeFunction)(void);

void initLED(void);
void toggleLED(void);
void turnOffLED(void);
void initTimers(void);

void enableIdleTimer(void);
void enablePulseEffect(uint8_t red, uint8_t green, uint8_t blue);
void enableFlashEffect(uint8_t red, uint8_t green, uint8_t blue);
void enableMoodlightEffect(void);

void setPWMDutyCycle(uint8_t redCycle, uint8_t greenCycle, uint8_t blueCycle);
void enablePWM(void);
void disablePWM(void);

void enableFade(void);
void disableFade(void);
void pulseEffect(void);
void flashEffect(void);
void idleTimer(void);
void rainbowEffect(void);
uint8_t randomColor(void);
void increaseColorSaturation(uint8_t *color);
uint8_t calculateDistance(uint8_t start[3], uint8_t end[3]);
uint8_t interpolate(uint8_t start, uint8_t end, uint8_t distance, uint8_t step);
void moodLightEffect(void);
