#pragma once

#include "CoreMinimal.h"

// One session for the input plugin and all game-side consumers (game thread only).
// Callbacks keep the retry policy testable without connecting real hardware.
// The session never shuts the SDK down while running: on real hardware a
// LogiSteeringShutdown() makes every later LogiSteeringInitialize fail, so an
// unavailable device is recovered by re-initializing in place. Only Close()
// (module unload) releases the SDK.
class FLogitechSdkSession
{
public:
    bool Initialize(double Now, TFunctionRef<bool()> Init)
    {
        if (bInitialized)
            return true;
        if (Now < NextAttempt)
            return false;
        NextAttempt = Now + 2.0;
        bInitialized = Init();
        if (bInitialized)
            bSdkStarted = true;
        return bInitialized;
    }

    bool Update(double Now, uint64 Frame, TFunctionRef<bool()> Init,
                TFunctionRef<bool()> Poll, TFunctionRef<bool()> HasDevice)
    {
        if (LastFrame == Frame)
            return bUpdated;
        LastFrame = Frame;
        bUpdated = false;
        if (!Initialize(Now, Init))
            return false;

        bUpdated = Poll();
        if (bUpdated && HasDevice())
            UnavailableSince = -1.0;
        else if (UnavailableSince < 0.0)
            UnavailableSince = Now;
        else if (Now - UnavailableSince >= 2.0)
        {
            bInitialized = false; // Re-initialize in place; a shutdown here would be unrecoverable.
            bUpdated = false;
            UnavailableSince = -1.0;
            NextAttempt = Now; // Reinitialize on the next frame, not just poll again.
        }
        return bUpdated;
    }

    void Close(TFunctionRef<void()> Shutdown)
    {
        if (bSdkStarted)
            Shutdown();
        *this = FLogitechSdkSession();
    }

private:
    bool bSdkStarted = false;
    bool bInitialized = false;
    bool bUpdated = false;
    double NextAttempt = 0.0;
    double UnavailableSince = -1.0;
    uint64 LastFrame = MAX_uint64;
};
