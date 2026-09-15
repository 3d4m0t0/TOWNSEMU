#pragma once

#include <QList>
#include <QString>

/*! Per-disc VM state saves keyed by CD fingerprint (state0_XXXXXXXX.TState = auto-resume). */
namespace TownsQtDiscStateSave
{
unsigned int FingerprintForDiscPath(const QString &cdPath);
/*! Auto-resume path: same file as manual slot 0 (state0_XXXXXXXX.TState). */
QString PathForFingerprint(unsigned int fingerprintHash32);
QString ProfilePathForFingerprint(unsigned int fingerprintHash32);
bool ProfileExistsForFingerprint(unsigned int fingerprintHash32);
bool ProfileExistsForDiscPath(const QString &cdPath);
/*! state0_XXXXXXXX.TState when fp_*.ini exists and the state-save file is present. */
QString StartupStateSavePathForDisc(const QString &cdPath);
/*! state0_XXXXXXXX.TState for the disc (auto-resume / slot 0), whether or not the file exists. */
QString ResumeStatePathForDisc(const QString &cdPath);
/*! Manual slots 0..9: <stateSaveDir>/stateN_XXXXXXXX.TState (per disc fingerprint). */
QString ManualStateSlotPath(int slot,unsigned int fingerprintHash32);
/*! Slots 0..9; slot 0 is also the auto-resume file. */
QString StateSlotPath(int slot,unsigned int fingerprintHash32);
/*! Matching PNG under statesave/image/ (may not exist on disk). */
QString StateSlotImagePath(int slot,unsigned int fingerprintHash32);
/*! Manual slots 0..9 that have a .TState file for this fingerprint. */
QList<int> ExistingStateSlots(unsigned int fingerprintHash32);
}
