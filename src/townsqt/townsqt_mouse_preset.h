#pragma once

#include <QVariantMap>

/*! Bundled / drop-in mouse-integration presets keyed by CD fingerprint.

    Files are named mouse_XXXXXXXX.ini (lowercase hex) — not fp_XXXXXXXX.ini
    (that name is the disc profile).  Section [mouse_preset] holds only
    Mouse integration keys plus disc_fingerprint_hash.

    Sources:
      - User: ~/.config/townsqt/mouse_presets/ (Load from file / Save to file)
      - System: share/townsqt/mouse_presets/, next to the binary, and
        :/mouse_presets/ embedded in the binary (Load system preset)

    Applying a preset fills the Mouse integration editor; Apply or OK
    writes it into the disc profile. */
namespace TownsQtMousePreset
{
bool ExistsUser(unsigned int fingerprintHash32);
bool ExistsSystem(unsigned int fingerprintHash32);
/*! True if a user or system preset exists for this fingerprint. */
bool Exists(unsigned int fingerprintHash32);
bool LoadUser(unsigned int fingerprintHash32,QVariantMap &out);
bool LoadSystem(unsigned int fingerprintHash32,QVariantMap &out);
/*! Prefer user config, then system/bundled. */
bool Load(unsigned int fingerprintHash32,QVariantMap &out);
/*! Write ~/.config/townsqt/mouse_presets/mouse_XXXXXXXX.ini (minimal [mouse_preset]). */
bool Save(unsigned int fingerprintHash32,const QVariantMap &profile,QString *errorOut=nullptr);
}
