#pragma once

class QActionGroup;
class QComboBox;
class QMenu;
class QString;

namespace TownsQtGamePortOptions
{
void PopulateCombo(QComboBox *combo,unsigned int select_emu);
void PopulateMenu(QMenu *menu,QActionGroup *group,unsigned int select_emu);
unsigned int ComboSelection(const QComboBox *combo,unsigned int fallback);
QString LabelForEmu(unsigned int emu);
}
