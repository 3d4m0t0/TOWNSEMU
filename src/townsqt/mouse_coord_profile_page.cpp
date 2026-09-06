#include "mouse_coord_profile_page.h"

#include "mouse_coord_write_scan.h"
#include "townsqt_mouse_preset.h"

#include <algorithm>
#include <initializer_list>

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QStandardItemModel>
#include <QVBoxLayout>

namespace
{
QSpinBox *MakeCoordSpin(QWidget *parent)
{
	auto *spin=new QSpinBox(parent);
	spin->setRange(-4096,4095);
	spin->setSingleStep(1);
	spin->setFrame(true);
	spin->setAlignment(Qt::AlignRight);
	spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
	return spin;
}

QSpinBox *MakeRangeSpin(QWidget *parent)
{
	auto *spin=new QSpinBox(parent);
	// Signed guest coords (Phys clamp); allow negative min/max.
	spin->setRange(-32768,32767);
	spin->setSingleStep(1);
	spin->setFrame(true);
	spin->setAlignment(Qt::AlignRight);
	spin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
	return spin;
}

int SpinWidthForSample(const QFontMetrics &fm,const QString &sample)
{
	// Text + spin arrows + frame. Too-tight fixed widths clip the border.
	return fm.horizontalAdvance(sample)+40;
}

int LineEditWidthForSample(const QFontMetrics &fm,const QString &sample)
{
	return fm.horizontalAdvance(sample)+20;
}

unsigned int ParsePhys(const QString &text,unsigned int fallback)
{
	bool ok=false;
	const unsigned int v=text.trimmed().toUInt(&ok,0);
	return ok ? v : fallback;
}

QString PhysText(unsigned int phys)
{
	if(0==phys)
	{
		return QString();
	}
	return QStringLiteral("0x%1").arg(phys,8,16,QLatin1Char('0'));
}

/*! Always emit hex (including 0x00000000) when the axis has a DS-off recipe. */
QString DsOffText(unsigned int off,bool present)
{
	if(true!=present)
	{
		return QString();
	}
	return QStringLiteral("0x%1").arg(off,8,16,QLatin1Char('0'));
}

}

MouseCoordProfilePage::MouseCoordProfilePage(QWidget *parent)
	:QWidget(parent)
{
	auto *root=new QVBoxLayout(this);
	root->setContentsMargins(0,0,0,0);
	root->setSpacing(8);

	const QFont mono=QFontDatabase::systemFont(QFontDatabase::FixedFont);
	const QFontMetrics monoFm(mono);
	const int physW=LineEditWidthForSample(monoFm,QStringLiteral("0x000000"));
	const int minMaxW=SpinWidthForSample(monoFm,QStringLiteral("9999"));
	const int offsetW=SpinWidthForSample(monoFm,QStringLiteral("9999"));
	const int scaleW=SpinWidthForSample(monoFm,QStringLiteral("9"));

	auto MakeHLine=[](QWidget *parent)->QFrame *{
		auto *line=new QFrame(parent);
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		return line;
	};

	mode_combo_=new QComboBox(this);
	mode_combo_->addItem(tr("Default"),MouseCoordWriteScan::INTEGRATION_AUTO);
	mode_combo_->addItem(tr("Mouse capture"),MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL);
	mode_combo_->addItem(tr("Mouse integration (Mouse BIOS)"),MouseCoordWriteScan::INTEGRATION_MOS);
	// Memory write and game-port share one UI entry; memory_write_ chooses DW vs GF.
	mode_combo_->addItem(tr("Mouse integration (app-specific settings)"),MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK);
	mode_combo_->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
	{
		const QFontMetrics fm(font());
		const int w=std::max({
		    fm.horizontalAdvance(tr("Default")),
		    fm.horizontalAdvance(tr("Mouse capture")),
		    fm.horizontalAdvance(tr("Mouse integration (Mouse BIOS)")),
		    fm.horizontalAdvance(tr("Mouse integration (app-specific settings)")),
		})+48;
		mode_combo_->setMinimumWidth(w);
	}

	clear_btn_=new QPushButton(tr("Clear"),this);
	clear_btn_->setEnabled(false);
	clear_btn_->setToolTip(
	    tr("Clear Game Phys / Phys 2 / range / scale / invert / Bind. "
	       "You can Apply or OK with Phys unset."));
	connect(clear_btn_,&QPushButton::clicked,this,[this](){
		clearAppSpecificFields();
	});

	auto *mode_row=new QHBoxLayout();
	mode_row->setContentsMargins(0,0,0,0);
	mode_row->setSpacing(8);
	mode_row->addWidget(new QLabel(tr("Mouse operation type"),this));
	mode_row->addWidget(mode_combo_);
	mode_row->addStretch(1);
	root->addLayout(mode_row);

	mode_note_=new QLabel(this);
	mode_note_->setWordWrap(true);
	mode_note_->setAlignment(Qt::AlignLeft|Qt::AlignTop);
	mode_note_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
	root->addWidget(mode_note_);
	connect(mode_combo_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){
		if(nullptr!=mode_combo_ && 0<=mode_combo_->currentIndex())
		{
			const QVariant data=mode_combo_->itemData(mode_combo_->currentIndex());
			if(true==data.isValid())
			{
				const int raw=data.toInt();
				if(MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==raw)
				{
					integration_mode_=
					    (nullptr!=memory_write_ && true==memory_write_->isChecked())
					        ? MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE
					        : MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK;
				}
				else
				{
					integration_mode_=raw;
				}
			}
		}
		updateModeNotes();
		emitContentChanged();
	});

	auto *app_exec_box=new QWidget(this);
	auto *app_exec_grid=new QGridLayout(app_exec_box);
	app_exec_grid->setContentsMargins(0,0,0,0);
	app_exec_grid->setHorizontalSpacing(8);
	app_exec_grid->setVerticalSpacing(4);

	auto *app_exec_live_title=new QLabel(tr("Current EXP:"),app_exec_box);
	auto *app_exec_bound_title=new QLabel(tr("Linked EXP:"),app_exec_box);
	{
		const QFontMetrics fm(app_exec_live_title->font());
		const int titleW=std::max({
		    fm.horizontalAdvance(tr("Current EXP:")),
		    fm.horizontalAdvance(tr("Linked EXP:")),
		    fm.horizontalAdvance(QStringLiteral("Current EXP:")),
		    fm.horizontalAdvance(QStringLiteral("Linked EXP:")),
		    fm.horizontalAdvance(QStringLiteral("現在のEXP：")),
		    fm.horizontalAdvance(QStringLiteral("紐付け済みEXP：")),
		});
		app_exec_live_title->setFixedWidth(titleW);
		app_exec_bound_title->setFixedWidth(titleW);
		app_exec_live_title->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		app_exec_bound_title->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
	}

	app_exec_live_label_=new QLabel(app_exec_box);
	app_exec_bound_label_=new QLabel(app_exec_box);
	app_exec_live_label_->setFont(mono);
	app_exec_bound_label_->setFont(mono);
	app_exec_live_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	app_exec_bound_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	app_exec_live_label_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
	app_exec_bound_label_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);

	bind_app_exec_btn_=new QPushButton(tr("Bind"),app_exec_box);
	bind_app_exec_btn_->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
	bind_app_exec_btn_->setToolTip(
	    tr("Bind app-specific mouse integration to the latest DOS Load/Exec (INT 21H AH=4BH). "
	       "Under DOS extenders (RUN386), bind the .EXP payload when shown."));
	connect(bind_app_exec_btn_,&QPushButton::clicked,this,[this](){
		bindLiveAppExec();
	});
	{
		const QFontMetrics fm(bind_app_exec_btn_->font());
		const int w=std::max({
		    fm.horizontalAdvance(tr("Bind")),
		    fm.horizontalAdvance(QStringLiteral("Bind")),
		    fm.horizontalAdvance(QStringLiteral("紐付け")),
		})+24;
		bind_app_exec_btn_->setFixedWidth(w);
	}

	app_exec_grid->addWidget(app_exec_live_title,0,0);
	app_exec_grid->addWidget(app_exec_live_label_,0,1);
	app_exec_grid->addWidget(bind_app_exec_btn_,0,2,2,1,Qt::AlignVCenter);
	app_exec_grid->addWidget(app_exec_bound_title,1,0);
	app_exec_grid->addWidget(app_exec_bound_label_,1,1);
	app_exec_grid->setColumnStretch(1,1);
	root->addWidget(app_exec_box);
	updateAppExecLabels();

	root->addWidget(MakeHLine(this));

	phys_search_btn_=new QPushButton(tr("Phys search"),this);
	phys_search_btn_->setToolTip(
	    tr("Open Memory scan to find guest RAM cursor coordinates.\n"
	       "Settings closes while the scan window is open."));
	connect(phys_search_btn_,&QPushButton::clicked,this,[this](){
		Q_EMIT openMemoryScanRequested();
	});
	preset_btn_=new QPushButton(tr("Load preset"),this);
	preset_btn_->setEnabled(false);
	preset_btn_->setToolTip(
	    tr("Load a mouse-integration preset for this CD fingerprint.\n"
	       "Enabled when mouse_XXXXXXXX.ini is found. Apply or OK saves it to the disc profile."));
	connect(preset_btn_,&QPushButton::clicked,this,[this](){
		applyMousePreset();
	});
	preset_save_btn_=new QPushButton(tr("Save to file"),this);
	preset_save_btn_->setEnabled(false);
	preset_save_btn_->setToolTip(
	    tr("Write the current app-specific mouse settings to\n"
	       "~/.config/townsqt/mouse_presets/mouse_XXXXXXXX.ini.\n"
	       "Enabled when app-specific Phys is set."));
	connect(preset_save_btn_,&QPushButton::clicked,this,[this](){
		saveMousePreset();
	});
	{
		const QFontMetrics fm(font());
		int w=0;
		for(const QString &s : {
		        tr("Phys search"),QStringLiteral("Phys search"),QStringLiteral("phys検索"),
		        tr("Load preset"),QStringLiteral("Load preset"),QStringLiteral("プリセット読込み"),
		        tr("Save to file"),QStringLiteral("Save to file"),QStringLiteral("ファイルへ保存"),
		        tr("Clear"),QStringLiteral("Clear"),QStringLiteral("クリア")})
		{
			w=std::max(w,fm.horizontalAdvance(s));
		}
		w+=16;
		for(QPushButton *btn : {phys_search_btn_,preset_btn_,preset_save_btn_,clear_btn_})
		{
			btn->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
			btn->setMinimumWidth(w);
		}
	}
	auto *phys_row=new QHBoxLayout();
	phys_row->setContentsMargins(0,0,0,0);
	phys_row->setSpacing(8);
	phys_row->addWidget(phys_search_btn_,1);
	phys_row->addWidget(preset_btn_,1);
	phys_row->addWidget(preset_save_btn_,1);
	phys_row->addWidget(clear_btn_,1);
	root->addLayout(phys_row);

	auto *coords_box=new QWidget(this);
	auto *grid=new QGridLayout(coords_box);
	grid->setContentsMargins(0,0,0,0);
	grid->setHorizontalSpacing(8);
	grid->setVerticalSpacing(4);

	grid->addWidget(new QLabel(tr("Axis"),coords_box),0,0);
	grid->addWidget(new QLabel(tr("Phys"),coords_box),0,1);
	grid->addWidget(new QLabel(tr("DS off"),coords_box),0,2);
	grid->addWidget(new QLabel(tr("Min"),coords_box),0,3);
	grid->addWidget(new QLabel(tr("Max"),coords_box),0,4);
	grid->addWidget(new QLabel(tr("Offset"),coords_box),0,5);
	grid->addWidget(new QLabel(tr("Scale"),coords_box),0,6);
	grid->addWidget(new QLabel(tr("Invert"),coords_box),0,7,Qt::AlignHCenter);

	game_phys_x_=new QLineEdit(coords_box);
	game_phys_y_=new QLineEdit(coords_box);
	game_phys2_x_=new QLineEdit(coords_box);
	game_phys2_y_=new QLineEdit(coords_box);
	game_ds_off_x_=new QLineEdit(coords_box);
	game_ds_off_y_=new QLineEdit(coords_box);
	game_ds_off2_x_=new QLineEdit(coords_box);
	game_ds_off2_y_=new QLineEdit(coords_box);
	for(QLineEdit *edit : {game_phys_x_,game_phys_y_,game_phys2_x_,game_phys2_y_,
	                       game_ds_off_x_,game_ds_off_y_,game_ds_off2_x_,game_ds_off2_y_})
	{
		edit->setPlaceholderText(QStringLiteral("0x........"));
		edit->setFont(mono);
		edit->setFrame(true);
		edit->setFixedWidth(physW);
	}
	const QString dsOffTip=
	    tr("Offset from live DS.base (WC-style). Prefer this over absolute Phys when "
	       "guest layout shifts with memory/CMOS/HDD changes.");
	for(QLineEdit *edit : {game_ds_off_x_,game_ds_off_y_,game_ds_off2_x_,game_ds_off2_y_})
	{
		edit->setToolTip(dsOffTip);
	}
	const QString phys2Tip=
	    tr("Optional second X/Y phys (below). Direct-write uses each row’s Scale on the mapped value.");
	game_phys2_x_->setToolTip(phys2Tip);
	game_phys2_y_->setToolTip(phys2Tip);
	game_min_x_=MakeRangeSpin(coords_box);
	game_max_x_=MakeRangeSpin(coords_box);
	game_min_y_=MakeRangeSpin(coords_box);
	game_max_y_=MakeRangeSpin(coords_box);
	offset_x_=MakeCoordSpin(coords_box);
	offset_y_=MakeCoordSpin(coords_box);
	scale_x_=MakeCoordSpin(coords_box);
	scale_y_=MakeCoordSpin(coords_box);
	scale2_x_=MakeCoordSpin(coords_box);
	scale2_y_=MakeCoordSpin(coords_box);
	for(QSpinBox *spin : {game_min_x_,game_max_x_,game_min_y_,game_max_y_})
	{
		spin->setFont(mono);
		spin->setFixedWidth(minMaxW);
	}
	for(QSpinBox *spin : {offset_x_,offset_y_})
	{
		spin->setFont(mono);
		spin->setFixedWidth(offsetW);
	}
	for(QSpinBox *spin : {scale_x_,scale_y_,scale2_x_,scale2_y_})
	{
		spin->setFont(mono);
		spin->setFixedWidth(scaleW);
		spin->setRange(-16,16);
		spin->setValue(1);
		spin->setToolTip(
		    tr("Multiplies the mapped write value for this Phys pair (0 = no multiply)."));
	}
	game_max_x_->setValue(0);
	game_max_y_->setValue(0);

	invert_x_=new QCheckBox(coords_box);
	invert_y_=new QCheckBox(coords_box);
	invert_x_->setToolTip(tr("Invert X"));
	invert_y_->setToolTip(tr("Invert Y"));

	grid->addWidget(new QLabel(tr("X"),coords_box),1,0);
	grid->addWidget(game_phys_x_,1,1);
	grid->addWidget(game_ds_off_x_,1,2);
	grid->addWidget(game_min_x_,1,3);
	grid->addWidget(game_max_x_,1,4);
	grid->addWidget(offset_x_,1,5);
	grid->addWidget(scale_x_,1,6);
	grid->addWidget(invert_x_,1,7,Qt::AlignHCenter);
	grid->addWidget(new QLabel(tr("Y"),coords_box),2,0);
	grid->addWidget(game_phys_y_,2,1);
	grid->addWidget(game_ds_off_y_,2,2);
	grid->addWidget(game_min_y_,2,3);
	grid->addWidget(game_max_y_,2,4);
	grid->addWidget(offset_y_,2,5);
	grid->addWidget(scale_y_,2,6);
	grid->addWidget(invert_y_,2,7,Qt::AlignHCenter);

	auto *phys2_lbl=new QLabel(tr("Phys 2"),coords_box);
	phys2_lbl->setToolTip(phys2Tip);
	// Same column as the Phys header so Phys / Phys 2 line up.
	grid->addWidget(phys2_lbl,3,1);

	grid->addWidget(new QLabel(tr("X2"),coords_box),4,0);
	grid->addWidget(game_phys2_x_,4,1);
	grid->addWidget(game_ds_off2_x_,4,2);
	grid->addWidget(scale2_x_,4,6);
	grid->addWidget(new QLabel(tr("Y2"),coords_box),5,0);
	grid->addWidget(game_phys2_y_,5,1);
	grid->addWidget(game_ds_off2_y_,5,2);
	grid->addWidget(scale2_y_,5,6);
	root->addWidget(coords_box);

	memory_write_=new QCheckBox(tr("Memory write"),this);
	memory_write_->setChecked(false);
	memory_write_->setToolTip(
	    tr("On: poke Game Phys in guest RAM (range clamp).\n"
	       "Off: feed host−guest deltas through the gameport."));
	wait_feedback_=new QCheckBox(tr("Wait for gameport input to apply"),this);
	wait_feedback_->setChecked(true);
	wait_feedback_->setToolTip(
	    tr("When on, hold the next gameport packet until Phys updates or the port is idle. "
	       "Turn off for continuous refill (may oscillate on delayed guest feedback). "
	       "Only used for gameport mode (memory write off)."));
	stop_soft_write_=new QCheckBox(tr("Stop writing to Mouse BIOS soft-cursor phys"),this);
	stop_soft_write_->setChecked(false);
	stop_soft_write_->setToolTip(
	    tr("When on, do not write Mouse BIOS soft coordinates while MOS is alive. "
	       "Game Phys / gameport still follow app-specific settings."));

	auto *opt_grid=new QGridLayout();
	opt_grid->setContentsMargins(0,0,0,0);
	opt_grid->setHorizontalSpacing(12);
	opt_grid->setVerticalSpacing(4);
	opt_grid->addWidget(memory_write_,0,0);
	opt_grid->addWidget(wait_feedback_,0,1);
	opt_grid->addWidget(stop_soft_write_,1,0,1,2);
	opt_grid->setColumnStretch(0,1);
	opt_grid->setColumnStretch(1,1);
	root->addLayout(opt_grid);

	root->addStretch(1);

	updateModeNotes();

	const int contentW=coords_box->sizeHint().width();
	mode_note_->setMaximumWidth(contentW);

	auto wireSpin=[&](QSpinBox *spin){
		if(nullptr!=spin)
		{
			connect(spin,qOverload<int>(&QSpinBox::valueChanged),this,[this](int){
				emitContentChanged();
			});
		}
	};
	wireSpin(game_min_x_);
	wireSpin(game_max_x_);
	wireSpin(game_min_y_);
	wireSpin(game_max_y_);
	wireSpin(offset_x_);
	wireSpin(offset_y_);
	wireSpin(scale_x_);
	wireSpin(scale_y_);
	wireSpin(scale2_x_);
	wireSpin(scale2_y_);
	connect(game_phys_x_,&QLineEdit::textChanged,this,[this](const QString &){
		refreshPresetButtons();
		emitContentChanged();
	});
	connect(game_phys_y_,&QLineEdit::textChanged,this,[this](const QString &){
		refreshPresetButtons();
		emitContentChanged();
	});
	connect(game_phys2_x_,&QLineEdit::textChanged,this,[this](const QString &){
		emitContentChanged();
	});
	connect(game_phys2_y_,&QLineEdit::textChanged,this,[this](const QString &){
		emitContentChanged();
	});
	for(QLineEdit *edit : {game_ds_off_x_,game_ds_off_y_,game_ds_off2_x_,game_ds_off2_y_})
	{
		if(nullptr!=edit)
		{
			connect(edit,&QLineEdit::textChanged,this,[this](const QString &){
				emitContentChanged();
			});
		}
	}
	if(nullptr!=invert_x_)
	{
		connect(invert_x_,&QCheckBox::toggled,this,[this](bool){
			emitContentChanged();
		});
	}
	if(nullptr!=invert_y_)
	{
		connect(invert_y_,&QCheckBox::toggled,this,[this](bool){
			emitContentChanged();
		});
	}
	if(nullptr!=wait_feedback_)
	{
		connect(wait_feedback_,&QCheckBox::toggled,this,[this](bool){
			emitContentChanged();
		});
	}
	if(nullptr!=stop_soft_write_)
	{
		connect(stop_soft_write_,&QCheckBox::toggled,this,[this](bool){
			emitContentChanged();
		});
	}
	if(nullptr!=memory_write_)
	{
		connect(memory_write_,&QCheckBox::toggled,this,[this](bool on){
			if(MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==
			       mode_combo_->itemData(mode_combo_->currentIndex()).toInt() ||
			   MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==integration_mode_ ||
			   MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==integration_mode_)
			{
				integration_mode_=on
				    ? MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE
				    : MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK;
			}
			updateModeNotes();
			emitContentChanged();
		});
	}
}

void MouseCoordProfilePage::emitContentChanged(void)
{
	if(true!=suppress_change_)
	{
		Q_EMIT contentChanged();
	}
}

void MouseCoordProfilePage::setEditorEnabled(bool enabled)
{
	// Do not call setEnabled(this) — disabling the page makes QComboBox::currentData()
	// unreliable (often reads as 0 / differential). Tab enablement is handled by Settings.
	if(nullptr!=mode_combo_)
	{
		mode_combo_->setEnabled(enabled);
	}
	const int mode=selectedMode();
	const bool gamePhys=
	    MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==mode ||
	    MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==mode;
	const bool memWrite=MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==mode;
	auto setOn=[&](QWidget *w,bool on)
	{
		if(nullptr!=w)
		{
			w->setEnabled(enabled && on);
		}
	};
	setOn(game_phys_x_,gamePhys);
	setOn(game_phys_y_,gamePhys);
	setOn(game_phys2_x_,gamePhys);
	setOn(game_phys2_y_,gamePhys);
	setOn(game_min_x_,gamePhys);
	setOn(game_max_x_,gamePhys);
	setOn(game_min_y_,gamePhys);
	setOn(game_max_y_,gamePhys);
	setOn(offset_x_,gamePhys);
	setOn(offset_y_,gamePhys);
	setOn(scale_x_,gamePhys);
	setOn(scale_y_,gamePhys);
	setOn(scale2_x_,gamePhys);
	setOn(scale2_y_,gamePhys);
	setOn(invert_x_,gamePhys);
	setOn(invert_y_,gamePhys);
	setOn(memory_write_,gamePhys);
	setOn(wait_feedback_,gamePhys && true!=memWrite);
	setOn(stop_soft_write_,gamePhys);
	setOn(clear_btn_,gamePhys);
	setOn(bind_app_exec_btn_,gamePhys && true!=live_app_exec_name_.isEmpty());
	setOn(app_exec_live_label_,gamePhys);
	setOn(app_exec_bound_label_,gamePhys);
	setOn(phys_search_btn_,true);
	setOn(preset_btn_,has_mouse_preset_);
	setOn(preset_save_btn_,0!=disc_fingerprint_hash32_ && hasAppSpecificSettings());
	if(nullptr!=mode_note_)
	{
		mode_note_->setEnabled(enabled);
	}
}

void MouseCoordProfilePage::updateModeNotes(void)
{
	const int mode=selectedMode();
	const bool gamePhys=
	    MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==mode ||
	    MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==mode;
	const bool memWrite=MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==mode;
	const bool pageOn=isEnabled() && (nullptr==mode_combo_ || mode_combo_->isEnabled());
	auto setWidgetsEnabled=[&](const std::initializer_list<QWidget*> &widgets,bool on)
	{
		for(QWidget *w : widgets)
		{
			if(nullptr!=w)
			{
				w->setEnabled(pageOn && on);
			}
		}
	};
	setWidgetsEnabled({game_phys_x_,game_phys_y_,game_phys2_x_,game_phys2_y_,
	                   game_min_x_,game_max_x_,game_min_y_,game_max_y_},gamePhys);
	setWidgetsEnabled({offset_x_,offset_y_,scale_x_,scale_y_,scale2_x_,scale2_y_},gamePhys);
	setWidgetsEnabled({invert_x_,invert_y_},gamePhys);
	if(nullptr!=memory_write_)
	{
		memory_write_->setEnabled(pageOn && gamePhys);
	}
	if(nullptr!=wait_feedback_)
	{
		wait_feedback_->setEnabled(pageOn && gamePhys && true!=memWrite);
	}
	if(nullptr!=stop_soft_write_)
	{
		stop_soft_write_->setEnabled(pageOn && gamePhys);
	}
	if(nullptr!=clear_btn_)
	{
		clear_btn_->setEnabled(pageOn && gamePhys);
	}
	if(nullptr!=bind_app_exec_btn_)
	{
		bind_app_exec_btn_->setEnabled(
		    pageOn && gamePhys && true!=live_app_exec_name_.isEmpty());
	}
	if(nullptr!=app_exec_live_label_)
	{
		app_exec_live_label_->setEnabled(pageOn && gamePhys);
	}
	if(nullptr!=app_exec_bound_label_)
	{
		app_exec_bound_label_->setEnabled(pageOn && gamePhys);
	}
	if(nullptr!=phys_search_btn_)
	{
		phys_search_btn_->setEnabled(pageOn);
	}
	refreshPresetButtons();
	switch(mode)
	{
	case MouseCoordWriteScan::INTEGRATION_AUTO:
		mode_note_->setText(
		    tr("Default: automatically switches between mouse integration (Mouse BIOS) "
		       "and mouse capture to match Mouse BIOS. "
		       "Titles without Mouse BIOS cannot use mouse integration."));
		break;
	case MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL:
		mode_note_->setText(
		    tr("Mouse capture: always uses mouse capture. "
		       "Mouse integration is not performed."));
		break;
	case MouseCoordWriteScan::INTEGRATION_MOS:
		mode_note_->setText(
		    mouse_bios_active_
		        ? tr("Mouse integration (Mouse BIOS): forces mouse integration via Mouse BIOS. "
		             "Titles without Mouse BIOS cannot use mouse integration.")
		        : tr("Mouse integration (Mouse BIOS): unavailable while the Mouse BIOS soft cursor "
		             "cannot be resolved."));
		break;
	case MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE:
	case MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK:
		mode_note_->setText(
		    tr("Mouse integration (app-specific settings): follows Default until a bound EXP starts, "
		       "then applies per-app Phys settings. Use Phys search to identify and set phys "
		       "addresses for reading and writing the in-game cursor. "
		       "“Memory write” pokes guest RAM; when off, host−guest deltas go through the gameport."));
		break;
	default:
		mode_note_->setText(QString());
		break;
	}
}

void MouseCoordProfilePage::clearAppSpecificFields(void)
{
	suppress_change_=true;
	if(nullptr!=game_phys_x_)
	{
		game_phys_x_->clear();
	}
	if(nullptr!=game_phys_y_)
	{
		game_phys_y_->clear();
	}
	if(nullptr!=game_phys2_x_)
	{
		game_phys2_x_->clear();
	}
	if(nullptr!=game_phys2_y_)
	{
		game_phys2_y_->clear();
	}
	for(QLineEdit *edit : {game_ds_off_x_,game_ds_off_y_,game_ds_off2_x_,game_ds_off2_y_})
	{
		if(nullptr!=edit)
		{
			edit->clear();
		}
	}
	pair0_ds_sel_=0;
	pair1_ds_sel_=0;
	if(nullptr!=game_min_x_)
	{
		game_min_x_->setValue(0);
	}
	if(nullptr!=game_max_x_)
	{
		game_max_x_->setValue(0);
	}
	if(nullptr!=game_min_y_)
	{
		game_min_y_->setValue(0);
	}
	if(nullptr!=game_max_y_)
	{
		game_max_y_->setValue(0);
	}
	if(nullptr!=offset_x_)
	{
		offset_x_->setValue(0);
	}
	if(nullptr!=offset_y_)
	{
		offset_y_->setValue(0);
	}
	if(nullptr!=scale_x_)
	{
		scale_x_->setValue(1);
	}
	if(nullptr!=scale_y_)
	{
		scale_y_->setValue(1);
	}
	if(nullptr!=scale2_x_)
	{
		scale2_x_->setValue(1);
	}
	if(nullptr!=scale2_y_)
	{
		scale2_y_->setValue(1);
	}
	if(nullptr!=invert_x_)
	{
		invert_x_->setChecked(false);
	}
	if(nullptr!=invert_y_)
	{
		invert_y_->setChecked(false);
	}
	if(nullptr!=stop_soft_write_)
	{
		stop_soft_write_->setChecked(false);
	}
	bound_app_exec_name_.clear();
	bound_app_exec_hash_=0;
	updateAppExecLabels();
	suppress_change_=false;
	emitContentChanged();
}

void MouseCoordProfilePage::applyMosAvailability(void)
{
	if(nullptr==mode_combo_)
	{
		return;
	}
	const int mosIdx=mode_combo_->findData(MouseCoordWriteScan::INTEGRATION_MOS);
	if(0<=mosIdx)
	{
		if(auto *model=qobject_cast<QStandardItemModel*>(mode_combo_->model()))
		{
			if(auto *item=model->item(mosIdx))
			{
				item->setEnabled(mouse_bios_active_);
			}
		}
		// Do not rewrite the saved mode when MOS is temporarily unavailable —
		// keep the selection and show the unavailable note in updateModeNotes().
	}
	updateModeNotes();
}

void MouseCoordProfilePage::setMouseBiosActive(bool active)
{
	mouse_bios_active_=active;
	applyMosAvailability();
}

int MouseCoordProfilePage::selectedMode(void) const
{
	if(nullptr!=mode_combo_ && 0<=mode_combo_->currentIndex())
	{
		const QVariant data=mode_combo_->itemData(mode_combo_->currentIndex());
		if(true==data.isValid())
		{
			const int raw=data.toInt();
			if(MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==raw)
			{
				return (nullptr!=memory_write_ && true==memory_write_->isChecked())
				    ? MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE
				    : MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK;
			}
			return raw;
		}
	}
	return integration_mode_;
}

void MouseCoordProfilePage::setSelectedMode(int mode)
{
	switch(mode)
	{
	case MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL:
	case MouseCoordWriteScan::INTEGRATION_MOS:
	case MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE:
	case MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK:
	case MouseCoordWriteScan::INTEGRATION_AUTO:
		integration_mode_=mode;
		break;
	default:
		integration_mode_=MouseCoordWriteScan::INTEGRATION_AUTO;
		break;
	}
	if(nullptr==mode_combo_)
	{
		return;
	}
	const bool gamePhys=
	    MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==integration_mode_ ||
	    MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==integration_mode_;
	if(nullptr!=memory_write_)
	{
		QSignalBlocker blocker(memory_write_);
		memory_write_->setChecked(
		    MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==integration_mode_);
	}
	const int comboData=true==gamePhys
	    ? MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK
	    : integration_mode_;
	const int idx=mode_combo_->findData(comboData);
	if(0<=idx && mode_combo_->currentIndex()!=idx)
	{
		mode_combo_->setCurrentIndex(idx);
	}
	else if(0>idx)
	{
		const int fallback=mode_combo_->findData(MouseCoordWriteScan::INTEGRATION_AUTO);
		if(0<=fallback)
		{
			integration_mode_=MouseCoordWriteScan::INTEGRATION_AUTO;
			mode_combo_->setCurrentIndex(fallback);
		}
	}
}

void MouseCoordProfilePage::setProfile(const QVariantMap &profile)
{
	suppress_change_=true;
	const QString base=QStringLiteral("prof_pair0");
	game_phys_x_->setText(PhysText(profile.value(base+QStringLiteral("_x")).toUInt()));
	game_phys_y_->setText(PhysText(profile.value(base+QStringLiteral("_y")).toUInt()));
	if(nullptr!=game_ds_off_x_)
	{
		const bool has=
		    profile.value(base+QStringLiteral("_has_ds_off")).toBool() ||
		    profile.contains(base+QStringLiteral("_ds_off_x"));
		game_ds_off_x_->setText(
		    DsOffText(profile.value(base+QStringLiteral("_ds_off_x")).toUInt(),has));
	}
	if(nullptr!=game_ds_off_y_)
	{
		const bool has=
		    profile.value(base+QStringLiteral("_has_ds_off")).toBool() ||
		    profile.contains(base+QStringLiteral("_ds_off_y"));
		game_ds_off_y_->setText(
		    DsOffText(profile.value(base+QStringLiteral("_ds_off_y")).toUInt(),has));
	}
	pair0_ds_sel_=profile.value(base+QStringLiteral("_ds_sel")).toUInt();
	game_min_x_->setValue(profile.value(base+QStringLiteral("_min_x"),0).toInt());
	game_max_x_->setValue(profile.value(base+QStringLiteral("_max_x"),0).toInt());
	game_min_y_->setValue(profile.value(base+QStringLiteral("_min_y"),0).toInt());
	game_max_y_->setValue(profile.value(base+QStringLiteral("_max_y"),0).toInt());
	const QString base1=QStringLiteral("prof_pair1");
	if(nullptr!=game_phys2_x_)
	{
		game_phys2_x_->setText(PhysText(profile.value(base1+QStringLiteral("_x")).toUInt()));
	}
	if(nullptr!=game_phys2_y_)
	{
		game_phys2_y_->setText(PhysText(profile.value(base1+QStringLiteral("_y")).toUInt()));
	}
	if(nullptr!=game_ds_off2_x_)
	{
		const bool has=
		    profile.value(base1+QStringLiteral("_has_ds_off")).toBool() ||
		    profile.contains(base1+QStringLiteral("_ds_off_x"));
		game_ds_off2_x_->setText(
		    DsOffText(profile.value(base1+QStringLiteral("_ds_off_x")).toUInt(),has));
	}
	if(nullptr!=game_ds_off2_y_)
	{
		const bool has=
		    profile.value(base1+QStringLiteral("_has_ds_off")).toBool() ||
		    profile.contains(base1+QStringLiteral("_ds_off_y"));
		game_ds_off2_y_->setText(
		    DsOffText(profile.value(base1+QStringLiteral("_ds_off_y")).toUInt(),has));
	}
	pair1_ds_sel_=profile.value(base1+QStringLiteral("_ds_sel")).toUInt();
	scale_x_->setValue(profile.value(base+QStringLiteral("_scale_x"),
	    profile.value(QStringLiteral("prof_scale_x"),1)).toInt());
	scale_y_->setValue(profile.value(base+QStringLiteral("_scale_y"),
	    profile.value(QStringLiteral("prof_scale_y"),1)).toInt());
	if(nullptr!=scale2_x_)
	{
		scale2_x_->setValue(profile.value(base1+QStringLiteral("_scale_x"),1).toInt());
	}
	if(nullptr!=scale2_y_)
	{
		scale2_y_->setValue(profile.value(base1+QStringLiteral("_scale_y"),1).toInt());
	}

	int mode=profile.value(
	    QStringLiteral("prof_integration_mode"),
	    MouseCoordWriteScan::INTEGRATION_AUTO).toInt();
	if(false==profile.contains(QStringLiteral("prof_integration_mode")))
	{
		// Legacy profiles may only have enabled / feedback_only.
		if(true==profile.contains(QStringLiteral("prof_enabled")) ||
		   true==profile.contains(QStringLiteral("prof_feedback_only")))
		{
			const bool enabled=profile.value(QStringLiteral("prof_enabled"),true).toBool();
			const bool feedbackOnly=profile.value(QStringLiteral("prof_feedback_only")).toBool();
			if(true!=enabled)
			{
				mode=MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL;
			}
			else if(true==feedbackOnly)
			{
				mode=MouseCoordWriteScan::INTEGRATION_MOS;
			}
			else
			{
				mode=MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE;
			}
		}
		else
		{
			mode=MouseCoordWriteScan::INTEGRATION_AUTO;
		}
	}
	setSelectedMode(mode);

	offset_x_->setValue(profile.value(QStringLiteral("prof_offset_x")).toInt());
	offset_y_->setValue(profile.value(QStringLiteral("prof_offset_y")).toInt());
	if(nullptr!=invert_x_)
	{
		invert_x_->setChecked(profile.value(QStringLiteral("prof_invert_x")).toBool());
	}
	if(nullptr!=invert_y_)
	{
		invert_y_->setChecked(profile.value(QStringLiteral("prof_invert_y")).toBool());
	}
	if(nullptr!=wait_feedback_)
	{
		wait_feedback_->setChecked(
		    profile.value(QStringLiteral("prof_wait_feedback"),true).toBool());
	}
	if(nullptr!=stop_soft_write_)
	{
		stop_soft_write_->setChecked(
		    profile.value(QStringLiteral("prof_stop_soft_write"),false).toBool());
	}
	bound_app_exec_name_=
	    profile.value(QStringLiteral("prof_app_exec_name")).toString().trimmed().toUpper();
	bound_app_exec_hash_=
	    profile.value(QStringLiteral("prof_app_exec_hash")).toUInt();
	if(true==profile.contains(QStringLiteral("current_app_exec_name")) ||
	   true==profile.contains(QStringLiteral("current_app_exec_hash")))
	{
		setLiveAppExec(
		    profile.value(QStringLiteral("current_app_exec_name")).toString(),
		    profile.value(QStringLiteral("current_app_exec_hash")).toUInt());
	}
	else
	{
		updateAppExecLabels();
	}
	verified_=profile.value(QStringLiteral("prof_verified"),true).toBool();
	unsigned int fp=profile.value(QStringLiteral("disc_fingerprint_hash32")).toUInt();
	if(0==fp)
	{
		fp=profile.value(QStringLiteral("prof_disc_fingerprint_hash32")).toUInt();
	}
	if(0!=fp)
	{
		disc_fingerprint_hash32_=fp;
	}
	applyMosAvailability();
	refreshPresetButtons();
	suppress_change_=false;
}

void MouseCoordProfilePage::setGameCursorFromSelection(
    unsigned int physX,unsigned int physY,
    unsigned int minX,unsigned int maxX,
    unsigned int minY,unsigned int maxY,
    bool hasRangeX,bool hasRangeY,
    unsigned int dsOffX,unsigned int dsOffY,
    unsigned int dsSelector,bool hasDsOff)
{
	if(0==physX || 0==physY)
	{
		return;
	}
	game_phys_x_->setText(PhysText(physX));
	game_phys_y_->setText(PhysText(physY));
	if(true==hasDsOff)
	{
		if(nullptr!=game_ds_off_x_)
		{
			game_ds_off_x_->setText(DsOffText(dsOffX,true));
		}
		if(nullptr!=game_ds_off_y_)
		{
			game_ds_off_y_->setText(DsOffText(dsOffY,true));
		}
		pair0_ds_sel_=dsSelector;
	}
	// Observed scan min..max as signed words.  Equal ends = current value only,
	// not a usable clamp — keep existing editor min/max in that case.
	const int sMinX=(int)(short)(minX&0xffffu);
	const int sMaxX=(int)(short)(maxX&0xffffu);
	const int sMinY=(int)(short)(minY&0xffffu);
	const int sMaxY=(int)(short)(maxY&0xffffu);
	if(true==hasRangeX && sMinX!=sMaxX)
	{
		game_min_x_->setValue(sMinX);
		game_max_x_->setValue(sMaxX);
	}
	if(true==hasRangeY && sMinY!=sMaxY)
	{
		game_min_y_->setValue(sMinY);
		game_max_y_->setValue(sMaxY);
	}
	emitContentChanged();
}

void MouseCoordProfilePage::setGameCursor2FromSelection(
    unsigned int physX,unsigned int physY,
    unsigned int dsOffX,unsigned int dsOffY,
    unsigned int dsSelector,bool hasDsOff)
{
	if(0==physX || 0==physY ||
	   nullptr==game_phys2_x_ || nullptr==game_phys2_y_)
	{
		return;
	}
	game_phys2_x_->setText(PhysText(physX));
	game_phys2_y_->setText(PhysText(physY));
	if(true==hasDsOff)
	{
		if(nullptr!=game_ds_off2_x_)
		{
			game_ds_off2_x_->setText(DsOffText(dsOffX,true));
		}
		if(nullptr!=game_ds_off2_y_)
		{
			game_ds_off2_y_->setText(DsOffText(dsOffY,true));
		}
		pair1_ds_sel_=dsSelector;
	}
	emitContentChanged();
}

QVariantMap MouseCoordProfilePage::profile(void)
{
	// Commit in-progress spinbox edits (Apply/OK often clicked while focused).
	for(QSpinBox *spin : {game_min_x_,game_max_x_,game_min_y_,game_max_y_,
	                      offset_x_,offset_y_,scale_x_,scale_y_,scale2_x_,scale2_y_})
	{
		if(nullptr!=spin)
		{
			spin->interpretText();
		}
	}

	QVariantMap out;
	// Soft phys is unused (MOS uses live soft; DW/gameport use Game Phys only).
	out.insert(QStringLiteral("prof_px"),0u);
	out.insert(QStringLiteral("prof_py"),0u);

	const unsigned int gameX=ParsePhys(game_phys_x_->text(),0);
	const unsigned int gameY=ParsePhys(game_phys_y_->text(),0);
	const QString base=QStringLiteral("prof_pair0");
	out.insert(base+QStringLiteral("_x"),gameX);
	out.insert(base+QStringLiteral("_y"),gameY);
	const unsigned int dsOffX=
	    (nullptr!=game_ds_off_x_) ? ParsePhys(game_ds_off_x_->text(),0) : 0u;
	const unsigned int dsOffY=
	    (nullptr!=game_ds_off_y_) ? ParsePhys(game_ds_off_y_->text(),0) : 0u;
	const bool hasDsOff=
	    (nullptr!=game_ds_off_x_ && true!=game_ds_off_x_->text().trimmed().isEmpty()) ||
	    (nullptr!=game_ds_off_y_ && true!=game_ds_off_y_->text().trimmed().isEmpty());
	if(true==hasDsOff)
	{
		out.insert(base+QStringLiteral("_ds_off_x"),dsOffX);
		out.insert(base+QStringLiteral("_ds_off_y"),dsOffY);
		out.insert(base+QStringLiteral("_ds_sel"),pair0_ds_sel_);
		out.insert(base+QStringLiteral("_has_ds_off"),true);
	}
	out.insert(base+QStringLiteral("_bias_x"),0);
	out.insert(base+QStringLiteral("_bias_y"),0);
	out.insert(base+QStringLiteral("_scale_x"),scale_x_->value());
	out.insert(base+QStringLiteral("_scale_y"),scale_y_->value());
	out.insert(base+QStringLiteral("_min_x"),game_min_x_->value());
	out.insert(base+QStringLiteral("_max_x"),game_max_x_->value());
	out.insert(base+QStringLiteral("_min_y"),game_min_y_->value());
	out.insert(base+QStringLiteral("_max_y"),game_max_y_->value());

	const unsigned int game2X=
	    (nullptr!=game_phys2_x_) ? ParsePhys(game_phys2_x_->text(),0) : 0u;
	const unsigned int game2Y=
	    (nullptr!=game_phys2_y_) ? ParsePhys(game_phys2_y_->text(),0) : 0u;
	const QString base1=QStringLiteral("prof_pair1");
	out.insert(base1+QStringLiteral("_x"),game2X);
	out.insert(base1+QStringLiteral("_y"),game2Y);
	const unsigned int dsOff2X=
	    (nullptr!=game_ds_off2_x_) ? ParsePhys(game_ds_off2_x_->text(),0) : 0u;
	const unsigned int dsOff2Y=
	    (nullptr!=game_ds_off2_y_) ? ParsePhys(game_ds_off2_y_->text(),0) : 0u;
	const bool hasDsOff2=
	    (nullptr!=game_ds_off2_x_ && true!=game_ds_off2_x_->text().trimmed().isEmpty()) ||
	    (nullptr!=game_ds_off2_y_ && true!=game_ds_off2_y_->text().trimmed().isEmpty());
	if(true==hasDsOff2)
	{
		out.insert(base1+QStringLiteral("_ds_off_x"),dsOff2X);
		out.insert(base1+QStringLiteral("_ds_off_y"),dsOff2Y);
		out.insert(base1+QStringLiteral("_ds_sel"),pair1_ds_sel_);
		out.insert(base1+QStringLiteral("_has_ds_off"),true);
	}
	out.insert(base1+QStringLiteral("_bias_x"),0);
	out.insert(base1+QStringLiteral("_bias_y"),0);
	out.insert(base1+QStringLiteral("_scale_x"),
	           nullptr!=scale2_x_ ? scale2_x_->value() : 1);
	out.insert(base1+QStringLiteral("_scale_y"),
	           nullptr!=scale2_y_ ? scale2_y_->value() : 1);
	// Same clamp as pair0 (scale is applied after clamp, per pair).
	out.insert(base1+QStringLiteral("_min_x"),game_min_x_->value());
	out.insert(base1+QStringLiteral("_max_x"),game_max_x_->value());
	out.insert(base1+QStringLiteral("_min_y"),game_min_y_->value());
	out.insert(base1+QStringLiteral("_max_y"),game_max_y_->value());

	const int mode=selectedMode();
	integration_mode_=mode;
	out.insert(QStringLiteral("prof_integration_mode"),mode);
	out.insert(QStringLiteral("prof_enabled"),
	           MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL!=mode);
	out.insert(QStringLiteral("prof_feedback_only"),
	           MouseCoordWriteScan::INTEGRATION_MOS==mode ||
	           MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==mode);
	out.insert(QStringLiteral("prof_offset_x"),offset_x_->value());
	out.insert(QStringLiteral("prof_offset_y"),offset_y_->value());
	out.insert(QStringLiteral("prof_scale_x"),scale_x_->value());
	out.insert(QStringLiteral("prof_scale_y"),scale_y_->value());
	out.insert(QStringLiteral("prof_invert_x"),
	           nullptr!=invert_x_ && invert_x_->isChecked());
	out.insert(QStringLiteral("prof_invert_y"),
	           nullptr!=invert_y_ && invert_y_->isChecked());
	out.insert(QStringLiteral("prof_wait_feedback"),
	           nullptr==wait_feedback_ || wait_feedback_->isChecked());
	out.insert(QStringLiteral("prof_stop_soft_write"),
	           nullptr!=stop_soft_write_ && stop_soft_write_->isChecked());
	out.insert(QStringLiteral("prof_app_exec_name"),bound_app_exec_name_);
	out.insert(QStringLiteral("prof_app_exec_hash"),bound_app_exec_hash_);
	out.insert(QStringLiteral("prof_verified"),verified_);
	out.insert(QStringLiteral("disc_fingerprint_hash32"),disc_fingerprint_hash32_);
	out.insert(QStringLiteral("prof_disc_fingerprint_hash32"),disc_fingerprint_hash32_);
	return out;
}

void MouseCoordProfilePage::setDiscFingerprint(unsigned int fingerprintHash32)
{
	disc_fingerprint_hash32_=fingerprintHash32;
	refreshPresetButtons();
}

bool MouseCoordProfilePage::hasAppSpecificSettings(void) const
{
	const int mode=selectedMode();
	if(MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE!=mode &&
	   MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK!=mode)
	{
		return false;
	}
	if(nullptr==game_phys_x_ || nullptr==game_phys_y_)
	{
		return false;
	}
	const bool hasAbs=
	    0!=ParsePhys(game_phys_x_->text(),0) &&
	    0!=ParsePhys(game_phys_y_->text(),0);
	const bool hasDs=
	    (nullptr!=game_ds_off_x_ && true!=game_ds_off_x_->text().trimmed().isEmpty()) &&
	    (nullptr!=game_ds_off_y_ && true!=game_ds_off_y_->text().trimmed().isEmpty());
	return true==hasAbs || true==hasDs;
}

void MouseCoordProfilePage::refreshPresetButtons(void)
{
	has_mouse_preset_=TownsQtMousePreset::Exists(disc_fingerprint_hash32_);
	const bool pageOn=
	    isEnabled() && (nullptr==mode_combo_ || mode_combo_->isEnabled());
	if(nullptr!=preset_btn_)
	{
		preset_btn_->setEnabled(pageOn && has_mouse_preset_);
	}
	if(nullptr!=preset_save_btn_)
	{
		preset_save_btn_->setEnabled(
		    pageOn && 0!=disc_fingerprint_hash32_ && hasAppSpecificSettings());
	}
}

void MouseCoordProfilePage::applyMousePreset(void)
{
	QVariantMap preset;
	if(true!=TownsQtMousePreset::Load(disc_fingerprint_hash32_,preset))
	{
		refreshPresetButtons();
		return;
	}
	const unsigned int fp=disc_fingerprint_hash32_;
	const QString liveName=live_app_exec_name_;
	const unsigned int liveHash=live_app_exec_hash_;
	setProfile(preset);
	setLiveAppExec(liveName,liveHash);
	setDiscFingerprint(fp);
	emitContentChanged();
}

void MouseCoordProfilePage::saveMousePreset(void)
{
	if(true!=hasAppSpecificSettings() || 0==disc_fingerprint_hash32_)
	{
		refreshPresetButtons();
		return;
	}
	QString err;
	if(true!=TownsQtMousePreset::Save(disc_fingerprint_hash32_,profile(),&err))
	{
		QMessageBox::warning(
		    this,
		    tr("Save to file"),
		    tr("Could not write mouse preset:\n%1").arg(err));
		return;
	}
	refreshPresetButtons();
}

void MouseCoordProfilePage::setLiveAppExec(const QString &name,unsigned int hash32)
{
	live_app_exec_name_=name.trimmed().toUpper();
	live_app_exec_hash_=hash32;
	updateAppExecLabels();
	updateModeNotes();
}

void MouseCoordProfilePage::bindLiveAppExec(void)
{
	if(true==live_app_exec_name_.isEmpty())
	{
		return;
	}
	bound_app_exec_name_=live_app_exec_name_;
	bound_app_exec_hash_=live_app_exec_hash_;
	updateAppExecLabels();
	emitContentChanged();
	Q_EMIT bindCurrentAppExecRequested();
}

void MouseCoordProfilePage::updateAppExecLabels(void)
{
	auto fmt=[](const QString &name,unsigned int hash)->QString
	{
		if(true==name.isEmpty())
		{
			return QStringLiteral("(none)");
		}
		return QStringLiteral("%1 hash=0x%2")
		    .arg(name)
		    .arg(hash,8,16,QLatin1Char('0'));
	};
	if(nullptr!=app_exec_live_label_)
	{
		app_exec_live_label_->setText(fmt(live_app_exec_name_,live_app_exec_hash_));
	}
	if(nullptr!=app_exec_bound_label_)
	{
		app_exec_bound_label_->setText(fmt(bound_app_exec_name_,bound_app_exec_hash_));
	}
}
