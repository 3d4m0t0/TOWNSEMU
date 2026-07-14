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

int DriveAccessOverlayOriginX(int area_width)
{
	constexpr int kMargin=2;
	const int centered=(area_width-kDriveAccessOverlayWidth)/2;
	return std::max(kMargin,centered);
}

void DrawDriveAccessOverlay(QPainter &painter,int origin_x,int origin_y,const Outside_World::StatusBarInfo &info)
{
	constexpr int kIcon=16;
	DrawIcon16(painter,origin_x+0*kIcon,origin_y,CD_IDLE,CD_BUSY,info.cdAccessLamp);
	for(int fd=0; fd<2; ++fd)
	{
		DrawIcon16(painter,origin_x+(1+fd)*kIcon,origin_y,FD_IDLE,FD_BUSY,info.fdAccessLamp[fd]);
	}
	for(int hdd=0; hdd<6; ++hdd)
	{
		DrawIcon16(painter,origin_x+(3+hdd)*kIcon,origin_y,HDD_IDLE,HDD_BUSY,info.scsiAccessLamp[hdd]);
	}
}
