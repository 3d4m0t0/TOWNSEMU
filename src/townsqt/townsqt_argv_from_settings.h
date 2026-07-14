#pragma once

class TownsARGV;

namespace TownsQtArgvFromSettings
{
void ApplyMachineFromSettings(TownsARGV &argv);
void ApplySessionSettings(TownsARGV &argv);
/*! Use ~/.config/townsqt/cmos.bin unless -CMOS or -DONTAUTOSAVECMOS was given. */
void ApplyDefaultCmosPath(TownsARGV &argv);
/*! Overlays all persisted TownsQt settings onto argv. */
void Apply(TownsARGV &argv);
}
