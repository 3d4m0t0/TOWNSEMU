TownsQt mouse-integration presets
=================================

One INI file per CD fingerprint, named:

  mouse_XXXXXXXX.ini

XXXXXXXX is the lowercase 8-digit hex of disc_fingerprint_hash.
Disc profiles stay as ~/.config/townsqt/profiles/fp_XXXXXXXX.ini —
do not reuse that name here.

Section [mouse_preset] holds only Mouse integration fields plus the
hash.  [machine], cd_size, verified, bias, and other disc-profile
keys are ignored.

Lookup order:
  1. ~/.config/townsqt/mouse_presets/     (user drop-in)
  2. share/townsqt/mouse_presets/         (install)
  3. embedded :/mouse_presets/            (files in this directory)

The Settings → Mouse integration “Load preset” button is enabled when a
matching file exists. “Save to file” writes the current app-specific
editor values to ~/.config/townsqt/mouse_presets/mouse_XXXXXXXX.ini
(enabled when app-specific Phys is set). Apply or OK still writes the
disc profile.

See example.ini. Copy it to mouse_<fingerprint>.ini and fill phys /
mode / hash. Prefer pairN_ds_off_* (DS.base-relative) when available so
Phys tracks guest layout changes; keep pairN_x/y as absolute fallback.
