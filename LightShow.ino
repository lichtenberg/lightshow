//
// Lightshow Arduino Sketch
// (C) 2020 Mitch Lichtenberg (both of them).
//

// Set STUDIO to 0 for the living room installation
// Set STUDIO to 1 for the studio (development) installation

#define STUDIO 0

// zip -r Lightshow.zip ./Lightshow -x '*.git*'

#include "src/ALA/AlaLedRgb.h"

#include "xtimer.h"

#define PAL_RGB         64
#define PAL_RAINBOW     65
#define PAL_RAINBOWSTRIPE 66
#define PAL_PARTY       67
#define PAL_HEAT        68
#define PAL_FIRE        69
#define PAL_COOL        70
#define PAL_WHITE       71
#define PAL_RED         80
#define PAL_GREEN       82
#define PAL_BLUE        84


int debug = 0;

#define MSGSIZE         16
#define STATE_SYNC1     0
#define STATE_SYNC2     1
#define STATE_RX        2

static int rxstate = STATE_SYNC1;
static int rxcount = 0;

typedef struct lsmessage_s {
//    uint8_t     ls_sync[2];
    uint16_t    ls_strips;
    uint16_t    ls_anim;
    uint16_t    ls_speed;
    uint16_t    ls_option;
    uint32_t    ls_color;
    uint32_t    ls_reserved2;
} lsmessage_t;

static lsmessage_t message;



/*  *********************************************************************
    *  Timer Stuff.  Macros are in xtimer.h
    ********************************************************************* */

// Our sense of "now".  It's just a number that counts at 1KHz.
extern "C" {
    volatile xtimer_t __now = 0;
}

static xtimer_t blinky_timer;
static char blinky_onoff = 0;

#define PIN_LED 13

// Flow control for our special dual-Arduino setup that
// uses a Pro Micro to process message the Uno to run the pixels
#define PIN_DATA_AVAIL  8                 // input, '1' if there is data
#define PIN_SEND_OK     9                 // output, '1' to send data


/*  *********************************************************************
    *  LED Strips
    ********************************************************************* */

#define MAXSTRIPS 12

AlaLedRgb rgbStrip0;            // These are the spokes
AlaLedRgb rgbStrip1;
AlaLedRgb rgbStrip2;
AlaLedRgb rgbStrip3;
AlaLedRgb rgbStrip4;
AlaLedRgb rgbStrip5;
AlaLedRgb rgbStrip6;
AlaLedRgb rgbStrip7;
AlaLedRgb rgbStrip8;            // This is the ring
AlaLedRgb rgbStrip9;            // Top perimeter
AlaLedRgb rgbStrip10;           // Bottom perimeter
AlaLedRgb rgbStrip11;           // Ringlets

//
// Constants to define each spoke
// These are "bit masks", meaning there is exactly one bit set in each of these
// numbers.  By using a bit mask, we can conveniently represent sets of small numbers
// by treating each bit to mean "I am in this set".
//
// So, if you have a binary nunber 00010111, then bits 0,1,2, and 4 are set.
// You could therefore use the binary number 00010111, which is 11 in decimal,
// to mean "the set of 0, 1, 2, and 4"
//
// Define some shorthands for sets of strips.  We can add as many
// as we want here.
//

#define SPOKE0  (1 << 0)        // Spokes, clockwise from top
#define SPOKE1  (1 << 1)
#define SPOKE2  (1 << 2)
#define SPOKE3  (1 << 3)
#define SPOKE4  (1 << 4)
#define SPOKE5  (1 << 5)
#define SPOKE6  (1 << 6)
#define SPOKE7  (1 << 7)
#define LEDRING (1 << 8)
#define TOP     (1 << 9)
#define BOTTOM  (1 << 10)
#define RINGLETS (1 << 11)

//
// Constants to represent groups of spokes
// It's easier than listing them out each time we use the same group.
//

#define ALLSPOKES (SPOKE0 | SPOKE1 | SPOKE2 | SPOKE3 | SPOKE4 | SPOKE5 | SPOKE6 | SPOKE7)
#define ALLSTRIPS (ALLSPOKES | LEDRING | TOP | BOTTOM | RINGLETS)
#define EVENSPOKES (SPOKE0 | SPOKE2 | SPOKE4 | SPOKE6)
#define ODDSPOKES (SPOKE1 | SPOKE3 | SPOKE5 | SPOKE7)

//
// Additional sets we might want to define:
// LEFTSPOKES, RIGHTSPOKES, TOPSPOKES, BOTTOMSPOKES
//


/*  *********************************************************************
    *  Global Variables
    ********************************************************************* */

//
// This array is an "array of pointers" to the above LED strip ALA objects.
// The reason we do this is so that we can
// make loops that operate on more than one strip at a time.
//
//  allstrips[0] would therefore correspond to rgbStrip0,
//  allstrips[1] would therefore correspond to rgbStrip1,  and so on.
//
// Having them in an array means we can use "for" loops and such
// to do things to strips in bunches or all at the same time.
//
// Remember, thse aren't actually strips, they just "point" at the strips.
//
// Note that C arrays are 0 based, so our strips go from 0..8.
// 

AlaLedRgb *allstrips[MAXSTRIPS] = {
    &rgbStrip0,
    &rgbStrip1,
    &rgbStrip2,
    &rgbStrip3,
    &rgbStrip4,
    &rgbStrip5,
    &rgbStrip6,
    &rgbStrip7,
    &rgbStrip8,
    &rgbStrip9,
    &rgbStrip10,
    &rgbStrip11       
};

//
// This array contains the Arduino pin numbers that
// correspond to each LED strip.
//?
// There are 9 entries, representing each spoke (rgbStrip0 to rgbStrip7)
// plus the last one (on pin 46) being the LED ring (rgbStrip8)
//

// The constants below show the mapping between the controller's breakout
// boards and the Arduino pins

#define PORT_A1         39
#define PORT_A2         41
#define PORT_A3         43
#define PORT_A4         45
#define PORT_A5         47
#define PORT_A6         49
#define PORT_A7         51
#define PORT_A8         53

#define PORT_B1         38
#define PORT_B2         40
#define PORT_B3         42
#define PORT_B4         44
#define PORT_B5         46
#define PORT_B6         48
#define PORT_B7         50
#define PORT_B8         52

// In the setup routine, we will make rgbStrip0 correspond to allPins[0] (which is 39),
// and rgbStrip1 correspond to allPins[1], which is 41, etc.
// using a nice 'for' loop to do them all at once.

#if STUDIO
// This is the mapping that is used in the studio
int allPins[MAXSTRIPS] = { 39, 41, 43, 45, 38, 40, 42, 44, 46,
                           47, 48, 51};
#else
// This is the mapping that we use on the actual star
//int allPins[MAXSTRIPS] = { 41, 43, 45, 46, 48, 50, 52, 39, 47};
int allPins[MAXSTRIPS] = {
    PORT_A2,            // SPOKE1
    PORT_A3,            // SPOKE2
    PORT_A4,            // SPOKE3
    PORT_B5,            // SPOKE4
    PORT_B6,            // SPOKE5
    PORT_B7,            // SPOKE6
    PORT_B8,            // SPOKE7
    PORT_A1,            // SPOKE8
    PORT_A5,            // RING
    PORT_A6,            // TOP
    PORT_B4,            // BOTTOM
    PORT_A7             // RINGLETS
};
#endif

//
// Similarly to allPins, allLengths[] is an array that tells us the size
// of each strip. For the rig on my desk, there are 30 LEDs per "spoke" and the 60 LED ring
// So, allLengths[0..7] = 30, and allLengths[8] is 60.
//

#if STUDIO
// Studio version:  30 LEDs on the perimeter strips
int allLengths[MAXSTRIPS] = {
    30,                 // SPOKE1
    30,                 // SPOKE2
    30,                 // SPOKE3
    30,                 // SPOKE4
    30,                 // SPOKE5
    30,                 // SPOKE6
    30,                 // SPOKE7
    30,                 // SPOKE8
    60,                 // RING
    30,                 // TOP
    30,                 // BOTTOM
    128,                // RINGLETS
};
#else
// actual on-wall version: 130 LEDs on perimeter
int allLengths[MAXSTRIPS] = {
    30,                 // SPOKE1
    30,                 // SPOKE2
    30,                 // SPOKE3
    30,                 // SPOKE4
    30,                 // SPOKE5
    30,                 // SPOKE6
    30,                 // SPOKE7
    30,                 // SPOKE8
    60,                 // RING
    130,                // TOP
    130,                // BOTTOM
    128                 // RINGLETS
};
#endif

// Special "None" palette used for passing direct colors in.
AlaColor alaPalNone_[] = { 0 };
AlaPalette alaPalNone = { 0, alaPalNone_ };

AlaColor alaPalWhite_[] = { 0xFFFFFF };
AlaPalette alaPalWhite = { 1, alaPalWhite_ };

AlaColor alaPalRed_[] = { 0xFF0000 };
AlaPalette alaPalRed = { 1, alaPalRed_ };

AlaColor alaPalGreen_[] = { 0x00FF00 };
AlaPalette alaPalGreen = { 1, alaPalGreen_ };

AlaColor alaPalBlue_[] = { 0x0000FF };
AlaPalette alaPalBlue = { 1, alaPalBlue_ };




/*  *********************************************************************
    *  setAnimation(strips, animation, speed, palette)
    *  
    *  Sets a set of strips to a particular animation.  The list of
    *  strips to be set is passed in as a "bitmask", which is a
    *  number where each bit in the number represesents a particular
    *  strip.  If bit 0 is set, we will operate on strip 0,  Bit 1 means
    *  strip 1, and so on.   Therefore, if you pass in "10" decimal
    *  for "strips", in binary that is 00001010, so setStrips will
    *  set the animation for strips 1 and 3.  Using a bitmask is a 
    *  convenient way to pass a set of small numbers as a single
    *  value.
    ********************************************************************* */

void setAnimation(unsigned int strips, int animation, int speed, unsigned int direction,
                  unsigned int option, 
                  AlaPalette palette, AlaColor color)
{
    int i;

    for (i = 0; i < MAXSTRIPS; i++) {
        // the "1 << i" means "shift 1 by 'i' positions to the left.
        // So, if i has the value 3, 1<<3 will mean the value 00001000 (binary)
        // Next, the "&" operator is a bitwise AND - for each bit
        // in "strips" we will AND that bit with the correspondig bit in (1<<i).
        // allowing us to test (check) if that particular bit is set.
        if ((strips & (1 << i)) != 0) {
            allstrips[i]->forceAnimation(animation, speed, direction, option, palette, color);
        }
    }
}


/*  *********************************************************************
    *  setup()
    *  
    *  Arduino SETUP routine - perform once-only initialization.
    ********************************************************************* */



void setup()
{
    int i;

    //
    // Set up our "timer", which lets us check to see how much time
    // has passed.  We might not use it for much, but it is handy to have.
    // For now, we'll mostly use it to blink the LED on the Arduino
    //

    TIMER_UPDATE();                 // remember current time
    TIMER_SET(blinky_timer,500);    // set timer for first blink
    pinMode(PIN_LED,OUTPUT);        // set the on-board LED to an output.

    //
    // Set up the regular serial port
    // The regular serial port can be used to show debugging print messages or
    // to receive characters from the PC that the Arduino software is on.
    //
    Serial.begin(115200);
    delay(1000);                    // this delay is to make sure the serial monitor connects.
    Serial.println("Lightshow at your command!");


    //
    // Set up the data port. 
    //

    Serial3.begin(115200);
    pinMode(PIN_DATA_AVAIL, INPUT);
    pinMode(PIN_SEND_OK, OUTPUT);
    digitalWrite(PIN_SEND_OK,0);
    Serial.println("Dual-Arduino\n");

    //
    // Initialize all of the strips.  Remember the array we created?  Now we can use it to
    // set up all of the strips in a loop.  Here is where we set up each strip with its length
    // and number of LEDs.
    //
    for (i = 0; i < MAXSTRIPS; i++) {

        // Initialize the strip
        allstrips[i]->initWS2812(allLengths[i], allPins[i]);
    }

    
    // By default, run the "idle white" animation on all strips.
    // We can change this later if we want the art exhibit to start quietly.
    setAnimation(ALLSTRIPS, ALA_IDLEWHITE, 1000, 0, 0, alaPalRgb, 0);
//    setAnimation(LEDRING, ALA_IDLEWHITE, 1000, 0, 0, alaPalRgb, 0);

}



/*  *********************************************************************
    *  blinky()
    *  
    *  Blink the onboard LED at 1hz
    ********************************************************************* */

static void blinky(void)
{
    if (TIMER_EXPIRED(blinky_timer)) {
        blinky_onoff = !blinky_onoff;
        digitalWrite(PIN_LED, blinky_onoff);
        TIMER_SET(blinky_timer,500);
    }
}


/*  *********************************************************************
    *  loop()
    *  
    *  This is the main Arduino loop.  Handle characters 
    *  received from the Basic Micro
    ********************************************************************* */

void handleMessage(lsmessage_t *msgp)
{
    unsigned int stripMask;
    int animation, speed;
    uint32_t palette;
    unsigned int direction;
    unsigned int option;
    AlaPalette ap;
    AlaColor color = 0;

    stripMask = msgp->ls_strips;
    animation = msgp->ls_anim;
    speed = msgp->ls_speed;
    // Use the top bit fo the animation to indicate the direction.
    direction = (animation & 0x8000) ? 1 : 0;
    animation = (animation & 0x7FFF);
    palette = msgp->ls_color;
    option = msgp->ls_option;

    if (palette & 0x1000000) {
        ap = alaPalNone;
        color = (palette & 0x00FFFFFF);
    } else {
        switch (palette) {
            default:
            case PAL_RGB:
                ap = alaPalRgb;
                break;
            case PAL_RAINBOW:
                ap = alaPalRainbow;
                break;
            case PAL_RAINBOWSTRIPE:
                ap = alaPalRainbowStripe;
                break;
            case PAL_PARTY:
                ap = alaPalParty;
                break;
            case PAL_HEAT:
                ap = alaPalHeat;
                break;
            case PAL_FIRE:
                ap = alaPalFire;
                break;
            case PAL_COOL:
                ap = alaPalCool;
                break;
            case PAL_WHITE:
                ap = alaPalWhite;
                break;
            case PAL_RED:
                ap = alaPalRed;
                break;
            case PAL_GREEN:
                ap = alaPalGreen;
                break;
            case PAL_BLUE:
                ap = alaPalBlue;
                break;
        }
    }

    setAnimation(stripMask, animation, speed, direction, option, ap, color);

    
}


void checkOtherArduino(void)
{
  uint8_t *msgPtr = (uint8_t *) &message;

  while (digitalRead(PIN_DATA_AVAIL) || (rxstate != STATE_SYNC1)) {
  
    digitalWrite(PIN_SEND_OK,1);

    while (Serial3.available()) {
        uint8_t b = Serial3.read();

        switch (rxstate) {
            case STATE_SYNC1: 
                if (b == 0x55) rxstate = STATE_SYNC2;
                break;
            case STATE_SYNC2:
                if (b == 0xAA) {
                    rxcount = 0;
                    rxstate = STATE_RX;
                }
                else {
                  rxstate = STATE_SYNC1;
                }
                break;
            case STATE_RX:
                msgPtr[rxcount] = b;
                rxcount++;
                if (rxcount == MSGSIZE) {
                    rxstate = STATE_SYNC1;
                    digitalWrite(PIN_SEND_OK,0);
                    handleMessage(&message);
                }
                break;
        }
    }
  }

  digitalWrite(PIN_SEND_OK,0);
  
}
void loop()
{
    int i;

    // Update the current number of milliseconds since start
    TIMER_UPDATE();

    // Blink the LED.
    blinky();

    // 
    // Check for data from our other Arduino, which is doing flow control for us.
    // 

    checkOtherArduino();

    //
    // Run animations on all strips
    //

    for (i = 0; i < MAXSTRIPS; i++) {
        allstrips[i]->runAnimation();
    }


}
