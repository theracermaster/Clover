/*
 * refit/screen.c
 * Screen handling functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//#include "Platform.h"
#include "libegint.h"

#include "egemb_refit_banner.h"

#ifndef DEBUG_ALL
#define DEBUG_SCR 1
#else
#define DEBUG_SCR DEBUG_ALL
#endif

#if DEBUG_SCR == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_SCR, __VA_ARGS__)	
#endif

// Console defines and variables

UINTN ConWidth;
UINTN ConHeight;
CHAR16 *BlankLine = NULL;

static VOID SwitchToText(IN BOOLEAN CursorEnabled);
static VOID SwitchToGraphics(VOID);
static VOID DrawScreenHeader(IN CHAR16 *Title);
static VOID UpdateConsoleVars(VOID);
static INTN ConvertEdgeAndPercentageToPixelPosition(INTN Edge, INTN DesiredPercentageFromEdge, INTN ImageDimension, INTN ScreenDimension);
static INTN CalculateNudgePosition(INTN Position, INTN NudgeValue, INTN ImageDimension, INTN ScreenDimension);
//INTN RecalculateImageOffset(INTN AnimDimension, INTN ValueToScale, INTN ScreenDimensionToFit, INTN ThemeDesignDimension);
static BOOLEAN IsImageWithinScreenLimits(INTN Value, INTN ImageDimension, INTN ScreenDimension);
static INTN RepositionFixedByCenter(INTN Value, INTN ScreenDimension, INTN DesignScreenDimension);
static INTN RepositionRelativeByGapsOnEdges(INTN Value, INTN ImageDimension, INTN ScreenDimension, INTN DesignScreenDimension);
static INTN HybridRepositioning(INTN Edge, INTN Value, INTN ImageDimension, INTN ScreenDimension, INTN DesignScreenDimension);

// UGA defines and variables

INTN   UGAWidth;
INTN   UGAHeight;
BOOLEAN AllowGraphicsMode;

EG_RECT  BannerPlace = {0, 0, 0, 0};

EG_PIXEL StdBackgroundPixel   = { 0xbf, 0xbf, 0xbf, 0xff};
EG_PIXEL MenuBackgroundPixel  = { 0x00, 0x00, 0x00, 0x00};
EG_PIXEL InputBackgroundPixel = { 0xcf, 0xcf, 0xcf, 0x80};
EG_PIXEL BlueBackgroundPixel  = { 0x7f, 0x0f, 0x0f, 0xff};

EG_IMAGE *BackgroundImage = NULL;


static BOOLEAN GraphicsScreenDirty;

// general defines and variables

static BOOLEAN haveError = FALSE;

//
// Screen initialization and switching
//

VOID InitScreen(IN BOOLEAN SetMaxResolution)
{
    // initialize libeg
    egInitScreen(SetMaxResolution);
    
    if (egHasGraphicsMode()) {
        egGetScreenSize(&UGAWidth, &UGAHeight);
        AllowGraphicsMode = TRUE;
    } else {
        AllowGraphicsMode = FALSE;
        egSetGraphicsModeEnabled(FALSE);   // just to be sure we are in text mode
    }
    GraphicsScreenDirty = TRUE;
    
    // disable cursor
    gST->ConOut->EnableCursor (gST->ConOut, FALSE);
    
    UpdateConsoleVars();

    // show the banner (even when in graphics mode)
//    DrawScreenHeader(L"Initializing...");
}

VOID SetupScreen(VOID)
{
    if (GlobalConfig.TextOnly) {
        // switch to text mode if requested
        AllowGraphicsMode = FALSE;
        SwitchToText(FALSE);
        
    } else if (AllowGraphicsMode) {
        // clear screen and show banner
        // (now we know we'll stay in graphics mode)
        SwitchToGraphics();
//        BltClearScreen(TRUE);
    }
}

static VOID SwitchToText(IN BOOLEAN CursorEnabled)
{
    egSetGraphicsModeEnabled(FALSE);
    gST->ConOut->EnableCursor (gST->ConOut, CursorEnabled);
}

static VOID SwitchToGraphics(VOID)
{
    if (AllowGraphicsMode && !egIsGraphicsModeEnabled()) {
      InitScreen(FALSE);
        egSetGraphicsModeEnabled(TRUE);
        GraphicsScreenDirty = TRUE;
    }
}

//
// Screen control for running tools
//

VOID BeginTextScreen(IN CHAR16 *Title)
{
    DrawScreenHeader(Title);
    SwitchToText(FALSE);
    
    // reset error flag
    haveError = FALSE;
}

VOID FinishTextScreen(IN BOOLEAN WaitAlways)
{
    if (haveError || WaitAlways) {
        SwitchToText(FALSE);
 //       PauseForKey(L"FinishTextScreen");
    }
    
    // reset error flag
    haveError = FALSE;
}

VOID BeginExternalScreen(IN BOOLEAN UseGraphicsMode, IN CHAR16 *Title)
{
    if (!AllowGraphicsMode)
        UseGraphicsMode = FALSE;
    
    if (UseGraphicsMode) {
        SwitchToGraphics();
//        BltClearScreen(FALSE);
    }
    
    // show the header
    DrawScreenHeader(Title);
    
    if (!UseGraphicsMode)
        SwitchToText(TRUE);
    
    // reset error flag
    haveError = FALSE;
}

VOID FinishExternalScreen(VOID)
{
    // make sure we clean up later
    GraphicsScreenDirty = TRUE;
    
    if (haveError) {
        // leave error messages on screen in case of error,
        // wait for a key press, and then switch
        PauseForKey(L"was error, press any key\n");
        SwitchToText(FALSE);
    }
    
    // reset error flag
    haveError = FALSE;
}

VOID TerminateScreen(VOID)
{
    // clear text screen
    gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
    gST->ConOut->ClearScreen (gST->ConOut);
    
    // enable cursor
    gST->ConOut->EnableCursor (gST->ConOut, TRUE);
}

static VOID DrawScreenHeader(IN CHAR16 *Title)
{
  UINTN i;
	CHAR16* BannerLine = AllocatePool((ConWidth + 1) * sizeof(CHAR16));
  BannerLine[ConWidth] = 0;


  // clear to black background
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  //    gST->ConOut->ClearScreen (gST->ConOut);

  // paint header background
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BANNER);
  for (i = 1; i < ConWidth-1; i++)
    BannerLine[i] = BOXDRAW_HORIZONTAL;
	BannerLine[0] = BOXDRAW_DOWN_RIGHT;
	BannerLine[ConWidth-1] = BOXDRAW_DOWN_LEFT;
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 0);
	Print(BannerLine);

	for (i = 1; i < ConWidth-1; i++)
    BannerLine[i] = ' ';
	BannerLine[0] = BOXDRAW_VERTICAL;
	BannerLine[ConWidth-1] = BOXDRAW_VERTICAL;
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 1);
	Print(BannerLine);

	for (i = 1; i < ConWidth-1; i++)
    BannerLine[i] = BOXDRAW_HORIZONTAL;
 	BannerLine[0] = BOXDRAW_UP_RIGHT;
	BannerLine[ConWidth-1] = BOXDRAW_UP_LEFT;
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 2);
	Print(BannerLine);

	FreePool(BannerLine);

  // print header text
  gST->ConOut->SetCursorPosition (gST->ConOut, 3, 1);
  Print(L"Clover - %s", Title);

  // reposition cursor
  gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 4);
}

//
// Keyboard input
//

BOOLEAN ReadAllKeyStrokes(VOID)
{
    BOOLEAN       GotKeyStrokes;
    EFI_STATUS    Status;
    EFI_INPUT_KEY key;
    
    GotKeyStrokes = FALSE;
    for (;;) {
        Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        break;
    }
    return GotKeyStrokes;
}

VOID PauseForKey(CHAR16* msg)
{
#if REFIT_DEBUG > 0  
    UINTN index;
    if (msg) {
      Print(L"\n %s", msg);
    }
    Print(L"\n* Hit any key to continue *");
    
    if (ReadAllKeyStrokes()) {  // remove buffered key strokes
        gBS->Stall(5000000);     // 5 seconds delay
        ReadAllKeyStrokes();    // empty the buffer again
    }
    
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);
    ReadAllKeyStrokes();        // empty the buffer to protect the menu
    
    Print(L"\n");
#endif
}

#if REFIT_DEBUG > 0
VOID DebugPause(VOID)
{
    // show console and wait for key
    SwitchToText(FALSE);
    PauseForKey(L"");
    
    // reset error flag
    haveError = FALSE;
}
#endif

VOID EndlessIdleLoop(VOID)
{
    UINTN index;
    
    for (;;) {
        ReadAllKeyStrokes();
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);
    }
}

//
// Error handling
//
/*
VOID
StatusToString (
				OUT CHAR16      *Buffer,
				EFI_STATUS      Status
				)
{
	UnicodeSPrint(Buffer, 64, L"EFI Error %r", Status);
}*/


BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where)
{
//    CHAR16 ErrorName[64];
    
    if (!EFI_ERROR(Status))
        return FALSE;
    
//    StatusToString(ErrorName, Status);
    gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
    Print(L"Fatal Error: %r %s\n", Status, where);
    gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
    haveError = TRUE;
    
    //gBS->Exit(ImageHandle, ExitStatus, ExitDataSize, ExitData);
    
    return TRUE;
}

BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where)
{
//    CHAR16 ErrorName[64];
    
    if (!EFI_ERROR(Status))
        return FALSE;
    
//    StatusToString(ErrorName, Status);
    gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
    Print(L"Error: %r %s\n", Status, where);
    gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
    haveError = TRUE;
    
    return TRUE;
}

//
// Graphics functions
//

VOID SwitchToGraphicsAndClear(VOID)
{
    SwitchToGraphics();
    if (GraphicsScreenDirty)
        BltClearScreen(TRUE);
}
/*
typedef struct {
  INTN     XPos;
  INTN     YPos;
  INTN     Width;
  INTN     Height;
} EG_RECT;
*/

EG_IMAGE *Banner = NULL;
EG_IMAGE *BigBack = NULL;


VOID BltClearScreen(IN BOOLEAN ShowBanner)
{
  
  INTN BanHeight = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + LAYOUT_BANNER_HEIGHT;
  INTN i, j, x, x1, x2, y, y1, y2, BannerPosX, BannerPosY;
  EG_PIXEL    *p1;
  
  // load banner on first call
  if (!Banner) {
    if (!GlobalConfig.BannerFileName || !ThemeDir) {
      if (GlobalConfig.Theme) { // regular theme - this points to refit built in image. should be changed to clover image at some point.
        Banner = egPrepareEmbeddedImage(&egemb_refit_banner, FALSE);
      } else { // embedded theme - use text as banner
        Banner = egCreateImage(7*StrLen(L"CLOVER"), 32, TRUE);
        egFillImage(Banner, &MenuBackgroundPixel);
        egRenderText(L"CLOVER", Banner, 0, 0, 0xFFFF);
        DebugLog(1, "Text <%s> rendered\n", L"Clover");
      }
    } else {
      Banner = egLoadImage(ThemeDir, GlobalConfig.BannerFileName, FALSE);
      if (Banner) {
        CopyMem(&BlueBackgroundPixel, &Banner->PixelData[0], sizeof(EG_PIXEL));
      }
    }
  }
  if (!Banner) {
    DBG("banner file not read\n"); 
    //TODO create BigText "Clover"
  }
  
  //load Background and scale
  //TODO - rescale image if UGAWidth changed
  if (!BigBack && (GlobalConfig.BackgroundName != NULL)) {
    BigBack = egLoadImage(ThemeDir, GlobalConfig.BackgroundName, FALSE);
  }

  if (BackgroundImage != NULL && (BackgroundImage->Width != UGAWidth || BackgroundImage->Height != UGAHeight)) {
    // Resolution changed
    FreePool(BackgroundImage);
    BackgroundImage = NULL;
  }
  
  if (BackgroundImage == NULL) {
    if (GlobalConfig.Theme) { // regular theme
      BackgroundImage = egCreateFilledImage(UGAWidth, UGAHeight, FALSE, &BlueBackgroundPixel); 
    } else { // embedded theme - use stdbackground color
      BackgroundImage = egCreateFilledImage(UGAWidth, UGAHeight, FALSE, &StdBackgroundPixel); 
    }
  }
  
  if (BigBack != NULL) {
    switch (GlobalConfig.BackgroundScale) {
      case Scale:
        ScaleImage(BackgroundImage, BigBack);
        break;
      case Crop:
        x = UGAWidth - BigBack->Width;
        if (x >= 0) {
          x1 = x >> 1;
          x2 = 0;
          x = BigBack->Width;
        } else {
          x1 = 0;
          x2 = (-x) >> 1;
          x = UGAWidth;
        }
        y = UGAHeight - BigBack->Height;
        if (y >= 0) {
          y1 = y >> 1;
          y2 = 0;
          y = BigBack->Height;
        } else {
          y1 = 0;
          y2 = (-y) >> 1;
          y = UGAHeight;
        }
        egRawCopy(BackgroundImage->PixelData + y1 * UGAWidth + x1,
                  BigBack->PixelData + y2 * BigBack->Width + x2,
                  x, y, UGAWidth, BigBack->Width);
        break;
      case Tile:
        x = (BigBack->Width * ((UGAWidth - 1) / BigBack->Width + 1) - UGAWidth) >> 1;
        y = (BigBack->Height * ((UGAHeight - 1) / BigBack->Height + 1) - UGAHeight) >> 1;
        p1 = BackgroundImage->PixelData;
        for (j = 0; j < UGAHeight; j++) {
          y2 = ((j + y) % BigBack->Height) * BigBack->Width;
          for (i = 0; i < UGAWidth; i++) {
            *p1++ = BigBack->PixelData[y2 + ((i + x) % BigBack->Width)];
          }
        }
        break;
      case None:
      default:
        // already scaled
        break;
    }
  }  

  if (ShowBanner && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)) {
    // clear and draw banner    
    if (BackgroundImage) {
      BltImage(BackgroundImage, 0, 0); //if NULL then do nothing
    } else {
      egClearScreen(&StdBackgroundPixel);
    }

    if (Banner != NULL){
      BannerPlace.Width = Banner->Width;
      BannerPlace.Height = (BanHeight >= Banner->Height) ? Banner->Height : BanHeight;

      if ((GlobalConfig.BannerPosX == 0xFFFF) && (GlobalConfig.BannerPosY == 0xFFFF)) {
        // Use rEFIt default
        BannerPlace.XPos = (UGAWidth - Banner->Width) >> 1;
        BannerPlace.YPos = (BanHeight >= Banner->Height) ? (BanHeight - Banner->Height) : 0;
      } else {
        // Has banner position been specified in the theme.plist?
        if ((GlobalConfig.BannerPosX >=0 && GlobalConfig.BannerPosX <=100) && (GlobalConfig.BannerPosY >=0 && GlobalConfig.BannerPosY <=100)) {
          BannerPosX = GlobalConfig.BannerPosX;
          BannerPosY = GlobalConfig.BannerPosY;
          // Check if screen size being used is different from theme origination size.
          // If yes, then recalculate the placement % value.
          // This is necessary because screen can be a different size, but banner is not scaled.
          BannerPosX = HybridRepositioning(GlobalConfig.BannerEdgeHorizontal, BannerPosX, BannerPlace.Width,  UGAWidth,  GlobalConfig.ThemeDesignWidth );
          BannerPosY = HybridRepositioning(GlobalConfig.BannerEdgeVertical,   BannerPosY, BannerPlace.Height, UGAHeight, GlobalConfig.ThemeDesignHeight);

          if (!IsImageWithinScreenLimits(BannerPosX, BannerPlace.Width, UGAWidth) || !IsImageWithinScreenLimits(BannerPosY, BannerPlace.Height, UGAHeight)) {
            // This banner can't be displayed
            return;
          }
          BannerPlace.XPos = BannerPosX;
          BannerPlace.YPos = BannerPosY;
        }
      
        // Check if banner is required to be nudged.
        BannerPlace.XPos = CalculateNudgePosition(BannerPlace.XPos,GlobalConfig.BannerNudgeX,Banner->Width,UGAWidth);
        BannerPlace.YPos = CalculateNudgePosition(BannerPlace.YPos,GlobalConfig.BannerNudgeY,Banner->Height,UGAHeight);
      }
      BltImageAlpha(Banner, BannerPlace.XPos, BannerPlace.YPos, &MenuBackgroundPixel, 16);
    }
  } else {
      // clear to standard background color
      egClearScreen(&StdBackgroundPixel);
      BannerPlace.XPos = 0;
      BannerPlace.YPos = 0;
      BannerPlace.Width = UGAWidth;
      BannerPlace.Height = BanHeight;
  }
  InputBackgroundPixel.r = (MenuBackgroundPixel.r + 0) & 0xFF;
  InputBackgroundPixel.g = (MenuBackgroundPixel.g + 0) & 0xFF;
  InputBackgroundPixel.b = (MenuBackgroundPixel.b + 0) & 0xFF;
  InputBackgroundPixel.a = (MenuBackgroundPixel.a + 0) & 0xFF;
  GraphicsScreenDirty = FALSE;
}

VOID BltImage(IN EG_IMAGE *Image, IN INTN XPos, IN INTN YPos)
{
  if (!Image) {
    return;
  }
  egDrawImageArea(Image, 0, 0, 0, 0, XPos, YPos);
  GraphicsScreenDirty = TRUE;
}

VOID BltImageAlpha(IN EG_IMAGE *Image, IN INTN XPos, IN INTN YPos, IN EG_PIXEL *BackgroundPixel, INTN Scale)
{
  EG_IMAGE *CompImage;
  EG_IMAGE *NewImage = NULL;
  INTN Width = Scale << 3;
  INTN Height = Width;
  GraphicsScreenDirty = TRUE;
  if (Image) {
    NewImage = egCopyScaledImage(Image, Scale); //will be Scale/16
    Width = NewImage->Width;
    Height = NewImage->Height;
  }
  // compose on background
  CompImage = egCreateFilledImage(Width, Height, (BackgroundImage != NULL), BackgroundPixel);
  egComposeImage(CompImage, NewImage, 0, 0);
  if (NewImage) {
    egFreeImage(NewImage);
  }
  if (!BackgroundImage) {
    egDrawImageArea(CompImage, 0, 0, 0, 0, XPos, YPos);
    egFreeImage(CompImage);
    return;
  }
  NewImage = egCreateImage(Width, Height, FALSE);
  if (!NewImage) return;
  
  egRawCopy(NewImage->PixelData,
            BackgroundImage->PixelData + YPos * BackgroundImage->Width + XPos,
            Width, Height,
            Width,
            BackgroundImage->Width);
  egComposeImage(NewImage, CompImage, 0, 0);
  egFreeImage(CompImage);

  // blit to screen and clean up
  egDrawImageArea(NewImage, 0, 0, 0, 0, XPos, YPos);
  if (NewImage) {
    egFreeImage(NewImage);
  }
}

VOID BltImageComposite(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN INTN XPos, IN INTN YPos)
{
    INTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
    EG_IMAGE *CompImage;
  
  if (!BaseImage || !TopImage) {
    return;
  }
  
    // initialize buffer with base image
    CompImage = egCopyImage(BaseImage);
    TotalWidth  = BaseImage->Width;
    TotalHeight = BaseImage->Height;
    
    // place the top image
    CompWidth = TopImage->Width;
    if (CompWidth > TotalWidth)
        CompWidth = TotalWidth;
    OffsetX = (TotalWidth - CompWidth) >> 1;
    CompHeight = TopImage->Height;
    if (CompHeight > TotalHeight)
        CompHeight = TotalHeight;
    OffsetY = (TotalHeight - CompHeight) >> 1;
    egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
    
    // blit to screen and clean up
//    egDrawImageArea(CompImage, 0, 0, TotalWidth, TotalHeight, XPos, YPos);
  BltImageAlpha(CompImage, XPos, YPos, &MenuBackgroundPixel, 16);
    egFreeImage(CompImage);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageCompositeBadge(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN EG_IMAGE *BadgeImage, IN INTN XPos, IN INTN YPos)
{
  INTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
  EG_IMAGE *CompImage;

  if (!BaseImage || !TopImage) {
    return;
  }

  TotalWidth  = BaseImage->Width;
  TotalHeight = BaseImage->Height;
  
  //  DBG("BaseImage: Width=%d Height=%d Alfa=%d\n", TotalWidth, TotalHeight, CompImage->HasAlpha);
  CompWidth = TopImage->Width;
  CompHeight = TopImage->Height;
  if (GlobalConfig.Theme) { // regular theme
    CompImage = egCreateFilledImage((CompWidth > TotalWidth)?CompWidth:TotalWidth,
                                    (CompHeight > TotalHeight)?CompHeight:TotalHeight,
                                    TRUE,
                                    &MenuBackgroundPixel);
  } else { // embedded theme - draw box around icons
    EG_PIXEL EmbeddedBackgroundPixel  = { 0xaa, 0xaa, 0xaa, 0xaa};
    CompImage = egCreateFilledImage((CompWidth > TotalWidth)?CompWidth:TotalWidth,
                                    (CompHeight > TotalHeight)?CompHeight:TotalHeight,
                                    TRUE,
                                    &EmbeddedBackgroundPixel);
  }
  if (!CompImage) {
    DBG("Can't create CompImage\n");
    return;
  }
  //to simplify suppose square images
  if (CompWidth < TotalWidth) {
    OffsetX = (TotalWidth - CompWidth) >> 1;
    OffsetY = (TotalHeight - CompHeight) >> 1;
    egComposeImage(CompImage, BaseImage, 0, 0);
    egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
    CompWidth = TotalWidth;
    CompHeight = TotalHeight;
  } else {
    OffsetX = (CompWidth - TotalWidth) >> 1;
    OffsetY = (CompHeight - TotalHeight) >> 1;
    egComposeImage(CompImage, BaseImage, OffsetX, OffsetY);
    egComposeImage(CompImage, TopImage, 0, 0);
  }

  // place the badge image
  if (BadgeImage != NULL &&
      (BadgeImage->Width + 8) < CompWidth &&
      (BadgeImage->Height + 8) < CompHeight) {
    //    OffsetX += CompWidth  - 8 - BadgeImage->Width;
    //    OffsetY += CompHeight - 8 - BadgeImage->Height;
    //blackosx
    // Check for user badge x offset from theme.plist
    if (GlobalConfig.BadgeOffsetX != 0xFFFF) {
      // Check if value is between 0 and ( width of the main icon - width of badge )
      if (GlobalConfig.BadgeOffsetX >= 0 && GlobalConfig.BadgeOffsetX <= (UINTN)(CompWidth - BadgeImage->Width)) {
        OffsetX += GlobalConfig.BadgeOffsetX;
      } else {
        DBG("User offset X %d is out of range\n",GlobalConfig.BadgeOffsetX);
        OffsetX += CompWidth  - 8 - BadgeImage->Width;
      }
    } else {
      // Set default position
      OffsetX += CompWidth  - 8 - BadgeImage->Width;
    }
    // Check for user badge y offset from theme.plist
    if (GlobalConfig.BadgeOffsetY != 0xFFFF) {
      // Check if value is between 0 and ( height of the main icon - height of badge )
      if (GlobalConfig.BadgeOffsetY >= 0 && GlobalConfig.BadgeOffsetY <= (UINTN)(CompHeight - BadgeImage->Height)) {
        OffsetY += GlobalConfig.BadgeOffsetY;
      } else {
        DBG("User offset Y %d is out of range\n",GlobalConfig.BadgeOffsetY);
        OffsetY += CompHeight - 8 - BadgeImage->Height;
      }
    } else {
      // Set default position
      OffsetY += CompHeight - 8 - BadgeImage->Height;
    }

    egComposeImage(CompImage, BadgeImage, OffsetX, OffsetY);
  }

  // blit to screen and clean up
  if (GlobalConfig.Theme) { // regular theme
    BltImageAlpha(CompImage, XPos, YPos, &MenuBackgroundPixel, 16);
  } else { // embedded theme - don't use BltImageAlpha as it can't handle refit's built in image
    egDrawImageArea(CompImage, 0, 0, TotalWidth, TotalHeight, XPos, YPos);
  }
  egFreeImage(CompImage);
  GraphicsScreenDirty = TRUE;
}

static EG_IMAGE    *CompImage = NULL;
#define MAX_SIZE_ANIME 256

VOID FreeAnime(GUI_ANIME *Anime)
{
   if (Anime) {
     if (Anime->Path) {
       FreePool(Anime->Path);
       Anime->Path = NULL;
     }
     FreePool(Anime);
     Anime = NULL;
   }
}

/* Replaced for now with Reposition* below
INTN RecalculateImageOffset(INTN AnimDimension, INTN ValueToScale, INTN ScreenDimensionToFit, INTN ThemeDesignDimension)
{
    INTN SuppliedGapDimensionPxDesigned=0;
    INTN OppositeGapDimensionPxDesigned=0;
    INTN OppositeGapPcDesigned=0;
    INTN ScreenDimensionLessAnim=0;
    INTN GapNumTimesLarger=0;
    INTN GapNumFinal=0;
    INTN NewSuppliedGapPx=0;
    INTN NewOppositeGapPx=0;
    INTN ReturnValue=0;
    
    SuppliedGapDimensionPxDesigned = (ThemeDesignDimension * ValueToScale) / 100;
    OppositeGapDimensionPxDesigned = ThemeDesignDimension - (SuppliedGapDimensionPxDesigned + AnimDimension);
    OppositeGapPcDesigned = (OppositeGapDimensionPxDesigned * 100)/ThemeDesignDimension;
    ScreenDimensionLessAnim = (ScreenDimensionToFit - AnimDimension);
    if (ValueToScale > OppositeGapPcDesigned) {
      GapNumTimesLarger = (ValueToScale * 100)/OppositeGapPcDesigned;
      GapNumFinal = GapNumTimesLarger + 100;
      NewOppositeGapPx = (ScreenDimensionLessAnim * 100)/GapNumFinal;
      NewSuppliedGapPx = (NewOppositeGapPx * GapNumTimesLarger)/100;
    } else if (ValueToScale < OppositeGapPcDesigned) {
      GapNumTimesLarger = (OppositeGapPcDesigned * 100)/ValueToScale;
      GapNumFinal = (GapNumTimesLarger + 100);
      NewSuppliedGapPx = (ScreenDimensionLessAnim * 100)/GapNumFinal;
      NewOppositeGapPx = (NewSuppliedGapPx * GapNumTimesLarger)/100;
    } else if (ValueToScale == OppositeGapPcDesigned) {
      NewSuppliedGapPx = (ScreenDimensionLessAnim * 100)/200;
      NewOppositeGapPx = (NewSuppliedGapPx * 100)/100;
    }
    ReturnValue = (NewSuppliedGapPx * 100)/ScreenDimensionToFit;
    
    if (ReturnValue>0 && ReturnValue<100) {
      //DBG("Different screen size being used. Adjusted original anim gap to %d\n",ReturnValue);
      return ReturnValue;
    } else {
      DBG("Different screen size being used. Adjusted value %d invalid. Returning original value %d\n",ReturnValue, ValueToScale);
      return ValueToScale;
    }
}
*/

static INTN ConvertEdgeAndPercentageToPixelPosition(INTN Edge, INTN DesiredPercentageFromEdge, INTN ImageDimension, INTN ScreenDimension)
{
  if (Edge == SCREEN_EDGE_LEFT || Edge == SCREEN_EDGE_TOP) {
      return ((ScreenDimension * DesiredPercentageFromEdge) / 100);
  } else if (Edge == SCREEN_EDGE_RIGHT || Edge == SCREEN_EDGE_BOTTOM) {
      return (ScreenDimension - ((ScreenDimension * DesiredPercentageFromEdge) / 100) - ImageDimension);
  }
  return 0xFFFF; // to indicate that wrong edge was specified.
}

static INTN CalculateNudgePosition(INTN Position, INTN NudgeValue, INTN ImageDimension, INTN ScreenDimension)
{
  INTN value=Position;
  
  if ((NudgeValue != INITVALUE) && (NudgeValue != 0) && (NudgeValue >= -32) && (NudgeValue <= 32)) {
    if ((value + NudgeValue >=0) && (value + NudgeValue <= ScreenDimension - ImageDimension)) {
     value += NudgeValue;
    }
  }
  return value;
}

static BOOLEAN IsImageWithinScreenLimits(INTN Value, INTN ImageDimension, INTN ScreenDimension)
{
  return (Value >= 0 && Value + ImageDimension <= ScreenDimension);
}

static INTN RepositionFixedByCenter(INTN Value, INTN ScreenDimension, INTN DesignScreenDimension)
{
  return (Value + ((ScreenDimension - DesignScreenDimension) / 2));
}

static INTN RepositionRelativeByGapsOnEdges(INTN Value, INTN ImageDimension, INTN ScreenDimension, INTN DesignScreenDimension)
{
  return (Value * (ScreenDimension - ImageDimension) / (DesignScreenDimension - ImageDimension));
}

static INTN HybridRepositioning(INTN Edge, INTN Value, INTN ImageDimension, INTN ScreenDimension, INTN DesignScreenDimension)
{
  INTN pos, posThemeDesign;
  
  if (DesignScreenDimension == 0xFFFF || ScreenDimension == DesignScreenDimension) {
    // Calculate the horizontal pixel to place the top left corner of the animation - by screen resolution
    pos = ConvertEdgeAndPercentageToPixelPosition(Edge, Value, ImageDimension, ScreenDimension);
  } else {
    // Calculate the horizontal pixel to place the top left corner of the animation - by theme design resolution
    posThemeDesign = ConvertEdgeAndPercentageToPixelPosition(Edge, Value, ImageDimension, DesignScreenDimension);
    // Try repositioning by center first
    pos = RepositionFixedByCenter(posThemeDesign, ScreenDimension, DesignScreenDimension);
    // If out of edges, try repositioning by gaps on edges
    if (!IsImageWithinScreenLimits(pos, ImageDimension, ScreenDimension)) {
      pos = RepositionRelativeByGapsOnEdges(posThemeDesign, ImageDimension, ScreenDimension, DesignScreenDimension);
    }
  }
  return pos;
}


VOID UpdateAnime(REFIT_MENU_SCREEN *Screen, EG_RECT *Place)
{
  UINT64      Now;
  INTN   x, y, animPosX, animPosY;
  
  //INTN LayoutAnimMoveForMenuX = 0;
  INTN MenuWidth = 50;
  
  if (!Screen || !Screen->AnimeRun || !Screen->Film || GlobalConfig.TextOnly) return;
  if (!CompImage ||
      (CompImage->Width != Screen->Film[0]->Width) ||
      (CompImage->Height != Screen->Film[0]->Height)){
    if (CompImage) {
      egFreeImage(CompImage);
    }
    CompImage = egCreateImage(Screen->Film[0]->Width, Screen->Film[0]->Height, TRUE);
  }
  
  // Retained for legacy themes without new anim placement options.
  x = Place->XPos + (Place->Width - CompImage->Width) / 2;
  y = Place->YPos + (Place->Height - CompImage->Height) / 2;
  
  // Work with new anim placement options that are in theme.plist
  animPosX = Screen->FilmX;
  animPosY = Screen->FilmY;
  
  // Check a placement value has been specified
  if ((animPosX >=0 && animPosX <=100) && (animPosY >=0 && animPosY <=100)) {

    // Check if screen size being used is different from theme origination size.
    // If yes, then recalculate the animation placement % value.
    // This is necessary because screen can be a different size, but anim is not scaled.
    // TO DO - Can this be run only once per anim run and not for every frame?
    x = HybridRepositioning(Screen->ScreenEdgeHorizontal, animPosX, Screen->Film[0]->Width,  UGAWidth,  GlobalConfig.ThemeDesignWidth );
    y = HybridRepositioning(Screen->ScreenEdgeVertical,   animPosY, Screen->Film[0]->Height, UGAHeight, GlobalConfig.ThemeDesignHeight);
  }
  
  if (!IsImageWithinScreenLimits(x, Screen->Film[0]->Width, UGAWidth) || !IsImageWithinScreenLimits(y, Screen->Film[0]->Height, UGAHeight)) {
    // This anime can't be displayed
    return;
  }
  
  // Check if the theme.plist setting for allowing an anim to be moved horizontally in the quest 
  // to avoid overlapping the menu text on menu pages at lower resolutions is set.
  if ((Screen->ID > 1) && (LayoutAnimMoveForMenuX != 0)) { // these screens have text menus which the anim may interfere with.
    MenuWidth = TEXT_XMARGIN * 2 + (50 * GlobalConfig.CharWidth); // taken from menu.c
    if ((x + Screen->Film[0]->Width) > (UGAWidth - MenuWidth) >> 1) {
      if ((x + LayoutAnimMoveForMenuX >= 0) || (UGAWidth-(x + LayoutAnimMoveForMenuX + Screen->Film[0]->Width)) <= 100) {
        x += LayoutAnimMoveForMenuX;
      }
    }
  }
  
  // Does the user want to fine tune the placement?
  x = CalculateNudgePosition(x,Screen->NudgeX,Screen->Film[0]->Width,UGAWidth);
  y = CalculateNudgePosition(y,Screen->NudgeY,Screen->Film[0]->Height,UGAHeight);
  
  Now = AsmReadTsc();
  if (Screen->LastDraw == 0) {
    //first start, we should save background into last frame
    egFillImageArea(CompImage, 0, 0, CompImage->Width, CompImage->Height, &MenuBackgroundPixel);
    egTakeImage(Screen->Film[Screen->Frames],
                x, y,
                Screen->Film[Screen->Frames]->Width,
                Screen->Film[Screen->Frames]->Height);
  }
  if (TimeDiff(Screen->LastDraw, Now) < Screen->FrameTime) return;
  if (Screen->Film[Screen->CurrentFrame]) {
    egRawCopy(CompImage->PixelData, Screen->Film[Screen->Frames]->PixelData,
              Screen->Film[Screen->Frames]->Width, 
              Screen->Film[Screen->Frames]->Height,
              CompImage->Width,
              Screen->Film[Screen->Frames]->Width);
    egComposeImage(CompImage, Screen->Film[Screen->CurrentFrame], 0, 0);
    BltImage(CompImage, x, y);
  }
  Screen->CurrentFrame++;
  if (Screen->CurrentFrame >= Screen->Frames) {
    Screen->AnimeRun = !Screen->Once;
    Screen->CurrentFrame = 0;
  }
  Screen->LastDraw = Now;
}


VOID InitAnime(REFIT_MENU_SCREEN *Screen)
{
  CHAR16      FileName[256];
  CHAR16      *Path;
  INTN        i;
  EG_IMAGE    *p = NULL;
  EG_IMAGE    *Last = NULL;
  GUI_ANIME   *Anime;
  
  if (!Screen || GlobalConfig.TextOnly) return;
  
  // Check if anime was already loaded, and if it wasn't then load it
  if (Screen->AnimeRun == TRUE && Screen->Film == NULL) {
    for (Anime = GuiAnime; Anime != NULL && Anime->ID != Screen->ID; Anime = Anime->Next);
    // Look through contents of the directory
    if (Anime && (Path = Anime->Path) && (Screen->Film = (EG_IMAGE**)AllocateZeroPool(Anime->Frames * sizeof(VOID*)))) {
      for (i=0; i<Anime->Frames; i++) {
        UnicodeSPrint(FileName, 512, L"%s\\%s_%03d.png", Path, Path, i);
        //DBG("Try to load file %s\n", FileName);
        p = egLoadImage(ThemeDir, FileName, TRUE);
        if (!p) {
          p = Last;
          if (!p) break;
        } else {
          Last = p;
        }
        Screen->Film[i] = p;
      }
      if (Screen->Film[0] != NULL) {
        Screen->Frames = i;
        DBG(" found %d frames of the anime\n", i);
        // Create background frame
        Screen->Film[i] = egCreateImage(Screen->Film[0]->Width, Screen->Film[0]->Height, FALSE);
      } else {
        DBG("Film[0] == NULL\n");
      }
    }
  }
  
  if (Screen->Film == NULL || Screen->Film[0] == NULL) {
    Screen->AnimeRun = FALSE;
    return;
  }
  
  Screen->AnimeRun = TRUE;
  Screen->CurrentFrame = 0;
  Screen->LastDraw = 0;
}

BOOLEAN GetAnime(REFIT_MENU_SCREEN *Screen)
{
  GUI_ANIME   *Anime;
  
  if (!Screen) return FALSE;
  
  if (Screen->Film) {
    FreePool(Screen->Film);
    Screen->Film = NULL;
  }
  
  if (!GuiAnime) return FALSE;
  
  for (Anime = GuiAnime; Anime != NULL && Anime->ID != Screen->ID; Anime = Anime->Next);
  if (Anime == NULL) {
    return FALSE;
  }
  
  DBG("Use anime=%s frames=%d\n", Anime->Path, Anime->Frames);
  
  Screen->FrameTime = Anime->FrameTime;
  Screen->FilmX = Anime->FilmX;
  Screen->FilmY = Anime->FilmY;
  Screen->ScreenEdgeHorizontal = Anime->ScreenEdgeHorizontal;
  Screen->ScreenEdgeVertical = Anime->ScreenEdgeVertical;
  Screen->NudgeX = Anime->NudgeX;
  Screen->NudgeY = Anime->NudgeY;
  Screen->Once = Anime->Once;
  return TRUE;
}

//
// Sets next/previous available screen resolution, according to specified offset
//

VOID SetNextScreenMode(INT32 Next)
{
    EFI_STATUS Status;

    Status = egSetMode(Next);
    if (!EFI_ERROR(Status)) {
        UpdateConsoleVars();
    }
}

//
// Updates console variables, according to ConOut resolution 
// This should be called when initializing screen, or when resolution changes
//

static VOID UpdateConsoleVars()
{
    UINTN i;

    // get size of text console
    if  (gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &ConWidth, &ConHeight) != EFI_SUCCESS) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;
    }

    // free old BlankLine when it exists
    if (BlankLine != NULL) {
        FreePool(BlankLine);
    }

    // make a buffer for a whole text line
    BlankLine = AllocatePool((ConWidth + 1) * sizeof(CHAR16));
    for (i = 0; i < ConWidth; i++)
        BlankLine[i] = ' ';
    BlankLine[i] = 0;
}
