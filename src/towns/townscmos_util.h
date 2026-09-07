#ifndef TOWNSCMOS_UTIL_H_IS_INCLUDED
#define TOWNSCMOS_UTIL_H_IS_INCLUDED

#include "townsdef.h"

#include <cstddef>
#include <cstdint>

/*! Host-side CMOS field helpers (no guest SETUP).
    Edits preserve the 8-bit sum of CMOSRAM[DRIVE_ASSIGN .. CHECKSUM_ADJUST]
    by rewriting CHECKSUM_ADJUST — required so IO.SYS does not treat CMOS as
    corrupt after SINGLE_DRIVE / drive-letter changes. */
namespace TownsCmos
{
constexpr unsigned int IoToIndex(unsigned int ioPort)
{
	return (ioPort-TOWNSIO_CMOS_BASE)/2;
}

constexpr unsigned int kDriveAssignIndex=IoToIndex(TOWNSIO_CMOS_DRIVE_ASSIGN);     // 0x00EE
constexpr unsigned int kSingleDriveIndex=IoToIndex(TOWNSIO_CMOS_SINGLE_DRIVE_MODE); // 0x0146
constexpr unsigned int kChecksumAdjustIndex=IoToIndex(TOWNSIO_CMOS_CHECKSUM_ADJUST); // 0x01E7

/*! A–P (16 letters); bytes sit before the peripheral-timing block at ~0x0110. */
constexpr int kDriveLetterCount=16;

constexpr unsigned char kTypeFd=0;
constexpr unsigned char kTypeScsi=2;
constexpr unsigned char kTypeRom=5;
constexpr unsigned char kTypeUnassigned=0xFF;

struct DriveAssignEntry
{
	unsigned char type=kTypeUnassigned;
	unsigned char unit=kTypeUnassigned;
};

inline bool IsUnassigned(const DriveAssignEntry &e)
{
	return kTypeUnassigned==e.type && kTypeUnassigned==e.unit;
}

unsigned char Sum8(const unsigned char *cmos,unsigned int beginInclusive,unsigned int endInclusive);

/*! Recompute CHECKSUM_ADJUST so sum[driveAssign..checksumAdjust] == targetSum. */
void SetChecksumAdjustForTarget(unsigned char *cmos,unsigned char targetSum);

/*! Capture current integrity sum (call before mutating covered fields). */
unsigned char CaptureIntegritySum(const unsigned char *cmos);

bool GetSingleDriveMode(const unsigned char *cmos);
/*! Set single-drive flag and repair CHECKSUM_ADJUST (preserves prior integrity sum). */
void SetSingleDriveMode(unsigned char *cmos,bool enabled);

void GetDriveAssign(const unsigned char *cmos,DriveAssignEntry out[kDriveLetterCount]);
/*! Replace A–P assignments and repair CHECKSUM_ADJUST. */
void SetDriveAssign(unsigned char *cmos,const DriveAssignEntry in[kDriveLetterCount]);

/*! Apply both fields under one checksum repair (preferred atomic update). */
void ApplyDriveSettings(unsigned char *cmos,
                        bool singleDrive,
                        const DriveAssignEntry driveAssign[kDriveLetterCount]);
}

#endif
