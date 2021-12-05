//
// Lightshow Arduino Sketch
// (C) 2020,2021 Mitch Lichtenberg (both of them).
//

// Set STUDIO to 0 for the living room installation
// Set STUDIO to 1 for the studio (development) installation

#define STUDIO 1

// Use this command to create archive
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
    uint16_t    ls_reserved;
    uint16_t    ls_anim;
    uint16_t    ls_speed;
    uint16_t    ls_option;
    uint32_t    ls_color;
    uint32_t    ls_strips;
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

#define MAXPHYSICALSTRIPS 12
#define MAXLOGICALSTRIPS 19


/*  *********************************************************************
    *  Global Variables
    ********************************************************************* */

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

// This struct describes a physical strip
typedef struct PhysicalStrip_s {
    uint8_t pin;                        // Pin number for strips
    uint8_t length;                     // Number of actual pixels
    Adafruit_NeoPixel *neopixels;       // Neopixel object.
} PhysicalStrip_t;

enum {
    PHYS_SPOKE1 = 0,
    PHYS_SPOKE2,
    PHYS_SPOKE3,
    PHYS_SPOKE4,
    PHYS_SPOKE5,
    PHYS_SPOKE6,
    PHYS_SPOKE7,
    PHYS_SPOKE8,
    PHYS_RING,
    PHYS_TOP,
    PHYS_BOTTOM,
    PHYS_RINGLETS
};

#if STUDIO
PhysicalStrip_t physicalStrips[MAXPHYSICALSTRIPS] = {
    [PHYS_SPOKE1] = { .pin = 39,   .length = 30 },         // SPOKE1
    [PHYS_SPOKE2] = { .pin = 41,   .length = 30 },         // SPOKE2
    [PHYS_SPOKE3] = { .pin = 43,   .length = 30 },         // SPOKE3
    [PHYS_SPOKE4] = { .pin = 45,   .length = 30 },         // SPOKE4
    [PHYS_SPOKE5] = { .pin = 38,   .length = 30 },         // SPOKE5
    [PHYS_SPOKE6] = { .pin = 40,   .length = 30 },         // SPOKE6
    [PHYS_SPOKE7] = { .pin = 42,   .length = 30 },         // SPOKE7
    [PHYS_SPOKE8] = { .pin = 44,   .length = 30 },         // SPOKE8
    [PHYS_RING]   = { .pin = 46,   .length = 60 },         // RING
    [PHYS_TOP]    = { .pin = 47,   .length = 30 },         // TOP
    [PHYS_BOTTOM] = { .pin = 48,   .length = 30 },         // BOTTOM
    [PHYS_RINGLETS] = { .pin = 51, .length = 128 },        // RINGLETS
};
#else
PhysicalStrip_t physicalStrips[MAXPHYSICALSTRIPS] = {
    [PHYS_SPOKE1] = { .pin = PORT_A2,   .length = 30 },         // SPOKE1
    [PHYS_SPOKE2] = { .pin = PORT_A3,   .length = 30 },         // SPOKE2
    [PHYS_SPOKE3] = { .pin = PORT_A4,   .length = 30 },         // SPOKE3
    [PHYS_SPOKE4] = { .pin = PORT_B5,   .length = 30 },         // SPOKE4
    [PHYS_SPOKE5] = { .pin = PORT_B6,   .length = 30 },         // SPOKE5
    [PHYS_SPOKE6] = { .pin = PORT_B7,   .length = 30 },         // SPOKE6
    [PHYS_SPOKE7] = { .pin = PORT_B8,   .length = 30 },         // SPOKE7
    [PHYS_SPOKE8] = { .pin = PORT_A1,   .length = 30 },         // SPOKE8
    [PHYS_RING] =   { .pin = PORT_A5,   .length = 60 },         // RING
    [PHYS_TOP]    = { .pin = PORT_A6,   .length = 130 },        // TOP
    [PHYS_BOTTOM] = { .pin = PORT_B4,   .length = 130 },        // BOTTOM
    [PHYS_RINGLETS] = { .pin = PORT_A7, .length = 128 },        // RINGLETS
};
#endif


//
// OK, here are the "logical" strips, which can be composed from pieces of phyiscal strips.
//

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


#define ALLSTRIPS ((1 << MAXLOGICALSTRIPS)-1)

enum {
    LOG_SPOKE1 = 0, LOG_SPOKE2, LOG_SPOKE3, LOG_SPOKE4,
    LOG_SPOKE5, LOG_SPOKE6, LOG_SPOKE7, LOG_SPOKE8,

    LOG_RING,
    LOG_TOP,
    LOG_BOTTOM,
    LOG_RINGLET1,LOG_RINGLET2,LOG_RINGLET3,LOG_RINGLET4,
    LOG_RINGLET5,LOG_RINGLET6,LOG_RINGLET7,LOG_RINGLET8,
};

//
// Additional sets we might want to define:
// LEFTSPOKES, RIGHTSPOKES, TOPSPOKES, BOTTOMSPOKES
//


typedef struct LogicalStrip_s {
    uint8_t     physical;               // which physical strip (index into array)
    uint8_t     startingLed;            // starting LED within this strip
    uint8_t     numLeds;                // number of LEDs (0 for whole strip)
    AlaLedRgb *alaStrip;                // ALA object we created.
} LogicalStrip_t;

LogicalStrip_t logicalStrips[MAXLOGICALSTRIPS] = {
    // The spokes just consume the whole strips
    [LOG_SPOKE1] = { .physical = PHYS_SPOKE1, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE2] = { .physical = PHYS_SPOKE2, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE3] = { .physical = PHYS_SPOKE3, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE4] = { .physical = PHYS_SPOKE4, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE5] = { .physical = PHYS_SPOKE5, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE6] = { .physical = PHYS_SPOKE6, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE7] = { .physical = PHYS_SPOKE7, .startingLed = 0, .numLeds = 0},
    [LOG_SPOKE8] = { .physical = PHYS_SPOKE8, .startingLed = 0, .numLeds = 0},

    [LOG_RING] = { .physical = PHYS_RING, .startingLed = 0, .numLeds = 0},
    [LOG_TOP]     = { .physical = PHYS_TOP, .startingLed = 0, .numLeds = 0},
    [LOG_BOTTOM]  = { .physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 0},
    [LOG_RINGLET1] = { .physical = PHYS_RINGLETS, .startingLed = 0, .numLeds = 16},
    [LOG_RINGLET2] = { .physical = PHYS_RINGLETS, .startingLed = 16, .numLeds = 16},
    [LOG_RINGLET3] = { .physical = PHYS_RINGLETS, .startingLed = 32, .numLeds = 16},
    [LOG_RINGLET4] = { .physical = PHYS_RINGLETS, .startingLed = 48, .numLeds = 16},

    [LOG_RINGLET5] = { .physical = PHYS_RINGLETS, .startingLed = 64, .numLeds = 16},
    [LOG_RINGLET6] = { .physical = PHYS_RINGLETS, .startingLed = 80, .numLeds = 16},
    [LOG_RINGLET7] = { .physical = PHYS_RINGLETS, .startingLed = 96, .numLeds = 16},
    [LOG_RINGLET8] = { .physical = PHYS_RINGLETS, .startingLed = 112, .numLeds = 16},
};

/*  *********************************************************************
    *  Palettes
    ********************************************************************* */

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

    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        // the "1 << i" means "shift 1 by 'i' positions to the left.
        // So, if i has the value 3, 1<<3 will mean the value 00001000 (binary)
        // Next, the "&" operator is a bitwise AND - for each bit
        // in "strips" we will AND that bit with the correspondig bit in (1<<i).
        // allowing us to test (check) if that particular bit is set.
        if ((strips & (1 << i)) != 0) {
            logicalStrips[i].alaStrip->forceAnimation(animation, speed, direction, option, palette, color);
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
    // Initialize all of the physical strips
    //
    for (i = 0; i < MAXPHYSICALSTRIPS; i++) {
        physicalStrips[i].neopixels =
            new Adafruit_NeoPixel(physicalStrips[i].length,physicalStrips[i].pin, NEO_GRB+NEO_KHZ800);
        if (!physicalStrips[i].neopixels) Serial.println("Out of memory creating physcial strips");
        physicalStrips[i].neopixels->begin();
    }

    // Now init the logical strips, but map them 1:1 to the physical ones.
    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        uint8_t startingLed, numLeds;
        // Create a new logical strip
        AlaLedRgb *alaLed = new AlaLedRgb;
        // If the logical strip's length is 0, it's the whole physical strip
        if (!alaLed) Serial.println("Out of memory creating logical strips");
        if (logicalStrips[i].numLeds == 0) {
            startingLed = 0;
            numLeds = physicalStrips[logicalStrips[i].physical].length;
        } else {
            startingLed = logicalStrips[i].startingLed;
            numLeds = logicalStrips[i].numLeds;
        }
        alaLed->initSubStrip(startingLed,
                             numLeds,
                             physicalStrips[logicalStrips[i].physical].neopixels);        
        logicalStrips[i].alaStrip = alaLed;
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

    // First compute new pixels on the LOGICAL Strips
    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        logicalStrips[i].alaStrip->runAnimation();
    }

    // Now send the data to the PHYSICAL strips
    for (i = 0; i < MAXPHYSICALSTRIPS; i++) {
        physicalStrips[i].neopixels->show();
    }


}
