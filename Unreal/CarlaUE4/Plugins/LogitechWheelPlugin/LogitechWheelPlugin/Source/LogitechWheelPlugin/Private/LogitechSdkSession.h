#pragma once

#include "CoreMinimal.h"

// One session for the input plugin and all game-side consumers (game thread only).
// Callbacks keep the retry policy testable without connecting real hardware.
class FLogitechSdkSession
{
public:
    bool Initialize(double Now, TFunctionRef<bool()> Init, TFunctionRef<void()> Shutdown)
    {
        if (bInitialized)
            return true;
        if (Now < NextAttempt)
            return false;
        NextAttempt = Now + 2.0;
        bInitialized = Init();
        if (!bInitialized)
            Shutdown(); // Clean up a partially initialized SDK before the next attempt.
        return bInitialized;
    }

    bool Update(double Now, uint64 Frame, TFunctionRef<bool()> Init,
                TFunctionRef<bool()> Poll, TFunctionRef<bool()> HasDevice,
                TFunctionRef<void()> Shutdown)
    {
        if (LastFrame == Frame)
            return bUpdated;
        LastFrame = Frame;
        bUpdated = false;
        if (!Initialize(Now, Init, Shutdown))
            return false;

        bUpdated = Poll();
        if (bUpdated && HasDevice())
            UnavailableSince = -1.0;
        else if (UnavailableSince < 0.0)
            UnavailableSince = Now;
        else if (Now - UnavailableSince >= 2.0)
        {
            Shutdown();
            bInitialized = false;
            bUpdated = false;
            UnavailableSince = -1.0;
            NextAttempt = Now; // Reinitialize on the next frame, not just poll again.
        }
        return bUpdated;
    }

    void Close(TFunctionRef<void()> Shutdown)
    {
        if (bInitialized)
            Shutdown();
        *this = FLogitechSdkSession();
    }

private:
    bool bInitialized = false;
    bool bUpdated = false;
    double NextAttempt = 0.0;
    double UnavailableSince = -1.0;
    uint64 LastFrame = MAX_uint64;
};
