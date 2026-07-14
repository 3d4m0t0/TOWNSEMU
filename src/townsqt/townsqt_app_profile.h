#pragma once

#include <QString>

struct TownsQtAppProfileEntry
{
	unsigned int app_value;
	const char *id;
	const char *label;
	const char *description;
};

int TownsQtAppProfileCount();
const TownsQtAppProfileEntry &TownsQtAppProfileAt(int index);
int TownsQtAppProfileDefaultIndex();
int TownsQtAppProfileIndexForApp(unsigned int app_value);
unsigned int TownsQtAppProfileApp(int index);
QString TownsQtAppProfileDescription(int index);

QString TownsQtAppProfileId(int index);
int TownsQtAppProfileIndexForId(const QString &id);
