//
// Lightshow Arduino Sketch
// (C) 2020 Mitch Lichtenberg (both of them).
//

#include "src/ALA/AlaLedRgb.h"

#include "xtimer.h"

//
// If the processor is an ARM, then assume we're on a Due and pull
// in the MIDIUSB library
//

#ifdef __arm__
#define _MIDIUSB_
#include "MIDIUSB.h"
#endif

#define _DUAL_ARDUINO_

/*  *********************************************************************
    *  MIDI note numbers (from the MIDI specification), but adjusted
    *  for what Logic Pro calls the octaves.
    *  See: https://djip.co/blog/logic-studio-9-midi-note-numbers
    *
    *  Logic Pro's octaves are actually one lower than MIDI,
    *  so C9 in the MIDI specification is called C8 on Logic Pro's
    *  on screen keyboard.
    *
    *  The constants below reflect what Logic Pro calls them.
    ********************************************************************* */

// First octave (below middle C).  
// These note names should match up with the names in the piano roll sequencer
// in Logic Pro X.

#define NOTE_C0         24
#define NOTE_C0S        25
#define NOTE_D0         26
#define NOTE_D0S        27
#define NOTE_E0         28
#define NOTE_F0         29
#define NOTE_F0S        30
#define NOTE_G0         31
#define NOTE_G0S        32
#define NOTE_A0         33
#define NOTE_A0S        34
#define NOTE_B0         35

// Second octave (below middle C).  

#define NOTE_C1         36
#define NOTE_C1S        37
#define NOTE_D1         38
#define NOTE_D1S        39
#define NOTE_E1         40
#define NOTE_F1         41
#define NOTE_F1S        42
#define NOTE_G1         43
#define NOTE_G1S        44
#define NOTE_A1         45
#define NOTE_A1S        46
#define NOTE_B1         47

// Third octave (also below Middle C)

#define NOTE_C2         48
#define NOTE_C2S        49
#define NOTE_D2         50
#define NOTE_D2S        51
#define NOTE_E2         52
#define NOTE_F2         53
#define NOTE_F2S        54
#define NOTE_G2         55
#define NOTE_G2S        56
#define NOTE_A2         57
#define NOTE_A2S        58
#define NOTE_B2         59

// Fourth octave (where Middle C is)

#define NOTE_C3         60
#define NOTE_C3S        61
#define NOTE_D3         62
#define NOTE_D3S        63
#define NOTE_E3         64
#define NOTE_F3         65
#define NOTE_F3S        66
#define NOTE_G3         67
#define NOTE_G3S        68
#define NOTE_A3         69
#define NOTE_A3S        70
#define NOTE_B3         71

// Fifth octave (above middle C)

#define NOTE_C4         72
#define NOTE_C4S        73
#define NOTE_D4         74
#define NOTE_D4S        75
#define NOTE_E4         76
#define NOTE_F4         77
#define NOTE_F4S        78
#define NOTE_G4         79
#define NOTE_G4S        80
#define NOTE_A4         81
#define NOTE_A4S        82
#define NOTE_B4         83

// Sixth octave (above middle C)

#define NOTE_C5         84
#define NOTE_C5S        85
#define NOTE_D5         86
#define NOTE_D5S        87
#define NOTE_E5         88
#define NOTE_F5         89
#define NOTE_F5S        90
#define NOTE_G5         91
#define NOTE_G5S        92
#define NOTE_A5         93
#define NOTE_A5S        94
#define NOTE_B5         95

// Seventh octave (above middle C)

#define NOTE_C6         96
#define NOTE_C6S        97
#define NOTE_D6         98
#define NOTE_D6S        99
#define NOTE_E6         100
#define NOTE_F6         101
#define NOTE_F6S        102
#define NOTE_G6         103
#define NOTE_G6S        104
#define NOTE_A6         105
#define NOTE_A6S        106
#define NOTE_B6         107

// Eighth octave (above middle C)

#define NOTE_C7         108
#define NOTE_C7S        109
#define NOTE_D7         110
#define NOTE_D7S        111
#define NOTE_E7         112
#define NOTE_F7         113
#define NOTE_F7S        114
#define NOTE_G7         115
#define NOTE_G7S        116
#define NOTE_A7         117
#define NOTE_A7S        118
#define NOTE_B7         119

// Ninth Octave (above middle C)

#define NOTE_C8         120
#define NOTE_C8S        121
#define NOTE_D8         122
#define NOTE_D8S        123
#define NOTE_E8         124
#define NOTE_F8         125
#define NOTE_F8S        126
#define NOTE_G8         127



/*  *********************************************************************
    *  MIDI messages (from the MIDI specification)
    ********************************************************************* */

#define MIDI_MSG_NOTE_OFF       0x08
#define MIDI_MSG_NOTE_ON        0x09
#define MIDI_MSG_POLYTOUCH      0x0A
#define MIDI_MSG_CONTROL_CHANGE 0x0B
#define MIDI_MSG_PROGRAM_CHANGE 0x0C
#define MIDI_MSG_CHANNEL_PRESS  0x0D
#define MIDI_MSG_PITCH_BEND     0x0E
#define MIDI_MSG_SYSTEM         0x0F

#define MIDI_MSG_COMMAND        0x80


/*  *********************************************************************
    *  MIDI receive state machine
    *  This is for our Arduino sketch, it keeps track of 
    *  where we are as we receive each MIDI message
    ********************************************************************* */

#define MIDI_RXSTATE_CMD 0              // next byte is a command
#define MIDI_RXSTATE_NOTE 1             // next byte is a note number
#define MIDI_RXSTATE_VEL 2              // next byte is a velocity
uint8_t midiState = MIDI_RXSTATE_CMD;       // state we are in now

//
// These variables will hold the last received MIDI command,
//
uint8_t midiCmd;                        // last received MIDI command
uint8_t midiNote;                       // last received MIDI note
uint8_t midiVel;                        // last received MIDI velocity

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

// Flow control for our special dual-Arduino MIDI setup that
// uses a Pro Micro to process MIDI and the Uno to run the pixels
#define PIN_DATA_AVAIL  8                 // input, '1' if there is MIDI data
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



/*  *********************************************************************
    *  Test progran stuff
    *  This is just for our test program, not part of the MIDI thing.
    ********************************************************************* */


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
    // Set up the MIDI port.  MIDI ports run at 31250 baud but are otherwise just plain
    // serial ports!    Only do this if we're not using USB
    //
#ifndef _MIDIUSB_
#ifdef _DUAL_ARDUINO_
    Serial3.begin(115200);
    pinMode(PIN_DATA_AVAIL, INPUT);
    pinMode(PIN_SEND_OK, OUTPUT);
    digitalWrite(PIN_SEND_OK,0);
    Serial.println("Dual-Arduino\n");
#else
    Serial3.begin(31250);
#endif
#endif



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
    *  MIDI Message Routines.
    *  
    *  The routines in this section are called for the various MIDI
    *  messages that we receive.
    *  
    *  The most important ones that we are likely to deal with are
    *  "Note ON" and "Note OFF", but we can handle other
    *  message types just in case we want to make them 
    *  meaningful in the future.
    ********************************************************************* */

//
// controlChange:  Called when we receive a MIDI CONTROL CHANGE command
//
// These controls, shown below, are for the Akai MIDI controller.
// This will go away soon, it is not needed anymore.
// In the future the control changes will come from Logic, if needed
// it's really not necessary for the art lightshow unless
// there is something fancy we can do.
// 

void controlChange(uint8_t chan,uint8_t ctl,uint8_t val)
{

    // The "#ifdef" means "if defined", and is a preprocessor directive.
    // in "C" you can use this to comment out blocks of code in a big batch
    // In this case, since "NOTUSED" is in fact not defined, the "ifdef" will
    // be FALSE, and none of this code will be included.
    
#ifdef NOTUSED    
    int numAnim = sizeof(animList) / sizeof(int);
    int cvalue = (int) val;
    int newAnim;
    int stripId;
    char buf[50];

    // controls 20-23 are the sliders
    // 3, 9, 14, 15 are the knobs

    switch (ctl) {
        case 20:
        case 21:
        case 22:
        case 23:

            newAnim = cvalue / 3;
            if (newAnim >= numAnim) {
                Serial.println("anim out of range");
                return;
            }

            stripId = ctl-20;

            curAnim[stripId] = animList[newAnim];

            snprintf(buf,sizeof(buf),"Strip %d anim is %d speed %d\n",stripId, curAnim[stripId],curDuration[stripId]);
            Serial.print(buf);

            allstrips[stripId]->forceAnimation(curAnim[stripId], curDuration[stripId], paletteList[palette%3]);
            break;

        case 3:
        case 9:
        case 14:
        case 15:
            stripId = 0;
            if (ctl == 9) stripId = 1;
            if (ctl == 14) stripId = 2;
            if (ctl == 15) stripId = 3;

            newAnim = (5120 - (cvalue * 40)) + 100;
            if (newAnim < 100) newAnim = 100;

            curDuration[stripId] = newAnim;

            snprintf(buf,sizeof(buf),"Strip %d anim is %d speed %d\n",stripId, curAnim[stripId],curDuration[stripId]);
            Serial.print(buf);

//            allstrips[stripId]->setAnimation(curAnim[stripId], curDuration[stripId], paletteList[palette%3]);
            allstrips[stripId]->forceAnimationSpeed(curDuration[stripId]);
            break;
    }

#endif

}


//
// convertVelocity(animationID, midivel)
//
// Given an ALA animation number, scale the MIDI velocity (how hard the key is struck)
// int a sensible value for that animation to give us some
// choice in the speed
//

int convertVelocity(int animationID, uint8_t midiVel)
{
    int vel = (int) midiVel;
    // larger numbers for ALA mean slower animations
    // but we would want higher MIDI velocities to mean faster animations.
    // Choosing mid-scale (64), figure out an offset (+/- 63)
    int offset = (64 - vel);
    
    switch (animationID) {
        case ALA_BOUNCINGBALLS:
        case ALA_MOVINGGRADIENT:
        case ALA_PIXELSFADECOLORS:
        case ALA_FADECOLORSLOOP:
        case ALA_PLASMA:
            return 1000 + (offset*8);
            
        case ALA_SPARKLE:
        case ALA_MOVINGBARS:
            return 1000 + (offset*12);

        case ALA_COMET:
            return 1000 + (offset*10);

        default:
            return 1000;
    }
}


//
// noteON : Process a MIDI NOTE ON
//
// When we receive a noteOn message, we will start an animation.   Which animation
// we start and which strips we start it on depend on the note number being sent.
//

void noteOn(uint8_t chan, uint8_t note, uint8_t vel)
{

    // For now we will ignore the MIDI channel and the velocity,
    // but we could definitely use these values to affect
    // the animations in the future.

    // we list the notes backwards from G8 (top octave) since that is how they
    // are represented in Logic Pro's Piano Roll screen

    // See the file "AnimationAssignments.numbers" for details on the assignments.

    switch (note) {

        // ========================================================================
        // OCTAVE 0
        // ========================================================================

        case NOTE_C0:
            setAnimation(SPOKE0, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_C0S:
            setAnimation(SPOKE1, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_D0:
            setAnimation(SPOKE2, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_D0S:
            setAnimation(SPOKE3, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_E0:
            setAnimation(SPOKE4, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_F0:
            setAnimation(SPOKE5, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_F0S:
            setAnimation(SPOKE6, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_G0:
            setAnimation(SPOKE7, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_G0S:
            setAnimation(LEDRING, ALA_OFF, 1000, alaPalRgb);
            break;
        case NOTE_A0:
            setAnimation(ALLSTRIPS, ALA_SOUNDPULSE, 150, alaWheelPalette[vel/2]);
            break;
        case NOTE_A0S:
            setAnimation(ALLSTRIPS, ALA_IDLEWHITE, 1000, alaPalRgb);
            break;
        case NOTE_B0:
            setAnimation(ALLSTRIPS, ALA_OFF, 1000, alaPalRgb);
            break;

            // ==================================================================================
            // OCTAVE 1
            // ==================================================================================

        case NOTE_C1:
            setAnimation(ALLSTRIPS, ALA_SPARKLE, convertVelocity(ALA_SPARKLE, vel), alaPalRgb);
            break;
        case NOTE_C1S:
            setAnimation(ALLSPOKES, ALA_MOVINGBARS, convertVelocity(ALA_MOVINGBARS, vel), alaPalRgb);
            break;
        case NOTE_D1:
            setAnimation(ALLSPOKES, ALA_BOUNCINGBALLS, convertVelocity(ALA_BOUNCINGBALLS, vel), alaPalRgb);
            break;
        case NOTE_D1S:
            setAnimation(ALLSPOKES, ALA_PLASMA, convertVelocity(ALA_PLASMA, vel), alaPalRgb);
            break;
        case NOTE_E1:
            setAnimation(ALLSPOKES, ALA_FIRE, convertVelocity(ALA_FIRE, vel), alaPalFire);
            break;
        case NOTE_F1:
            setAnimation(ALLSPOKES, ALA_MOVINGGRADIENT, convertVelocity(ALA_MOVINGGRADIENT, vel), alaPalRgb);
            break;
        case NOTE_F1S:
            setAnimation(ALLSPOKES, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_G1:
            setAnimation(ALLSPOKES, ALA_FADECOLORSLOOP, convertVelocity(ALA_FADECOLORSLOOP, vel), alaPalRgb);
            break;
        case NOTE_G1S:
            setAnimation(ALLSPOKES, ALA_BUBBLES, convertVelocity(ALA_BUBBLES, vel), alaPalRgb);
            break;
        case NOTE_A1:
            setAnimation(ALLSPOKES, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_A1S:
            setAnimation(ALLSPOKES, ALA_LARSONSCANNER, convertVelocity(ALA_LARSONSCANNER, vel), alaPalRgb);
            break;
        case NOTE_B1:
            setAnimation(LEDRING, ALA_LARSONSCANNER, convertVelocity(ALA_LARSONSCANNER, vel), alaPalRgb);
            break;
            

            // ==================================================================================
            // OCTAVE 2
            // ==================================================================================

        case NOTE_C2:
            setAnimation(SPOKE0, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_C2S:
            setAnimation(SPOKE1, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_D2:
            setAnimation(SPOKE2, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_D2S:
            setAnimation(SPOKE3, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_E2:
            setAnimation(SPOKE4, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_F2:
            setAnimation(SPOKE5, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_F2S:
            setAnimation(SPOKE6, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_G2:
            setAnimation(SPOKE7, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_G2S:
            setAnimation(LEDRING, ALA_COMET, convertVelocity(ALA_COMET, vel), alaPalRgb);
            break;
        case NOTE_A2:
            break;
        case NOTE_A2S:
            break;
        case NOTE_B2:
            break;
            

            // ==================================================================================
            // OCTAVE 3
            // ==================================================================================

        case NOTE_C3:
            setAnimation(SPOKE0, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_C3S:
            setAnimation(SPOKE1, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_D3:
            setAnimation(SPOKE2, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_D3S:
            setAnimation(SPOKE3, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_E3:
            setAnimation(SPOKE4, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_F3:
            setAnimation(SPOKE5, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_F3S:
            setAnimation(SPOKE6, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_G3:
            setAnimation(SPOKE7, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_G3S:
            setAnimation(LEDRING, ALA_PIXELSFADECOLORS, convertVelocity(ALA_PIXELSFADECOLORS, vel), alaPalRgb);
            break;
        case NOTE_A3:
            break;
        case NOTE_A3S:
            break;
        case NOTE_B3:
            break;
            
            // ==================================================================================
            // OCTAVE 4
            // ==================================================================================

        case NOTE_C4:
            setAnimation(SPOKE0, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_C4S:
            setAnimation(SPOKE1, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_D4:
            setAnimation(SPOKE2, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_D4S:
            setAnimation(SPOKE3, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_E4:
            setAnimation(SPOKE4, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_F4:
            setAnimation(SPOKE5, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_F4S:
            setAnimation(SPOKE6, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_G4:
            setAnimation(SPOKE7, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_G4S:
            setAnimation(LEDRING, ALA_PIXELSMOOTHSHIFTRIGHT, convertVelocity(ALA_PIXELSMOOTHSHIFTRIGHT, vel), alaPalRgb);
            break;
        case NOTE_A4:
            break;
        case NOTE_A4S:
            break;
        case NOTE_B4:
            break;
            
            // ==================================================================================
            // OCTAVE 5
            // ==================================================================================

        case NOTE_C5:
            setAnimation(SPOKE0, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_C5S:
            setAnimation(SPOKE1, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_D5:
            setAnimation(SPOKE2, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_D5S:
            setAnimation(SPOKE3, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_E5:
            setAnimation(SPOKE4, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_F5:
            setAnimation(SPOKE5, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_F5S:
            setAnimation(SPOKE6, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_G5:
            setAnimation(SPOKE7, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_G5S:
            setAnimation(LEDRING, ALA_GLOW, convertVelocity(ALA_GLOW, vel), alaPalRgb);
            break;
        case NOTE_A5:
            break;
        case NOTE_A5S:
            break;
        case NOTE_B5:
            break;
            

    }
  

    
}

//
// noteOFF : Process a MIDI NOTE OFF
//
// We don't use this (yet) - in the future we might turn OFF certain amimations
// when the note is released, so it will play as long as a note is held
// and stop afterwards.   For example, if we had a strobe light
// effect or other fast animation, it might make sense to represent this
// on the piano roll as a long sustained note where the strobe stops when the note
// is lifted.
//

void noteOff(uint8_t chan, uint8_t note, uint8_t vel)
{
    // Nothing happening here yet.
}

//
// printMidi : print out a received MIDI command to the serial port (for debug)
// It is not used for anything else, see handleMidiCommand below for how we actually
// "process" a MIDI message.
//

void printNoteName(uint8_t num)
{
    char notename[5];
    char *ptr = notename;
    char sharp;
    
    int octave = (num / 12)-2;
    int note = (num % 12);

    *ptr++ = "CCDDEFFGGAAB"[note];
    sharp = " # #  # # # "[note];
    if (sharp != ' ') *ptr++ = sharp;
    if (octave >= 0) {
        *ptr++ = "0123456789X"[octave];
    } else {
        *ptr++ = '-';
        if (octave == -1)  *ptr++ = '1';
        if (octave == -2)  *ptr++ = '2';
    }
    *ptr++ = 0;
    Serial.print(notename);
}

void printMidi(void)
{
    uint8_t cmd = midiCmd >> 4;
    uint8_t chan = midiCmd & 0x0F;
    uint16_t bend;

    // Hack: don't print note off messages.
    //if (cmd == MIDI_MSG_NOTE_OFF) return;

    Serial.print("Ch "); Serial.print(chan, DEC); Serial.print(":");
    switch (cmd) {
        case MIDI_MSG_NOTE_ON:
            Serial.print("NoteOn  ");
            Serial.print(midiNote, DEC);
            Serial.print(" (");
            printNoteName(midiNote);
            Serial.print(") V");
            Serial.println(midiVel, DEC);
            break;
        case MIDI_MSG_NOTE_OFF:
            Serial.print("NoteOff ");
            Serial.print(midiNote, DEC);
            Serial.print(" (");
            printNoteName(midiNote);
            Serial.print(") V");
            Serial.println(midiVel, DEC);
            break;
        case MIDI_MSG_POLYTOUCH:
            Serial.print("Plytch  ");
            Serial.print(midiNote, DEC);
            Serial.print(" P");
            Serial.println(midiVel, DEC);
            break;
        case MIDI_MSG_CONTROL_CHANGE:
            Serial.print("CtlChg  ");
            Serial.print(midiNote, DEC);
            Serial.print("  ");
            Serial.println(midiVel, DEC);
            break;
        case MIDI_MSG_PROGRAM_CHANGE:
            Serial.print("PrgChg  ");
            Serial.println(midiNote, DEC);
            break;
        case MIDI_MSG_CHANNEL_PRESS:
            Serial.print("Chprss  ");
            Serial.println(midiNote, DEC);
            break;
        case MIDI_MSG_PITCH_BEND:
            bend = (((uint16_t) midiNote) << 7)|(uint16_t)midiVel;
            Serial.print("Ptchbnd ");
            Serial.println(bend, HEX);
            break;
        case MIDI_MSG_SYSTEM:
        default:
            Serial.print("Cmd: "); Serial.print(cmd, HEX); Serial.print(" ");
            Serial.print(midiNote, HEX); Serial.print(" ");
            Serial.println(midiVel, HEX);
            break;
    }
}


//
// handleMidiCommand : Act on a complete received MIDI command
//

void handleMidiCommand(void)
{
    uint8_t cmd = midiCmd >> 4;
    uint8_t chan = midiCmd & 0x0F;
    uint16_t bend;

    switch (cmd) {
        case MIDI_MSG_NOTE_ON:
            noteOn(chan,midiNote,midiVel);
            break;
        case MIDI_MSG_NOTE_OFF:
            noteOff(chan,midiNote,midiVel);
            break;
        case MIDI_MSG_POLYTOUCH:
            break;
        case MIDI_MSG_CONTROL_CHANGE:
            controlChange(chan,midiNote,midiVel);
            break;
        case MIDI_MSG_PROGRAM_CHANGE:
            break;
        case MIDI_MSG_CHANNEL_PRESS:
            break;
        case MIDI_MSG_PITCH_BEND:
            break;
        case MIDI_MSG_SYSTEM:
        default:
            break;
    }
}

//
// procMidi: process a byte received from the MIDI port.  When we have
// a complete command, we will process it.  This routine forms something called
// a "state machine", which uses a variable to tell us what data we expect to
// recieve next.
// 
// MIDI messages look something like this:
//
//     <command> <note> <velocity>       That's 3 bytes.
//
// So, if you received  3 hex bytes   "93"  "40" "46"
// that would mean "Note ON", "MIDI Channel 3", with a note number of E3 and a velocity of 100 (decimal).
// As the bytes come in from the serial port, we need to know which piece is next - the command, the note,
// or the velocity.  That's what "midiState" is for, it tells what *state* we are in.   This value changes
// and also tells us when we have a complete MIDI message to process.
//
// MIDI has some shortcuts to handle pressing many notes at once, but this is basically what
// all MIDI messages look like.
//
// This only applies to MIDI-over-serial.   USB is handled differently.
//

#ifndef _MIDIUSB_
void procMidi(uint8_t c)
{
    // Warning: if you print stuff out, we can drop characters on the MIDI port!
    //Serial.print("["); Serial.print(c,HEX); Serial.println("]");
    //
    // The upper bit is always set if this is a MIDI command.   If it is not set, then
    // keep receiving notes using the previous command.   This is the shortcut mentioned above.
    //
    if (c & MIDI_MSG_COMMAND) {
        midiCmd = c;
        midiState = MIDI_RXSTATE_NOTE;
        return;
    }

    //
    // Otherwise, depending on what state we are in, decide what the byte from the
    // serial stream is for.
    //
    
    switch (midiState) {
        case MIDI_RXSTATE_CMD:
            // If it was a command, we already remembered it from the 'if' statement above.
            break;
            
        case MIDI_RXSTATE_NOTE:
            // If it is a note, remember the note, and set the state to say the next byte will be 'velocity'
            midiNote = c;
            midiState = MIDI_RXSTATE_VEL;
            break;
            
        case MIDI_RXSTATE_VEL:
            // If it's the velocity, then we go back to 'command' again.   Also, if it is
            // velocity, we have a complete MIDI messsage so we can start handling it.
            midiVel = c;
            midiState = MIDI_RXSTATE_CMD;

            // After we have received the velocity byte, we have a completed
            // MIDI command.  Print it out (Debug) and
            // act on it.
            
//            printMidi();                        // print it out (Debug)
            handleMidiCommand();                // do something with it.
                
            break;
    }
}
#endif


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
    *  received from either the MIDI or the serial port
    *  and change the animations accordingly
    ********************************************************************* */

uint8_t midibuffer[100];
int midiptr = 0;

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
        case 'n':
            Serial.print("MIDI note "); Serial.println(args[0]);
            noteOn(0, (uint8_t) args[0], (argcnt > 1) ? args[1] : 64);
            break;
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


void checkOtherArduino(void)
{
  uint8_t message[5];
  size_t cnt;
  bool handle = false;
  static int state = 0;

  while (digitalRead(PIN_DATA_AVAIL)) {
  
    digitalWrite(PIN_SEND_OK,1);

    while (Serial3.available()) {
      uint8_t b = Serial3.read();

      switch (state) {
        case 0: 
          if (b == 0x55) state = 1;
          break;
        case 1:
          if (b == 0xAA) state = 2;
          else state = 0;
          break;
        case 2:
          midiCmd = b;
          state = 3;
          break;
        case 3:
          midiNote = b;
          state = 4;
          break;
        case 4:
          midiVel = b;
          digitalWrite(PIN_SEND_OK,0);
          //printMidi();
          handleMidiCommand();
          state = 0;         
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
    // Process received MIDI bytes
    // What we do here depends on if we're using the MIDIUSB
    // interface or the serial MIDI
    //
#ifdef _MIDIUSB_
    if (1) {
        // In the case of USB MIDI, we just pull the packets from the MIDIUSB library
        // and pretend like we got them over the serial port.
        midiEventPacket_t rxMidi;

        do {
            rxMidi = MidiUSB.read();
            if (rxMidi.header != 0) {
                midiCmd = rxMidi.byte1;
                midiNote = rxMidi.byte2;
                midiVel = rxMidi.byte3;
                if (debug) printMidi();
                handleMidiCommand();
            }
        } while (rxMidi.header != 0);
    }
#else
    // In the case of serial port MIDI (like the E-MU interface), we get
    // characters from the Serial3 port and feed them into the
    // USB state machine.

#ifdef _DUAL_ARDUINO_
    checkOtherArduino();
#else
    if (Serial3.available()) {
        uint8_t c = Serial3.read();

        // Capture the first 100 received MIDI messages and
        // print them out when we enter an "X" command over the serial port.
        if (midiptr < sizeof(midibuffer)) midibuffer[midiptr++] = c;
        
        procMidi(c);
    }
#endif
#endif

    //
    // Run animations on all strips
    //

    for (i = 0; i < MAXSTRIPS; i++) {
        allstrips[i]->runAnimation();
    }


}
