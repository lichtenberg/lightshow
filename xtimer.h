#ifndef _XTIMER_H_
#define _XTIMER_H_

/*  *********************************************************************
    *  Timer Macros
    ********************************************************************* */

extern "C" {
typedef uint32_t xtimer_t;

#define TIMER_RUNNING(t) ((t) & 1)
#define TIMER_CLEAR(t) t = 0
#define TIMER_SET(t,v) t = (__now + (xtimer_t)(v)) | 1
#define TIMER_EXPIRED(t) ( ((t) & 1) && (((int) (__now - (t))) >= 0) )

#define TIMER_UPDATE() __now = millis()

#define SECONDS 1000

// Our sense of "now".  It's just a number that counts at 1KHz.
extern volatile xtimer_t __now ;
};

#endif
