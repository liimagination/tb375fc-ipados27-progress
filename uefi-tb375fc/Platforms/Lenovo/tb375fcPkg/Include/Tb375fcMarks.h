#ifndef TB375FC_MARKS_H_
#define TB375FC_MARKS_H_

//
// TB375FC bring-up checkpoints, drawn as a permanent column of numbered bands.
//
// Every earlier attempt at this failed for a structural reason worth writing
// down, because it cost real debugging time:
//
//   attempt 1  three different checkpoints shared one colour -> unreadable.
//   attempt 2  a full-panel fill from the BDS checkpoint that runs AFTER the
//              application returned erased everything the application painted,
//              so the panel always showed that single colour.
//   attempt 3  several checkpoints shared one screen region, so only the last
//              one survived and the earlier ones were invisible.
//
// The fix: checkpoint N gets its OWN horizontal band, rows [N*92, N*92+92).
// Bands never overlap, so nothing can be overwritten, and the final screen is
// a permanent top-to-bottom record of how far the boot got.
//
// 20 bands * 92 rows = 1840 rows = the whole panel.
//
// To keep this readable without relying on colour (the panel has several dark
// shades that are hard to tell apart by eye), each band also carries a BAR
// COUNT equal to its checkpoint number: band 13 has 13 white bars. Counting is
// unambiguous for small numbers, and the topmost band is always checkpoint 0.
//
#define TB375FC_MARK_COUNT  20
#define TB375FC_MARK_ROWS   92
#define TB375FC_MARK_WIDTH  2944

#define TB375FC_FB_BASE     0xFD619000ULL
#define TB375FC_FB_PITCH    11776ULL

STATIC
VOID
Tb375fcMarkBand (
  IN UINTN   Step,
  IN UINT32  Colour
  )
{
  UINT8   *Panel;
  UINT32  *Row;
  UINTN   X;
  UINTN   Y;
  UINTN   Bar;

  if (Step >= TB375FC_MARK_COUNT) {
    return;
  }

  Panel = (UINT8 *)(UINTN)TB375FC_FB_BASE + Step * TB375FC_MARK_ROWS * TB375FC_FB_PITCH;

  //
  // Band fill.
  //
  for (Y = 0; Y < TB375FC_MARK_ROWS - 4; Y++) {
    Row = (UINT32 *)(Panel + Y * TB375FC_FB_PITCH);
    for (X = 0; X < TB375FC_MARK_WIDTH; X++) {
      Row[X] = Colour;
    }
  }

  //
  // White bars: one per checkpoint number, 60 px wide, 140 px apart, so they
  // fit even for checkpoint 19.
  //
  for (Bar = 0; Bar < Step; Bar++) {
    UINTN  Left = 20 + Bar * 150;

    for (Y = 8; Y < TB375FC_MARK_ROWS - 12; Y++) {
      Row = (UINT32 *)(Panel + Y * TB375FC_FB_PITCH);
      for (X = 0; X < 60; X++) {
        Row[Left + X] = 0xFFFFFFFF;
      }
    }
  }
}

//
// Checkpoint colours. Index == checkpoint number. The colour is a cross-check
// only; the bar count in the band is the authoritative reading.
//
//   0  white      BDS:    DeviceBootManagerBdsEntry entered
//   1  red        BDS:    application GUID resolved
//   2  green      BDS:    FV handle buffer located
//   3  blue       BDS:    UniXNU file found inside an FV
//   4  yellow     BDS:    LoadImage succeeded
//   5  pink       BDS:    StartImage returned
//   6  black      app:    UniXNURun entered
//   7  maroon     app:    UniXNUConnectAll done
//   8  olive      app:    config loaded
//   9  teal       app:    drivers and ACPI injected
//  10  navy       app:    UniXNUUiInit returned
//  11  purple     app:    UniXNUMenuRun returned
//  12  silver     UiInit: entered
//  13  orange     UiInit: entering the GOP lookup
//  14  salmon     UiInit: calling HandleProtocol on ConsoleOutHandle
//  15  cyan       UiInit: HandleProtocol returned
//  16  dgreen     UiInit: HandleProtocol failed, trying LocateProtocol
//  17  dgray      UiInit: LocateProtocol returned
//  18  lgreen     UiInit: protocol lookup complete
//  19  darkest    UiInit: finished
//
#endif /* TB375FC_MARKS_H_ */
