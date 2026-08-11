#pragma once

#include <QVariantMap>

/*! Bundled / drop-in mouse-integration presets keyed by CD fingerprint.

    Files are named mouse_XXXXXXXX.ini (lowercase hex) — not fp_XXXXXXXX.ini
    (that name is the disc profile).  Section [mouse_preset] holds only
    Mouse integration keys plus disc_fingerprint_hash.

    Lookup order:
      1. ~/.config/townsqt/mouse_presets/
      2. share/townsqt/mouse_presets/ (install / next to the binary)
      3. :/mouse_presets/ (embedded in the binary)

    Applying a preset fills the Mouse integration editor; Apply or OK
    writes it into the disc profile. */
namespace TownsQtMousePreset
{
bool Exists(unsigned int fingerprintHash32);
bool Load(unsigned int fingerprintHash32,QVariantMap &out);
/*! Write ~/.config/townsqt/mouse_presets/mouse_XXXXXXXX.ini (minimal [mouse_preset]). */
bool Save(unsigned int fingerprintHash32,const QVariantMap &profile,QString *errorOut=nullptr);
}
