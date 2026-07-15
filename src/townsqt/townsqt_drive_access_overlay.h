#pragma once

#include "outside_world.h"

class QPainter;

struct DriveAccessPresence
{
	bool cd=true;
	bool fd[2]={false,false};
	bool hdd[6]={false,false,false,false,false,false};

	int IconCount() const;
};

bool DriveAccessPresenceEqual(const DriveAccessPresence &a,const DriveAccessPresence &b);

void DrawDriveAccessOverlay(QPainter &painter,
                            int origin_x,
                            int origin_y,
                            const Outside_World::StatusBarInfo &info,
                            const DriveAccessPresence &presence);

constexpr int kDriveAccessIconSize=16;
constexpr int kDriveAccessIconCountMax=9;
constexpr int kDriveAccessOverlayHideMs=2000;

bool DriveAccessStatusEqual(const Outside_World::StatusBarInfo &a,const Outside_World::StatusBarInfo &b);
int DriveAccessOverlayWidth(const DriveAccessPresence &presence);
int DriveAccessOverlayOriginX(int area_width,const DriveAccessPresence &presence);
