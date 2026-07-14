#pragma once

#include "outside_world.h"

class QPainter;

void DrawDriveAccessOverlay(QPainter &painter,int origin_x,int origin_y,const Outside_World::StatusBarInfo &info);

constexpr int kDriveAccessIconSize=16;
constexpr int kDriveAccessIconCount=9;
constexpr int kDriveAccessOverlayWidth=kDriveAccessIconCount*kDriveAccessIconSize;
constexpr int kDriveAccessOverlayHideMs=2000;

bool DriveAccessStatusEqual(const Outside_World::StatusBarInfo &a,const Outside_World::StatusBarInfo &b);
int DriveAccessOverlayOriginX(int area_width);
