#pragma once

#include <QString>

/*! Per-disc VM state saves keyed by CD fingerprint (state0_XXXXXXXX.TState). */
namespace TownsQtDiscStateSave
{
unsigned int FingerprintForDiscPath(const QString &cdPath);
QString PathForFingerprint(unsigned int fingerprintHash32);
QString ProfilePathForFingerprint(unsigned int fingerprintHash32);
bool ProfileExistsForFingerprint(unsigned int fingerprintHash32);
bool ProfileExistsForDiscPath(const QString &cdPath);
/*! state0_XXXXXXXX.TState when fp_*.ini exists and the state-save file is present. */
QString StartupStateSavePathForDisc(const QString &cdPath);
/*! state0_XXXXXXXX.TState for the disc (resume slot 0), whether or not the file exists. */
QString ResumeStatePathForDisc(const QString &cdPath);
/*! Manual slots 1..9: <stateSaveDir>/stateN.TState */
QString ManualStateSlotPath(int slot);
}
