//
// Lightshow Arduino Sketch
//

#include "src/ALA/AlaLedRgb.h"

#include "xtimer.h"


/*  *********************************************************************
    *  MIDI note numbers
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

/*  *********************************************************************
    *  MIDI messages
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

AlaLedRgb rgbStrip1;
AlaLedRgb rgbStrip2;
AlaLedRgb rgbStrip3;
AlaLedRgb rgbStrip4;
AlaLedRgb rgbStrip5;
AlaLedRgb rgbStrip6;
AlaLedRgb rgbStrip7;
AlaLedRgb rgbStrip8;
AlaLedRgb rgbStrip9;

#define MAXSTRIPS 9

AlaLedRgb *allstrips[9] = {
    &rgbStrip1,
    &rgbStrip2,
    &rgbStrip3,
    &rgbStrip4,
    &rgbStrip5,
    &rgbStrip6,
    &rgbStrip7,
    &rgbStrip8,
    &rgbStrip9
};


/*  *********************************************************************
    *  Amimation List.  This will eventually go away when we 
    *  assign notes to animations.
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


#if 0
int animList[] = {
    ALA_ON,
    ALA_SOUNDPULSE,
    ALA_SPARKLE,
    ALA_SPARKLE2,
    ALA_MOVINGBARS,
    ALA_PIXELSHIFTRIGHT,
    ALA_PIXELSMOOTHSHIFTRIGHT,
    ALA_COMET,
    ALA_COMETCOL,
    ALA_GLOW,
    ALA_CYCLECOLORS,
    ALA_FADECOLORS,
    ALA_FIRE,
    ALA_BOUNCINGBALLS,
    ALA_BUBBLES
};
#endif

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

int allPins[] = { 39, 41, 43, 45, 38, 40, 42, 44, 46};

int curStrip = 0;
int curAnim[4] = {0};
int curDuration[4] = {1000,1000,1000,1000};


/*  *********************************************************************
    *  setup()
    *  
    *  Arduino SETUP routine - perform once-only initialization.
    ********************************************************************* */



void setup()
{
    int i;

    TIMER_UPDATE();                 // remember current time
    TIMER_SET(blinky_timer,500);    // set timer for first blink
    pinMode(PIN_LED,OUTPUT);

    // Set up the regular serial port
    Serial.begin(115200);
    delay(1000);                    // this delay is to make sure the serial monitor connects.

    Serial.println("Lightshow at your command!");

    // Set up the MIDI port.  MIDI ports run at 31250 baud.
    Serial3.begin(31250);


    // By default, run the rainbow animation on all strips.
    for (i = 0; i < MAXSTRIPS; i++) {
        allstrips[i]->initWS2812(30, allPins[i]);
        allstrips[i]->setAnimation(animList[animation%NUMANIM], durationList[duration%3], paletteList[palette%3]);
    }

}



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
// a complete command, we will process it.
//

void procMidi(uint8_t c)
{
    if (c & MIDI_MSG_COMMAND) {
        midiCmd = c;
        midiState = MIDI_RXSTATE_NOTE;
        return;
    }
    
    switch (midiState) {
        case MIDI_RXSTATE_CMD:
            break;
            
        case MIDI_RXSTATE_NOTE:
            midiNote = c;
            midiState = MIDI_RXSTATE_VEL;
            break;
            
        case MIDI_RXSTATE_VEL:
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
    Serial.print(" and pallette "); Serial.println(palette);
    allstrips[curStrip]->setAnimation(animList[animation%NUMANIM], durationList[duration%3], paletteList[palette%3]);
}
