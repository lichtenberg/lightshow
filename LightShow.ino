#include "src/ALA/AlaLedRgb.h"


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

    // Set up the regular serial port
    Serial.begin(115200);
    delay(1000);

    Serial.println("Lightshow at your command!");

    // Set up the MIDI port
    Serial3.begin(31250);


  for (i = 0; i < MAXSTRIPS; i++) {
      allstrips[i]->initWS2812(30, allPins[i]);
      allstrips[i]->setAnimation(animList[animation%NUMANIM], durationList[duration%3], paletteList[palette%3]);
  }

}


/*  *********************************************************************
    *  MIDI stuff
    ********************************************************************* */


#define MIDI_CMD 0
#define MIDI_NOTE 1
#define MIDI_VEL 2
int midiState = MIDI_CMD;

//
// These variables will hold the last received MIDI command,
//
uint8_t midiCmd;
uint8_t midiNote;
uint8_t midiVel;


//
// controlChange:  Called when we receive a MIDI CONTROL CHANGE command
// 

void controlChange(uint8_t ctl,uint8_t val)
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

void noteOn(uint8_t note, uint8_t vel)
{
    if ((note < 36) || (note > 84)) {
        return;
    }

    note -= 36;
    note %= 8;
    allstrips[note]->setAnimation(pulseOn);
}

//
// noteOFF : Process a MIDI NOTE OFF
//

void noteOff(uint8_t note, uint8_t vel)
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
        case 0x09:
            Serial.print("NoteOn  ");
            Serial.print(midiNote, DEC);
            Serial.print(" V");
            Serial.println(midiVel, DEC);
            noteOn(midiNote,midiVel);
            break;
        case 0x08:
            Serial.print("NoteOff ");
            Serial.print(midiNote, DEC);
            Serial.print(" V");
            Serial.println(midiVel, DEC);
            noteOff(midiNote,midiVel);
            break;
        case 0x0A:
            Serial.print("Plytch  ");
            Serial.print(midiNote, DEC);
            Serial.print(" P");
            Serial.println(midiVel, DEC);
            break;
        case 0xB:
            Serial.print("CtlChg  ");
            Serial.print(midiNote, DEC);
            Serial.print("  ");
            Serial.println(midiVel, DEC);
            controlChange(midiNote,midiVel);
            break;
        case 0x0C:
            Serial.print("PrgChg  ");
            Serial.println(midiNote, DEC);
            break;
        case 0x0D:
            Serial.print("Chprss  ");
            Serial.println(midiNote, DEC);
            break;
        case 0x0E:
            bend = (((uint16_t) midiNote) << 7)|(uint16_t)midiVel;
            Serial.print("Ptchbnd ");
            Serial.println(bend, HEX);
            break;
        case 0x0F:
        default:
            Serial.print("Cmd: "); Serial.print(cmd, HEX); Serial.print(" ");
            break;
    }
}


//
// procMidi: Handle a byte received from the MIDI port
//

void procMidi(uint8_t c)
{
    if (c & 0x80) {
        midiCmd = c;
        midiState = MIDI_NOTE;
        return;
    }
    switch (midiState) {
        case MIDI_CMD:
            break;
        case MIDI_NOTE:
            midiNote = c;
            midiState = MIDI_VEL;
            break;
        case MIDI_VEL:
            midiVel = c;
            midiState = MIDI_CMD;
            printMidi();
            break;
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
