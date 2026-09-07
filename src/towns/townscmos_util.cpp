#include "townscmos_util.h"

#include <cstring>

namespace TownsCmos
{

unsigned char Sum8(const unsigned char *cmos,unsigned int beginInclusive,unsigned int endInclusive)
{
	unsigned int sum=0;
	for(unsigned int i=beginInclusive; i<=endInclusive; ++i)
	{
		sum+=cmos[i];
	}
	return static_cast<unsigned char>(sum&0xFF);
}

void SetChecksumAdjustForTarget(unsigned char *cmos,unsigned char targetSum)
{
	const unsigned char body=
	    Sum8(cmos,kDriveAssignIndex,kChecksumAdjustIndex-1);
	cmos[kChecksumAdjustIndex]=
	    static_cast<unsigned char>((targetSum-body)&0xFF);
}

unsigned char CaptureIntegritySum(const unsigned char *cmos)
{
	return Sum8(cmos,kDriveAssignIndex,kChecksumAdjustIndex);
}

bool GetSingleDriveMode(const unsigned char *cmos)
{
	return 0!=cmos[kSingleDriveIndex];
}

void SetSingleDriveMode(unsigned char *cmos,bool enabled)
{
	const unsigned char target=CaptureIntegritySum(cmos);
	cmos[kSingleDriveIndex]=enabled ? 1 : 0;
	SetChecksumAdjustForTarget(cmos,target);
}

void GetDriveAssign(const unsigned char *cmos,DriveAssignEntry out[kDriveLetterCount])
{
	for(int i=0; i<kDriveLetterCount; ++i)
	{
		out[i].type=cmos[kDriveAssignIndex+static_cast<unsigned int>(i)*2];
		out[i].unit=cmos[kDriveAssignIndex+static_cast<unsigned int>(i)*2+1];
	}
}

void SetDriveAssign(unsigned char *cmos,const DriveAssignEntry in[kDriveLetterCount])
{
	const unsigned char target=CaptureIntegritySum(cmos);
	for(int i=0; i<kDriveLetterCount; ++i)
	{
		cmos[kDriveAssignIndex+static_cast<unsigned int>(i)*2]=in[i].type;
		cmos[kDriveAssignIndex+static_cast<unsigned int>(i)*2+1]=in[i].unit;
	}
	SetChecksumAdjustForTarget(cmos,target);
}

void ApplyDriveSettings(unsigned char *cmos,
                        bool singleDrive,
                        const DriveAssignEntry driveAssign[kDriveLetterCount])
{
	const unsigned char target=CaptureIntegritySum(cmos);
	cmos[kSingleDriveIndex]=singleDrive ? 1 : 0;
	for(int i=0; i<kDriveLetterCount; ++i)
	{
		cmos[kDriveAssignIndex+static_cast<unsigned int>(i)*2]=driveAssign[i].type;
		cmos[kDriveAssignIndex+static_cast<unsigned int>(i)*2+1]=driveAssign[i].unit;
	}
	SetChecksumAdjustForTarget(cmos,target);
}

}
