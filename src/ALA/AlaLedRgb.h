#ifndef AlaLedRgb_h
#define AlaLedRgb_h

#include "Ala.h"

#include "ExtNeoPixel.h"

/**
 *  AlaLedRgb can be used to drive a single or multiple RGB leds to perform animations.
 */
class AlaLedRgb
{

public:

    AlaLedRgb();

    /**
    * Initializes WS2812 LEDs. It be invoked in the setup() function of the main Arduino sketch.
    *
    * The type field can be used to set the RGB order and chipset frequency. Constants are ExtNeoPixel.h file.
    * It is set by default to NEO_GRB + NEO_KHZ800.
    */
//    void initWS2812(int numLeds, byte pin, byte type=NEO_GRB+NEO_KHZ800);
    void initSubStrip(int startingLed, int numLeds, Adafruit_NeoPixel *pixels);

    /**
    * Sets the maximum brightness level.
    */
    void setBrightness(AlaColor maxOut);

    /**
    * Sets the maximum refresh rate in Hz (default value is 50 Hz).
    * May be useful to reduce flickering in some cases.
    */
    void setRefreshRate(int refreshRate);

    int getCurrentRefreshRate();

    void setAnimationSpeed(long speed);

//    void setAnimation(int animation, long speed, unsigned int direction, AlaColor color);
    void setAnimation(int animation, long speed, unsigned int direction, unsigned int option, AlaPalette palette, AlaColor color);

//    void forceAnimation(int animation, long speed, unsigned int direction, AlaColor color);
    void forceAnimation(int animation, long speed, unsigned int direction, unsigned int option, AlaPalette palette, AlaColor color);

//    void setAnimation(AlaSeq animSeq[]);
    int getAnimation();

    bool runAnimation();



private:

    void setAnimationFunc(int animation);
    void on();
    void off();
    void blink();
    void blinkAlt();
    void sparkle();
    void sparkle2();
    void strobo();
    void cycleColors();

    // New animations for art project
    void soundPulse();
    void idleWhite();
    void onePixel();
    void pixelLine();
    void grow();
    void shrink();
    void pixelMarch();

    void pixelShiftRight();
    void pixelShiftLeft();
    void pixelBounce();
    void pixelSmoothShiftRight();
    void pixelSmoothShiftLeft();
    void comet();
    void cometCol();
    void pixelSmoothBounce();
    void larsonScanner();
    void larsonScanner2();

    void fadeIn();
    void fadeOut();
    void fadeInOut();
    void glow();
    void plasma();
    void fadeColors();
    void pixelsFadeColors();
    void fadeColorsLoop();

    void movingBars();
    void movingGradient();

    void fire();
    void bouncingBalls();
    void bubbles();


    AlaColor *leds; // array to store leds brightness values
    int startingLed; // Index of the first LED
    int numLeds;    // number of leds

    int animation;
    long speed;
    unsigned int option;
    unsigned int direction;
    AlaColor singleColor;
    AlaPalette palette;
    AlaSeq *animSeq;
    int animSeqLen;
    long animSeqDuration;

    void (AlaLedRgb::*animFunc)();
    AlaColor maxOut;
    int refreshMillis;
    int refreshRate;   // current refresh rate
    unsigned long animStartTime;
    unsigned long animSeqStartTime;
    unsigned long lastRefreshTime;

    float *pxPos;
    float *pxSpeed;

    Adafruit_NeoPixel *neopixels;

};


#endif
