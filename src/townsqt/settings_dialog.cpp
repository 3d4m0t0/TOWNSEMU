#include "settings_dialog.h"
#include "townsqt_app_profile.h"
#include "townsqt_gameport_options.h"
#include "townsqt_miniaudio_devices.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_rom_availability.h"
#include "townsqt_wayland_idle_inhibit.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
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
constexpr int kModelDescriptionLineCount=5;

int ModelDescriptionFixedHeight(const QWidget *widget)
{
	const QFontMetrics fm(widget->font());
	return fm.lineSpacing()*kModelDescriptionLineCount+4;
}

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

void PopulateSoundBackendCombo(QComboBox *combo)
{
	if(nullptr==combo)
	{
		return;
	}
	combo->clear();
	combo->addItem(QCoreApplication::translate("SettingsDialog","自動"),QString());
	combo->addItem(QStringLiteral("PulseAudio"),QStringLiteral("pulse"));
	combo->addItem(QStringLiteral("ALSA"),QStringLiteral("alsa"));
	combo->addItem(QStringLiteral("JACK"),QStringLiteral("jack"));
}
}

SettingsDialog::Values SettingsDialog::defaultValues()
{
	Values v;
	v.cpuFrequencyMhz=33;
	v.memSizeInMB=4;
	v.cpuHighFidelity=false;
	v.pretend386DX=false;
	v.useFPU=false;
	v.modelGroupIndex=TownsQtModelGroupDefaultIndex();
	v.displayScale=1;
	v.autoScaling=false;
	v.maintainAspect=true;
	v.damperWireLine=false;
	v.scanLineEffectIn15KHz=false;
	v.fullscreenVsync=true;
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
	v.differentialMouseIntegration=false;
	v.snapMouseIntegration=false;
	v.snapMouseWarmupFrames=60;
	v.mouseMinX=TownsStartParameters::DEFAULT_MOUSE_MINX;
	v.mouseMinY=TownsStartParameters::DEFAULT_MOUSE_MINY;
	v.mouseMaxX=TownsStartParameters::DEFAULT_MOUSE_MAXX;
	v.mouseMaxY=TownsStartParameters::DEFAULT_MOUSE_MAXY;
	v.appSpecificSetting=TOWNS_APPSPECIFIC_NONE;
	return v;
}

SettingsDialog::SettingsDialog(const Values &initial,const QString &romDir,QWidget *parent)
	: QDialog(parent),
	  values_(initial),
	  default_values_(defaultValues()),
	  rom_dir_(romDir.isEmpty() ? TownsQtPaths::romsDir() : romDir),
	  marty_ex_rom_present_(TownsQtRomAvailability::MartyExRomPresent(rom_dir_)),
	  sys_rom_profile_(TownsQtRomAvailability::ClassifySysRom(rom_dir_)),
	  marty_model_index_(TownsQtModelGroupMartyIndex())
{
	setWindowTitle(tr("設定"));
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
	loadFromValues(values_);
	updateRestrictedModelComboItems();
	updateSysRomInfoLabel();
	updateMachineTabControls();
	setApplyEnabled(false);
	applyFixedDialogSize();
}

SettingsDialog::Values SettingsDialog::values() const
{
	return values_;
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
	constexpr int kTabWidthFudge=8;
	const int width=bar->sizeHint().width()+margins.left()+margins.right()+kTabWidthFudge;
	bar->setUsesScrollButtons(false);

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
		max_height=std::max(max_height,sizeHint().height());
	}
	tabs_->setCurrentIndex(0<=prev_tab ? prev_tab : 0);
	setFixedSize(width,max_height);
}

void SettingsDialog::buildUi()
{
	auto *layout=new QVBoxLayout(this);
	layout->setContentsMargins(6,6,6,6);
	layout->setSpacing(4);

	tabs_=new QTabWidget(this);
	if(QTabBar *bar=tabs_->tabBar())
	{
		bar->setExpanding(false);
		bar->setUsesScrollButtons(false);
	}
	layout->addWidget(tabs_);

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		auto *main_grid=new QGridLayout();
		main_grid->setContentsMargins(0,0,0,0);
		main_grid->setHorizontalSpacing(16);
		main_grid->setVerticalSpacing(4);

		auto *model_header_widget=new QWidget(page);
		auto *model_header_layout=new QHBoxLayout(model_header_widget);
		model_header_layout->setContentsMargins(0,0,0,0);
		model_header_layout->setSpacing(8);
		auto *model_caption=new QLabel(tr("モデル"),model_header_widget);
		sys_rom_info_label_=new QLabel(model_header_widget);
		sys_rom_info_label_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		sys_rom_info_label_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
		QFont info_font=sys_rom_info_label_->font();
		if(0<info_font.pointSize())
		{
			info_font.setPointSize(std::max(7,info_font.pointSize()-1));
		}
		sys_rom_info_label_->setFont(info_font);
		sys_rom_info_label_->setForegroundRole(QPalette::PlaceholderText);
		model_header_layout->addWidget(model_caption);
		model_header_layout->addWidget(sys_rom_info_label_,1);
		main_grid->addWidget(model_header_widget,0,0);

		model_group_=new QComboBox(page);
		model_group_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		for(int i=0; i<TownsQtModelGroupCount(); ++i)
		{
			model_group_->addItem(
			    QString::fromUtf8(TownsQtModelGroupAt(i).label),
			    i);
		}
		main_grid->addWidget(model_group_,1,0);
		connect(model_group_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			updateModelDescription();
			updateMachineTabControls();
			markDirty();
		});

		auto *mem_widget=new QWidget(page);
		auto *mem_layout=new QHBoxLayout(mem_widget);
		mem_layout->setContentsMargins(0,0,0,0);
		mem_layout->setSpacing(8);
		mem_label_=new QLabel(tr("メモリ"),mem_widget);
		mem_size_mb_=new QSpinBox(mem_widget);
		mem_size_mb_->setRange(1,64);
		mem_size_mb_->setSuffix(tr(" MB"));
		mem_layout->addStretch();
		mem_layout->addWidget(mem_label_);
		mem_layout->addWidget(mem_size_mb_);
		mem_layout->addStretch();
		main_grid->addWidget(mem_widget,1,1,Qt::AlignVCenter);

		model_description_=new QLabel(page);
		model_description_->setWordWrap(true);
		model_description_->setTextFormat(Qt::RichText);
		model_description_->setAlignment(Qt::AlignTop|Qt::AlignLeft);
		model_description_->setFixedHeight(ModelDescriptionFixedHeight(page));
		model_description_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		main_grid->addWidget(model_description_,2,0,1,1);

		fidelity_box_=new QGroupBox(tr("CPUの再現性"),page);
		ShrinkGroupBox(fidelity_box_);
		fidelity_box_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		auto *fidelity_layout=new QHBoxLayout(fidelity_box_);
		CompactGroupBoxLayout(fidelity_layout);
		fidelity_group_=new QButtonGroup(fidelity_box_);
		auto *fidelity_mid=new QRadioButton(tr("MID"),fidelity_box_);
		auto *fidelity_high=new QRadioButton(tr("HIGH"),fidelity_box_);
		fidelity_group_->addButton(fidelity_mid,0);
		fidelity_group_->addButton(fidelity_high,1);
		fidelity_layout->addStretch(1);
		fidelity_layout->addWidget(fidelity_mid);
		fidelity_layout->addStretch(1);
		fidelity_layout->addWidget(fidelity_high);
		fidelity_layout->addStretch(1);
		main_grid->addWidget(fidelity_box_,2,1,Qt::AlignBottom);

		main_grid->setColumnStretch(0,1);
		main_grid->setColumnStretch(1,1);
		main_grid->setRowStretch(2,1);
		v->addLayout(main_grid);
		v->addWidget(MakeHorizontalSeparator(page));

		opt_grid_widget_=new QWidget(page);
		auto *opt_grid=new QGridLayout(opt_grid_widget_);
		CompactGrid(opt_grid);
		opt_grid->setColumnStretch(0,1);
		opt_grid->setColumnStretch(1,1);
		pretend_386_=new QCheckBox(tr("pretend386DX"),opt_grid_widget_);
		use_fpu_=new QCheckBox(tr("80386FPU"),opt_grid_widget_);
		fast_scsi_=new QCheckBox(tr("FAST SCSI"),opt_grid_widget_);
		midi_board_=new QCheckBox(tr("MIDI ボード"),opt_grid_widget_);
		opt_grid->addWidget(pretend_386_,0,0);
		opt_grid->addWidget(use_fpu_,0,1);
		opt_grid->addWidget(fast_scsi_,1,0);
		opt_grid->addWidget(midi_board_,1,1);
		v->addWidget(opt_grid_widget_);
		connect(midi_board_,&QCheckBox::toggled,this,&SettingsDialog::updateAudioTabMidiSection);
		v->addWidget(MakeHorizontalSeparator(page));

		auto *app_row=new QHBoxLayout();
		app_row->setSpacing(8);
		app_row->addWidget(new QLabel(tr("アプリ別特殊設定"),page));
		app_specific_=new QComboBox(page);
		app_specific_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		for(int i=0; i<TownsQtAppProfileCount(); ++i)
		{
			app_specific_->addItem(
			    QString::fromUtf8(TownsQtAppProfileAt(i).label),
			    TownsQtAppProfileId(i));
		}
		app_row->addWidget(app_specific_,1);
		v->addLayout(app_row);
		connect(app_specific_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
			updateAppSpecificDescription();
			markDirty();
		});

		app_specific_description_=MakeIndentedNote(page,QString());
		v->addWidget(app_specific_description_);

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("変更は［適用］または［OK］で保存され、設定を反映するためエミュレータが再起動されます。")));
		machine_page_=page;
		tabs_->addTab(page,tr("マシン"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		auto *joystick_grid=new QGridLayout();
		joystick_grid->setHorizontalSpacing(16);
		joystick_grid->setVerticalSpacing(4);
		joystick_grid->setColumnStretch(0,4);
		joystick_grid->setColumnStretch(1,1);
		joystick_grid->addWidget(new QLabel(tr("ジョイパッド／マウス端子"),page),0,0);
		joystick_grid->addWidget(new QLabel(tr("最大ボタン押下時間"),page),0,1);

		gameport0_=new QComboBox(page);
		gameport1_=new QComboBox(page);
		gameport0_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		gameport1_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		TownsQtGamePortOptions::PopulateCombo(gameport0_,TOWNS_GAMEPORTEMU_PHYSICAL0);
		TownsQtGamePortOptions::PopulateCombo(gameport1_,TOWNS_GAMEPORTEMU_MOUSE);

		max_button_hold0_=new QSpinBox(page);
		max_button_hold1_=new QSpinBox(page);
		for(QSpinBox *spin : {max_button_hold0_,max_button_hold1_})
		{
			spin->setRange(0,9999);
			spin->setSuffix(tr(" ms"));
			spin->setSpecialValueText(tr("任意"));
		}

		auto *port0_row=new QHBoxLayout();
		port0_row->setSpacing(8);
		port0_row->addWidget(new QLabel(QStringLiteral("0"),page));
		port0_row->addWidget(gameport0_,1);
		joystick_grid->addLayout(port0_row,1,0);

		auto *hold0_row=new QHBoxLayout();
		hold0_row->setSpacing(8);
		hold0_row->addWidget(new QLabel(tr("ボタン0"),page));
		hold0_row->addWidget(max_button_hold0_);
		joystick_grid->addLayout(hold0_row,1,1);

		auto *port1_row=new QHBoxLayout();
		port1_row->setSpacing(8);
		port1_row->addWidget(new QLabel(QStringLiteral("1"),page));
		port1_row->addWidget(gameport1_,1);
		joystick_grid->addLayout(port1_row,2,0);

		auto *hold1_row=new QHBoxLayout();
		hold1_row->setSpacing(8);
		hold1_row->addWidget(new QLabel(tr("ボタン1"),page));
		hold1_row->addWidget(max_button_hold1_);
		joystick_grid->addLayout(hold1_row,2,1);

		v->addLayout(joystick_grid);

		auto *speed_box=new QGroupBox(tr("マウス移動速度"),page);
		ShrinkGroupBox(speed_box);
		auto *speed_layout=new QHBoxLayout(speed_box);
		CompactGroupBoxLayout(speed_layout);
		mouse_speed_slider_=new QSlider(Qt::Horizontal,speed_box);
		mouse_speed_slider_->setRange(32,256);
		mouse_speed_value_=new QLabel(speed_box);
		mouse_speed_value_->setMinimumWidth(28);
		mouse_speed_value_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		speed_layout->addWidget(mouse_speed_slider_,1);
		speed_layout->addWidget(mouse_speed_value_);
		connect(mouse_speed_slider_,&QSlider::valueChanged,this,[this](int value){
			if(nullptr!=mouse_speed_value_)
			{
				mouse_speed_value_->setText(QString::number(value));
			}
		});
		v->addWidget(speed_box);

		auto *range_box=new QGroupBox(tr("マウス座標範囲"),page);
		ShrinkGroupBox(range_box);
		auto *range_layout=new QGridLayout(range_box);
		CompactGrid(range_layout);
		mouse_vram_offset_=new QCheckBox(tr("VRAM オフセット考慮"),range_box);
		diff_mouse_integration_=new QCheckBox(tr("差分マウス統合"),range_box);
		mouse_min_x_=new QSpinBox(range_box);
		mouse_min_y_=new QSpinBox(range_box);
		mouse_max_x_=new QSpinBox(range_box);
		mouse_max_y_=new QSpinBox(range_box);
		for(QSpinBox *spin : {mouse_min_x_,mouse_min_y_,mouse_max_x_,mouse_max_y_})
		{
			spin->setRange(-32768,32767);
		}
		range_layout->addWidget(mouse_vram_offset_,0,0);
		range_layout->addWidget(new QLabel(tr("MinX"),range_box),0,1);
		range_layout->addWidget(mouse_min_x_,0,2);
		range_layout->addWidget(new QLabel(tr("MinY"),range_box),0,3);
		range_layout->addWidget(mouse_min_y_,0,4);
		range_layout->addWidget(diff_mouse_integration_,1,0);
		range_layout->addWidget(new QLabel(tr("MaxX"),range_box),1,1);
		range_layout->addWidget(mouse_max_x_,1,2);
		range_layout->addWidget(new QLabel(tr("MaxY"),range_box),1,3);
		range_layout->addWidget(mouse_max_y_,1,4);
		range_layout->setColumnStretch(0,1);
		v->addWidget(range_box);

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("変更は［適用］または［OK］ですべて即時に反映されます。")));
		peripheral_page_=page;
		tabs_->addTab(page,tr("周辺機器"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		auto *scale_row=new QHBoxLayout();
		scale_row->setSpacing(12);
		display_scale_=new QSpinBox(page);
		display_scale_->setRange(1,8);
		display_scale_->setSuffix(tr("x"));
		scale_row->addWidget(new QLabel(tr("ウインドウ倍率:"),page));
		scale_row->addWidget(display_scale_);
		scale_row->addSpacing(16);
		scale_row->addWidget(new QLabel(tr("スプライト転送"),page));
		sprite_group_=new QButtonGroup(page);
		auto *sprite_standard=new QRadioButton(tr("標準"),page);
		auto *sprite_double=new QRadioButton(tr("倍速"),page);
		auto *sprite_max=new QRadioButton(tr("最大"),page);
		sprite_group_->addButton(sprite_standard,0);
		sprite_group_->addButton(sprite_double,1);
		sprite_group_->addButton(sprite_max,2);
		scale_row->addWidget(sprite_standard);
		scale_row->addWidget(sprite_double);
		scale_row->addWidget(sprite_max);
		scale_row->addStretch();
		v->addLayout(scale_row);

		auto *video_grid=new QGridLayout();
		CompactGrid(video_grid);
		auto_scale_=new QCheckBox(tr("ウィンドウに合わせて倍率調整"),page);
		maintain_aspect_=new QCheckBox(tr("縦横比維持"),page);
		scanline_15k_=new QCheckBox(tr("15kHz 走査線効果"),page);
		damper_wire_=new QCheckBox(tr("ダンパーワイヤー線"),page);
		fullscreen_vsync_=new QCheckBox(tr("全画面時 Vsync同期"),page);
		video_grid->addWidget(auto_scale_,0,0);
		video_grid->addWidget(maintain_aspect_,0,1);
		video_grid->addWidget(scanline_15k_,1,0);
		video_grid->addWidget(damper_wire_,1,1);
		video_grid->addWidget(fullscreen_vsync_,2,0);
		video_grid->setColumnStretch(0,1);
		video_grid->setColumnStretch(1,1);
		v->addLayout(video_grid);

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("変更は［適用］または［OK］ですべて即時に反映されます。")));
		video_page_=page;
		tabs_->addTab(page,tr("映像"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		auto make_volume_row=[&](const QString &label,QSlider *&slider,QLabel *&value_label){
			auto *row=new QHBoxLayout();
			row->setSpacing(8);
			auto *name=new QLabel(label,page);
			name->setMinimumWidth(48);
			row->addWidget(name);
			slider=new QSlider(Qt::Horizontal,page);
			slider->setRange(0,100);
			value_label=new QLabel(page);
			value_label->setMinimumWidth(36);
			value_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
			row->addWidget(slider,1);
			row->addWidget(value_label);
			v->addLayout(row);
			connect(slider,&QSlider::valueChanged,this,[value_label](int value){
				if(nullptr!=value_label)
				{
					value_label->setText(QStringLiteral("%1%").arg(value));
				}
			});
		};

		auto *volume_heading=new QLabel(tr("音量"),page);
		volume_heading->setAlignment(Qt::AlignCenter);
		v->addWidget(volume_heading);
		make_volume_row(tr("FM"),fm_volume_slider_,fm_volume_value_);
		make_volume_row(tr("PCM"),pcm_volume_slider_,pcm_volume_value_);
		make_volume_row(tr("CDDA"),cdda_volume_slider_,cdda_volume_value_);
		make_volume_row(tr("MIDI"),midi_volume_slider_,midi_volume_value_);

		auto *pcm_grid=new QGridLayout();
		CompactGrid(pcm_grid);
		pcm_resample_sinc_=new QCheckBox(tr("PCMリサンプルでsinc補間を使う"),page);
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
		    MakeIndentedNote(page,tr("sinc補間を使わない場合は線形補間を使用します。")),
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
		device_row->addWidget(new QLabel(tr("出力デバイス:"),page));
		device_row->addWidget(sound_device_,1);
		device_row->addWidget(new QLabel(tr("バックエンド:"),page));
		device_row->addWidget(sound_backend_);
		v->addLayout(device_row);
		connect(sound_backend_,qOverload<int>(&QComboBox::currentIndexChanged),this,&SettingsDialog::onAudioBackendChanged);
		populateSoundDeviceCombo(QString());

		auto *midi_output_row=new QHBoxLayout();
		midi_output_row->setSpacing(8);
		midi_output_row->addWidget(new QLabel(tr("MIDI出力:"),page));
		midi_output_=new QComboBox(page);
		midi_output_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
		midi_output_row->addWidget(midi_output_,1);
		v->addLayout(midi_output_row);

		midi_fluidsynth_panel_=new QWidget(page);
		auto *sf_layout=new QVBoxLayout(midi_fluidsynth_panel_);
		sf_layout->setContentsMargins(0,0,0,0);
		sf_layout->setSpacing(3);
		auto *sf_row=new QHBoxLayout();
		sf_row->setSpacing(8);
		sf_row->addWidget(new QLabel(tr("サウンドフォント:"),midi_fluidsynth_panel_));
		midi_soundfont_edit_=new QLineEdit(midi_fluidsynth_panel_);
		midi_soundfont_edit_->setReadOnly(true);
		midi_soundfont_edit_->setPlaceholderText(tr("（未指定）"));
		midi_soundfont_browse_=new QPushButton(tr("参照…"),midi_fluidsynth_panel_);
		sf_row->addWidget(midi_soundfont_edit_,1);
		sf_row->addWidget(midi_soundfont_browse_);
		sf_layout->addLayout(sf_row);
		sf_layout->addWidget(MakeIndentedNote(
		    midi_fluidsynth_panel_,
		    tr("FluidSynth で使用する SoundFont（.sf2）ファイルを指定します。未指定の場合は環境変数やシステム既定のパスを探します。")));
		v->addWidget(midi_fluidsynth_panel_);
		connect(midi_soundfont_browse_,&QPushButton::clicked,this,&SettingsDialog::browseMidiSoundFont);

		midi_alsa_note_=MakeIndentedNote(
		    page,
		    tr("ALSA シーケンサで MIDI を出力します。外部シンセやソフトウェア音源へ接続するには、"
		       "パッチベイ（aconnect 等）で TownsEMU の出力ポートを宛先ポートに接続してください。"));
		midi_alsa_note_->setVisible(false);
		v->addWidget(midi_alsa_note_);

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("変更は［適用］または［OK］ですべて即時に反映されます。")));
		audio_page_=page;
		tabs_->addTab(page,tr("音声"));
	}

	{
		auto *page=new QWidget(tabs_);
		auto *v=new QVBoxLayout(page);
		CompactVBox(v);

		idle_inhibit_=new QCheckBox(tr("ディスプレイのアイドルを抑制"),page);
		v->addWidget(idle_inhibit_);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("Wayland セッションで画面の自動消灯や暗転を防ぎます。")));

		snap_mouse_integration_=new QCheckBox(tr("マウス即時統合（テスト）"),page);
		v->addWidget(snap_mouse_integration_);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("ゲストのマウス座標をホストに即座に合わせるテスト機能です。\n"
		       "有効にした直後はウォームアップ期間、段階的統合を行ってから即時統合に切り替わります。")));

		auto *warmup_row=new QHBoxLayout();
		warmup_row->setContentsMargins(22,0,0,0);
		warmup_row->addWidget(new QLabel(tr("即時統合ウォームアップ:"),page));
		snap_mouse_warmup_=new QSpinBox(page);
		snap_mouse_warmup_->setRange(0,600);
		snap_mouse_warmup_->setSuffix(tr(" フレーム"));
		warmup_row->addWidget(snap_mouse_warmup_);
		warmup_row->addStretch();
		v->addLayout(warmup_row);
		v->addWidget(MakeIndentedNote(
		    page,
		    tr("0 にするとウォームアップなしで、最初から即時統合します。")));
		connect(snap_mouse_integration_,&QCheckBox::toggled,snap_mouse_warmup_,&QWidget::setEnabled);

		auto *separator=new QFrame(page);
		separator->setFrameShape(QFrame::HLine);
		separator->setFrameShadow(QFrame::Sunken);
		v->addWidget(separator);

		FinishTabPage(v,MakeTabFooterNote(
		    page,
		    tr("変更は［適用］または［OK］ですべて即時に反映されます。")));
		function_page_=page;
		tabs_->addTab(page,tr("機能"));
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
	auto *defaults_button=new QPushButton(tr("標準設定(&D)"),this);
	connect(defaults_button,&QPushButton::clicked,this,&SettingsDialog::resetCurrentTabToDefaults);
	button_row->addWidget(defaults_button);
	button_row->addStretch();
	button_row->addWidget(buttons);
	layout->addLayout(button_row);

	connectDirtyTracking();
	connect(tabs_,&QTabWidget::currentChanged,this,[this](int index){
		if(nullptr!=tabs_ && tabs_->widget(index)==audio_page_)
		{
			updateAudioTabMidiSection();
		}
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
		sound_device_->addItem(tr("既定"),QString());
		sound_device_->setCurrentIndex(0);
		return;
	}

	const QString keep=select_device.isNull() ? sound_device_->currentData().toString() : select_device;
	sound_device_->clear();
	sound_device_->addItem(tr("既定"),QString());
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
		sound_device_->addItem(tr("%1 (見つかりません)").arg(keep),keep);
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
		    tr("Wayland の idle-inhibit に対応したセッションでのみ利用できます。"));
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
	midi_output_->clear();
#if defined(__linux__)
	const MidiBackendProbe::Kind backend=MidiBackendProbe::PreferredBackend();
	if(MidiBackendProbe::Kind::FluidSynth==backend)
	{
		midi_output_->addItem(tr("FluidSynth（内蔵）"),QStringLiteral("fluidsynth"));
	}
	else if(MidiBackendProbe::Kind::AlsaSeq==backend)
	{
		midi_output_->addItem(tr("（接続先を選択）"),QString());
		for(const auto &entry : ListAlsaMidiOutputDestinations())
		{
			midi_output_->addItem(
			    QString::fromStdString(entry.label),
			    QString::fromStdString(entry.id));
		}
	}
	else
	{
		midi_output_->addItem(tr("（利用可能な MIDI 出力なし）"),QString());
		midi_output_->setEnabled(false);
		return;
	}
	midi_output_->setEnabled(true);
	const int idx=midi_output_->findData(keep);
	if(0<=idx)
	{
		midi_output_->setCurrentIndex(idx);
	}
	else
	{
		midi_output_->setCurrentIndex(0);
	}
#else
	midi_output_->addItem(tr("（非対応）"),QString());
	midi_output_->setEnabled(false);
#endif
}

void SettingsDialog::updateAudioTabMidiSection()
{
	const bool midi_board=nullptr!=midi_board_ && midi_board_->isChecked();
#if defined(__linux__)
	const MidiBackendProbe::Kind backend=MidiBackendProbe::PreferredBackend();
	const bool fluidsynth=(MidiBackendProbe::Kind::FluidSynth==backend);
	const bool alsa_seq=(MidiBackendProbe::Kind::AlsaSeq==backend);
	const bool midi_vol_enabled=midi_board && fluidsynth;
	if(nullptr!=midi_volume_slider_)
	{
		midi_volume_slider_->setEnabled(midi_vol_enabled);
	}
	if(nullptr!=midi_volume_value_)
	{
		midi_volume_value_->setEnabled(midi_vol_enabled);
	}
	if(nullptr!=midi_output_)
	{
		const QString keep=midi_output_->currentData().toString();
		QSignalBlocker blocker(midi_output_);
		midi_output_->setEnabled(midi_board && (fluidsynth || alsa_seq));
		populateMidiOutputCombo(keep);
	}
		if(nullptr!=midi_fluidsynth_panel_)
		{
			midi_fluidsynth_panel_->setVisible(midi_board && fluidsynth);
		}
		if(nullptr!=midi_soundfont_edit_)
		{
			midi_soundfont_edit_->setEnabled(midi_board && fluidsynth);
		}
		if(nullptr!=midi_soundfont_browse_)
		{
			midi_soundfont_browse_->setEnabled(midi_board && fluidsynth);
		}
		if(nullptr!=midi_alsa_note_)
	{
		midi_alsa_note_->setVisible(midi_board && alsa_seq);
	}
#else
	if(nullptr!=midi_volume_slider_)
	{
		midi_volume_slider_->setEnabled(false);
	}
	if(nullptr!=midi_volume_value_)
	{
		midi_volume_value_->setEnabled(false);
	}
	if(nullptr!=midi_output_)
	{
		midi_output_->setEnabled(false);
	}
	if(nullptr!=midi_fluidsynth_panel_)
	{
		midi_fluidsynth_panel_->setVisible(false);
	}
	if(nullptr!=midi_alsa_note_)
	{
		midi_alsa_note_->setVisible(false);
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
	    tr("SoundFont を選択"),
	    start_dir,
	    tr("SoundFont (*.sf2);;すべてのファイル (*)"));
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

void SettingsDialog::updateModelDescription()
{
	if(nullptr==model_group_ || nullptr==model_description_)
	{
		return;
	}
	const int index=model_group_->currentData().toInt();
	model_description_->setText(TownsQtModelGroupDescription(index));
}

void SettingsDialog::updateRestrictedModelComboItems()
{
	if(nullptr==model_group_)
	{
		return;
	}
	auto *model=qobject_cast<QStandardItemModel*>(model_group_->model());
	if(nullptr==model)
	{
		return;
	}
	for(int i=0; i<TownsQtModelGroupCount(); ++i)
	{
		QStandardItem *item=model->item(i);
		if(nullptr==item)
		{
			continue;
		}
		bool enabled=TownsQtRomAvailability::ModelGroupAllowedForSysRom(
		    i,
		    sys_rom_profile_,
		    marty_ex_rom_present_);
		item->setEnabled(enabled);
	}
}

bool SettingsDialog::isModelSelectionAllowed(int model_index) const
{
	return TownsQtRomAvailability::ModelGroupAllowedForSysRom(
	    model_index,
	    sys_rom_profile_,
	    marty_ex_rom_present_);
}

void SettingsDialog::updateSysRomInfoLabel()
{
	if(nullptr==sys_rom_info_label_)
	{
		return;
	}
	sys_rom_info_label_->setText(TownsQtRomAvailability::SysRomSummary(rom_dir_));
}

void SettingsDialog::updateMachineTabControls()
{
	updateRestrictedModelComboItems();
	if(nullptr==model_group_)
	{
		return;
	}

	int model_index=model_group_->currentData().toInt();
	if(model_index<0)
	{
		model_index=model_group_->currentIndex();
	}
	const bool marty_mode=TownsQtModelGroupIsMarty(model_index);
	const bool editable=!marty_mode;

	if(nullptr!=mem_label_)
	{
		mem_label_->setEnabled(editable);
	}
	if(nullptr!=mem_size_mb_)
	{
		const int max_mem=TownsQtModelGroupMaxMemMb(model_index);
		mem_size_mb_->setMaximum(max_mem);
		if(max_mem<mem_size_mb_->value())
		{
			mem_size_mb_->setValue(max_mem);
		}
		mem_size_mb_->setEnabled(editable);
	}
	if(nullptr!=pretend_386_)
	{
		pretend_386_->setEnabled(editable);
	}
	if(nullptr!=use_fpu_)
	{
		use_fpu_->setEnabled(editable);
	}
	if(nullptr!=fast_scsi_)
	{
		fast_scsi_->setEnabled(editable);
	}
	if(nullptr!=midi_board_)
	{
		midi_board_->setEnabled(editable);
	}
	if(nullptr!=opt_grid_widget_)
	{
		opt_grid_widget_->setEnabled(editable);
	}
}

void SettingsDialog::updateAppSpecificDescription()
{
	if(nullptr==app_specific_ || nullptr==app_specific_description_)
	{
		return;
	}
	const int index=app_specific_->currentIndex();
	app_specific_description_->setText(TownsQtAppProfileDescription(index));
}

void SettingsDialog::loadFromValues(const Values &values)
{
	loading_=true;
	if(nullptr!=mem_size_mb_)
	{
		const int max_mem=TownsQtModelGroupMaxMemMb(values.modelGroupIndex);
		mem_size_mb_->setMaximum(max_mem);
		mem_size_mb_->setValue(std::clamp(values.memSizeInMB,1,max_mem));
	}
	if(nullptr!=fidelity_group_)
	{
		if(QAbstractButton *btn=fidelity_group_->button(values.cpuHighFidelity ? 1 : 0))
		{
			btn->setChecked(true);
		}
	}
	if(nullptr!=pretend_386_)
	{
		pretend_386_->setChecked(values.pretend386DX);
	}
	if(nullptr!=use_fpu_)
	{
		use_fpu_->setChecked(values.useFPU);
	}
	if(nullptr!=fast_scsi_)
	{
		fast_scsi_->setChecked(values.fastScsi);
	}
	if(nullptr!=midi_board_)
	{
		midi_board_->setChecked(values.midiBoard);
	}
	if(nullptr!=model_group_)
	{
		int model_index=std::clamp(
		    values.modelGroupIndex,
		    0,
		    TownsQtModelGroupCount()-1);
		if(false==isModelSelectionAllowed(model_index))
		{
			model_index=TownsQtRomAvailability::PreferredModelGroupForSysRom(sys_rom_profile_);
		}
		model_group_->setCurrentIndex(model_group_->findData(model_index));
		updateModelDescription();
		updateMachineTabControls();
	}
	display_scale_->setValue(values.displayScale);
	auto_scale_->setChecked(values.autoScaling);
	maintain_aspect_->setChecked(values.maintainAspect);
	damper_wire_->setChecked(values.damperWireLine);
	scanline_15k_->setChecked(values.scanLineEffectIn15KHz);
	fullscreen_vsync_->setChecked(values.fullscreenVsync);
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
	if(nullptr!=fm_volume_slider_)
	{
		fm_volume_slider_->setValue(std::clamp(values.fmVolumePercent,0,100));
	}
	if(nullptr!=pcm_volume_slider_)
	{
		pcm_volume_slider_->setValue(std::clamp(values.pcmVolumePercent,0,100));
	}
	if(nullptr!=cdda_volume_slider_)
	{
		cdda_volume_slider_->setValue(std::clamp(values.cddaVolumePercent,0,100));
	}
	if(nullptr!=midi_volume_slider_)
	{
		midi_volume_slider_->setValue(std::clamp(values.midiVolumePercent,0,100));
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
	if(nullptr!=snap_mouse_warmup_)
	{
		snap_mouse_warmup_->setValue(std::clamp(values.snapMouseWarmupFrames,0,600));
		snap_mouse_warmup_->setEnabled(values.snapMouseIntegration);
	}
	if(nullptr!=gameport0_)
	{
		TownsQtGamePortOptions::PopulateCombo(gameport0_,values.gamePort0);
	}
	if(nullptr!=gameport1_)
	{
		TownsQtGamePortOptions::PopulateCombo(gameport1_,values.gamePort1);
	}
	if(nullptr!=max_button_hold0_)
	{
		max_button_hold0_->setValue(std::max(0,values.maxButtonHoldTimeMs0));
	}
	if(nullptr!=max_button_hold1_)
	{
		max_button_hold1_->setValue(std::max(0,values.maxButtonHoldTimeMs1));
	}
	if(nullptr!=mouse_speed_slider_)
	{
		const int speed=std::clamp(values.mouseIntegrationSpeed,32,256);
		mouse_speed_slider_->setValue(speed);
		if(nullptr!=mouse_speed_value_)
		{
			mouse_speed_value_->setText(QString::number(speed));
		}
	}
	if(nullptr!=mouse_vram_offset_)
	{
		mouse_vram_offset_->setChecked(values.considerVRAMOffsetInMouseIntegration);
	}
	if(nullptr!=diff_mouse_integration_)
	{
		diff_mouse_integration_->setChecked(values.differentialMouseIntegration);
	}
	if(nullptr!=mouse_min_x_)
	{
		mouse_min_x_->setValue(values.mouseMinX);
	}
	if(nullptr!=mouse_min_y_)
	{
		mouse_min_y_->setValue(values.mouseMinY);
	}
	if(nullptr!=mouse_max_x_)
	{
		mouse_max_x_->setValue(values.mouseMaxX);
	}
	if(nullptr!=mouse_max_y_)
	{
		mouse_max_y_->setValue(values.mouseMaxY);
	}
	if(nullptr!=app_specific_)
	{
		const int app_index=TownsQtAppProfileIndexForApp(values.appSpecificSetting);
		app_specific_->setCurrentIndex(app_index);
		updateAppSpecificDescription();
	}
	updateFunctionTab();
	updateAudioTabMidiSection();
	loading_=false;
}

void SettingsDialog::applyToValues(Values &out) const
{
	out.cpuFrequencyMhz=values_.cpuFrequencyMhz;
	if(nullptr!=mem_size_mb_)
	{
		out.memSizeInMB=mem_size_mb_->value();
	}
	if(nullptr!=fidelity_group_)
	{
		out.cpuHighFidelity=(1==fidelity_group_->checkedId());
	}
	if(nullptr!=pretend_386_)
	{
		out.pretend386DX=pretend_386_->isChecked();
	}
	if(nullptr!=use_fpu_)
	{
		out.useFPU=use_fpu_->isChecked();
	}
	if(nullptr!=fast_scsi_)
	{
		out.fastScsi=fast_scsi_->isChecked();
	}
	if(nullptr!=midi_board_)
	{
		out.midiBoard=midi_board_->isChecked();
	}
	if(nullptr!=model_group_)
	{
		out.modelGroupIndex=model_group_->currentData().toInt();
		if(out.modelGroupIndex<0)
		{
			out.modelGroupIndex=TownsQtModelGroupDefaultIndex();
		}
		if(false==isModelSelectionAllowed(out.modelGroupIndex))
		{
			out.modelGroupIndex=TownsQtRomAvailability::PreferredModelGroupForSysRom(sys_rom_profile_);
		}
	}
	out.displayScale=display_scale_->value();
	out.autoScaling=auto_scale_->isChecked();
	out.maintainAspect=maintain_aspect_->isChecked();
	out.damperWireLine=damper_wire_->isChecked();
	out.scanLineEffectIn15KHz=scanline_15k_->isChecked();
	out.fullscreenVsync=fullscreen_vsync_->isChecked();
	out.pcmResampleHighQuality=(nullptr!=pcm_resample_sinc_ && pcm_resample_sinc_->isChecked());
	if(nullptr!=sound_backend_)
	{
		out.audioBackend=sound_backend_->currentData().toString();
	}
	if(nullptr!=sound_device_)
	{
		out.audioDevice=sound_device_->currentData().toString();
	}
	if(nullptr!=fm_volume_slider_)
	{
		out.fmVolumePercent=fm_volume_slider_->value();
	}
	if(nullptr!=pcm_volume_slider_)
	{
		out.pcmVolumePercent=pcm_volume_slider_->value();
	}
	if(nullptr!=cdda_volume_slider_)
	{
		out.cddaVolumePercent=cdda_volume_slider_->value();
	}
	if(nullptr!=midi_volume_slider_)
	{
		out.midiVolumePercent=midi_volume_slider_->value();
	}
	if(nullptr!=midi_soundfont_edit_)
	{
		out.midiSoundFont=midi_soundfont_path_.trimmed();
	}
	if(nullptr!=midi_output_)
	{
		out.midiOutput=midi_output_->currentData().toString();
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
	if(nullptr!=snap_mouse_warmup_)
	{
		out.snapMouseWarmupFrames=snap_mouse_warmup_->value();
	}
	out.spriteTransferMode=sprite_group_->checkedId();
	if(out.spriteTransferMode<0)
	{
		out.spriteTransferMode=0;
	}
	if(nullptr!=gameport0_)
	{
		out.gamePort0=TownsQtGamePortOptions::ComboSelection(gameport0_,TOWNS_GAMEPORTEMU_PHYSICAL0);
	}
	if(nullptr!=gameport1_)
	{
		out.gamePort1=TownsQtGamePortOptions::ComboSelection(gameport1_,TOWNS_GAMEPORTEMU_MOUSE);
	}
	if(nullptr!=max_button_hold0_)
	{
		out.maxButtonHoldTimeMs0=max_button_hold0_->value();
	}
	if(nullptr!=max_button_hold1_)
	{
		out.maxButtonHoldTimeMs1=max_button_hold1_->value();
	}
	if(nullptr!=mouse_speed_slider_)
	{
		out.mouseIntegrationSpeed=mouse_speed_slider_->value();
	}
	if(nullptr!=mouse_vram_offset_)
	{
		out.considerVRAMOffsetInMouseIntegration=mouse_vram_offset_->isChecked();
	}
	if(nullptr!=diff_mouse_integration_)
	{
		out.differentialMouseIntegration=diff_mouse_integration_->isChecked();
	}
	if(nullptr!=mouse_min_x_)
	{
		out.mouseMinX=mouse_min_x_->value();
	}
	if(nullptr!=mouse_min_y_)
	{
		out.mouseMinY=mouse_min_y_->value();
	}
	if(nullptr!=mouse_max_x_)
	{
		out.mouseMaxX=mouse_max_x_->value();
	}
	if(nullptr!=mouse_max_y_)
	{
		out.mouseMaxY=mouse_max_y_->value();
	}
	if(nullptr!=app_specific_)
	{
		const int index=app_specific_->currentIndex();
		out.appSpecificSetting=TownsQtAppProfileApp(index);
	}
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
		if(nullptr!=mem_size_mb_)
		{
			mem_size_mb_->setValue(default_values_.memSizeInMB);
		}
		if(nullptr!=fidelity_group_)
		{
			if(QAbstractButton *btn=fidelity_group_->button(default_values_.cpuHighFidelity ? 1 : 0))
			{
				btn->setChecked(true);
			}
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
		if(nullptr!=midi_board_)
		{
			midi_board_->setChecked(default_values_.midiBoard);
		}
		if(nullptr!=model_group_)
		{
			int model_index=default_values_.modelGroupIndex;
			if(false==marty_ex_rom_present_ && model_index==marty_model_index_)
			{
				model_index=TownsQtModelGroupDefaultIndex();
			}
			model_group_->setCurrentIndex(model_group_->findData(model_index));
			updateModelDescription();
			updateMachineTabControls();
		}
		if(nullptr!=app_specific_)
		{
			const int app_index=TownsQtAppProfileIndexForApp(default_values_.appSpecificSetting);
			app_specific_->setCurrentIndex(app_index);
			updateAppSpecificDescription();
		}
	}
	else if(page==peripheral_page_)
	{
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
		if(nullptr!=mouse_speed_slider_)
		{
			mouse_speed_slider_->setValue(default_values_.mouseIntegrationSpeed);
		}
		if(nullptr!=mouse_vram_offset_)
		{
			mouse_vram_offset_->setChecked(default_values_.considerVRAMOffsetInMouseIntegration);
		}
		if(nullptr!=diff_mouse_integration_)
		{
			diff_mouse_integration_->setChecked(default_values_.differentialMouseIntegration);
		}
		if(nullptr!=mouse_min_x_)
		{
			mouse_min_x_->setValue(default_values_.mouseMinX);
		}
		if(nullptr!=mouse_min_y_)
		{
			mouse_min_y_->setValue(default_values_.mouseMinY);
		}
		if(nullptr!=mouse_max_x_)
		{
			mouse_max_x_->setValue(default_values_.mouseMaxX);
		}
		if(nullptr!=mouse_max_y_)
		{
			mouse_max_y_->setValue(default_values_.mouseMaxY);
		}
	}
	else if(page==video_page_)
	{
		display_scale_->setValue(default_values_.displayScale);
		auto_scale_->setChecked(default_values_.autoScaling);
		maintain_aspect_->setChecked(default_values_.maintainAspect);
		damper_wire_->setChecked(default_values_.damperWireLine);
		scanline_15k_->setChecked(default_values_.scanLineEffectIn15KHz);
		fullscreen_vsync_->setChecked(default_values_.fullscreenVsync);
		if(QAbstractButton *btn=sprite_group_->button(default_values_.spriteTransferMode))
		{
			btn->setChecked(true);
		}
	}
	else if(page==audio_page_)
	{
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
		if(nullptr!=fm_volume_slider_)
		{
			fm_volume_slider_->setValue(default_values_.fmVolumePercent);
		}
		if(nullptr!=pcm_volume_slider_)
		{
			pcm_volume_slider_->setValue(default_values_.pcmVolumePercent);
		}
		if(nullptr!=cdda_volume_slider_)
		{
			cdda_volume_slider_->setValue(default_values_.cddaVolumePercent);
		}
		if(nullptr!=midi_volume_slider_)
		{
			midi_volume_slider_->setValue(default_values_.midiVolumePercent);
		}
		if(nullptr!=midi_soundfont_edit_)
		{
			midi_soundfont_path_.clear();
			midi_soundfont_edit_->clear();
			midi_soundfont_edit_->setToolTip(QString());
		}
		if(nullptr!=midi_output_)
		{
			populateMidiOutputCombo(default_values_.midiOutput);
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
		updateAudioTabMidiSection();
	}
	else if(page==function_page_)
	{
		if(nullptr!=idle_inhibit_)
		{
			idle_inhibit_->setChecked(default_values_.waylandIdleInhibit);
		}
		if(nullptr!=snap_mouse_integration_)
		{
			snap_mouse_integration_->setChecked(default_values_.snapMouseIntegration);
		}
		if(nullptr!=snap_mouse_warmup_)
		{
			snap_mouse_warmup_->setValue(default_values_.snapMouseWarmupFrames);
		}
		updateFunctionTab();
	}
	loading_=false;
	markDirty();
}
