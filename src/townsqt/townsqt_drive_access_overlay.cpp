#include "townsqt_drive_access_overlay.h"

#include "icons.h"

#include <QPainter>

#include <algorithm>
#include <cstring>

namespace
{
void DrawIcon16(QPainter &painter,int x,int y,const unsigned char *idle,const unsigned char *busy,bool busy_lamp)
{
	const unsigned char *icon=true==busy_lamp ? busy : idle;
	QImage img(icon,16,16,16*4,QImage::Format_RGBA8888);
	if(true!=img.isNull())
	{
		painter.drawImage(x,y,img);
	}
}
}

int DriveAccessPresence::IconCount() const
{
	int count=0;
	if(cd)
	{
		++count;
	}
	for(bool enabled : fd)
	{
		if(enabled)
		{
			++count;
		}
	}
	for(bool enabled : hdd)
	{
		if(enabled)
		{
			++count;
		}
	}
	return count;
}

bool DriveAccessPresenceEqual(const DriveAccessPresence &a,const DriveAccessPresence &b)
{
	if(a.cd!=b.cd)
	{
		return false;
	}
	if(0!=std::memcmp(a.fd,b.fd,sizeof(a.fd)))
	{
		return false;
	}
	if(0!=std::memcmp(a.hdd,b.hdd,sizeof(a.hdd)))
	{
		return false;
	}
	return true;
}

bool DriveAccessStatusEqual(const Outside_World::StatusBarInfo &a,const Outside_World::StatusBarInfo &b)
{
	if(a.cdAccessLamp!=b.cdAccessLamp)
	{
		return false;
	}
	if(0!=std::memcmp(a.fdAccessLamp,b.fdAccessLamp,sizeof(a.fdAccessLamp)))
	{
		return false;
	}
	if(0!=std::memcmp(a.scsiAccessLamp,b.scsiAccessLamp,sizeof(a.scsiAccessLamp)))
	{
		return false;
	}
	return true;
}

int DriveAccessOverlayWidth(const DriveAccessPresence &presence)
{
	return presence.IconCount()*kDriveAccessIconSize;
}

int DriveAccessOverlayOriginX(int area_width,const DriveAccessPresence &presence)
{
	constexpr int kMargin=2;
	const int width=DriveAccessOverlayWidth(presence);
	if(width<=0)
	{
		return kMargin;
	}
	const int centered=(area_width-width)/2;
	return std::max(kMargin,centered);
}

void DrawDriveAccessOverlay(QPainter &painter,
                            int origin_x,
                            int origin_y,
                            const Outside_World::StatusBarInfo &info,
                            const DriveAccessPresence &presence)
{
	constexpr int kIcon=16;
	int slot=0;
	if(presence.cd)
	{
		DrawIcon16(painter,origin_x+slot*kIcon,origin_y,CD_IDLE,CD_BUSY,info.cdAccessLamp);
		++slot;
	}
	for(int fd=0; fd<2; ++fd)
	{
		if(!presence.fd[fd])
		{
			continue;
		}
		DrawIcon16(painter,origin_x+slot*kIcon,origin_y,FD_IDLE,FD_BUSY,info.fdAccessLamp[fd]);
		++slot;
	}
	for(int hdd=0; hdd<6; ++hdd)
	{
		if(!presence.hdd[hdd])
		{
			continue;
		}
		DrawIcon16(painter,origin_x+slot*kIcon,origin_y,HDD_IDLE,HDD_BUSY,info.scsiAccessLamp[hdd]);
		++slot;
	}
}
