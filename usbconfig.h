/* Name: usbconfig.h
 * Project: Olark Observer Hardware
 * Original Author: Christian Starkjohann
 * Creation Date: 2007-06-23
 * Edits by: Nathan Sollenberger
 * Last edited: 2016-01-11
 * Tabsize: 4
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt)
 * This Revision: $Id: usbconfig.h 537 2008-02-28 21:13:01Z cs $
 */

#ifndef __usbconfig_h_included__
#define __usbconfig_h_included__

/* ---------------------------- Hardware Config ---------------------------- */

#define USB_CFG_IOPORTNAME      B
/* This is the port where the USB bus is connected. When you configure it to
 * "B", the registers PORTB, PINB and DDRB will be used.
 */
#define USB_CFG_DMINUS_BIT      2
/* This is the bit number in USB_CFG_IOPORT where the USB D- line is connected.
 * This may be any bit in the port.
 */
#define USB_CFG_DPLUS_BIT       3
/* This is the bit number in USB_CFG_IOPORT where the USB D+ line is connected.
 * This may be any bit in the port. Please note that D+ must also be connected
 * to interrupt pin INT0!
 */
#define USB_CFG_CLOCK_KHZ       (F_CPU/1000)
/* Clock rate of the AVR in MHz. Legal values are 12000, 16000 or 16500.
 * The 16.5 MHz version of the code requires no crystal, it tolerates +/- 1%
 * deviation from the nominal frequency. All other rates require a precision
 * of 2000 ppm and thus a crystal!
 * Default if not specified: 12 MHz
 */

/* --------------------------- Functional Range ---------------------------- */

#define USB_CFG_HAVE_INTRIN_ENDPOINT    0
/* Define this to 1 if you want to compile a version with two endpoints: The
 * default control endpoint 0 and an interrupt-in endpoint 1.
 */
#define USB_CFG_HAVE_INTRIN_ENDPOINT3   0
/* Define this to 1 if you want to compile a version with three endpoints: The
 * default control endpoint 0, an interrupt-in endpoint 1 and an interrupt-in
 * endpoint 3. You must also enable endpoint 1 above.
 */
#define USB_CFG_IMPLEMENT_HALT          0
/* Define this to 1 if you also want to implement the ENDPOINT_HALT feature
 * for endpoint 1 (interrupt endpoint). Although you may not need this feature,
 * it is required by the standard. We have made it a config option because it
 * bloats the code considerably.
 */
#define USB_CFG_INTR_POLL_INTERVAL      10
/* If you compile a version with endpoint 1 (interrupt-in), this is the poll
 * interval. The value is in milliseconds and must not be less than 10 ms for
 * low speed devices.
 */
#define USB_CFG_IS_SELF_POWERED         0
/* Define this to 1 if the device has its own power supply. Set it to 0 if the
 * device is powered from the USB bus.
 */
#define USB_CFG_MAX_BUS_POWER           100
/* Set this variable to the maximum USB bus power consumption of your device.
 * The value is in milliamperes. [It will be divided by two since USB
 * communicates power requirements in units of 2 mA.]
 */
#define USB_CFG_IMPLEMENT_FN_WRITE      0
/* Set this to 1 if you want usbFunctionWrite() to be called for control-out
 * transfers. Set it to 0 if you don't need it and want to save a couple of
 * bytes.
 */
#define USB_CFG_IMPLEMENT_FN_READ       0
/* Set this to 1 if you need to send control replies which are generated
 * "on the fly" when usbFunctionRead() is called. If you only want to send
 * data from a static buffer, set it to 0 and return the data from
 * usbFunctionSetup(). This saves a couple of bytes.
 */
#define USB_CFG_IMPLEMENT_FN_WRITEOUT   0
/* Define this to 1 if you want to use interrupt-out (or bulk out) endpoint 1.
 * You must implement the function usbFunctionWriteOut() which receives all
 * interrupt/bulk data sent to endpoint 1.
 */
#define USB_CFG_HAVE_FLOWCONTROL        0
/* Define this to 1 if you want flowcontrol over USB data. See the definition
 * of the macros usbDisableAllRequests() and usbEnableAllRequests() in
 * usbdrv.h.
 */
#ifndef __ASSEMBLER__
extern void usbEventResetReady(void);
#endif
#define USB_RESET_HOOK(isReset)             if(!isReset){usbEventResetReady();}
/* This macro is a hook if you need to know when an USB RESET occurs. It has
 * one parameter which distinguishes between the start of RESET state and its
 * end.
 */
#define USB_CFG_HAVE_MEASURE_FRAME_LENGTH   1
/* define this macro to 1 if you want the function usbMeasureFrameLength()
 * compiled in. This function can be used to calibrate the AVR's RC oscillator.
 */

/* -------------------------- Device Description --------------------------- */

#define  USB_CFG_VENDOR_ID       0xc0, 0x16
/* USB vendor ID for the device, low byte first. If you have registered your
 * own Vendor ID, define it here. Otherwise you use obdev's free shared
 * VID/PID pair. Be sure to read USBID-License.txt for rules!
 * This template uses obdev's shared VID/PID pair for HIDs: 0x16c0/0x5df.
 * Use this VID/PID pair ONLY if you understand the implications!
 */
#define  USB_CFG_DEVICE_ID       0xdc, 0x05
/* This is the ID of the product, low byte first. It is interpreted in the
 * scope of the vendor ID. If you have registered your own VID with usb.org
 * or if you have licensed a PID from somebody else, define it here. Otherwise
 * you use obdev's free shared VID/PID pair. Be sure to read the rules in
 * USBID-License.txt!
 * This template uses obdev's shared VID/PID pair for HIDs: 0x16c0/0x5df.
 * Use this VID/PID pair ONLY if you understand the implications!
 */
#define USB_CFG_DEVICE_VERSION  0x01, 0x00
/* Version number of the device: Minor number first, then major number.
 */
#define USB_CFG_VENDOR_NAME     'S', 'p', 'a', 'c', 'e', 'n', 'a', 't', 'e', '.', 'c', 'o', 'm'
#define USB_CFG_VENDOR_NAME_LEN 13
/* These two values define the vendor name returned by the USB device. The name
 * must be given as a list of characters under single quotes. The characters
 * are interpreted as Unicode (UTF-16) entities.
 * If you don't want a vendor name string, undefine these macros.
 * ALWAYS define a vendor name containing your Internet domain name if you use
 * obdev's free shared VID/PID pair. See the file USBID-License.txt for
 * details.
 */
#define USB_CFG_DEVICE_NAME     'O', 'l', 'a', 'r', 'k', 'O', 'b', 's', 'e', 'r', 'v', 'e', 'r'
#define USB_CFG_DEVICE_NAME_LEN 13

#define USB_CFG_DEVICE_CLASS        0xff /* Vendor specific */
#define USB_CFG_DEVICE_SUBCLASS     0
#define USB_CFG_INTERFACE_CLASS     0
#define USB_CFG_INTERFACE_SUBCLASS  0
#define USB_CFG_INTERFACE_PROTOCOL  0

/* #define USB_PUBLIC static */
/* Use the define above if you #include usbdrv.c instead of linking against it.
 * This technique saves a couple of bytes in flash memory.
 */

/* ----------------------- Optional MCU Description ------------------------ */

/* Configure Pin Change Interrupt for D+ */
#define USB_INTR_CFG            PCMSK
#define USB_INTR_CFG_SET        (1 << USB_CFG_DPLUS_BIT)
#define USB_INTR_CFG_CLR        0
#define USB_INTR_ENABLE         GIMSK
#define USB_INTR_ENABLE_BIT     PCIE
#define USB_INTR_PENDING        GIFR
#define USB_INTR_PENDING_BIT    PCIF
#define USB_INTR_VECTOR         PCINT0_vect

#endif /* __usbconfig_h_included__ */
