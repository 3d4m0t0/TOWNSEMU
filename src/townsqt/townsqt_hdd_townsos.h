#pragma once

#include <QString>

/*! Create a 127MB sparse TownsOS HDD image: IPL + one max-size partition + empty FAT16.
    Geometry matches local formatted 127MB Towns HD images (2048-byte FS sectors). */
bool CreateTownsOsFormattedHdd127Mb(const QString &path);

constexpr int kTownsOsHddSizeMb=127;
constexpr int kTownsOsHd0DriveLetterIndex=3; // D:
constexpr int kTownsOsHd0ScsiUnit=0;         // HD0 = SCSI ID 0
