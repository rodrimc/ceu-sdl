#include <stdio.h>

// definitely lost: 2,478 bytes in 17 blocks

#ifdef __ANDROID__
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
//#include "SDL_net.h"
//#include "SDL_opengles.h"
#ifdef DEBUG
#define printf(args...)     __android_log_print(4, "SDL", ## args);
#define fprintf(x, args...) __android_log_print(4, "SDL", ## args);
#else
#define printf(args...)
#define fprintf(x, args...)
#endif
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_mixer.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_opengl.h"
#endif

#include <assert.h>

#include "_ceu_app.h"

int ret_val;

s32 WCLOCK_nxt;
#ifndef CEU_IN_SDL_DT
#define ceu_out_wclock(us) WCLOCK_nxt = us;
#endif

#ifdef CEU_ASYNCS
int ASYNC_nxt = 0;
#define ceu_out_async(v) ASYNC_nxt = v;
#endif

#ifdef __ANDROID__
#define SDL_MOTION_FLOOD_AVOID
#endif
#ifdef SDL_MOTION_FLOOD_AVOID
int FLOOD_FILTER (SDL_Event* evt, void* fingerId) {
    return evt->type == SDL_FINGERMOTION &&
           ((SDL_TouchFingerEvent*)evt)->fingerId == (int)fingerId;
}
#endif

#include "_ceu_app.c"

#ifdef __ANDROID__
int SDL_main (int argc, char *argv[])
#else
int main (int argc, char *argv[])
#endif
{
    int err = SDL_Init(SDL_INIT_EVERYTHING);
    if (err != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return err;
    }

    WCLOCK_nxt = CEU_WCLOCK_INACTIVE;
#if defined(CEU_WCLOCKS) || defined(CEU_IN_SDL_DT)
    u32 old = SDL_GetTicks();
#endif

#ifdef CEU_THREADS
    // just before executing CEU code
    CEU_THREADS_MUTEX_LOCK(&CEU.threads_mutex);
#endif

    if (ceu_go_init(&ret_val))
        goto END;

// TODO: push START into queue
#ifdef CEU_IN_START
    if (ceu_go_event(&ret_val, CEU_IN_START, NULL))
        goto END;
#endif
#ifdef CEU_IN_SDL_REDRAW
    if (ceu_go_event(&ret_val, CEU_IN_SDL_REDRAW, NULL))
        goto END;
#endif

    SDL_Event evt;
#ifdef __ANDROID__
    int isPaused = 0;
#endif

    for (;;)
    {
#ifdef CEU_THREADS
        // unlock from INIT->START->REDRAW or last loop iteration
        CEU_THREADS_MUTEX_UNLOCK(&CEU.threads_mutex);
#endif

#ifndef SDL_SIMUL

        /*
         * With    SDL_DT, 'tm=0' (update as fast as possible).
         * Without SDL_DT, 'tm=?' respects the timers.
         */
#ifdef CEU_IN_SDL_DT
        s32 tm =  0;
#else
        s32 tm = -1;
#ifdef CEU_WCLOCKS
        if (WCLOCK_nxt != CEU_WCLOCK_INACTIVE)
            tm = WCLOCK_nxt / 1000;
#endif
#ifdef CEU_ASYNCS
        if (ASYNC_nxt)
            tm = 0;
#endif
#endif  // CEU_IN_SDL_DT

        int has;
#ifdef __ANDROID__
        if (isPaused) {
            has = SDL_WaitEvent(&evt);
        } else
#endif
        {
            has = SDL_WaitEventTimeout(&evt, tm);
        }
//if (has)
    //printf("EVENT %x\n", evt.type);

#if defined(CEU_WCLOCKS) || defined(CEU_IN_SDL_DT)
        u32 now = SDL_GetTicks();
        s32 dt = now - old;
        old = now;
#endif

        // redraw on wclock|handled|DT
        // (avoids redrawing for undefined events)
        int redraw = 0;

#ifdef CEU_THREADS
        // just before executing CEU code
        CEU_THREADS_MUTEX_LOCK(&CEU.threads_mutex);
#endif

#ifdef __ANDROID__
        if (!isPaused)
#endif
        {
#ifdef CEU_WCLOCKS
#ifndef CEU_IN_SDL_DT
            if (WCLOCK_nxt != CEU_WCLOCK_INACTIVE)
            {
                redraw = WCLOCK_nxt <= 1000*dt;
#endif
                if (ceu_go_wclock(&ret_val, 1000*dt))
                    goto END;
                while (WCLOCK_nxt <= 0) {
                    if (ceu_go_wclock(&ret_val, 0))
                        goto END;
                }
#ifndef CEU_IN_SDL_DT
            }
#endif
#endif
#ifdef CEU_IN_SDL_DT
            if (ceu_go_event(&ret_val, CEU_IN_SDL_DT, (void*)dt))
                goto END;
            redraw = 1;
#endif
        }

        // OTHER EVENTS
        if (has)
        {
            int handled = 1;        // =1 for defined events
//printf("EVT: %x\n", evt.type);
            int ret;
            switch (evt.type) {
#ifdef CEU_IN_SDL_QUIT
                case SDL_QUIT:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_QUIT, NULL);
                    break;
#endif
#ifdef CEU_IN_SDL_WINDOWEVENT
                case SDL_WINDOWEVENT:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_WINDOWEVENT, &evt);
                    break;
#endif

#if defined(CEU_IN_SDL_APP_WILLENTERBACKGROUND) || defined(__ANDROID__)
                case SDL_APP_WILLENTERBACKGROUND:
#ifdef __ANDROID__
                    // handle onPause/onResume
                    isPaused = 1;
#endif
#ifdef CEU_IN_SDL_APP_WILLENTERBACKGROUND
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_APP_WILLENTERBACKGROUND, &evt);
#endif
                    break;
#endif
#if defined(CEU_IN_SDL_APP_WILLENTERFOREGROUND) || defined(__ANDROID__)
                case SDL_APP_WILLENTERFOREGROUND:
#ifdef __ANDROID__
                    // handle onPause/onResume
                    isPaused = 0;
                    old = SDL_GetTicks();   // ignores previous 'old' on resume
#endif
#ifdef CEU_IN_SDL_APP_WILLENTERFOREGROUND
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_APP_WILLENTERFOREGROUND, &evt);
#endif
                    break;
#endif
#ifdef CEU_IN_SDL_KEYDOWN
                case SDL_KEYDOWN:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_KEYDOWN, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_KEYUP
                case SDL_KEYUP:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_KEYUP, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_TEXTINPUT
                case SDL_TEXTINPUT:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_TEXTINPUT, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_TEXTEDITING
                case SDL_TEXTEDITING:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_TEXTEDITING, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_MOUSEMOTION
                case SDL_MOUSEMOTION:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_MOUSEMOTION, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_MOUSEBUTTONDOWN
                case SDL_MOUSEBUTTONDOWN:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_MOUSEBUTTONDOWN, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_MOUSEBUTTONUP
                case SDL_MOUSEBUTTONUP:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_MOUSEBUTTONUP, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_FINGERDOWN
                case SDL_FINGERDOWN:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_FINGERDOWN, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_FINGERUP
                case SDL_FINGERUP:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_FINGERUP, &evt);
                    break;
#endif
#ifdef CEU_IN_SDL_FINGERMOTION
                case SDL_FINGERMOTION:
                    ret = ceu_go_event(&ret_val, CEU_IN_SDL_FINGERMOTION, &evt);
#ifdef SDL_MOTION_FLOOD_AVOID
                    // handle MOTION floods
                    SDL_FlushEventsFilter(FLOOD_FILTER,
                        (void*)((SDL_TouchFingerEvent*)&evt)->fingerId);
                    SDL_FlushEvents(SDL_DOLLARGESTURE, SDL_MULTIGESTURE);
#endif
                    break;
#endif
                default:
                    handled = 0;    // undefined event
            }
            if (ret) goto END;
            redraw = redraw || handled;
        }

#ifdef CEU_IN_SDL_REDRAW
        if (redraw) {
            if (ceu_go_event(&ret_val, CEU_IN_SDL_REDRAW, NULL))
                goto END;
        }
#endif

#endif  // SDL_SIMUL

#ifdef CEU_ASYNCS
        if (ASYNC_nxt) {
            if (ceu_go_async(&ret_val, NULL))
                goto END;
        }
#endif
    }
END:
#ifdef CEU_THREADS
    // only reachable if LOCKED
    CEU_THREADS_MUTEX_UNLOCK(&CEU.threads_mutex);
#endif
    SDL_Quit();         // TODO: slow
    return ret_val;
}

