//
// Lightshow Arduino Sketch
// (C) 2020 Mitch Lichtenberg (both of them).
//

#include "src/ALA/AlaLedRgb.h"

#include "xtimer.h"



int debug = 0;

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

#define MAXSTRIPS 9

AlaLedRgb rgbStrip0;            // These are the spokes
AlaLedRgb rgbStrip1;
AlaLedRgb rgbStrip2;
AlaLedRgb rgbStrip3;
AlaLedRgb rgbStrip4;
AlaLedRgb rgbStrip5;
AlaLedRgb rgbStrip6;
AlaLedRgb rgbStrip7;
AlaLedRgb rgbStrip8;            // This is the ring

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

//
// Constants to represent groups of spokes
// It's easier than listing them out each time we use the same group.
//

#define ALLSPOKES (SPOKE0 | SPOKE1 | SPOKE2 | SPOKE3 | SPOKE4 | SPOKE5 | SPOKE6 | SPOKE7)
#define ALLSTRIPS (ALLSPOKES | LEDRING)
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
    &rgbStrip8
};

//
// This array contains the Arduino pin numbers that
// correspond to each LED strip.
//
// There are 9 entries, representing each spoke (rgbStrip0 to rgbStrip7)
// plus the last one (on pin 46) being the LED ring (rgbStrip8)
//

// In the setup routine, we will make rgbStrip0 correspond to allPins[0] (which is 39),
// and rgbStrip1 correspond to allPins[1], which is 41, etc.
// using a nice 'for' loop to do them all at once.

int allPins[MAXSTRIPS] = { 39, 41, 43, 45, 38, 40, 42, 44, 46};

//
// Similarly to allPins, allLengths[] is an array that tells us the size
// of each strip. For the rig on my desk, there are 30 LEDs per "spoke" and the 60 LED ring
// So, allLengths[0..7] = 30, and allLengths[8] is 60.
//

int allLengths[MAXSTRIPS] = { 30, 30, 30, 30, 30, 30, 30, 30, 60 };




int palette = 0;




// Red,Green,Blue sequence
AlaColor alaPalWhite_[] = { 0xFFFFFF };
AlaPalette alaPalWhite = { 1, alaPalWhite_ };

// Create an array of 64 palettes.

AlaColor alaColorWheel[64];
AlaPalette alaWheelPalette[64];

AlaSeq pulseOn[] = {
    {ALA_SOUNDPULSE, 150, 150, alaPalWhite},
    {ALA_STOPSEQ, 0, 0, NULL},
    {ALA_ENDSEQ, 0, 0, NULL}
};

AlaSeq pulseOff[] = {
    {ALA_OFF, 10, 10, alaPalWhite},
    {ALA_STOPSEQ, 0, 0, NULL},
    {ALA_ENDSEQ, 0, 0, NULL}
};


/*  *********************************************************************
    *  colorFromVelocity(vel)
    *  
    *  Take a MIDI velocity (0-127) and turn it into an RGB
    *  color along a simplified color wheel.
    *  
    *  Returns a 24-bit RGB color
    ********************************************************************* */

uint32_t colorFromVelocity(uint8_t vel)
{
    unsigned int wheel = vel*2;
    unsigned int red,green,blue;
    
    wheel = 255 - wheel;
    if (wheel < 85) {
        red = 255-wheel*3;
        green = 0;
        blue = wheel*3;
    } else if(wheel < 170) {
        wheel -= 85;
        red = 0;
        green = wheel*3;
        blue = 255-wheel*3;
    } else {
        wheel -= 170;
        red = wheel*3;
        green = 255-wheel*3;
        blue = 0;
    }

    if (red > 255) red = 255;
    if (green > 255) green = 255;
    if (blue > 255) blue = 255;

    return ((red << 16) | (green << 8) | blue);
    
}

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

void setAnimation(unsigned int strips, int animation, int speed, AlaPalette palette)
{
    int i;

    for (i = 0; i < MAXSTRIPS; i++) {
        // the "1 << i" means "shift 1 by 'i' positions to the left.
        // So, if i has the value 3, 1<<3 will mean the value 00001000 (binary)
        // Next, the "&" operator is a bitwise AND - for each bit
        // in "strips" we will AND that bit with the correspondig bit in (1<<i).
        // allowing us to test (check) if that particular bit is set.
        if ((strips & (1 << i)) != 0) {
            allstrips[i]->forceAnimation(animation, speed, palette);
        }
    }
}

/*  *********************************************************************
    *  setSequence(strips, sequence)
    *  
    *  This is similar to setAnimation above, but sets an ALA "Sequence"
    *  which is ALA's way of describing a chain of animations.
    *  We might change the way this works
    ********************************************************************* */

void setSequence(unsigned int strips, AlaSeq *sequence)
{
    int i;

    for (i = 0; i < MAXSTRIPS; i++) {
        if ((strips & (1 << i)) != 0) {
            allstrips[i]->setAnimation(sequence);
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
    // Create the color wheel
    //

    for (unsigned int c = 0; c < 256; c += 4) {
        alaColorWheel[c/4] = AlaColor(colorFromVelocity(c));
        alaWheelPalette[c/4].numColors = 1;
        alaWheelPalette[c/4].colors = &alaColorWheel[c/4];
    }

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
    setAnimation(ALLSTRIPS, ALA_IDLEWHITE, 1000, alaPalRgb);

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
    *  received from either the Pro Micro or the serial port
    *  and change the animations accordingly
    ********************************************************************* */


#define CONSOLEBUFSIZE 50
static char consoleBuffer[CONSOLEBUFSIZE];
static int consoleIdx = 0;

static void processConsole(char *buf)
{
    char cmd;
    char *tok;
    int args[5];
    int argcnt;
    
    Serial.print("Debug command: "); Serial.println(buf);

    cmd = *buf++;

    tok = strtok(buf," ");

    for (argcnt = 0; argcnt < 5; argcnt++) args[argcnt] = 0;
    
    for (argcnt = 0; argcnt < 5; argcnt++) {
        if (tok) {
            args[argcnt] = atoi(tok);
            tok = strtok(NULL, " ");
        }
        else  {
            break;
        }
    }
          

    switch (cmd) {
        case 'a':
            Serial.print("Set strip "); Serial.print(args[0]);
            Serial.print(" to "); Serial.println(args[1]);
            setAnimation((1<<args[0]), args[1], (argcnt > 2) ? args[2] : 1000, alaPalRgb);
            break;
        case 'x':
            Serial.println("All strips off");
            setAnimation(ALLSTRIPS, ALA_OFF, 1000, alaPalRgb);
            break;
        case 'd':
            debug = !debug;
            Serial.println(debug ? "Debug is ON" : "Debug is OFF");
            break;
        default:
            Serial.println("Unrecognized command");
            break;
    }
    
    
}


static void processConsoleCharacter(char ch)
{
    switch (ch) {
        case '\b':
            if (consoleIdx > 0) {
                consoleIdx--;
                consoleBuffer[consoleIdx] = 0;
            }
            break;
        case '\r':
        case '\n':
            consoleBuffer[consoleIdx] = 0;
            processConsole(consoleBuffer);
            consoleIdx = 0;
            break;
        default:
            if (consoleIdx < (CONSOLEBUFSIZE-1)) {
                consoleBuffer[consoleIdx++] = ch;
                consoleBuffer[consoleIdx] = 0;
            }
            break;
    }
}

#define MSGSIZE 8
#define STATE_SYNC1     0
#define STATE_SYNC2     1
#define STATE_RX        2

static uint8_t message[MSGSIZE];
static int rxstate = STATE_SYNC1;
static int rxcount = 0;

void handleMessage(uint8_t *message)
{
    unsigned int stripMask;
    int animation, speed, palette;

    stripMask = ((unsigned int) message[0]) | (((unsigned int) message[1]) << 8);
    animation = ((unsigned int) message[2]) | (((unsigned int) message[3]) << 8);
    speed = ((unsigned int) message[4]) | (((unsigned int) message[5]) << 8);
    palette = ((unsigned int) message[6]) | (((unsigned int) message[7]) << 8);

#if 0
    Serial.print("Msk ");
    Serial.print(stripMask, HEX);
    Serial.print("  anim ");
    Serial.print(animation);
    Serial.println("");
#endif

    setAnimation(stripMask, animation, speed, alaPalRgb);
    
}

void checkOtherArduino(void)
{

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
                message[rxcount] = b;
                rxcount++;
                if (rxcount == MSGSIZE) {
                    rxstate = STATE_SYNC1;
                    digitalWrite(PIN_SEND_OK,0);
                    handleMessage(message);
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
    char ch;

    // Update the current number of milliseconds since start
    TIMER_UPDATE();

    // Blink the LED.
    blinky();

    // Handle input from serial port for debugging.
    if (Serial.available()) {
        ch = Serial.read();
//        Serial.write(ch);
        processConsoleCharacter(ch);
    }

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
