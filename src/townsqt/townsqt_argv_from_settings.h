#pragma once

class TownsARGV;

namespace TownsQtArgvFromSettings
{
void ApplyMachineFromSettings(TownsARGV &argv);
void ApplySessionSettings(TownsARGV &argv);
/*! Sync SCSI HDD slots from TownsQt settings (skips CD-ROM slots). */
void ApplyHardDiskFromSettings(TownsARGV &argv);
/*! Use ~/.config/townsqt/cmos.bin unless -CMOS or -DONTAUTOSAVECMOS was given. */
void ApplyDefaultCmosPath(TownsARGV &argv);
/*! Overlays all persisted TownsQt settings onto argv. */
void Apply(TownsARGV &argv);
}
