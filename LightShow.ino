//
// Lightshow Arduino Sketch
// (C) 2020 Mitch Lichtenberg (both of them).
//

#include "src/ALA/AlaLedRgb.h"

#include "xtimer.h"


/*  *********************************************************************
    *  MIDI note numbers (from the MIDI specification)
    *  See: https://djip.co/blog/logic-studio-9-midi-note-numbers
    ********************************************************************* */

// Second octave (below middle C).  We don't bother with sharps and flats
// These note names should match up with the names in the step sequencer
// in Logic Pro X.

#define NOTE_C2         36
#define NOTE_D2         38
#define NOTE_E2         40
#define NOTE_F2         41
#define NOTE_G2         43
#define NOTE_A2         45
#define NOTE_B2         47

// Third octave (also below Middle C)

#define NOTE_C3         48
#define NOTE_D3         50
#define NOTE_E3         52
#define NOTE_F3         53
#define NOTE_G3         55
#define NOTE_A3         57
#define NOTE_B3         59

// Fourth octave (where Middle C is)

#define NOTE_C4         60
#define NOTE_D4         62
#define NOTE_E4         64
#define NOTE_F4         65
#define NOTE_G4         67
#define NOTE_A4         69
#define NOTE_B4         71

// Fifth octave (above middle C)

#define NOTE_C5         72
#define NOTE_D5         74
#define NOTE_E5         76
#define NOTE_F5         77
#define NOTE_G5         79
#define NOTE_A5         81
#define NOTE_B5         83

// Sixth octave (above middle C)

#define NOTE_C6         84
#define NOTE_D6         86
#define NOTE_E6         87
#define NOTE_F6         88
#define NOTE_G6         90
#define NOTE_A6         93
#define NOTE_B6         95

// Seventh octave (above middle C)

#define NOTE_C7         96
#define NOTE_D7         98
#define NOTE_E7         100
#define NOTE_F7         101
#define NOTE_G7         103
#define NOTE_A7         105
#define NOTE_B7         107

// Eighth octave (above middle C)

#define NOTE_C8         108
#define NOTE_D8         110
#define NOTE_E8         112
#define NOTE_F8         113
#define NOTE_G8         115


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
// 
//

#define ALLSPOKES (SPOKE0 | SPOKE1 | SPOKE2 | SPOKE3 | SPOKE4 | SPOKE5 | SPOKE6 | SPOKE7)
#define ALLSTRIPS (ALLSPOKES | LEDRING)
#define EVENSPOKES (SPOKE0 | SPOKE2 | SPOKE4 | SPOKE6)
#define ODDSPOKES (SPOKE1 | SPOKE3 | SPOKE5 | SPOKE7)

//
// Additional sets we might want to define:
// LEFTSPOKES, RIGHTSPOKES, TOPSPOKES, BOTTOMSPOKES
//


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



/*  *********************************************************************
    *  Amimation List.  This will eventually go away when we 
    *  assign notes to animations.
    *  TEST PROGRAM ONLY
    ********************************************************************* */

int animation = 0;
int duration = 0;
int palette = 0;

int animList[] = {
    ALA_MOVINGBARS,
    ALA_ON,
    ALA_OFF,
    ALA_BLINK,
    ALA_BLINKALT,
    ALA_SPARKLE,
    ALA_SPARKLE2,
    ALA_STROBO,

    ALA_SOUNDPULSE,

    ALA_CYCLECOLORS,

    ALA_PIXELSHIFTRIGHT,
    ALA_PIXELSHIFTLEFT,
    ALA_PIXELBOUNCE,
    ALA_PIXELSMOOTHSHIFTRIGHT,
    ALA_PIXELSMOOTHSHIFTLEFT,
    ALA_PIXELSMOOTHBOUNCE,
    ALA_COMET,
    ALA_COMETCOL,
    ALA_BARSHIFTRIGHT,
    ALA_BARSHIFTLEFT,
    ALA_MOVINGGRADIENT,
    ALA_LARSONSCANNER,
    ALA_LARSONSCANNER2,

    ALA_FADEIN,
    ALA_FADEOUT,
    ALA_FADEINOUT,
    ALA_GLOW,
    ALA_PLASMA,

    ALA_FADECOLORS,
    ALA_FADECOLORSLOOP,
    ALA_PIXELSFADECOLORS,
    ALA_FLAME,

    ALA_FIRE,
    ALA_BOUNCINGBALLS,
    ALA_BUBBLES
};




/*  *********************************************************************
    *  Test progran stuff
    *  This is just for our test program, not part of the MIDI thing.
    ********************************************************************* */


// Red,Green,Blue sequence
AlaColor alaPalWhite_[] = { 0xFFFFFF };
AlaPalette alaPalWhite = { 1, alaPalWhite_ };


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

#define NUMANIM (sizeof(animList)/sizeof(int))

AlaPalette paletteList[3] = {
    alaPalRgb,
    alaPalRainbow,
    alaPalFire
};

int durationList[3] = {
    1000,
    2000,
    5000
};


int curStrip = 0;
int curAnim[4] = {0};
int curDuration[4] = {1000,1000,1000,1000};

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

void setAnimation(unsigned int strips, int animation, int speed, int palette)
{
    int i;

    for (i = 0; i < MAXSTRIPS; i++) {
        // the "1 << i" means "shift 1 by 'i' positions to the left.
        // So, if i has the value 3, 1<<3 will mean the value 00001000 (binary)
        // Next, the "&" operator is a bitwise AND - for each bit
        // in "strips" we will AND that bit with the correspondig bit in (1<<i).
        // allowing us to test (check) if that particular bit is set.
        if ((strips & (1 << i)) != 0) {
            allstrips[i]->setAnimation(animation, speed, paletteList[palette]);
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
    // serial ports!
    //
    Serial3.begin(31250);


    //
    // Initialize all of the strips.  Remember the array we created?  Now we can use it to
    // set up all of the strips in a loop.  Here is where we set up each strip with its length
    // and number of LEDs.
    //
    for (i = 0; i < MAXSTRIPS; i++) {

        // Initialize the strip
        allstrips[i]->initWS2812(allLengths[i], allPins[i]);
        
        // By default, run the "idle white" animation on all strips.
        // We can change this later if we want the art exhibit to start quietly.
        allstrips[i]->setAnimation(ALA_IDLEWHITE, durationList[duration%3], paletteList[palette%3]);
    }

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

            allstrips[stripId]->setAnimation(curAnim[stripId], curDuration[stripId], paletteList[palette%3]);
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
            allstrips[stripId]->setAnimationSpeed(curDuration[stripId]);
            break;
    }
            


}



//
// noteON : Process a MIDI NOTE ON
//

void noteOn(uint8_t chan, uint8_t note, uint8_t vel)
{
    if ((note < 36) || (note > 84)) {
        return;
    }


    // Someday there will be a switch() statement here to run an animation
    // based on which note (NOTE_C3, for example) we receive.
    //
    // So, if we get a NOTE_C3, we will change some set of animations right here.
    
    note -= 36;
    note %= 8;
    allstrips[note]->setAnimation(pulseOn);
}

//
// noteOFF : Process a MIDI NOTE OFF
//

void noteOff(uint8_t chan, uint8_t note, uint8_t vel)
{
    if ((note < 36) || (note > 84)) {
        return;
    }

    note -= 36;
    note %= 8;
    allstrips[note]->setAnimation(pulseOff);

    
}

//
// printMidi : print out a received MIDI command to the serial port (for debug)
// It is not used for anything else, see handleMidiCommand below for how we actually
// "process" a MIDI message.
//

void printMidi(void)
{
    uint8_t cmd = midiCmd >> 4;
    uint8_t chan = midiCmd & 0x0F;
    uint16_t bend;

    Serial.print("Ch "); Serial.print(chan, DEC); Serial.print(":");
    switch (cmd) {
        case MIDI_MSG_NOTE_ON:
            Serial.print("NoteOn  ");
            Serial.print(midiNote, DEC);
            Serial.print(" V");
            Serial.println(midiVel, DEC);
            break;
        case MIDI_MSG_NOTE_OFF:
            Serial.print("NoteOff ");
            Serial.print(midiNote, DEC);
            Serial.print(" V");
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

void procMidi(uint8_t c)
{
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
            
            printMidi();                        // print it out (Debug)
            handleMidiCommand();                // do something with it.
                
            break;
    }
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
    *  received from either the MIDI or the serial port
    *  and change the animations accordingly
    ********************************************************************* */

void loop()
{
    int i;

    // Update the current number of milliseconds since start
    TIMER_UPDATE();

    // Blink the LED.
    blinky();

    if (Serial.available()) {
        switch (Serial.read()) {
            case '0':  curStrip = 0; break;
            case '1':  curStrip = 1; break;
            case '2':  curStrip = 2; break;
            case '3':  curStrip = 3; break;
            case '4':  curStrip = 4; break;
            case '5':  curStrip = 5; break;
            case '6':  curStrip = 6; break;
            case '7':  curStrip = 7; break;
            case '8':  curStrip = 8; break;
            case 'a':
                animation++;
                updateAnimation();
                break;
            case 'p':
                palette++;
                updateAnimation();
                break;
            case 'd':
                duration++;
                updateAnimation();
            default:
                break;
        }

        if (curStrip >= MAXSTRIPS) curStrip = 0;
    }

    //
    // Process received MIDI bytes
    //

    if (Serial3.available()) {
        procMidi(Serial3.read());
    }

    //
    // Run animations on all strips
    //

    for (i = 0; i < MAXSTRIPS; i++) {
        allstrips[i]->runAnimation();
    }


}

void updateAnimation()
{
    Serial.print("Updating strip "); Serial.print(curStrip); Serial.print(" to animation ");
    Serial.print(animation); Serial.print(" with duration "); Serial.print(durationList[duration%3]);
    Serial.print(" and palette "); Serial.println(palette);
    allstrips[curStrip]->setAnimation(animList[animation%NUMANIM], durationList[duration%3], paletteList[palette%3]);
}
