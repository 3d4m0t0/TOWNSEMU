#include "settings_dialog.h"
#include "mouse_coord_profile_page.h"
#include "townsqt_cpu_profile.h"
#include "townsqt_gameport_options.h"
#include "townsqt_miniaudio_devices.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_rom_availability.h"
#include "townsqt_wayland_idle_inhibit.h"

#include <algorithm>
#include <initializer_list>

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include "townsparam.h"

#if defined(__linux__)
#include "linux/midi_backend_probe.h"
#include "linux/midi_fluidsynth_host.h"
#include "linux/midi_output_ports.h"
#endif

#include <algorithm>

namespace
{
void CompactVBox(QVBoxLayout *layout)
{
	layout->setContentsMargins(4,4,4,4);
	layout->setSpacing(3);
	layout->setAlignment(Qt::AlignTop);
}

void FinishTabPage(QVBoxLayout *layout,QWidget *footer_note=nullptr)
{
	layout->addStretch(1);
	if(nullptr!=footer_note)
	{
		layout->addWidget(footer_note);
	}
}

QLabel *MakeTabFooterNote(QWidget *parent,const QString &text)
{
	auto *note=new QLabel(text,parent);
	note->setWordWrap(true);
	return note;
}

QFrame *MakeHorizontalSeparator(QWidget *parent)
{
	auto *separator=new QFrame(parent);
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);
	return separator;
}

void ShrinkGroupBox(QGroupBox *box)
{
	if(nullptr==box)
	{
		return;
	}
	QSizePolicy policy=box->sizePolicy();
	policy.setVerticalPolicy(QSizePolicy::Maximum);
	box->setSizePolicy(policy);
}

void CompactGrid(QGridLayout *layout)
{
	layout->setContentsMargins(4,4,4,4);
	layout->setVerticalSpacing(4);
	layout->setHorizontalSpacing(8);
}

void CompactGroupBoxLayout(QLayout *layout)
{
	layout->setContentsMargins(4,2,4,2);
	layout->setSpacing(2);
}

QLabel *MakeIndentedNote(QWidget *parent,const QString &text)
{
	auto *label=new QLabel(text,parent);
	label->setWordWrap(true);
	label->setContentsMargins(22,0,0,6);
	return label;
}

QLabel *MakeSectionLabel(QWidget *parent,const QString &text)
{
	auto *label=new QLabel(text,parent);
	QFont font=label->font();
	font.setBold(true);
	label->setFont(font);
	return label;
}

void PopulateSoundBackendCombo(QComboBox *combo)
{
	if(nullptr==combo)
	{
		return;
	}
	combo->clear();
	combo->addItem(QCoreApplication::translate("SettingsDialog","Auto"),QString());
	combo->addItem(QStringLiteral("PulseAudio"),QStringLiteral("pulse"));
	combo->addItem(QStringLiteral("ALSA"),QStringLiteral("alsa"));
	combo->addItem(QStringLiteral("JACK"),QStringLiteral("jack"));
}

/*! Width for combo text, including JP/EN samples so locale switches do not clip. */
int ComboTextWidth(const QComboBox *combo,const QStringList &also_consider);

const QStringList &FidelityWidthSamples();

void FitComboToText(QComboBox *combo,const QStringList &also_consider=QStringList())
{
	if(nullptr==combo)
	{
		return;
	}
	combo->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
	combo->setFixedWidth(ComboTextWidth(combo,also_consider));
}

/*! Match CPU-fidelity label and combo to the wider of JP/EN label and combo samples. */
void FitFidelityLabelAndCombo(QLabel *label,QComboBox *combo)
{
	if(nullptr==combo)
	{
		return;
	}
	FitComboToText(combo,FidelityWidthSamples());
	int w=combo->width();
	if(nullptr!=label)
	{
		const QFontMetrics fm(label->font());
		w=std::max({
		    w,
		    fm.horizontalAdvance(label->text()),
		    fm.horizontalAdvance(QStringLiteral("CPU fidelity")),
		    fm.horizontalAdvance(QStringLiteral("CPUの再現性")),
		});
		label->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
		label->setFixedWidth(w);
	}
	combo->setFixedWidth(w);
}

int ComboTextWidth(const QComboBox *combo,const QStringList &also_consider)
{
	const QFontMetrics fm(combo->font());
	int w=0;
	for(int i=0; i<combo->count(); ++i)
	{
		w=std::max(w,fm.horizontalAdvance(combo->itemText(i)));
	}
	for(const QString &sample : also_consider)
	{
		w=std::max(w,fm.horizontalAdvance(sample));
	}
	const int arrow=combo->style()->pixelMetric(QStyle::PM_MenuButtonIndicator,nullptr,combo);
	const int frame=2*combo->style()->pixelMetric(QStyle::PM_DefaultFrameWidth,nullptr,combo);
	return w+arrow+frame+12;
}

void FitSpinToSample(QSpinBox *spin,const QString &sample)
{
	if(nullptr==spin)
	{
		return;
	}
	const QFontMetrics fm(spin->font());
	const int buttons=spin->style()->pixelMetric(QStyle::PM_SpinBoxSliderHeight,nullptr,spin);
	const int frame=2*spin->style()->pixelMetric(QStyle::PM_DefaultFrameWidth,nullptr,spin);
	// Button column is roughly the control height; use a stable pad for up/down.
	const int pad=std::max(28,buttons+8);
	// Numeric spin boxes: +1 digit cell so monospace values (e.g. "8 MB") do not clip.
	const int mono_pad=fm.horizontalAdvance(QLatin1Char('0'));
	spin->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
	spin->setFixedWidth(fm.horizontalAdvance(sample)+frame+pad+mono_pad);
}

void FitButtonToText(QPushButton *button,const QStringList &also_consider=QStringList())
{
	if(nullptr==button)
	{
		return;
	}
	const QFontMetrics fm(button->font());
	int w=fm.horizontalAdvance(button->text());
	for(const QString &sample : also_consider)
	{
		w=std::max(w,fm.horizontalAdvance(sample));
	}
	const int frame=2*button->style()->pixelMetric(QStyle::PM_DefaultFrameWidth,nullptr,button);
	button->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
	button->setFixedWidth(w+frame+16);
}

/*! Fixed combo width from samples only (long ALSA destination names must not stretch the row). */
void FitComboToSamples(QComboBox *combo,const QStringList &samples)
{
	if(nullptr==combo || samples.isEmpty())
	{
		return;
	}
	const QFontMetrics fm(combo->font());
	int w=0;
	for(const QString &sample : samples)
	{
		w=std::max(w,fm.horizontalAdvance(sample));
	}
	const int arrow=combo->style()->pixelMetric(QStyle::PM_MenuButtonIndicator,nullptr,combo);
	const int frame=2*combo->style()->pixelMetric(QStyle::PM_DefaultFrameWidth,nullptr,combo);
	combo->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
	combo->setFixedWidth(w+arrow+frame+12);
}

QStringList MidiOutputWidthSamples()
{
	return {
	    QStringLiteral("FluidSynth"),
	    QStringLiteral("ALSA"),
	    QStringLiteral("(none)"),
	};
}

QStringList MidiAlsaPortWidthSamples()
{
	return {
	    QStringLiteral("(select)"),
	    QStringLiteral("(選択)"),
	    QStringLiteral("128:0 TiMidity"),
	};
}

const QStringList &CpuKindWidthSamples()
{
	static const QStringList samples={
	    QStringLiteral("80386DX"),
	    QStringLiteral("80386SX"),
	    QStringLiteral("80486SX"),
	    QStringLiteral("80486DX"),
	    QStringLiteral("Pentium"),
	    QStringLiteral("Marty"),
	};
	return samples;
}

const QStringList &ModelWidthSamples()
{
	static const QStringList samples={
	    QStringLiteral("MODEL2"),
	    QStringLiteral("MARTY"),
	    QStringLiteral("20F"),
	};
	return samples;
}

const QStringList &FidelityWidthSamples()
{
	// EN "HIGH" is wider than JA "高" / "中".
	static const QStringList samples={
	    QStringLiteral("MID"),
	    QStringLiteral("HIGH"),
	    QStringLiteral("中"),
	    QStringLiteral("高"),
	};
	return samples;
}
}

SettingsDialog::Values SettingsDialog::defaultValues()
{
	Values v;
	v.cpuFrequencyMhz=33;
	v.cpuCustomFrequencyMhz=33;
	v.cpuFastMode=true;
	v.bootKeyComb=BOOT_KEYCOMB_CD;
	v.memSizeInMB=4;
	v.cpuHighFidelity=false;
	v.pretend386DX=false;
	v.useFPU=false;
	v.fastScsi=false;
	v.fastFd=false;
	v.midiBoard=false;
	v.singleDrive=false;
	v.highResCrtc=true;
	v.highResPcm=true;
	v.cpuKind=TownsQtCpuKindDefault();
	v.modelGroupIndex=TownsQtModelGroupDefaultIndex();
	v.displayScale=1;
	v.autoScaling=false;
	v.maintainAspect=true;
	v.damperWireLine=false;
	v.scanLineEffectIn15KHz=false;
	v.fullscreenVsync=true;
	v.windowedVsync=false;
	v.spriteTransferMode=0;
	v.pcmResampleHighQuality=false;
	v.fmVolumePercent=50;
	v.pcmVolumePercent=50;
	v.cddaVolumePercent=100;
	v.midiVolumePercent=100;
	v.pcmLpfEnabled=false;
	v.pcmLpfCutoffHz=8000;
	v.waylandIdleInhibit=false;
	v.gamePort0=TOWNS_GAMEPORTEMU_PHYSICAL0;
	v.gamePort1=TOWNS_GAMEPORTEMU_MOUSE;
	v.maxButtonHoldTimeMs0=0;
	v.maxButtonHoldTimeMs1=0;
	v.mouseIntegrationSpeed=256;
	v.considerVRAMOffsetInMouseIntegration=true;
	v.autoDifferentialOnMosUnused=false;
	v.snapMouseIntegration=false;
	v.snapMouseWarmupFrames=10;
	v.cddaCacheDuringDataRead=true;
	v.cddaCachePostReadGraceSec=3;
	v.mouseMinX=TownsStartParameters::DEFAULT_MOUSE_MINX;
	v.mouseMinY=TownsStartParameters::DEFAULT_MOUSE_MINY;
	v.mouseMaxX=TownsStartParameters::DEFAULT_MOUSE_MAXX;
	v.mouseMaxY=TownsStartParameters::DEFAULT_MOUSE_MAXY;
	v.appSpecificSetting=TOWNS_APPSPECIFIC_NONE;
	v.useDiscProfiles=true;
	v.autoResumeEnabled=true;
	v.discMounted=false;
	v.discProfileAvailable=false;
	v.discProfileCreateRequested=false;
	v.discProfileFileName.clear();
	v.profileCpuFrequencyMhz=33;
	v.profileCpuCustomFrequencyMhz=33;
	v.profileCpuFastMode=true;
	v.profileBootKeyComb=BOOT_KEYCOMB_CD;
	v.profileMemSizeInMB=4;
	v.profileGamePort0=TOWNS_GAMEPORTEMU_PHYSICAL0;
	v.profileGamePort1=TOWNS_GAMEPORTEMU_MOUSE;
	v.profileMaxButtonHoldTimeMs0=0;
	v.profileMaxButtonHoldTimeMs1=0;
	v.profileCpuHighFidelity=false;
	v.profilePretend386DX=false;
	v.profileUseFPU=false;
	v.profileFastScsi=false;
	v.profileFastFd=false;
	v.profileMidiBoard=false;
	v.profileSingleDrive=false;
	v.profileHasMouseIntegration=false;
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		v.hdd[slot]=Values::HddSlot{};
	}
	return v;
}

SettingsDialog::SettingsDialog(const Values &initial,const QString &romDir,QWidget *parent)
	: QDialog(parent),
	  values_(initial),
	  default_values_(defaultValues()),
	  rom_dir_(romDir.isEmpty() ? TownsQtPaths::romsDir() : romDir),
	  marty_ex_rom_present_(TownsQtRomAvailability::MartyExRomPresent(rom_dir_)),
	  sys_rom_profile_(TownsQtRomAvailability::ClassifySysRom(rom_dir_)),
	  sys_rom_level_(TownsQtRomAvailability::SysRomTownsOsLevel(rom_dir_))
{
	setWindowTitle(tr("Settings"));
	QFont dlg_font=font();
	if(0<dlg_font.pointSize())
	{
		dlg_font.setPointSize(std::max(8,dlg_font.pointSize()-2));
	}
	else
	{
		dlg_font.setPointSize(9);
	}
	setFont(dlg_font);
	if(nullptr!=QApplication::instance())
	{
		setPalette(QApplication::palette());
	}
	buildUi();
	{
		const QFont mono=QFontDatabase::systemFont(QFontDatabase::FixedFont);
		for(QSpinBox *spin : findChildren<QSpinBox*>())
		{
			spin->setFont(mono);
		}
	}
	updateCpuComboItems();
	updateModelComboItems();
	updateDisplayScaleRange();
	loadFromValues(values_);
	updateSysRomInfoLabel();
	updateMachineTabControls();
	setApplyEnabled(false);
	applyFixedDialogSize();
}

SettingsDialog::Values SettingsDialog::values() const
{
	Values out=values_;
	applyToValues(out);
	return out;
}

void SettingsDialog::setDiscProfileState(const Values &values)
{
	values_.discMounted=values.discMounted;
	values_.discProfileAvailable=values.discProfileAvailable;
	values_.discProfileFileName=values.discProfileFileName;
	values_.profileCpuFrequencyMhz=values.profileCpuFrequencyMhz;
	values_.profileCpuCustomFrequencyMhz=values.profileCpuCustomFrequencyMhz;
	values_.profileCpuFastMode=values.profileCpuFastMode;
	values_.profileBootKeyComb=values.profileBootKeyComb;
	values_.profileMemSizeInMB=values.profileMemSizeInMB;
	values_.profileGamePort0=values.profileGamePort0;
	values_.profileGamePort1=values.profileGamePort1;
	values_.profileMaxButtonHoldTimeMs0=values.profileMaxButtonHoldTimeMs0;
	values_.profileMaxButtonHoldTimeMs1=values.profileMaxButtonHoldTimeMs1;
	values_.profileCpuHighFidelity=values.profileCpuHighFidelity;
	values_.profilePretend386DX=values.profilePretend386DX;
	values_.profileUseFPU=values.profileUseFPU;
	values_.profileFastScsi=values.profileFastScsi;
	values_.profileFastFd=values.profileFastFd;
	values_.profileMidiBoard=values.profileMidiBoard;
	values_.profileSingleDrive=values.profileSingleDrive;
	values_.profileHasMouseIntegration=values.profileHasMouseIntegration;
	loading_=true;
	loadSharedMachineWidgets(values_,editingDiscProfile());
	loading_=false;
	updateProfileTabControls();
	updateMachineTabControls();
}

void SettingsDialog::setMouseCoordProfile(const QVariantMap &profile,bool mosActive)
{
	if(nullptr==mouse_coord_profile_page_)
	{
		return;
	}
	mouse_coord_profile_page_->setMouseBiosActive(mosActive);
	mouse_coord_profile_page_->setProfile(profile);
	updateProfileTabControls();
}

QVariantMap SettingsDialog::mouseCoordProfile(void) const
{
	if(nullptr==mouse_coord_profile_page_)
	{
		return QVariantMap();
	}
	return mouse_coord_profile_page_->profile();
}

void SettingsDialog::setGameCursorFromSelection(
    unsigned int physX,unsigned int physY,
    unsigned int minX,unsigned int maxX,
    unsigned int minY,unsigned int maxY,
    bool hasRangeX,bool hasRangeY,
    unsigned int dsOffX,unsigned int dsOffY,
    unsigned int dsSelector,bool hasDsOff)
{
	if(nullptr==mouse_coord_profile_page_)
	{
		return;
	}
	mouse_coord_profile_page_->setGameCursorFromSelection(
	    physX,physY,minX,maxX,minY,maxY,hasRangeX,hasRangeY,
	    dsOffX,dsOffY,dsSelector,hasDsOff);
}

void SettingsDialog::setGameCursor2FromSelection(
    unsigned int physX,unsigned int physY,
    unsigned int dsOffX,unsigned int dsOffY,
    unsigned int dsSelector,bool hasDsOff)
{
	if(nullptr==mouse_coord_profile_page_)
	{
		return;
	}
	mouse_coord_profile_page_->setGameCursor2FromSelection(
	    physX,physY,dsOffX,dsOffY,dsSelector,hasDsOff);
}

void SettingsDialog::setMouseBiosActive(bool active)
{
	if(nullptr==mouse_coord_profile_page_)
	{
		return;
	}
	mouse_coord_profile_page_->setMouseBiosActive(active);
}

void SettingsDialog::setLiveAppExec(const QString &name,unsigned int hash32)
{
	if(nullptr==mouse_coord_profile_page_)
	{
		return;
	}
	mouse_coord_profile_page_->setLiveAppExec(name,hash32);
}

void SettingsDialog::focusMouseIntegrationTab(void)
{
	if(nullptr==tabs_ || nullptr==mouse_integration_page_)
	{
		return;
	}
	tabs_->setCurrentWidget(mouse_integration_page_);
}

void SettingsDialog::focusBasicsTab(void)
{
	if(nullptr==tabs_ || nullptr==machine_page_)
	{
		return;
	}
	tabs_->setCurrentWidget(machine_page_);
}

void SettingsDialog::setDriveConfig(const DriveConfigPage::Values &values)
{
	default_drive_config_=values;
	if(nullptr!=drive_config_page_)
	{
		const bool was=loading_;
		loading_=true;
		drive_config_page_->setValues(values);
		loading_=was;
	}
}

DriveConfigPage::Values SettingsDialog::driveConfig(void) const
{
	if(nullptr!=drive_config_page_)
	{
		return drive_config_page_->values();
	}
	return default_drive_config_;
}

int SettingsDialog::mouseIntegrationTabIndex(void) const
{
	if(nullptr==tabs_ || nullptr==mouse_integration_page_)
	{
		return -1;
	}
	return tabs_->indexOf(mouse_integration_page_);
}

void SettingsDialog::applyFixedDialogSize()
{
	if(nullptr==tabs_)
	{
		return;
	}
	auto *main=qobject_cast<QVBoxLayout *>(layout());
	if(nullptr==main)
	{
		return;
	}

	QTabBar *bar=tabs_->tabBar();
	const QMargins margins=main->contentsMargins();
	// Size the dialog from page/button content with compact tabs first.
	bar->setUsesScrollButtons(false);
	bar->setExpanding(false);
	bar->setElideMode(Qt::ElideNone);
	bar->setStyleSheet(QString());

	// Width from minimumSizeHint avoids Expanding rows (game ports / audio device)
	// pulling the dialog far wider than the MIDI / machine controls.
	int max_width=0;
	int max_height=0;
	const int prev_tab=tabs_->currentIndex();
	for(int i=0; i<tabs_->count(); ++i)
	{
		tabs_->setCurrentIndex(i);
		if(QWidget *page=tabs_->widget(i))
		{
			if(QLayout *page_layout=page->layout())
			{
				page_layout->activate();
			}
		}
		max_width=std::max(max_width,minimumSizeHint().width());
		max_height=std::max(max_height,sizeHint().height());
	}
	tabs_->setCurrentIndex(0<=prev_tab ? prev_tab : 0);
	main->activate();

	// Include compact tab captions and bottom button row when deciding final fixed size.
	const int tab_width=bar->sizeHint().width()+margins.left()+margins.right()+12;
	int button_row_width=0;
	if(0<main->count())
	{
		if(QLayoutItem *item=main->itemAt(main->count()-1))
		{
			button_row_width=item->sizeHint().width()+margins.left()+margins.right();
		}
	}
	const int width=std::max({max_width,tab_width,button_row_width});
	setFixedSize(width,max_height);
	// Keep dialog width; stretch tabs evenly across the bar.
	bar->setExpanding(true);
}

void SettingsDialog::buildUi()
{
	auto *layout=new QVBoxLayout(this);
	layout->setContentsMargins(6,6,6,6);
	layout->setSpacing(4);

	tabs_=new QTabWidget(this);
	if(QTabBar *bar=tabs_->tabBar())
	{
		bar->setExpanding(true);
		bar->setUsesScrollButtons(false);
		bar->setElideMode(Qt::ElideNone);
	}
	layout->addWidget(tabs_);

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		machine_global_bar_=new QWidget(page);
		auto *machine_top=new QHBoxLayout(machine_global_bar_);
		machine_top->setContentsMargins(0,0,0,0);
		machine_top->setSpacing(6);

		auto *machine_left=new QVBoxLayout();
		machine_left->setContentsMargins(0,0,0,0);
		machine_left->setSpacing(4);

		auto *bios_row=new QHBoxLayout();
		bios_row->setContentsMargins(0,0,0,0);
		bios_row->setSpacing(6);
		bios_row->addWidget(new QLabel(tr("BIOS"),machine_global_bar_));
		sys_rom_info_label_=new QLabel(machine_global_bar_);
		sys_rom_info_label_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
		sys_rom_info_label_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
		QFont info_font=sys_rom_info_label_->font();
		if(0<info_font.pointSize())
		{
			info_font.setPointSize(std::max(7,info_font.pointSize()-1));
		}
		sys_rom_info_label_->setFont(info_font);
		sys_rom_info_label_->setForegroundRole(QPalette::PlaceholderText);
		bios_row->addWidget(sys_rom_info_label_,0,Qt::AlignLeft|Qt::AlignVCenter);
		bios_row->addStretch(1);
		machine_left->addLayout(bios_row);

		auto *machine_row=new QHBoxLayout();
		machine_row->setContentsMargins(0,0,0,0);
		machine_row->setSpacing(6);
		machine_row->addWidget(new QLabel(tr("CPU"),machine_global_bar_));
		cpu_kind_=new QComboBox(machine_global_bar_);
		FitComboToText(cpu_kind_,CpuKindWidthSamples());
		machine_row->addWidget(cpu_kind_);
		connect(cpu_kind_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			updateModelComboItems();
			updateMachineTabControls();
			markDirty();
		});

		machine_row->addWidget(new QLabel(tr("Model"),machine_global_bar_));
		model_group_=new QComboBox(machine_global_bar_);
		FitComboToText(model_group_,ModelWidthSamples());
		machine_row->addWidget(model_group_);
		connect(model_group_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			updateMachineTabControls();
			markDirty();
		});
		machine_left->addLayout(machine_row);
		machine_top->addLayout(machine_left);

		machine_top->addStretch(1);

		auto *cap_grid=new QGridLayout();
		cap_grid->setContentsMargins(0,0,0,0);
		cap_grid->setHorizontalSpacing(6);
		cap_grid->setVerticalSpacing(4);
		auto *high_res_cap_label=new QLabel(tr("High-res / high-res PCM"),machine_global_bar_);
		auto *ug_io_cap_label=new QLabel(tr("UG SCSI / peripheral I/O"),machine_global_bar_);
		high_res_status_label_=new QLabel(tr("Disabled"),machine_global_bar_);
		ug_io_status_label_=new QLabel(tr("Disabled"),machine_global_bar_);
		cap_grid->addWidget(high_res_cap_label,0,0,Qt::AlignRight|Qt::AlignVCenter);
		cap_grid->addWidget(high_res_status_label_,0,1,Qt::AlignLeft|Qt::AlignVCenter);
		cap_grid->addWidget(ug_io_cap_label,1,0,Qt::AlignRight|Qt::AlignVCenter);
		cap_grid->addWidget(ug_io_status_label_,1,1,Qt::AlignLeft|Qt::AlignVCenter);
		machine_top->addLayout(cap_grid);

		machine_top->addStretch(1);
		v->addWidget(machine_global_bar_);
		v->addWidget(MakeHorizontalSeparator(page));

		disc_profile_bar_=new QWidget(page);
		auto *prof_bar=new QVBoxLayout(disc_profile_bar_);
		prof_bar->setContentsMargins(0,0,0,0);
		prof_bar->setSpacing(4);
		auto *prof_row=new QHBoxLayout();
		prof_row->setContentsMargins(0,0,0,0);
		prof_row->setSpacing(8);
		disc_profile_status_label_=new QLabel(disc_profile_bar_);
		disc_profile_status_label_->setWordWrap(true);
		disc_profile_status_label_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		prof_row->addWidget(disc_profile_status_label_,1);
		disc_profile_action_btn_=new QPushButton(tr("Create profile"),disc_profile_bar_);
		connect(disc_profile_action_btn_,&QPushButton::clicked,this,[this](){
			if(true==values_.discProfileAvailable)
			{
				const QString name=values_.discProfileFileName.isEmpty() ?
				    tr("(unnamed)") : values_.discProfileFileName;
				const auto reply=QMessageBox::question(
				    this,
				    tr("Delete disc profile"),
				    tr("Delete the disc profile \"%1\"?\n"
				       "Per-disc machine and mouse settings will be removed.\n"
				       "This cannot be undone.").arg(name),
				    QMessageBox::Yes|QMessageBox::No,
				    QMessageBox::No);
				if(QMessageBox::Yes!=reply)
				{
					return;
				}
				Q_EMIT deleteDiscProfileRequested();
				return;
			}
			values_.discProfileCreateRequested=true;
			Q_EMIT createDiscProfileRequested();
			markDirty();
		});
		prof_row->addWidget(disc_profile_action_btn_,0,Qt::AlignTop);
		prof_bar->addLayout(prof_row);
		auto *file_row=new QHBoxLayout();
		file_row->setContentsMargins(0,0,0,0);
		file_row->setSpacing(8);
		file_row->addWidget(new QLabel(tr("Profile file"),disc_profile_bar_));
		disc_profile_file_label_=new QLabel(disc_profile_bar_);
		disc_profile_file_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
		disc_profile_file_label_->setWordWrap(true);
		disc_profile_file_label_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		file_row->addWidget(disc_profile_file_label_,1);
		prof_bar->addLayout(file_row);
		v->addWidget(disc_profile_bar_);

		profile_fields_box_=new QWidget(page);
		profile_fields_box_->setObjectName(QStringLiteral("profileFieldsBox"));
		auto *profile_v=new QVBoxLayout(profile_fields_box_);
		profile_v->setContentsMargins(4,4,4,4);
		profile_v->setSpacing(4);

		auto *fast_row_host=new QWidget(profile_fields_box_);
		auto *fast_row=new QHBoxLayout(fast_row_host);
		fast_row->setContentsMargins(0,0,0,0);
		fast_row->setSpacing(8);
		cpu_freq_group_=new QButtonGroup(fast_row_host);
		cpu_freq_group_->setExclusive(true);
		cpu_freq_preset_radio_=new QRadioButton(fast_row_host);
		cpu_freq_preset_=new QComboBox(fast_row_host);
		// Compatible = slow/CMOS mode; 16/20/25 = FAST presets.
		cpu_freq_preset_->addItem(tr("Compatible"),-1);
		cpu_freq_preset_->addItem(QStringLiteral("16MHz"),16);
		cpu_freq_preset_->addItem(QStringLiteral("20MHz"),20);
		cpu_freq_preset_->addItem(QStringLiteral("25MHz"),25);
		FitComboToText(cpu_freq_preset_,QStringList{
		    tr("Compatible"),QStringLiteral("25MHz")});
		cpu_freq_custom_=new QRadioButton(fast_row_host);
		cpu_freq_group_->addButton(cpu_freq_preset_radio_,1);
		cpu_freq_group_->addButton(cpu_freq_custom_,0);
		cpu_freq_custom_mhz_=new QSpinBox(fast_row_host);
		cpu_freq_custom_mhz_->setRange(33,60);
		cpu_freq_custom_mhz_->setSuffix(tr(" MHz"));
		FitSpinToSample(cpu_freq_custom_mhz_,QStringLiteral("60")+tr(" MHz"));
		fast_row->addWidget(cpu_freq_preset_radio_);
		fast_row->addWidget(cpu_freq_preset_);
		fast_row->addWidget(cpu_freq_custom_);
		fast_row->addWidget(cpu_freq_custom_mhz_);
		boot_drive_label_=new QLabel(tr("Boot drive"),fast_row_host);
		boot_drive_=new QComboBox(fast_row_host);
		// Tsugaru boot-key strings: CD / F0 / F1 / H0 (BOOT_KEYCOMB_*).
		boot_drive_->addItem(tr("CD-ROM"),static_cast<int>(BOOT_KEYCOMB_CD));
		boot_drive_->addItem(QStringLiteral("FD0"),static_cast<int>(BOOT_KEYCOMB_F0));
		boot_drive_->addItem(QStringLiteral("FD1"),static_cast<int>(BOOT_KEYCOMB_F1));
		boot_drive_->addItem(QStringLiteral("HDD0"),static_cast<int>(BOOT_KEYCOMB_H0));
		FitComboToText(boot_drive_,QStringList{
		    tr("CD-ROM"),QStringLiteral("FD0"),QStringLiteral("FD1"),QStringLiteral("HDD0")});
		fast_row->addSpacing(12);
		fast_row->addWidget(boot_drive_label_);
		fast_row->addWidget(boot_drive_);
		fast_row->addStretch(1);
		connect(cpu_freq_group_,&QButtonGroup::idToggled,this,[this](int id,bool checked){
			if(!checked || loading_)
			{
				return;
			}
			if(0==id)
			{
				values_.cpuFrequencyMhz=selectedCpuCustomFrequencyMhz();
				values_.cpuCustomFrequencyMhz=values_.cpuFrequencyMhz;
			}
			else if(true==selectedCpuFastMode())
			{
				values_.cpuFrequencyMhz=selectedCpuFrequencyMhz();
			}
			updateFastModeControls();
			markDirty();
		});
		connect(cpu_freq_preset_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			if(loading_)
			{
				return;
			}
			if(nullptr!=cpu_freq_preset_radio_ && true!=cpu_freq_preset_radio_->isChecked())
			{
				QSignalBlocker blocker(cpu_freq_preset_radio_);
				cpu_freq_preset_radio_->setChecked(true);
			}
			if(true==selectedCpuFastMode())
			{
				values_.cpuFrequencyMhz=selectedCpuFrequencyMhz();
			}
			updateFastModeControls();
			markDirty();
		});
		connect(cpu_freq_custom_mhz_,qOverload<int>(&QSpinBox::valueChanged),this,[this](int){
			if(loading_)
			{
				return;
			}
			if(nullptr!=cpu_freq_custom_ && true!=cpu_freq_custom_->isChecked())
			{
				QSignalBlocker blocker(cpu_freq_custom_);
				cpu_freq_custom_->setChecked(true);
			}
			values_.cpuFrequencyMhz=selectedCpuCustomFrequencyMhz();
			values_.cpuCustomFrequencyMhz=values_.cpuFrequencyMhz;
			updateFastModeControls();
			markDirty();
		});
		connect(boot_drive_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			if(loading_)
			{
				return;
			}
			markDirty();
		});
		profile_v->addWidget(fast_row_host);

		opt_grid_widget_=new QWidget(profile_fields_box_);
		auto *opt_outer=new QVBoxLayout(opt_grid_widget_);
		CompactVBox(opt_outer);
		opt_outer->setContentsMargins(0,0,0,0);

		auto *opt_top=new QHBoxLayout();
		opt_top->setContentsMargins(0,0,0,0);
		opt_top->setSpacing(6);

		auto *checks=new QGridLayout();
		CompactGrid(checks);
		checks->setContentsMargins(0,0,0,0);
		checks->setColumnStretch(0,0);
		checks->setColumnStretch(1,0);
		checks->setColumnStretch(2,0);
		pretend_386_=new QCheckBox(tr("Pretend 386DX"),opt_grid_widget_);
		use_fpu_=new QCheckBox(tr("80386FPU"),opt_grid_widget_);
		fast_scsi_=new QCheckBox(tr("Fast SCSI"),opt_grid_widget_);
		fast_fd_=new QCheckBox(tr("Fast FD"),opt_grid_widget_);
		midi_board_=new QCheckBox(tr("MIDI board"),opt_grid_widget_);
		connect(midi_board_,&QCheckBox::toggled,this,&SettingsDialog::updateAudioTabMidiSection);
		checks->addWidget(pretend_386_,0,0,Qt::AlignLeft|Qt::AlignVCenter);
		checks->addWidget(use_fpu_,0,1,Qt::AlignLeft|Qt::AlignVCenter);
		checks->addWidget(fast_scsi_,1,0,Qt::AlignLeft|Qt::AlignVCenter);
		checks->addWidget(fast_fd_,1,1,Qt::AlignLeft|Qt::AlignVCenter);
		checks->addWidget(midi_board_,1,2,Qt::AlignLeft|Qt::AlignVCenter);
		opt_top->addLayout(checks,1);

		auto *opt_sep=new QFrame(opt_grid_widget_);
		opt_sep->setFrameShape(QFrame::VLine);
		opt_sep->setFrameShadow(QFrame::Sunken);
		opt_top->addWidget(opt_sep);

		auto *mem_fidelity=new QGridLayout();
		mem_fidelity->setContentsMargins(0,0,0,0);
		mem_fidelity->setHorizontalSpacing(6);
		mem_fidelity->setVerticalSpacing(checks->verticalSpacing());
		mem_label_=new QLabel(tr("Memory"),opt_grid_widget_);
		mem_size_mb_=new QSpinBox(opt_grid_widget_);
		mem_size_mb_->setRange(1,64);
		mem_size_mb_->setSuffix(tr(" MB"));
		FitSpinToSample(mem_size_mb_,QStringLiteral("64")+tr(" MB"));
		auto *fidelity_label=new QLabel(tr("CPU fidelity"),opt_grid_widget_);
		cpu_fidelity_=new QComboBox(opt_grid_widget_);
		cpu_fidelity_->addItem(tr("MID"),0);
		cpu_fidelity_->addItem(tr("HIGH"),1);
		FitFidelityLabelAndCombo(fidelity_label,cpu_fidelity_);
		mem_fidelity->addWidget(fidelity_label,0,0,Qt::AlignRight|Qt::AlignVCenter);
		mem_fidelity->addWidget(cpu_fidelity_,0,1,Qt::AlignLeft|Qt::AlignVCenter);
		mem_fidelity->addWidget(mem_label_,1,0,Qt::AlignRight|Qt::AlignVCenter);
		mem_fidelity->addWidget(mem_size_mb_,1,1,Qt::AlignLeft|Qt::AlignVCenter);
		opt_top->addLayout(mem_fidelity);
		opt_outer->addLayout(opt_top);
		profile_v->addWidget(opt_grid_widget_);

		profile_v->addWidget(MakeHorizontalSeparator(profile_fields_box_));

		auto *joystick_grid=new QGridLayout();
		joystick_grid->setHorizontalSpacing(16);
		joystick_grid->setVerticalSpacing(4);
		joystick_grid->setColumnStretch(0,1);
		joystick_grid->setColumnStretch(1,0);
		joystick_grid->addWidget(new QLabel(tr("Game pad / mouse ports"),profile_fields_box_),0,0);
		joystick_grid->addWidget(new QLabel(tr("Max button hold time"),profile_fields_box_),0,1);

		gameport0_=new QComboBox(profile_fields_box_);
		gameport1_=new QComboBox(profile_fields_box_);
		gameport0_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		gameport1_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		TownsQtGamePortOptions::PopulateCombo(gameport0_,TOWNS_GAMEPORTEMU_PHYSICAL0);
		TownsQtGamePortOptions::PopulateCombo(gameport1_,TOWNS_GAMEPORTEMU_MOUSE);

		max_button_hold0_=new QSpinBox(profile_fields_box_);
		max_button_hold1_=new QSpinBox(profile_fields_box_);
		for(QSpinBox *spin : {max_button_hold0_,max_button_hold1_})
		{
			spin->setRange(0,9999);
			spin->setSuffix(tr(" ms"));
			spin->setSpecialValueText(tr("Any"));
		}

		auto *port0_row=new QHBoxLayout();
		port0_row->setSpacing(8);
		port0_row->addWidget(new QLabel(QStringLiteral("0"),profile_fields_box_));
		port0_row->addWidget(gameport0_,1);
		joystick_grid->addLayout(port0_row,1,0);

		auto *hold0_row=new QHBoxLayout();
		hold0_row->setSpacing(8);
		hold0_row->addWidget(new QLabel(tr("Button 0"),profile_fields_box_));
		hold0_row->addWidget(max_button_hold0_);
		joystick_grid->addLayout(hold0_row,1,1);

		auto *port1_row=new QHBoxLayout();
		port1_row->setSpacing(8);
		port1_row->addWidget(new QLabel(QStringLiteral("1"),profile_fields_box_));
		port1_row->addWidget(gameport1_,1);
		joystick_grid->addLayout(port1_row,2,0);

		auto *hold1_row=new QHBoxLayout();
		hold1_row->setSpacing(8);
		hold1_row->addWidget(new QLabel(tr("Button 1"),profile_fields_box_));
		hold1_row->addWidget(max_button_hold1_);
		joystick_grid->addLayout(hold1_row,2,1);

		profile_v->addLayout(joystick_grid);
		v->addWidget(profile_fields_box_);

		auto *basics_footer=MakeTabFooterNote(
		    page,
		    tr("Editing Basics defaults (townsqt.conf). Apply or OK saves here.\n"
		       "Memory, boot drive, and CPU fidelity changes restart the emulator; game-port changes apply immediately.\n"
		       "Create a disc profile when a CD is mounted to save per-disc settings\n"
		       "(FD0 / FD1 / HD0–HD6 mounts, and CMOS under cmos/cmos_XXXXXXXX.bin)."));
		basics_footer->setMaximumWidth(640);
		basics_footer_label_=basics_footer;
		FinishTabPage(v,basics_footer);
		machine_page_=page;
		tabs_->addTab(page,tr("Basics"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		v->setContentsMargins(4,4,4,4);
		v->setSpacing(3);
		drive_config_page_=new DriveConfigPage(page);
		v->addWidget(drive_config_page_,1);
		v->addWidget(MakeTabFooterNote(
		    page,
		    tr("Edits the active CMOS (global cmos.bin or profile cmos/cmos_XXXXXXXX.bin).\n"
		       "Apply or OK updates VM CMOS RAM and the file. Towns OS usually needs reset/boot.")));
		drive_config_page_host_=page;
		tabs_->addTab(page,tr("Drive configuration"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		mouse_coord_profile_page_=new MouseCoordProfilePage(page);
		connect(mouse_coord_profile_page_,&MouseCoordProfilePage::contentChanged,
		        this,&SettingsDialog::markDirty);
		connect(mouse_coord_profile_page_,&MouseCoordProfilePage::bindCurrentAppExecRequested,
		        this,&SettingsDialog::bindCurrentAppExecRequested);
		connect(mouse_coord_profile_page_,&MouseCoordProfilePage::openMemoryScanRequested,
		        this,&SettingsDialog::openMemoryScanRequested);
		v->addWidget(mouse_coord_profile_page_,1);

		FinishTabPage(v);
		mouse_integration_page_=page;
		tabs_->addTab(page,tr("Mouse integration"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		v->addWidget(MakeSectionLabel(page,tr("Video")));

		display_scale_=new QSpinBox(page);
		display_scale_->setRange(1,8);
		display_scale_->setSuffix(tr("x"));
		sprite_group_=new QButtonGroup(page);
		auto *sprite_standard=new QRadioButton(tr("Normal"),page);
		auto *sprite_double=new QRadioButton(tr("Double"),page);
		auto *sprite_max=new QRadioButton(tr("Max"),page);
		sprite_group_->addButton(sprite_standard,0);
		sprite_group_->addButton(sprite_double,1);
		sprite_group_->addButton(sprite_max,2);

		auto *scale_lbl=new QLabel(tr("Window scale:"),page);
		auto *sprite_lbl=new QLabel(tr("Sprite transfer speed:"),page);
		{
			const QFontMetrics fm(scale_lbl->font());
			const int titleW=std::max({
			    fm.horizontalAdvance(tr("Window scale:")),
			    fm.horizontalAdvance(tr("Sprite transfer speed:")),
			    fm.horizontalAdvance(QStringLiteral("Window scale:")),
			    fm.horizontalAdvance(QStringLiteral("Sprite transfer speed:")),
			    fm.horizontalAdvance(QStringLiteral("ウインドウ倍率：")),
			    fm.horizontalAdvance(QStringLiteral("スプライト転送速度：")),
			});
			scale_lbl->setFixedWidth(titleW);
			sprite_lbl->setFixedWidth(titleW);
			scale_lbl->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
			sprite_lbl->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		}

		auto *scale_row=new QHBoxLayout();
		scale_row->setSpacing(8);
		scale_row->addWidget(scale_lbl);
		scale_row->addWidget(display_scale_);
		scale_row->addStretch();
		v->addLayout(scale_row);

		auto *sprite_row=new QHBoxLayout();
		sprite_row->setSpacing(8);
		sprite_row->addWidget(sprite_lbl);
		sprite_row->addWidget(sprite_standard);
		sprite_row->addWidget(sprite_double);
		sprite_row->addWidget(sprite_max);
		sprite_row->addStretch();
		v->addLayout(sprite_row);

		auto *video_grid=new QGridLayout();
		CompactGrid(video_grid);
		scanline_15k_=new QCheckBox(tr("15 kHz scan-line effect"),page);
		damper_wire_=new QCheckBox(tr("Damper-wire line"),page);
		fullscreen_vsync_=new QCheckBox(tr("VSync in fullscreen"),page);
		windowed_vsync_=new QCheckBox(tr("VSync in windowed mode"),page);
		video_grid->addWidget(scanline_15k_,0,0);
		video_grid->addWidget(damper_wire_,0,1);
		video_grid->addWidget(fullscreen_vsync_,1,0);
		video_grid->addWidget(windowed_vsync_,1,1);
		video_grid->setColumnStretch(0,1);
		video_grid->setColumnStretch(1,1);
		v->addLayout(video_grid);

		v->addWidget(MakeHorizontalSeparator(page));

		v->addWidget(MakeSectionLabel(page,tr("Audio")));

		auto *pcm_grid=new QGridLayout();
		CompactGrid(pcm_grid);
		pcm_resample_sinc_=new QCheckBox(tr("Use sinc interpolation for PCM resampling"),page);
		pcm_lpf_enabled_=new QCheckBox(tr("PCM LPF"),page);
		pcm_lpf_cutoff_=new QSpinBox(page);
		pcm_lpf_cutoff_->setRange(200,20000);
		pcm_lpf_cutoff_->setSingleStep(100);
		pcm_lpf_cutoff_->setSuffix(tr(" Hz"));
		pcm_lpf_cutoff_->setFixedWidth(96);
		auto *lpf_row=new QHBoxLayout();
		lpf_row->setSpacing(8);
		lpf_row->addWidget(pcm_lpf_enabled_);
		lpf_row->addWidget(pcm_lpf_cutoff_);
		lpf_row->addStretch();
		pcm_grid->addWidget(pcm_resample_sinc_,0,0);
		pcm_grid->addLayout(lpf_row,0,1);
		pcm_grid->addWidget(
		    MakeIndentedNote(page,tr("Without sinc interpolation, linear interpolation is used.")),
		    1,0,1,2);
		pcm_grid->setColumnStretch(0,1);
		pcm_grid->setColumnStretch(1,1);
		connect(pcm_lpf_enabled_,&QCheckBox::toggled,pcm_lpf_cutoff_,&QWidget::setEnabled);
		v->addLayout(pcm_grid);

		sound_backend_=new QComboBox(page);
		sound_backend_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
		PopulateSoundBackendCombo(sound_backend_);
		sound_device_=new QComboBox(page);
		sound_device_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

		auto *device_row=new QHBoxLayout();
		device_row->setSpacing(8);
		device_row->addWidget(new QLabel(tr("Output device:"),page));
		device_row->addWidget(sound_device_,1);
		device_row->addWidget(new QLabel(tr("Backend:"),page));
		device_row->addWidget(sound_backend_);
		v->addLayout(device_row);
		connect(sound_backend_,qOverload<int>(&QComboBox::currentIndexChanged),this,&SettingsDialog::onAudioBackendChanged);
		populateSoundDeviceCombo(QString());

		auto *midi_line=new QHBoxLayout();
		midi_line->setContentsMargins(0,0,0,0);
		midi_line->setSpacing(6);
		auto *midi_output_label=new QLabel(tr("MIDI output"),page);
		midi_output_=new QComboBox(page);
		midi_output_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
		midi_line->addWidget(midi_output_label);
		midi_line->addWidget(midi_output_);
		connect(midi_output_,qOverload<int>(&QComboBox::currentIndexChanged),this,&SettingsDialog::onMidiOutputChanged);

		midi_detail_stack_=new QStackedWidget(page);
		midi_detail_stack_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);

		midi_fluidsynth_panel_=new QWidget(midi_detail_stack_);
		auto *sf_row=new QHBoxLayout(midi_fluidsynth_panel_);
		sf_row->setContentsMargins(0,0,0,0);
		sf_row->setSpacing(4);
		auto *sf_label=new QLabel(tr("SoundFont"),midi_fluidsynth_panel_);
		midi_soundfont_edit_=new QLineEdit(midi_fluidsynth_panel_);
		midi_soundfont_edit_->setReadOnly(true);
		midi_soundfont_edit_->setPlaceholderText(tr("(not set)"));
		midi_soundfont_edit_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
		midi_soundfont_edit_->setMinimumWidth(0);
		{
			const QFontMetrics fm(midi_soundfont_edit_->font());
			const int sample=std::max({
			    fm.horizontalAdvance(QStringLiteral("GeneralUser.sf2")),
			    fm.horizontalAdvance(QStringLiteral("(not set)")),
			    fm.horizontalAdvance(QStringLiteral("（未指定）")),
			});
			midi_soundfont_edit_->setFixedWidth(sample+20);
		}
		midi_soundfont_browse_=new QPushButton(tr("Browse…"),midi_fluidsynth_panel_);
		FitButtonToText(
		    midi_soundfont_browse_,
		    {QStringLiteral("Browse…"),QStringLiteral("参照…"),QStringLiteral("参照")});
		{
			const QFontMetrics fm(sf_label->font());
			const int label_w=std::max({
			    fm.horizontalAdvance(tr("MIDI output")),
			    fm.horizontalAdvance(QStringLiteral("MIDI output")),
			    fm.horizontalAdvance(QStringLiteral("MIDI出力")),
			    fm.horizontalAdvance(tr("SoundFont")),
			    fm.horizontalAdvance(QStringLiteral("SoundFont")),
			    fm.horizontalAdvance(tr("port")),
			    fm.horizontalAdvance(QStringLiteral("port")),
			    fm.horizontalAdvance(QStringLiteral("ポート")),
			});
			midi_output_label->setFixedWidth(label_w);
			midi_output_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
			sf_label->setFixedWidth(label_w);
			sf_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		}
		sf_row->addWidget(sf_label);
		sf_row->addWidget(midi_soundfont_edit_);
		sf_row->addWidget(midi_soundfont_browse_);
		connect(midi_soundfont_browse_,&QPushButton::clicked,this,&SettingsDialog::browseMidiSoundFont);

		midi_alsa_port_panel_=new QWidget(midi_detail_stack_);
		auto *port_row=new QHBoxLayout(midi_alsa_port_panel_);
		port_row->setContentsMargins(0,0,0,0);
		port_row->setSpacing(sf_row->spacing());
		auto *port_label=new QLabel(tr("port"),midi_alsa_port_panel_);
		port_label->setFixedWidth(sf_label->width());
		port_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		midi_alsa_port_=new QComboBox(midi_alsa_port_panel_);
		midi_alsa_port_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
		midi_alsa_port_->setFixedWidth(
		    midi_soundfont_edit_->width()+sf_row->spacing()+midi_soundfont_browse_->width());
		port_row->addWidget(port_label);
		port_row->addWidget(midi_alsa_port_);
		connect(midi_alsa_port_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			markDirty();
		});

		midi_detail_stack_->addWidget(midi_fluidsynth_panel_);
		midi_detail_stack_->addWidget(midi_alsa_port_panel_);
		midi_line->addWidget(midi_detail_stack_);
		midi_line->addStretch();
		v->addLayout(midi_line);

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("Changes take effect immediately when you press Apply or OK.")));
		display_audio_page_=page;
		tabs_->addTab(page,tr("Video / Audio"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		auto_resume_enabled_=new QCheckBox(tr("Enable auto-resume"),page);
		v->addWidget(auto_resume_enabled_);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("When a disc profile exists for the mounted CD image, automatically save and\n"
		       "resume from per-disc state save slot 0 (state0_XXXXXXXX.TState) at CD eject,\n"
		       "app exit, startup, and CD change. Manual restart with the same CD does not\n"
		       "load a saved state. When off, state saves are neither written nor loaded.")));

		snap_mouse_integration_=new QCheckBox(tr("Faster mouse integration"),page);
		v->addWidget(snap_mouse_integration_);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("Mouse BIOS integration that writes guest memory to reduce latency.")));

		auto *cdda_cache_row=new QHBoxLayout();
		cdda_cache_during_data_read_=new QCheckBox(tr("CDDA cache:"),page);
		cdda_cache_post_read_grace_sec_=new QSpinBox(page);
		cdda_cache_post_read_grace_sec_->setRange(1,60);
		cdda_cache_post_read_grace_sec_->setSuffix(tr(" s"));
		cdda_cache_row->addWidget(cdda_cache_during_data_read_);
		cdda_cache_row->addStretch();
		cdda_cache_row->addWidget(cdda_cache_post_read_grace_sec_);
		v->addLayout(cdda_cache_row);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("Bulk-prefetch the CDDA audio track and keep playing through data reads\n"
		       "without interrupting playback. The value is how many seconds until\n"
		       "playback is considered finished.")));
		connect(cdda_cache_during_data_read_,&QCheckBox::toggled,cdda_cache_post_read_grace_sec_,&QWidget::setEnabled);

		idle_inhibit_=new QCheckBox(tr("Inhibit display idle"),page);
		v->addWidget(idle_inhibit_);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("Prevents automatic screen blanking/dimming on Wayland sessions.")));

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("Changes take effect immediately when you press Apply or OK.")));
		function_page_=page;
		tabs_->addTab(page,tr("Features"));
	}

	auto *buttons=new QDialogButtonBox(
	    QDialogButtonBox::Apply|QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
	apply_button_=buttons->button(QDialogButtonBox::Apply);
	apply_button_->setEnabled(false);
	connect(apply_button_,&QPushButton::clicked,this,&SettingsDialog::onApply);
	connect(buttons,&QDialogButtonBox::accepted,this,[this]{
		Values next=values_;
		applyToValues(next);
		values_=next;
		accept();
	});
	connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);

	auto *button_row=new QHBoxLayout();
	auto *defaults_button=new QPushButton(tr("Standard settings (&D)"),this);
	connect(defaults_button,&QPushButton::clicked,this,&SettingsDialog::resetCurrentTabToDefaults);
	button_row->addWidget(defaults_button);
	button_row->addStretch();
	button_row->addWidget(buttons);
	layout->addLayout(button_row);

	connectDirtyTracking();
	connect(tabs_,&QTabWidget::currentChanged,this,[this](int index){
		if(nullptr!=tabs_ && tabs_->widget(index)==display_audio_page_)
		{
			updateAudioTabMidiSection();
		}
		Q_UNUSED(index);
	});
	updateAudioTabMidiSection();
}

void SettingsDialog::connectDirtyTracking()
{
	for(QSpinBox *spin : findChildren<QSpinBox*>())
	{
		connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,&SettingsDialog::markDirty);
	}
	for(QCheckBox *box : findChildren<QCheckBox*>())
	{
		connect(box,&QCheckBox::toggled,this,&SettingsDialog::markDirty);
	}
	for(QRadioButton *radio : findChildren<QRadioButton*>())
	{
		connect(radio,&QRadioButton::toggled,this,&SettingsDialog::markDirty);
	}
	for(QComboBox *combo : findChildren<QComboBox*>())
	{
		connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,&SettingsDialog::markDirty);
	}
	for(QSlider *slider : findChildren<QSlider*>())
	{
		connect(slider,&QSlider::valueChanged,this,&SettingsDialog::markDirty);
	}
	for(QLineEdit *edit : findChildren<QLineEdit*>())
	{
		connect(edit,&QLineEdit::textChanged,this,&SettingsDialog::markDirty);
	}
}

void SettingsDialog::setApplyEnabled(bool enabled)
{
	if(nullptr!=apply_button_)
	{
		apply_button_->setEnabled(enabled);
	}
}

bool SettingsDialog::applyPending(void) const
{
	return nullptr!=apply_button_ && apply_button_->isEnabled();
}

void SettingsDialog::setApplyPending(bool pending)
{
	setApplyEnabled(pending);
}

void SettingsDialog::markDirty()
{
	if(loading_)
	{
		return;
	}
	setApplyEnabled(true);
}

void SettingsDialog::onApply()
{
	Values next=values_;
	applyToValues(next);
	values_=next;
	Q_EMIT settingsApplied(values_);
	setApplyEnabled(false);
}

void SettingsDialog::populateSoundDeviceCombo(const QString &backend,const QString &select_device)
{
	if(nullptr==sound_device_)
	{
		return;
	}
	const QByteArray backend_utf8=backend.toUtf8();
	const bool auto_backend=TownsQtMiniaudioDevices::IsAutoBackend(backend_utf8.constData());
	sound_device_->setEnabled(!auto_backend);
	if(auto_backend)
	{
		sound_device_->clear();
		sound_device_->addItem(tr("Default"),QString());
		sound_device_->setCurrentIndex(0);
		return;
	}

	const QString keep=select_device.isNull() ? sound_device_->currentData().toString() : select_device;
	sound_device_->clear();
	sound_device_->addItem(tr("Default"),QString());
	for(const auto &dev : TownsQtMiniaudioDevices::ListPlayback(backend_utf8.constData()))
	{
		const QString display=QString::fromUtf8(dev.name.c_str());
		const QString id=dev.id.empty() ? display : QString::fromUtf8(dev.id.c_str());
		sound_device_->addItem(display,id);
	}
	int idx=sound_device_->findData(keep);
	if(idx<0 && !keep.isEmpty())
	{
		for(int i=1; i<sound_device_->count(); ++i)
		{
			if(sound_device_->itemText(i)==keep)
			{
				idx=i;
				break;
			}
		}
	}
	if(idx<0 && !keep.isEmpty())
	{
		sound_device_->addItem(tr("%1 (not found)").arg(keep),keep);
		idx=sound_device_->count()-1;
	}
	sound_device_->setCurrentIndex(0<=idx ? idx : 0);
}

void SettingsDialog::updateFunctionTab()
{
	if(nullptr==idle_inhibit_)
	{
		return;
	}
	const bool idle_ok=TownsQtWaylandIdleInhibit::Available();
	if(!idle_ok && idle_inhibit_->isChecked())
	{
		loading_=true;
		idle_inhibit_->setChecked(false);
		loading_=false;
	}
	idle_inhibit_->setEnabled(idle_ok);
	if(!idle_ok)
	{
		idle_inhibit_->setToolTip(
		    tr("Available only on sessions that support Wayland idle-inhibit."));
	}
	else
	{
		idle_inhibit_->setToolTip(QString());
	}
}

void SettingsDialog::onAudioBackendChanged()
{
	if(nullptr==sound_backend_)
	{
		return;
	}
	populateSoundDeviceCombo(sound_backend_->currentData().toString());
	markDirty();
}

void SettingsDialog::populateMidiOutputCombo(const QString &select_id)
{
	if(nullptr==midi_output_)
	{
		return;
	}
	const QString keep=select_id.isNull() ? midi_output_->currentData().toString() : select_id;
	const QSignalBlocker blocker(midi_output_);
	midi_output_->clear();
#if defined(__linux__)
	const bool have_fluid=MidiBackendProbe::IsFluidSynthLibraryAvailable();
	const bool have_alsa=MidiBackendProbe::IsAlsaSequencerAvailable();
	if(true==have_fluid)
	{
		midi_output_->addItem(tr("FluidSynth"),QStringLiteral("fluidsynth"));
	}
	if(true==have_alsa)
	{
		midi_output_->addItem(tr("ALSA"),QStringLiteral("alsa"));
	}
	if(0==midi_output_->count())
	{
		midi_output_->addItem(tr("(none)"),QString());
		midi_output_->setEnabled(false);
		FitComboToSamples(midi_output_,MidiOutputWidthSamples());
		return;
	}
	midi_output_->setEnabled(true);
	QString select=keep;
	if(select!=QStringLiteral("fluidsynth") && select!=QStringLiteral("alsa"))
	{
		if(select.contains(QLatin1Char(':')))
		{
			select=QStringLiteral("alsa");
		}
		else if(true==have_fluid)
		{
			select=QStringLiteral("fluidsynth");
		}
		else
		{
			select=QStringLiteral("alsa");
		}
	}
	const int idx=midi_output_->findData(select);
	midi_output_->setCurrentIndex(0<=idx ? idx : 0);
	FitComboToSamples(midi_output_,MidiOutputWidthSamples());
#else
	midi_output_->addItem(tr("(none)"),QString());
	midi_output_->setEnabled(false);
	FitComboToSamples(midi_output_,MidiOutputWidthSamples());
#endif
}

void SettingsDialog::populateMidiAlsaPortCombo(const QString &select_id)
{
	if(nullptr==midi_alsa_port_)
	{
		return;
	}
	const QString keep=select_id.isNull() ? midi_alsa_port_->currentData().toString() : select_id;
	const QSignalBlocker blocker(midi_alsa_port_);
	midi_alsa_port_->clear();
#if defined(__linux__)
	midi_alsa_port_->addItem(tr("(select)"),QString());
	for(const auto &entry : ListAlsaMidiOutputDestinations())
	{
		midi_alsa_port_->addItem(
		    QString::fromStdString(entry.label),
		    QString::fromStdString(entry.id));
	}
	const int idx=midi_alsa_port_->findData(keep);
	midi_alsa_port_->setCurrentIndex(0<=idx ? idx : 0);
#else
	midi_alsa_port_->addItem(tr("(select)"),QString());
#endif
	if(nullptr!=midi_soundfont_edit_ && nullptr!=midi_soundfont_browse_)
	{
		midi_alsa_port_->setFixedWidth(
		    midi_soundfont_edit_->width()+4+midi_soundfont_browse_->width());
	}
	else
	{
		FitComboToSamples(midi_alsa_port_,MidiAlsaPortWidthSamples());
	}
}

void SettingsDialog::onMidiOutputChanged()
{
	updateAudioTabMidiSection();
	markDirty();
}

void SettingsDialog::updateAudioTabMidiSection()
{
	const bool midi_board=nullptr!=midi_board_ && midi_board_->isChecked();
#if defined(__linux__)
	const QString output=
	    nullptr!=midi_output_ ? midi_output_->currentData().toString() : QString();
	const bool fluidsynth=(output==QStringLiteral("fluidsynth"));
	const bool alsa_seq=(output==QStringLiteral("alsa"));
	if(nullptr!=midi_output_)
	{
		const bool have_choice=
		    0<=midi_output_->findData(QStringLiteral("fluidsynth")) ||
		    0<=midi_output_->findData(QStringLiteral("alsa"));
		midi_output_->setEnabled(midi_board && have_choice);
	}
	if(nullptr!=midi_detail_stack_)
	{
		const bool show_detail=midi_board && (fluidsynth || alsa_seq);
		midi_detail_stack_->setVisible(show_detail);
		if(true==show_detail)
		{
			midi_detail_stack_->setCurrentWidget(fluidsynth ? midi_fluidsynth_panel_ : midi_alsa_port_panel_);
		}
	}
	if(nullptr!=midi_soundfont_edit_)
	{
		midi_soundfont_edit_->setEnabled(midi_board && fluidsynth);
	}
	if(nullptr!=midi_soundfont_browse_)
	{
		midi_soundfont_browse_->setEnabled(midi_board && fluidsynth);
	}
	if(nullptr!=midi_alsa_port_)
	{
		midi_alsa_port_->setEnabled(midi_board && alsa_seq);
		if(nullptr!=midi_soundfont_edit_ && nullptr!=midi_soundfont_browse_)
		{
			midi_alsa_port_->setFixedWidth(
			    midi_soundfont_edit_->width()+4+midi_soundfont_browse_->width());
		}
	}
#else
	if(nullptr!=midi_output_)
	{
		midi_output_->setEnabled(false);
	}
	if(nullptr!=midi_detail_stack_)
	{
		midi_detail_stack_->setVisible(false);
	}
#endif
}

void SettingsDialog::browseMidiSoundFont()
{
	const QString start_dir=
	    midi_soundfont_path_.isEmpty()
	        ? QString()
	        : QFileInfo(midi_soundfont_path_).absolutePath();
	const QString path=QFileDialog::getOpenFileName(
	    this,
	    tr("Select SoundFont"),
	    start_dir,
	    tr("SoundFont (*.sf2);;All files (*)"));
	if(path.isEmpty())
	{
		return;
	}
	midi_soundfont_path_=path;
	if(nullptr!=midi_soundfont_edit_)
	{
		midi_soundfont_edit_->setText(QFileInfo(path).fileName());
		midi_soundfont_edit_->setToolTip(path);
	}
	markDirty();
}

void SettingsDialog::updateCpuComboItems()
{
	if(nullptr==cpu_kind_)
	{
		return;
	}
	const TownsQtCpuKind previous=currentCpuKind();
	const auto allowed=TownsQtCpuKindsAllowedForSysRom(
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	const QSignalBlocker blocker(cpu_kind_);
	cpu_kind_->clear();
	for(TownsQtCpuKind kind : allowed)
	{
		cpu_kind_->addItem(TownsQtCpuKindLabel(kind),static_cast<int>(kind));
	}
	const TownsQtCpuKind select=TownsQtCpuKindClampToAllowed(
	    previous,
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	const int row=cpu_kind_->findData(static_cast<int>(select));
	if(0<=row)
	{
		cpu_kind_->setCurrentIndex(row);
	}
	else if(0<cpu_kind_->count())
	{
		cpu_kind_->setCurrentIndex(0);
	}
	FitComboToText(cpu_kind_,CpuKindWidthSamples());
}

void SettingsDialog::updateModelComboItems()
{
	if(nullptr==model_group_)
	{
		return;
	}
	const TownsQtCpuKind cpu=currentCpuKind();
	const int previous=currentModelGroupIndex();
	const auto allowed=TownsQtModelGroupsAllowedForCpuAndSysRom(
	    cpu,
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	const QSignalBlocker blocker(model_group_);
	model_group_->clear();
	for(int idx : allowed)
	{
		model_group_->addItem(TownsQtModelGroupLabel(idx),idx);
	}
	const int select=TownsQtModelGroupClampToAllowed(
	    previous,
	    cpu,
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	const int row=model_group_->findData(select);
	if(0<=row)
	{
		model_group_->setCurrentIndex(row);
	}
	else if(0<model_group_->count())
	{
		model_group_->setCurrentIndex(0);
	}
	FitComboToText(model_group_,ModelWidthSamples());
}

TownsQtCpuKind SettingsDialog::currentCpuKind() const
{
	if(nullptr==cpu_kind_ || 0>cpu_kind_->currentIndex())
	{
		return TownsQtCpuKindPreferredForSysRom(
		    sys_rom_profile_,
		    sys_rom_level_,
		    marty_ex_rom_present_);
	}
	const int data=cpu_kind_->currentData().toInt();
	if(0>data)
	{
		return TownsQtCpuKindPreferredForSysRom(
		    sys_rom_profile_,
		    sys_rom_level_,
		    marty_ex_rom_present_);
	}
	return static_cast<TownsQtCpuKind>(data);
}

int SettingsDialog::currentModelGroupIndex() const
{
	if(nullptr==model_group_ || 0>model_group_->currentIndex())
	{
		return TownsQtModelGroupPreferredForCpuAndSysRom(
		    currentCpuKind(),
		    sys_rom_profile_,
		    sys_rom_level_,
		    marty_ex_rom_present_);
	}
	const int data=model_group_->currentData().toInt();
	if(0>data)
	{
		return TownsQtModelGroupPreferredForCpuAndSysRom(
		    currentCpuKind(),
		    sys_rom_profile_,
		    sys_rom_level_,
		    marty_ex_rom_present_);
	}
	return data;
}

void SettingsDialog::updateDisplayScaleRange()
{
	if(nullptr==display_scale_)
	{
		return;
	}
	QScreen *screen=this->screen();
	if(nullptr==screen)
	{
		screen=QGuiApplication::primaryScreen();
	}
	const QSize avail=nullptr!=screen ? screen->availableGeometry().size() : QSize(1920,1080);
	// Approximate menu/status chrome; MainWindow uses live heights. DE scale is in availableGeometry (DIP).
	const int max_scale=TownsQtSettings::maxDisplayScaleForAvailableSize(avail,0,52);
	display_scale_->setMaximum(std::max(1,max_scale));
	if(display_scale_->value()>display_scale_->maximum())
	{
		display_scale_->setValue(display_scale_->maximum());
	}
}

void SettingsDialog::updateSysRomInfoLabel()
{
	if(nullptr==sys_rom_info_label_)
	{
		return;
	}
	sys_rom_info_label_->setText(TownsQtRomAvailability::SysRomSummary(rom_dir_));
}

void SettingsDialog::updateFastModeControls()
{
	const bool custom=nullptr!=cpu_freq_custom_ && cpu_freq_custom_->isChecked();
	if(nullptr!=cpu_freq_preset_)
	{
		cpu_freq_preset_->setEnabled(true!=custom);
	}
	if(nullptr!=cpu_freq_custom_mhz_)
	{
		cpu_freq_custom_mhz_->setEnabled(custom);
	}
}

int SettingsDialog::selectedCpuCustomFrequencyMhz() const
{
	if(nullptr!=cpu_freq_custom_mhz_)
	{
		return std::clamp(cpu_freq_custom_mhz_->value(),33,60);
	}
	const int fallback=editingDiscProfile() ?
	    values_.profileCpuCustomFrequencyMhz : values_.cpuCustomFrequencyMhz;
	return std::clamp(fallback,33,60);
}

bool SettingsDialog::selectedCpuFastMode() const
{
	if(nullptr!=cpu_freq_custom_ && cpu_freq_custom_->isChecked())
	{
		return true;
	}
	if(nullptr!=cpu_freq_preset_)
	{
		return -1!=cpu_freq_preset_->currentData().toInt();
	}
	return editingDiscProfile() ? values_.profileCpuFastMode : values_.cpuFastMode;
}

int SettingsDialog::selectedCpuFrequencyMhz() const
{
	if(nullptr!=cpu_freq_custom_ && cpu_freq_custom_->isChecked())
	{
		return selectedCpuCustomFrequencyMhz();
	}
	if(nullptr!=cpu_freq_preset_)
	{
		const int id=cpu_freq_preset_->currentData().toInt();
		if(16==id || 20==id || 25==id)
		{
			return id;
		}
	}
	// Compatibility mode (or unset): keep the last active / custom frequency.
	const int active=editingDiscProfile() ?
	    values_.profileCpuFrequencyMhz : values_.cpuFrequencyMhz;
	if(16==active || 20==active || 25==active)
	{
		return active;
	}
	return selectedCpuCustomFrequencyMhz();
}

void SettingsDialog::setCpuFrequencyWidgets(bool fast_mode,int active_mhz,int custom_mhz)
{
	custom_mhz=std::clamp(custom_mhz,33,60);
	if(nullptr!=cpu_freq_custom_mhz_)
	{
		QSignalBlocker blocker(cpu_freq_custom_mhz_);
		cpu_freq_custom_mhz_->setValue(custom_mhz);
	}
	const bool use_custom=
	    true==fast_mode && 16!=active_mhz && 20!=active_mhz && 25!=active_mhz;
	if(nullptr!=cpu_freq_preset_)
	{
		QSignalBlocker blocker(cpu_freq_preset_);
		int idx=-1;
		if(true!=fast_mode)
		{
			idx=cpu_freq_preset_->findData(-1);
		}
		else if(true!=use_custom)
		{
			idx=cpu_freq_preset_->findData(active_mhz);
		}
		else
		{
			// Keep last preset selection while custom radio is active.
			idx=cpu_freq_preset_->currentIndex();
			if(0>idx || -1==cpu_freq_preset_->itemData(idx).toInt())
			{
				idx=cpu_freq_preset_->findData(25);
			}
		}
		if(0>idx)
		{
			idx=0;
		}
		cpu_freq_preset_->setCurrentIndex(idx);
	}
	QRadioButton *target=cpu_freq_preset_radio_;
	if(true==use_custom)
	{
		target=cpu_freq_custom_;
	}
	if(nullptr!=target)
	{
		QSignalBlocker blocker(target);
		target->setChecked(true);
	}
	updateFastModeControls();
}

unsigned int SettingsDialog::selectedBootKeyComb() const
{
	if(nullptr!=boot_drive_)
	{
		const QVariant data=boot_drive_->currentData();
		if(true==data.isValid())
		{
			return static_cast<unsigned int>(data.toInt());
		}
	}
	return editingDiscProfile() ? values_.profileBootKeyComb : values_.bootKeyComb;
}

void SettingsDialog::setBootDriveWidget(unsigned int keyComb)
{
	if(nullptr==boot_drive_)
	{
		return;
	}
	int idx=boot_drive_->findData(static_cast<int>(keyComb));
	if(0>idx)
	{
		idx=boot_drive_->findData(static_cast<int>(BOOT_KEYCOMB_CD));
	}
	if(0>idx)
	{
		idx=0;
	}
	QSignalBlocker blocker(boot_drive_);
	boot_drive_->setCurrentIndex(idx);
}

bool SettingsDialog::editingDiscProfile(void) const
{
	return true==values_.discProfileAvailable;
}

void SettingsDialog::applyProfileEditAppearance(void)
{
	const bool editing=editingDiscProfile();
	if(nullptr!=profile_fields_box_)
	{
		// Borderless container: amber fill only while editing a disc profile.
		// Use palette (not stylesheet) so children keep the dialog font/size.
		if(true==editing)
		{
			profile_fields_box_->setAutoFillBackground(true);
			QPalette pal=QApplication::palette(profile_fields_box_);
			const QColor amber(255,243,220);
			pal.setColor(QPalette::Window,amber);
			pal.setColor(QPalette::Base,amber);
			profile_fields_box_->setPalette(pal);
		}
		else
		{
			profile_fields_box_->setAutoFillBackground(false);
			profile_fields_box_->setPalette(QApplication::palette(profile_fields_box_));
		}
		profile_fields_box_->setFont(font());
	}

	if(nullptr!=basics_footer_label_)
	{
		if(true==editing)
		{
			basics_footer_label_->setText(
			    tr("Editing the disc profile (fp_XXXXXXXX.ini, amber block).\n"
			       "Apply or OK saves clock, boot drive, memory, ports, options,\n"
			       "and FD0 / FD1 / HD0–HD6 mount state to the profile (restored on next load).\n"
			       "CMOS (drive letters, single drive) uses cmos/cmos_XXXXXXXX.bin for this disc — set in Towns SETUP.\n"
			       "CPU and model stay global in townsqt.conf and are not stored in the profile.\n"
			       "Memory, boot drive, and CPU fidelity changes restart the emulator."));
		}
		else if(true==values_.discMounted)
		{
			basics_footer_label_->setText(
			    tr("No disc profile for this CD yet. Use Create profile to save per-disc settings\n"
			       "(including FD0, FD1, and HD0–HD6 mount state for restore).\n"
			       "A profile also gets its own CMOS file (cmos/cmos_XXXXXXXX.bin) for Towns SETUP.\n"
			       "Until then, Apply or OK saves Basics defaults to townsqt.conf.\n"
			       "Memory, boot drive, and CPU fidelity changes restart the emulator; game-port changes apply immediately."));
		}
		else
		{
			basics_footer_label_->setText(
			    tr("Editing Basics defaults (townsqt.conf). Apply or OK saves here.\n"
			       "Memory, boot drive, and CPU fidelity changes restart the emulator; game-port changes apply immediately.\n"
			       "Create a disc profile when a CD is mounted to save per-disc settings\n"
			       "(FD0 / FD1 / HD0–HD6 mounts, and CMOS under cmos/cmos_XXXXXXXX.bin)."));
		}
	}
}

void SettingsDialog::loadSharedMachineWidgets(const Values &values,bool fromProfile)
{
	const int freq=fromProfile ? values.profileCpuFrequencyMhz : values.cpuFrequencyMhz;
	const int custom=fromProfile ? values.profileCpuCustomFrequencyMhz : values.cpuCustomFrequencyMhz;
	const bool fast=fromProfile ? values.profileCpuFastMode : values.cpuFastMode;
	const unsigned int bootKey=fromProfile ? values.profileBootKeyComb : values.bootKeyComb;
	const int mem=fromProfile ? values.profileMemSizeInMB : values.memSizeInMB;
	const bool fidelity=fromProfile ? values.profileCpuHighFidelity : values.cpuHighFidelity;
	const bool pretend=fromProfile ? values.profilePretend386DX : values.pretend386DX;
	const bool fpu=fromProfile ? values.profileUseFPU : values.useFPU;
	const bool scsi=fromProfile ? values.profileFastScsi : values.fastScsi;
	const bool fd=fromProfile ? values.profileFastFd : values.fastFd;
	const bool midi=fromProfile ? values.profileMidiBoard : values.midiBoard;
	const unsigned int gp0=fromProfile ? values.profileGamePort0 : values.gamePort0;
	const unsigned int gp1=fromProfile ? values.profileGamePort1 : values.gamePort1;
	const int hold0=fromProfile ? values.profileMaxButtonHoldTimeMs0 : values.maxButtonHoldTimeMs0;
	const int hold1=fromProfile ? values.profileMaxButtonHoldTimeMs1 : values.maxButtonHoldTimeMs1;

	setCpuFrequencyWidgets(fast,freq,custom);
	setBootDriveWidget(bootKey);
	if(nullptr!=mem_size_mb_)
	{
		const int max_mem=TownsQtModelGroupMaxMemMb(currentModelGroupIndex());
		mem_size_mb_->setMaximum(max_mem);
		mem_size_mb_->setValue(std::clamp(mem,1,max_mem));
	}
	if(nullptr!=cpu_fidelity_)
	{
		const int row=cpu_fidelity_->findData(fidelity ? 1 : 0);
		cpu_fidelity_->setCurrentIndex(0<=row ? row : 0);
	}
	if(nullptr!=pretend_386_)
	{
		pretend_386_->setChecked(pretend);
	}
	if(nullptr!=use_fpu_)
	{
		use_fpu_->setChecked(fpu);
	}
	if(nullptr!=fast_scsi_)
	{
		fast_scsi_->setChecked(scsi);
	}
	if(nullptr!=fast_fd_)
	{
		fast_fd_->setChecked(fd);
	}
	if(nullptr!=midi_board_)
	{
		midi_board_->setChecked(midi);
	}
	if(nullptr!=gameport0_)
	{
		TownsQtGamePortOptions::PopulateCombo(gameport0_,gp0);
	}
	if(nullptr!=gameport1_)
	{
		TownsQtGamePortOptions::PopulateCombo(gameport1_,gp1);
	}
	if(nullptr!=max_button_hold0_)
	{
		max_button_hold0_->setValue(std::max(0,hold0));
	}
	if(nullptr!=max_button_hold1_)
	{
		max_button_hold1_->setValue(std::max(0,hold1));
	}
}

void SettingsDialog::readSharedMachineWidgets(Values &out,bool toProfile) const
{
	const int freq=selectedCpuFrequencyMhz();
	const int custom=selectedCpuCustomFrequencyMhz();
	const bool fast=selectedCpuFastMode();
	const unsigned int bootKey=selectedBootKeyComb();
	const int mem=nullptr!=mem_size_mb_ ? mem_size_mb_->value() :
	    (toProfile ? values_.profileMemSizeInMB : values_.memSizeInMB);
	const bool fidelity=nullptr!=cpu_fidelity_ ?
	    (1==cpu_fidelity_->currentData().toInt()) :
	    (toProfile ? values_.profileCpuHighFidelity : values_.cpuHighFidelity);
	const bool pretend=nullptr!=pretend_386_ ? pretend_386_->isChecked() :
	    (toProfile ? values_.profilePretend386DX : values_.pretend386DX);
	const bool fpu=nullptr!=use_fpu_ ? use_fpu_->isChecked() :
	    (toProfile ? values_.profileUseFPU : values_.useFPU);
	const bool scsi=nullptr!=fast_scsi_ ? fast_scsi_->isChecked() :
	    (toProfile ? values_.profileFastScsi : values_.fastScsi);
	const bool fd=nullptr!=fast_fd_ ? fast_fd_->isChecked() :
	    (toProfile ? values_.profileFastFd : values_.fastFd);
	const bool midi=nullptr!=midi_board_ ? midi_board_->isChecked() :
	    (toProfile ? values_.profileMidiBoard : values_.midiBoard);
	const unsigned int gp0=nullptr!=gameport0_ ?
	    TownsQtGamePortOptions::ComboSelection(gameport0_,TOWNS_GAMEPORTEMU_PHYSICAL0) :
	    (toProfile ? values_.profileGamePort0 : values_.gamePort0);
	const unsigned int gp1=nullptr!=gameport1_ ?
	    TownsQtGamePortOptions::ComboSelection(gameport1_,TOWNS_GAMEPORTEMU_MOUSE) :
	    (toProfile ? values_.profileGamePort1 : values_.gamePort1);
	const int hold0=nullptr!=max_button_hold0_ ? max_button_hold0_->value() :
	    (toProfile ? values_.profileMaxButtonHoldTimeMs0 : values_.maxButtonHoldTimeMs0);
	const int hold1=nullptr!=max_button_hold1_ ? max_button_hold1_->value() :
	    (toProfile ? values_.profileMaxButtonHoldTimeMs1 : values_.maxButtonHoldTimeMs1);

	if(true==toProfile)
	{
		out.profileCpuFrequencyMhz=freq;
		out.profileCpuCustomFrequencyMhz=custom;
		out.profileCpuFastMode=fast;
		out.profileBootKeyComb=bootKey;
		out.profileMemSizeInMB=mem;
		out.profileCpuHighFidelity=fidelity;
		out.profilePretend386DX=pretend;
		out.profileUseFPU=fpu;
		out.profileFastScsi=scsi;
		out.profileFastFd=fd;
		out.profileMidiBoard=midi;
		out.profileGamePort0=gp0;
		out.profileGamePort1=gp1;
		out.profileMaxButtonHoldTimeMs0=hold0;
		out.profileMaxButtonHoldTimeMs1=hold1;
	}
	else
	{
		out.cpuFrequencyMhz=freq;
		out.cpuCustomFrequencyMhz=custom;
		out.cpuFastMode=fast;
		out.bootKeyComb=bootKey;
		out.memSizeInMB=mem;
		out.cpuHighFidelity=fidelity;
		out.pretend386DX=pretend;
		out.useFPU=fpu;
		out.fastScsi=scsi;
		out.fastFd=fd;
		out.midiBoard=midi;
		out.gamePort0=gp0;
		out.gamePort1=gp1;
		out.maxButtonHoldTimeMs0=hold0;
		out.maxButtonHoldTimeMs1=hold1;
		// Seed profile fields for Create profile from current Basics.
		out.profileCpuFrequencyMhz=freq;
		out.profileCpuCustomFrequencyMhz=custom;
		out.profileCpuFastMode=fast;
		out.profileBootKeyComb=bootKey;
		out.profileMemSizeInMB=mem;
		out.profileCpuHighFidelity=fidelity;
		out.profilePretend386DX=pretend;
		out.profileUseFPU=fpu;
		out.profileFastScsi=scsi;
		out.profileFastFd=fd;
		out.profileMidiBoard=midi;
		out.profileGamePort0=gp0;
		out.profileGamePort1=gp1;
		out.profileMaxButtonHoldTimeMs0=hold0;
		out.profileMaxButtonHoldTimeMs1=hold1;
	}
}

void SettingsDialog::updateMachineTabControls()
{
	const TownsQtCpuKind kind=currentCpuKind();
	const int model_index=currentModelGroupIndex();
	const bool marty_mode=TownsQtCpuKindIsMarty(kind);
	const bool editable=!marty_mode;

	if(nullptr!=mem_size_mb_)
	{
		const int max_mem=TownsQtModelGroupMaxMemMb(model_index);
		mem_size_mb_->setMaximum(max_mem);
		if(max_mem<mem_size_mb_->value())
		{
			mem_size_mb_->setValue(max_mem);
		}
	}
	if(nullptr!=model_group_)
	{
		model_group_->setEnabled(0<model_group_->count());
	}
	if(nullptr!=opt_grid_widget_)
	{
		opt_grid_widget_->setEnabled(editable);
		opt_grid_widget_->setToolTip(QString());
	}
	for(QWidget *w : std::initializer_list<QWidget *>{
	    mem_label_,mem_size_mb_,pretend_386_,use_fpu_,fast_scsi_,fast_fd_,
	    midi_board_,cpu_fidelity_,cpu_freq_preset_radio_,cpu_freq_preset_,
	    cpu_freq_custom_,cpu_freq_custom_mhz_,boot_drive_label_,boot_drive_,
	    gameport0_,gameport1_,max_button_hold0_,max_button_hold1_})
	{
		if(nullptr!=w)
		{
			w->setEnabled(editable);
			w->setToolTip(QString());
		}
	}
	if(true==editable)
	{
		updateFastModeControls();
	}
	applyProfileEditAppearance();
	updateCapabilityStatusLabels();
}

void SettingsDialog::updateCapabilityStatusLabels()
{
	const int model_index=currentModelGroupIndex();
	const bool hi=TownsQtModelGroupEffectiveHighRes(model_index,sys_rom_profile_);
	const bool ug=TownsQtModelGroupEffectiveUgGenerationIO(model_index,rom_dir_);
	if(nullptr!=high_res_status_label_)
	{
		high_res_status_label_->setText(hi ? tr("Enabled") : tr("Disabled"));
	}
	if(nullptr!=ug_io_status_label_)
	{
		ug_io_status_label_->setText(ug ? tr("Enabled") : tr("Disabled"));
	}
}

void SettingsDialog::updateProfileTabControls()
{
	const bool mounted=values_.discMounted;
	const bool haveProfile=values_.discProfileAvailable;
	if(nullptr!=disc_profile_bar_)
	{
		disc_profile_bar_->setVisible(true);
	}
	if(nullptr!=disc_profile_status_label_)
	{
		disc_profile_status_label_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
		if(true!=mounted)
		{
			disc_profile_status_label_->setText(tr("No CD mounted."));
		}
		else if(true!=haveProfile)
		{
			disc_profile_status_label_->setText(tr("No profile for this disc."));
		}
		else
		{
			disc_profile_status_label_->setText(tr("Disc profile loaded."));
		}
	}
	if(nullptr!=disc_profile_file_label_)
	{
		if(true!=mounted)
		{
			disc_profile_file_label_->setText(tr("(no CD)"));
		}
		else if(true!=haveProfile)
		{
			disc_profile_file_label_->setText(tr("(none)"));
		}
		else if(values_.discProfileFileName.isEmpty())
		{
			disc_profile_file_label_->setText(tr("(unnamed)"));
		}
		else
		{
			disc_profile_file_label_->setText(values_.discProfileFileName);
		}
	}
	if(nullptr!=disc_profile_action_btn_)
	{
		disc_profile_action_btn_->setVisible(mounted);
		disc_profile_action_btn_->setEnabled(mounted);
		if(true==haveProfile)
		{
			disc_profile_action_btn_->setText(tr("Delete profile"));
		}
		else
		{
			disc_profile_action_btn_->setText(tr("Create profile"));
		}
	}
	applyProfileEditAppearance();
	const bool mouseTabOn=mounted && haveProfile;
	if(nullptr!=tabs_ && nullptr!=mouse_integration_page_)
	{
		const int mouseIdx=tabs_->indexOf(mouse_integration_page_);
		if(0<=mouseIdx)
		{
			// Use the tab bar only. QTabWidget::setTabEnabled() also setEnabled(false)
			// on the page, which makes QComboBox item data read as 0 and corrupts
			// integration_mode on Apply/OK.
			if(nullptr!=tabs_->tabBar())
			{
				tabs_->tabBar()->setTabEnabled(mouseIdx,mouseTabOn);
			}
			// Keep the page widget itself enabled so combo itemData stays trustworthy
			// even if an older path left it disabled.
			mouse_integration_page_->setEnabled(true);
			if(true!=mouseTabOn && tabs_->currentIndex()==mouseIdx)
			{
				tabs_->setCurrentWidget(machine_page_);
			}
		}
	}
	if(nullptr!=mouse_coord_profile_page_)
	{
		mouse_coord_profile_page_->setEditorEnabled(mouseTabOn);
	}
}

void SettingsDialog::loadFromValues(const Values &values)
{
	loading_=true;
	const TownsQtCpuKind kind=TownsQtCpuKindClampToAllowed(
	    values.cpuKind,
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	const int model_index=TownsQtModelGroupClampToAllowed(
	    values.modelGroupIndex,
	    kind,
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	if(nullptr!=cpu_kind_)
	{
		const int row=cpu_kind_->findData(static_cast<int>(kind));
		if(0<=row)
		{
			cpu_kind_->setCurrentIndex(row);
		}
		updateModelComboItems();
		if(nullptr!=model_group_)
		{
			const int model_row=model_group_->findData(model_index);
			if(0<=model_row)
			{
				model_group_->setCurrentIndex(model_row);
			}
		}
	}
	values_.discMounted=values.discMounted;
	values_.discProfileAvailable=values.discProfileAvailable;
	values_.discProfileFileName=values.discProfileFileName;
	values_.profileCpuFrequencyMhz=values.profileCpuFrequencyMhz;
	values_.profileCpuCustomFrequencyMhz=values.profileCpuCustomFrequencyMhz;
	values_.profileCpuFastMode=values.profileCpuFastMode;
	values_.profileBootKeyComb=values.profileBootKeyComb;
	values_.profileMemSizeInMB=values.profileMemSizeInMB;
	values_.profileGamePort0=values.profileGamePort0;
	values_.profileGamePort1=values.profileGamePort1;
	values_.profileMaxButtonHoldTimeMs0=values.profileMaxButtonHoldTimeMs0;
	values_.profileMaxButtonHoldTimeMs1=values.profileMaxButtonHoldTimeMs1;
	values_.profileCpuHighFidelity=values.profileCpuHighFidelity;
	values_.profilePretend386DX=values.profilePretend386DX;
	values_.profileUseFPU=values.profileUseFPU;
	values_.profileFastScsi=values.profileFastScsi;
	values_.profileFastFd=values.profileFastFd;
	values_.profileMidiBoard=values.profileMidiBoard;
	values_.profileSingleDrive=values.profileSingleDrive;
	values_.profileHasMouseIntegration=values.profileHasMouseIntegration;
	values_.discProfileCreateRequested=false;
	// CPU/model always from global Values; shared fields from profile when present.
	loadSharedMachineWidgets(values,values.discProfileAvailable);
	updateMachineTabControls();
	display_scale_->setValue(values.displayScale);
	damper_wire_->setChecked(values.damperWireLine);
	scanline_15k_->setChecked(values.scanLineEffectIn15KHz);
	fullscreen_vsync_->setChecked(values.fullscreenVsync);
	windowed_vsync_->setChecked(values.windowedVsync);
	if(nullptr!=pcm_resample_sinc_)
	{
		pcm_resample_sinc_->setChecked(values.pcmResampleHighQuality);
	}
	if(nullptr!=sound_backend_)
	{
		const int backend_idx=sound_backend_->findData(values.audioBackend);
		sound_backend_->setCurrentIndex(0<=backend_idx ? backend_idx : 0);
		populateSoundDeviceCombo(values.audioBackend,values.audioDevice);
	}
	if(QAbstractButton *sprite_btn=sprite_group_->button(values.spriteTransferMode))
	{
		sprite_btn->setChecked(true);
	}
	if(nullptr!=midi_soundfont_edit_)
	{
		midi_soundfont_path_=values.midiSoundFont;
		if(midi_soundfont_path_.isEmpty())
		{
			midi_soundfont_edit_->clear();
			midi_soundfont_edit_->setToolTip(QString());
		}
		else
		{
			midi_soundfont_edit_->setText(QFileInfo(midi_soundfont_path_).fileName());
			midi_soundfont_edit_->setToolTip(midi_soundfont_path_);
		}
	}
	if(nullptr!=midi_output_)
	{
		populateMidiOutputCombo(values.midiOutput);
		populateMidiAlsaPortCombo(values.midiAlsaPort);
	}
	if(nullptr!=pcm_lpf_enabled_)
	{
		pcm_lpf_enabled_->setChecked(values.pcmLpfEnabled);
	}
	if(nullptr!=pcm_lpf_cutoff_)
	{
		pcm_lpf_cutoff_->setValue(std::clamp(values.pcmLpfCutoffHz,200,20000));
		pcm_lpf_cutoff_->setEnabled(values.pcmLpfEnabled);
	}
	if(nullptr!=idle_inhibit_)
	{
		idle_inhibit_->setChecked(values.waylandIdleInhibit);
	}
	if(nullptr!=snap_mouse_integration_)
	{
		snap_mouse_integration_->setChecked(values.snapMouseIntegration);
	}
	if(nullptr!=cdda_cache_during_data_read_)
	{
		cdda_cache_during_data_read_->setChecked(values.cddaCacheDuringDataRead);
	}
	if(nullptr!=cdda_cache_post_read_grace_sec_)
	{
		cdda_cache_post_read_grace_sec_->setValue(std::clamp(values.cddaCachePostReadGraceSec,1,60));
		cdda_cache_post_read_grace_sec_->setEnabled(values.cddaCacheDuringDataRead);
	}
	if(nullptr!=auto_diff_on_mos_unused_)
	{
		auto_diff_on_mos_unused_->setChecked(values.autoDifferentialOnMosUnused);
	}
	if(nullptr!=auto_resume_enabled_)
	{
		auto_resume_enabled_->setChecked(values.autoResumeEnabled);
	}
	updateProfileTabControls();
	updateFunctionTab();
	updateAudioTabMidiSection();
	loading_=false;
}

void SettingsDialog::applyToValues(Values &out) const
{
	// CPU / model always global.
	out.cpuKind=currentCpuKind();
	out.modelGroupIndex=currentModelGroupIndex();
	const bool profileMode=out.discProfileAvailable || values_.discProfileAvailable;
	// Preserve global machine fields when editing a profile (Basics widgets bind to profile*).
	if(true==profileMode)
	{
		out.cpuFrequencyMhz=values_.cpuFrequencyMhz;
		out.cpuCustomFrequencyMhz=values_.cpuCustomFrequencyMhz;
		out.cpuFastMode=values_.cpuFastMode;
		out.bootKeyComb=values_.bootKeyComb;
		out.memSizeInMB=values_.memSizeInMB;
		out.cpuHighFidelity=values_.cpuHighFidelity;
		out.pretend386DX=values_.pretend386DX;
		out.useFPU=values_.useFPU;
		out.fastScsi=values_.fastScsi;
		out.fastFd=values_.fastFd;
		out.midiBoard=values_.midiBoard;
		out.gamePort0=values_.gamePort0;
		out.gamePort1=values_.gamePort1;
		out.maxButtonHoldTimeMs0=values_.maxButtonHoldTimeMs0;
		out.maxButtonHoldTimeMs1=values_.maxButtonHoldTimeMs1;
		readSharedMachineWidgets(out,true);
	}
	else
	{
		readSharedMachineWidgets(out,false);
	}
	out.cpuKind=TownsQtCpuKindClampToAllowed(
	    currentCpuKind(),
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	out.modelGroupIndex=TownsQtModelGroupClampToAllowed(
	    currentModelGroupIndex(),
	    out.cpuKind,
	    sys_rom_profile_,
	    sys_rom_level_,
	    marty_ex_rom_present_);
	{
		const bool hi=TownsQtModelGroupEffectiveHighRes(out.modelGroupIndex,sys_rom_profile_);
		out.highResCrtc=hi;
		out.highResPcm=hi;
	}
	out.displayScale=display_scale_->value();
	out.autoScaling=false;
	out.maintainAspect=true;
	out.damperWireLine=damper_wire_->isChecked();
	out.scanLineEffectIn15KHz=scanline_15k_->isChecked();
	out.fullscreenVsync=fullscreen_vsync_->isChecked();
	out.windowedVsync=windowed_vsync_->isChecked();
	out.pcmResampleHighQuality=(nullptr!=pcm_resample_sinc_ && pcm_resample_sinc_->isChecked());
	if(nullptr!=sound_backend_)
	{
		out.audioBackend=sound_backend_->currentData().toString();
	}
	if(nullptr!=sound_device_)
	{
		out.audioDevice=sound_device_->currentData().toString();
	}
	// Volumes are owned by the Audio mixer dialog.
	out.fmVolumePercent=TownsQtSettings::fmVolumePercent();
	out.pcmVolumePercent=TownsQtSettings::pcmVolumePercent();
	out.cddaVolumePercent=TownsQtSettings::cddaVolumePercent();
	out.midiVolumePercent=TownsQtSettings::midiVolumePercent();
	if(nullptr!=midi_soundfont_edit_)
	{
		out.midiSoundFont=midi_soundfont_path_.trimmed();
	}
	if(nullptr!=midi_output_)
	{
		out.midiOutput=midi_output_->currentData().toString();
	}
	if(nullptr!=midi_alsa_port_)
	{
		out.midiAlsaPort=midi_alsa_port_->currentData().toString();
	}
	if(nullptr!=pcm_lpf_enabled_)
	{
		out.pcmLpfEnabled=pcm_lpf_enabled_->isChecked();
	}
	if(nullptr!=pcm_lpf_cutoff_)
	{
		out.pcmLpfCutoffHz=pcm_lpf_cutoff_->value();
	}
	out.waylandIdleInhibit=nullptr!=idle_inhibit_ && idle_inhibit_->isChecked();
	if(nullptr!=snap_mouse_integration_)
	{
		out.snapMouseIntegration=snap_mouse_integration_->isChecked();
	}
	if(nullptr!=cdda_cache_during_data_read_)
	{
		out.cddaCacheDuringDataRead=cdda_cache_during_data_read_->isChecked();
	}
	if(nullptr!=cdda_cache_post_read_grace_sec_)
	{
		out.cddaCachePostReadGraceSec=cdda_cache_post_read_grace_sec_->value();
	}
	out.spriteTransferMode=sprite_group_->checkedId();
	if(out.spriteTransferMode<0)
	{
		out.spriteTransferMode=0;
	}
	if(nullptr!=auto_diff_on_mos_unused_)
	{
		out.autoDifferentialOnMosUnused=auto_diff_on_mos_unused_->isChecked();
	}
	out.useDiscProfiles=true;
	out.autoResumeEnabled=nullptr!=auto_resume_enabled_ && auto_resume_enabled_->isChecked();
	out.discMounted=values_.discMounted;
	out.discProfileAvailable=values_.discProfileAvailable;
	out.discProfileFileName=values_.discProfileFileName;
	out.discProfileCreateRequested=values_.discProfileCreateRequested;
	out.profileHasMouseIntegration=values_.profileHasMouseIntegration;
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		out.hdd[slot]=values_.hdd[slot];
	}
	// Machine tab no longer exposes app-specific settings; keep NONE.
	out.appSpecificSetting=TOWNS_APPSPECIFIC_NONE;
}

void SettingsDialog::resetCurrentTabToDefaults()
{
	if(nullptr==tabs_)
	{
		return;
	}
	const QWidget *page=tabs_->currentWidget();
	loading_=true;
	if(page==machine_page_)
	{
		setCpuFrequencyWidgets(
		    default_values_.cpuFastMode,
		    default_values_.cpuFrequencyMhz,
		    default_values_.cpuCustomFrequencyMhz);
		setBootDriveWidget(default_values_.bootKeyComb);
		if(nullptr!=mem_size_mb_)
		{
			mem_size_mb_->setValue(default_values_.memSizeInMB);
		}
		if(nullptr!=cpu_fidelity_)
		{
			const int row=cpu_fidelity_->findData(default_values_.cpuHighFidelity ? 1 : 0);
			cpu_fidelity_->setCurrentIndex(0<=row ? row : 0);
		}
		if(nullptr!=pretend_386_)
		{
			pretend_386_->setChecked(default_values_.pretend386DX);
		}
		if(nullptr!=use_fpu_)
		{
			use_fpu_->setChecked(default_values_.useFPU);
		}
		if(nullptr!=fast_scsi_)
		{
			fast_scsi_->setChecked(default_values_.fastScsi);
		}
		if(nullptr!=fast_fd_)
		{
			fast_fd_->setChecked(default_values_.fastFd);
		}
		if(nullptr!=midi_board_)
		{
			midi_board_->setChecked(default_values_.midiBoard);
		}
		for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
		{
			values_.hdd[slot]=default_values_.hdd[slot];
		}
		if(nullptr!=cpu_kind_)
		{
			const TownsQtCpuKind kind=TownsQtCpuKindClampToAllowed(
			    default_values_.cpuKind,
			    sys_rom_profile_,
			    sys_rom_level_,
			    marty_ex_rom_present_);
			const int row=cpu_kind_->findData(static_cast<int>(kind));
			if(0<=row)
			{
				cpu_kind_->setCurrentIndex(row);
			}
			updateModelComboItems();
			if(nullptr!=model_group_)
			{
				const int model_index=TownsQtModelGroupClampToAllowed(
				    default_values_.modelGroupIndex,
				    kind,
				    sys_rom_profile_,
				    sys_rom_level_,
				    marty_ex_rom_present_);
				const int model_row=model_group_->findData(model_index);
				if(0<=model_row)
				{
					model_group_->setCurrentIndex(model_row);
				}
			}
			updateMachineTabControls();
		}
		if(nullptr!=gameport0_)
		{
			TownsQtGamePortOptions::PopulateCombo(gameport0_,default_values_.gamePort0);
		}
		if(nullptr!=gameport1_)
		{
			TownsQtGamePortOptions::PopulateCombo(gameport1_,default_values_.gamePort1);
		}
		if(nullptr!=max_button_hold0_)
		{
			max_button_hold0_->setValue(default_values_.maxButtonHoldTimeMs0);
		}
		if(nullptr!=max_button_hold1_)
		{
			max_button_hold1_->setValue(default_values_.maxButtonHoldTimeMs1);
		}
	}
	else if(page==drive_config_page_host_)
	{
		if(nullptr!=drive_config_page_)
		{
			drive_config_page_->setValues(default_drive_config_);
		}
	}
	else if(page==display_audio_page_)
	{
		display_scale_->setValue(default_values_.displayScale);
		damper_wire_->setChecked(default_values_.damperWireLine);
		scanline_15k_->setChecked(default_values_.scanLineEffectIn15KHz);
		fullscreen_vsync_->setChecked(default_values_.fullscreenVsync);
		windowed_vsync_->setChecked(default_values_.windowedVsync);
		if(QAbstractButton *btn=sprite_group_->button(default_values_.spriteTransferMode))
		{
			btn->setChecked(true);
		}
		if(nullptr!=pcm_resample_sinc_)
		{
			pcm_resample_sinc_->setChecked(default_values_.pcmResampleHighQuality);
		}
		if(nullptr!=sound_backend_)
		{
			const int backend_idx=sound_backend_->findData(default_values_.audioBackend);
			sound_backend_->setCurrentIndex(0<=backend_idx ? backend_idx : 0);
			populateSoundDeviceCombo(default_values_.audioBackend,default_values_.audioDevice);
		}
		if(nullptr!=pcm_lpf_enabled_)
		{
			pcm_lpf_enabled_->setChecked(default_values_.pcmLpfEnabled);
		}
		if(nullptr!=pcm_lpf_cutoff_)
		{
			pcm_lpf_cutoff_->setValue(default_values_.pcmLpfCutoffHz);
			pcm_lpf_cutoff_->setEnabled(default_values_.pcmLpfEnabled);
		}
		if(nullptr!=midi_soundfont_edit_)
		{
			midi_soundfont_path_=default_values_.midiSoundFont;
			if(midi_soundfont_path_.isEmpty())
			{
				midi_soundfont_edit_->clear();
				midi_soundfont_edit_->setToolTip(QString());
			}
			else
			{
				midi_soundfont_edit_->setText(QFileInfo(midi_soundfont_path_).fileName());
				midi_soundfont_edit_->setToolTip(midi_soundfont_path_);
			}
		}
		if(nullptr!=midi_output_)
		{
			populateMidiOutputCombo(default_values_.midiOutput);
			populateMidiAlsaPortCombo(default_values_.midiAlsaPort);
		}
	}
	else if(page==function_page_)
	{
		if(nullptr!=auto_resume_enabled_)
		{
			auto_resume_enabled_->setChecked(default_values_.autoResumeEnabled);
		}
		if(nullptr!=idle_inhibit_)
		{
			idle_inhibit_->setChecked(default_values_.waylandIdleInhibit);
		}
		if(nullptr!=snap_mouse_integration_)
		{
			snap_mouse_integration_->setChecked(default_values_.snapMouseIntegration);
		}
		values_.snapMouseWarmupFrames=default_values_.snapMouseWarmupFrames;
		if(nullptr!=cdda_cache_during_data_read_)
		{
			cdda_cache_during_data_read_->setChecked(default_values_.cddaCacheDuringDataRead);
		}
		if(nullptr!=cdda_cache_post_read_grace_sec_)
		{
			cdda_cache_post_read_grace_sec_->setValue(default_values_.cddaCachePostReadGraceSec);
		}
		if(nullptr!=auto_diff_on_mos_unused_)
		{
			auto_diff_on_mos_unused_->setChecked(default_values_.autoDifferentialOnMosUnused);
		}
		updateFunctionTab();
	}
	loading_=false;
	updateAudioTabMidiSection();
	markDirty();
}
