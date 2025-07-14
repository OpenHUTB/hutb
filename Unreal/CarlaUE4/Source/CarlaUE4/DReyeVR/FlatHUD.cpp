#include "FlatHUD.h"

#include "DReyeVRUtils.h" // GetTimeSeconds

// fixing issue described in CarlaHUD.h regarding DrawText on Windows
#ifdef DrawText
#undef DrawText
#endif

void ADReyeVRHUD::SetPlayer(APlayerController *P)
{
    Player = P;
    // Player = GetOwningPlayerController();
    if (Player == nullptr)
    {
        LOG_ERROR("Can't find player controller!");
    }
    check(Player != nullptr);
}

void ADReyeVRHUD::DrawHUD()
{
    Super::DrawHUD();

    /*
    double Now = FPlatformTime::Seconds();
    int i = 0;
    while (i < StaticTextList.Num())
    {
        const TimedText &T = StaticTextList[i];
        DrawText(T.HT.Text, T.HT.Colour, T.HT.Screen.X, T.HT.Screen.Y, T.HT.TypeFace, T.HT.Scale);
        if (Now >= T.TimeToDie)
        {
            StaticTextList.RemoveAt(i);
        }
        else
        {
            // only increment if this static entity is kept, else all others are pushed to its place
            i++;
        }
    }

    // Draw all dynamic text entities once, then remove them (to be replaced)
    for (const HUDText &H : DynamicTextList)
    {
        DrawText(H.Text, H.Colour, H.Screen.X, H.Screen.Y, H.TypeFace, H.Scale);
    }
    DynamicTextList.Empty(); // clear to reuse on next DrawHUD

    // Draw texture components
    for (const HUDTexture &HT : DynamicTextures)
    {
        DrawTextureSimple(HT.U, HT.Screen.X, HT.Screen.Y, 1.f, false);
    }
    DynamicTextures.Empty(); // clear to reuse on next DrawHUD

    // Draw reticle texture
    DrawTextureSimple(ReticleTexture.U, ReticleTexture.Screen.X, ReticleTexture.Screen.Y, 1.f, false);

    // Draw line components
    for (const VRLine &L : DynamicLineList)
    {
        DrawLine(L.Start.X, L.Start.Y, L.End.X, L.End.Y, L.Colour, L.Thickness);
    }
    DynamicLineList.Empty(); // clear to reuse on next DrawHUD

    // Draw squares
    for (const HUDRect &R : DynamicRectList)
    {
        // DrawRect is a solid rectangle, we are interested in drawing a stroked-rectangle (4 lines)
        // if (R.Thickness == 0)
        // {
        //     const FVector2D Dims = R.BottomRight - R.TopLeft;
        //     const float Width = Dims.X;
        //     const float Height = Dims.Y;
        //     DrawRect(R.Colour, R.TopLeft.X + Width / 2.0, R.TopLeft.Y + Height / 2.0, Width, Height);
        // }
        // else
        {
            // DrawLine has the ability to have different thicknesses
            DrawLine(R.TopLeft.X, R.TopLeft.Y, R.BottomRight.X, R.TopLeft.Y, R.Colour, R.Thickness);         // top
            DrawLine(R.TopLeft.X, R.BottomRight.Y, R.BottomRight.X, R.BottomRight.Y, R.Colour, R.Thickness); // bottom
            DrawLine(R.TopLeft.X, R.TopLeft.Y, R.TopLeft.X, R.BottomRight.Y, R.Colour, R.Thickness);         // left
            DrawLine(R.BottomRight.X, R.TopLeft.Y, R.BottomRight.X, R.BottomRight.Y, R.Colour, R.Thickness); // right
        }
    }
    DynamicRectList.Empty(); // clear to reuse on next DrawHUD

    // Draw crosshairs
    for (const HUDCrosshair &C : DynamicCrosshairList)
    {
        const float Radius = C.Diameter / 2.f;
        FVector2D CirclePt = FVector2D(0, -Radius); // about origin so it can get rotated correctly
        const int NumSides = 16;
        const float RotAngleDeg = 360.f / NumSides;
        for (i = 0; i < NumSides; i++) // draw all the line segments
        {
            FVector2D NextPt = CirclePt.GetRotated(RotAngleDeg);
            DrawLine(C.Center.X + CirclePt.X, C.Center.Y + CirclePt.Y, C.Center.X + NextPt.X, C.Center.Y + NextPt.Y,
                     C.Colour, C.Thickness);
            CirclePt = NextPt; // draw next segment from here
        }
        if (C.InnerT) // draw the inner cross (+)
        {
            DrawLine(C.Center.X, C.Center.Y - Radius, C.Center.X, C.Center.Y + Radius, C.Colour, C.Thickness);
            DrawLine(C.Center.X - Radius, C.Center.Y, C.Center.X + Radius, C.Center.Y, C.Colour, C.Thickness);
        }
    }
    */
    DynamicCrosshairList.Empty(); // clear to reuse on next DrawHUD
}

void ADReyeVRHUD::DrawDynamicText(const FString &Str, const FVector &Loc, const FColor &Colour, const float Scale,
                                  const bool bIsRelative, const UFont *Font)
{
    FVector2D Screen;
    check(Player != nullptr);
    if (Player->ProjectWorldLocationToScreen(Loc, Screen, bIsRelative))
        DrawDynamicText(Str, Screen, Colour, Scale, Font);
}

void ADReyeVRHUD::DrawDynamicText(const FString &Str, const FVector2D &Screen, const FColor &Colour, const float Scale,
                                  const UFont *Font)
{
    HUDText DText{Str, Screen, Colour, Scale, const_cast<UFont *>(Font)};
    DynamicTextList.Add(std::move(DText));
}

void ADReyeVRHUD::DrawReticle(UTexture *U, const FVector2D &Screen)
{
    ReticleTexture.U = U;
    ReticleTexture.Screen = Screen;
}

void ADReyeVRHUD::DrawDynamicTexture(UTexture *U, const FVector2D &Screen)
{
    HUDTexture HTexture{U, Screen};
    DynamicTextures.Add(std::move(HTexture));
}

void ADReyeVRHUD::DrawStaticText(const FString &Str, const FVector &Loc, const FColor &Colour, const float Scale,
                                 const float LifeTime, const bool bIsRelative, const UFont *Font)
{
    FVector2D Screen;
    check(Player != nullptr);
    if (Player->ProjectWorldLocationToScreen(Loc, Screen, bIsRelative))
        DrawStaticText(Str, Screen, Colour, Scale, LifeTime, Font);
}

void ADReyeVRHUD::DrawStaticText(const FString &Str, const FVector2D &Screen, const FColor &Colour, const float Scale,
                                 const float LifeTime, const UFont *Font)
{
    HUDText SText{Str, Screen, Colour, Scale, const_cast<UFont *>(Font)};
    float Now = FPlatformTime::Seconds();
    TimedText TText{SText, Now + LifeTime};
    StaticTextList.Add(std::move(TText));
}

void ADReyeVRHUD::DrawDynamicLine(const FVector &Start, const FVector &End, const FColor &Colour, const float Thickness)
{
    /// TODO: store player rather than re-get
    check(Player != nullptr);
    FVector2D StartScreen, EndScreen;
    if (Player->ProjectWorldLocationToScreen(Start, StartScreen, true) &&
        Player->ProjectWorldLocationToScreen(End, EndScreen, true))
        DrawDynamicLine(StartScreen, EndScreen, Colour, Thickness);
}

void ADReyeVRHUD::DrawDynamicLine(const FVector2D &Start, const FVector2D &End, const FColor &Colour,
                                  const float Thickness)
{
    VRLine DLine{Start, End, Colour, Thickness};
    DynamicLineList.Add(std::move(DLine));
}

void ADReyeVRHUD::DrawDynamicSquare(const FVector &Center, const float Size, const FColor &Colour,
                                    const float Thickness)
{
    FVector2D CenterScreen;
    if (Player->ProjectWorldLocationToScreen(Center, CenterScreen, true))
        DrawDynamicSquare(CenterScreen, Size, Colour, Thickness);
}

void ADReyeVRHUD::DrawDynamicSquare(const FVector2D &Center, const float Size, const FColor &Colour,
                                    const float Thickness)
{
    DrawDynamicRect(Center - FVector2D(Size / 2.0, Size / 2.0), // top left
                    Center + FVector2D(Size / 2.0, Size / 2.0), // bottom right
                    Colour, Thickness);
}

void ADReyeVRHUD::DrawDynamicRect(const FVector2D &TopLeft, const FVector2D &BottomRight, const FColor &Colour,
                                  const float Thickness)
{
    HUDRect Square{TopLeft, BottomRight, Colour, Thickness};
    DynamicRectList.Add(std::move(Square));
}

void ADReyeVRHUD::DrawDynamicCrosshair(const FVector &Center, const float Diameter, const FColor &Colour,
                                       const bool InnerT, const float Thickness)
{
    FVector2D CenterScreen;
    if (Player->ProjectWorldLocationToScreen(Center, CenterScreen, true))
        DrawDynamicCrosshair(CenterScreen, Diameter, Colour, InnerT, Thickness);
}

void ADReyeVRHUD::DrawDynamicCrosshair(const FVector2D &Center, const float Diameter, const FColor &Colour,
                                       const bool InnerT, const float Thickness)
{
    HUDCrosshair Crosshair{Center, Diameter, Colour, InnerT, Thickness};
    DynamicCrosshairList.Add(std::move(Crosshair));
}