#include "townsqt_gameport_options.h"

#include "townsdef.h"
#include "ysgamepad.h"

#include <QActionGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QMenu>

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
QString Translated(const char *text)
{
	return QCoreApplication::translate("TownsQtGamePortOptions",text);
}

QString LabelFromId(const std::string &id)
{
	if("NONE"==id)
	{
		return Translated("None");
	}
	if("MOUSE"==id)
	{
		return Translated("Mouse");
	}
	if("KEY"==id)
	{
		return Translated("Keyboard");
	}
	if("CYBERSTICK"==id)
	{
		return Translated("CyberStick");
	}
	if("KEYMOUSE"==id)
	{
		return Translated("Mouse via keyboard");
	}
	if("NUMPADMOUSE"==id)
	{
		return Translated("Mouse via numeric keypad");
	}
	if("KEYCAPCOM"==id)
	{
		return Translated("Keyboard (CAPCOM CPSF)");
	}
	if("KEYPAD6"==id)
	{
		return Translated("Keyboard (6-button pad)");
	}
	if("KEYLIBBLE"==id)
	{
		return Translated("Keyboard (Libble Rabble)");
	}
	if("KEYMARTY"==id)
	{
		return Translated("Keyboard (Marty Pad)");
	}

	auto phys_label=[&](int index){
		return Translated("Gamepad #%1 (digital)").arg(index);
	};
	if(id.size()>=5 && 0==id.compare(0,4,"PHYS") && std::isdigit(static_cast<unsigned char>(id[4])))
	{
		const int index=id[4]-'0';
		if(5==id.size())
		{
			return phys_label(index);
		}
		if("MOUSE"==id.substr(5))
		{
			return Translated("Mouse via gamepad #%1").arg(index);
		}
		if("CYB"==id.substr(5))
		{
			return Translated("Gamepad #%1 (CyberStick)").arg(index);
		}
		if("CPSF"==id.substr(5))
		{
			return Translated("Gamepad #%1 (CAPCOM CPSF)").arg(index);
		}
		if("PAD6"==id.substr(5))
		{
			return Translated("Gamepad #%1 (6-button pad)").arg(index);
		}
		if("MARTY"==id.substr(5))
		{
			return Translated("Gamepad #%1 (Marty Pad)").arg(index);
		}
	}
	if(id.size()>=4 && 0==id.compare(0,3,"ANA") && std::isdigit(static_cast<unsigned char>(id[3])))
	{
		const int index=id[3]-'0';
		if(4==id.size())
		{
			return Translated("Gamepad #%1 (analog)").arg(index);
		}
		if("MOUSE"==id.substr(4))
		{
			return Translated("Mouse via analog #%1").arg(index);
		}
	}
	if(id.size()>=7 && 0==id.compare(0,6,"LIBBLE") && std::isdigit(static_cast<unsigned char>(id[6])))
	{
		return Translated("Gamepad #%1 (Libble Rabble)").arg(id[6]-'0');
	}

	return QString::fromStdString(id);
}

int RequiredGamePadIndex(unsigned int emu)
{
	const std::string id=TownsGamePortEmuToStr(emu);
	if(id.size()>=5 && 0==id.compare(0,4,"PHYS") && std::isdigit(static_cast<unsigned char>(id[4])))
	{
		return id[4]-'0';
	}
	if(id.size()>=4 && 0==id.compare(0,3,"ANA") && std::isdigit(static_cast<unsigned char>(id[3])))
	{
		return id[3]-'0';
	}
	if(id.size()>=7 && 0==id.compare(0,6,"LIBBLE") && std::isdigit(static_cast<unsigned char>(id[6])))
	{
		return id[6]-'0';
	}
	return -1;
}

bool IsEmuAvailable(unsigned int emu,int num_gamepads)
{
	const int index=RequiredGamePadIndex(emu);
	if(index<0)
	{
		return true;
	}
	return index<num_gamepads;
}

void AdjustComboMinimumWidth(QComboBox *combo)
{
	if(nullptr==combo)
	{
		return;
	}
	const QFontMetrics fm(combo->font());
	int max_w=combo->minimumSizeHint().width();
	for(int i=0; i<combo->count(); ++i)
	{
		max_w=std::max(max_w,fm.horizontalAdvance(combo->itemText(i))+48);
	}
	combo->setMinimumWidth(max_w);
}
}

namespace TownsQtGamePortOptions
{
QString LabelForEmu(unsigned int emu)
{
	if(TOWNS_GAMEPORTEMU_ERROR==emu || TOWNS_GAMEPORTEMU_NUM_DEVICES<=emu)
	{
		return Translated("None");
	}
	return LabelFromId(TownsGamePortEmuToStr(emu));
}

void PopulateCombo(QComboBox *combo,unsigned int select_emu)
{
	if(nullptr==combo)
	{
		return;
	}
	YsGamePadInitialize();
	const int num_gamepads=YsGamePadGetNumDevices();
	combo->clear();
	for(unsigned int emu=0; emu<TOWNS_GAMEPORTEMU_NUM_DEVICES; ++emu)
	{
		if(true!=IsEmuAvailable(emu,num_gamepads))
		{
			continue;
		}
		const QString id=QString::fromStdString(TownsGamePortEmuToStr(emu));
		combo->addItem(LabelForEmu(emu),id);
	}
	int idx=combo->findData(QString::fromStdString(TownsGamePortEmuToStr(select_emu)));
	if(idx<0)
	{
		idx=combo->findData(QString::fromLatin1("NONE"));
	}
	combo->setCurrentIndex(0<=idx ? idx : 0);
	AdjustComboMinimumWidth(combo);
}

void PopulateMenu(QMenu *menu,QActionGroup *group,unsigned int select_emu)
{
	if(nullptr==menu || nullptr==group)
	{
		return;
	}
	for(QAction *action : group->actions())
	{
		group->removeAction(action);
	}
	menu->clear();

	YsGamePadInitialize();
	const int num_gamepads=YsGamePadGetNumDevices();
	for(unsigned int emu=0; emu<TOWNS_GAMEPORTEMU_NUM_DEVICES; ++emu)
	{
		if(true!=IsEmuAvailable(emu,num_gamepads))
		{
			continue;
		}
		auto *action=menu->addAction(LabelForEmu(emu));
		action->setCheckable(true);
		action->setData(static_cast<unsigned int>(emu));
		action->setChecked(emu==select_emu);
		group->addAction(action);
	}
}

unsigned int ComboSelection(const QComboBox *combo,unsigned int fallback)
{
	if(nullptr==combo)
	{
		return fallback;
	}
	const QString id=combo->currentData().toString();
	if(id.isEmpty())
	{
		return fallback;
	}
	const unsigned int emu=TownsStrToGamePortEmu(id.toStdString());
	if(TOWNS_GAMEPORTEMU_ERROR==emu)
	{
		return fallback;
	}
	return emu;
}
}
