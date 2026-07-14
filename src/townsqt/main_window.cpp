#include "main_window.h"

#include "emulator_controller.h"
#include "townsargv.h"
#include "townsqt_argv_from_settings.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_rom_availability.h"
#include "townsqt_app_profile.h"
#include "townsqt_settings.h"
#include "townsqt_wayland_idle_inhibit.h"

#if defined(__linux__)
#include "linux/midi_fluidsynth_host.h"
#endif

#include "d77.h"
#include "diskimg.h"
#include "townsqt_gameport_options.h"

#include <QSignalBlocker>
#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QWidgetAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QCursor>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QEventLoop>
#include <QPixmap>
#include <QShowEvent>
#include <QStatusBar>
#include <QTimer>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
QString TsugaruQuotedName()
{
	return QStringLiteral("\"津軽\"");
}

QString AboutTsugaruTitleText()
{
	return QCoreApplication::translate("MainWindow","Tsugaru %1 について")
	    .arg(TsugaruQuotedName());
}

bool IsCpuFrequencyPreset(int mhz)
{
	return 16==mhz || 20==mhz || 25==mhz;
}

void AddMenuWidget(QMenu *menu,QWidget *widget)
{
	auto *action=new QWidgetAction(menu);
	action->setDefaultWidget(widget);
	menu->addAction(action);
}

QString CreateBlankFdImagePath()
{
	if(true!=TownsQtPaths::ensureLayout())
	{
		return {};
	}
	const QString file_name=QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
	                      +QStringLiteral(".d77");
	const QString path=TownsQtPaths::blankFdDir()+QStringLiteral("/")+file_name;

	// Towns 2HD 1232KB blank (same template as CUI -GENFD).
	const auto raw=Get1232KBFloppyDiskImage();
	D77File d77;
	if(true!=d77.SetRawBinary(raw))
	{
		return {};
	}
	const auto image=d77.MakeD77Image();
	if(image.empty())
	{
		return {};
	}
	QFile file(path);
	if(!file.open(QIODevice::WriteOnly))
	{
		return {};
	}
	if(image.size()!=static_cast<size_t>(file.write(reinterpret_cast<const char *>(image.data()),static_cast<qint64>(image.size()))))
	{
		return {};
	}
	return path;
}

bool MachineSettingsNeedRestart(const SettingsDialog::Values &values)
{
	if(values.modelGroupIndex!=TownsQtSettings::modelGroupIndex())
	{
		return true;
	}
	if(values.memSizeInMB!=TownsQtSettings::memSizeInMB())
	{
		return true;
	}
	if(values.cpuHighFidelity!=TownsQtSettings::cpuHighFidelity())
	{
		return true;
	}
	if(values.pretend386DX!=TownsQtSettings::pretend386DX())
	{
		return true;
	}
	if(values.useFPU!=TownsQtSettings::useFPU())
	{
		return true;
	}
	if(values.fastScsi!=TownsQtSettings::fastScsi())
	{
		return true;
	}
	return false;
}
}

MainWindow::MainWindow(const TownsARGV &argv,int scale,QWidget *parent)
	: QMainWindow(parent),argv_(argv)
{
	setWindowTitle(QStringLiteral("TownsQt"));
	view_=new EmuView(this);
	view_->attachFramebuffer(&framebuffer_);
	view_->attachInputQueue(&inputQueue_);
	view_->setScale(scale);
	view_->setVideoOptions(scale,TownsQtSettings::autoScaling(),TownsQtSettings::maintainAspect());
	view_->setMinimumSize(640*scale,480*scale);
	setCentralWidget(view_);

	setupMenuBar();
	const int content_w=640*scale;
	const int content_h=480*scale;
	resize(content_w+16,
	       content_h+menuBar()->sizeHint().height()+statusBar()->sizeHint().height()+16);

	statusBar()->showMessage(tr("Starting…"));
	drive_debug_label_=new QLabel(statusBar());
	drive_debug_label_->setMinimumWidth(320);
	statusBar()->addPermanentWidget(drive_debug_label_);
	mouse_debug_label_=new QLabel(statusBar());
	mouse_debug_label_->setMinimumWidth(520);
	statusBar()->addPermanentWidget(mouse_debug_label_);
	applyDriveAccessVisibility();
	applyMouseDebugVisibility();
	if(nullptr!=view_)
	{
		view_->setDriveAccessOverlayEnabled(TownsQtSettings::showDriveAccessOverlay());
	}

	fullscreen_chrome_hide_timer_=new QTimer(this);
	fullscreen_chrome_hide_timer_->setSingleShot(true);
	connect(fullscreen_chrome_hide_timer_,&QTimer::timeout,this,&MainWindow::hideFullscreenChrome);
	connectFullscreenMenuHooks();
	connect(view_,&EmuView::emuPictureClicked,this,&MainWindow::noteEmuPictureClicked);
	if(QApplication *app=qApp)
	{
		app->installEventFilter(this);
	}
}

void MainWindow::setupEmulatorConnections()
{
	if(nullptr==controller_)
	{
		return;
	}
	disconnect(emu_thread_,&QThread::started,nullptr,nullptr);
	disconnect(&poll_timer_,nullptr,nullptr,nullptr);
	connect(emu_thread_,&QThread::started,controller_,&EmulatorController::run);
	connect(controller_,&EmulatorController::finished,emu_thread_,&QThread::quit);
	connect(controller_,&EmulatorController::frameReady,view_,&EmuView::refreshFrame);
	connect(controller_,&EmulatorController::frameReady,this,&MainWindow::onFrameReady);
	connect(controller_,&EmulatorController::statsUpdated,this,&MainWindow::onStatsUpdated);
	connect(controller_,&EmulatorController::finished,this,&MainWindow::onControllerFinished);
	connect(controller_,&EmulatorController::failed,this,&MainWindow::onFailed);
	connect(controller_,&EmulatorController::cdPathChanged,this,&MainWindow::onCdPathChanged);
	connect(controller_,&EmulatorController::fdPathChanged,this,&MainWindow::onFdPathChanged);
	connect(controller_,&EmulatorController::fdWriteProtectChanged,this,&MainWindow::onFdWriteProtectChanged);
	connect(controller_,&EmulatorController::fastModeLampChanged,this,&MainWindow::onGuestFastModeLampChanged);
	connect(this,&MainWindow::cdLoadRequested,controller_,&EmulatorController::loadCdImage,Qt::QueuedConnection);
	connect(this,&MainWindow::fdLoadRequested,controller_,&EmulatorController::loadFdImage,Qt::QueuedConnection);
	connect(&poll_timer_,&QTimer::timeout,this,&MainWindow::onPollTimer);
}

void MainWindow::teardownEmulatorConnections()
{
	disconnect(&poll_timer_,nullptr,nullptr,nullptr);
	if(nullptr!=emu_thread_)
	{
		disconnect(emu_thread_,&QThread::started,nullptr,nullptr);
	}
	if(nullptr!=controller_)
	{
		disconnect(controller_,nullptr,this,nullptr);
		disconnect(controller_,nullptr,view_,nullptr);
		disconnect(this,nullptr,controller_,nullptr);
	}
}

void MainWindow::startEmulator()
{
	if(nullptr!=emu_thread_)
	{
		return;
	}
#if defined(__linux__)
	{
		const QByteArray sf_utf8=TownsQtSettings::midiSoundFont().toUtf8();
		MidiFluidSynthHost::SetSoundFontPath(sf_utf8.constData());
		MidiFluidSynthHost::SetMasterVolumePercent(TownsQtSettings::midiVolumePercent());
	}
#endif
	emu_thread_=new QThread(this);
	controller_=new EmulatorController(argv_,&framebuffer_,&inputQueue_);
	controller_->moveToThread(emu_thread_);
	setupEmulatorConnections();
	if(nullptr!=view_)
	{
		view_->pollMousePosition();
	}
	poll_timer_.setTimerType(Qt::PreciseTimer);
	poll_timer_.setInterval(static_cast<int>((TOWNS_RENDERING_FREQUENCY*1000ULL+999999999ULL)/1000000000ULL));
	poll_timer_.start();
	emu_thread_->start();
}

void MainWindow::scheduleRestartEmulator()
{
	if(emu_restarting_)
	{
		emu_restart_pending_=true;
		return;
	}
	emu_restarting_=true;
	statusBar()->showMessage(tr("エミュレータを再起動しています…"));
	stopEmulatorAsync([this]{
		TownsQtArgvFromSettings::Apply(argv_);
		startEmulator();
		emu_restarting_=false;
		if(emu_restart_pending_)
		{
			emu_restart_pending_=false;
			scheduleRestartEmulator();
			return;
		}
		statusBar()->showMessage(tr("エミュレータを再起動しました"),5000);
	});
}

void MainWindow::restartEmulator()
{
	scheduleRestartEmulator();
}

MainWindow::~MainWindow()
{
	TownsQtWaylandIdleInhibit::Shutdown();
	inputQueue_.CancelCursorWarp();
	showFullscreenCursor();
	if(QApplication *app=qApp)
	{
		app->removeEventFilter(this);
	}
	stopEmulator();
}

void MainWindow::setupMenuBar()
{
	auto *operationMenu=menuBar()->addMenu(tr("操作(&O)"));

	auto *resetAction=operationMenu->addAction(tr("リセット(&R)"));
	resetAction->setShortcut(QKeySequence(Qt::Key_F12));
	resetAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(resetAction,&QAction::triggered,this,[this]{
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(controller_,"resetMachine",Qt::QueuedConnection);
		}
	});

	operationMenu->addSeparator();

	fast_mode_action_=operationMenu->addAction(tr("高速モード"));
	fast_mode_action_->setCheckable(true);
	fast_mode_action_->setChecked(TownsQtSettings::cpuFastModeEnabled());
	connect(fast_mode_action_,&QAction::toggled,this,[this](bool enabled){
		setCpuFastModeEnabled(enabled);
	});
	connect(operationMenu,&QMenu::aboutToShow,this,&MainWindow::syncGuestFastModeFromEmulator);

	cpu_clock_group_=new QActionGroup(this);
	cpu_clock_group_->setExclusive(true);
	const int current_mhz=TownsQtSettings::cpuFrequencyMhz();
	const struct {int mhz; const char *label;} cpu_presets[]={
	    {16,"16MHz"},{20,"20MHz"},{25,"25MHz"},
	};
	for(const auto &preset : cpu_presets)
	{
		auto *action=operationMenu->addAction(QString::fromUtf8(preset.label));
		action->setCheckable(true);
		action->setData(preset.mhz);
		action->setChecked(preset.mhz==current_mhz);
		cpu_clock_group_->addAction(action);
		connect(action,&QAction::triggered,this,[this,mhz=preset.mhz]{
			setCpuFrequencyMhz(mhz);
		});
	}
	cpu_clock_custom_action_=operationMenu->addAction(tr("任意…"));
	cpu_clock_custom_action_->setCheckable(true);
	cpu_clock_custom_action_->setChecked(!IsCpuFrequencyPreset(current_mhz));
	cpu_clock_group_->addAction(cpu_clock_custom_action_);
	connect(cpu_clock_custom_action_,&QAction::triggered,this,[this]{
		const int current_mhz=TownsQtSettings::cpuFrequencyMhz();
		const int initial_mhz=IsCpuFrequencyPreset(current_mhz) ? 25 : current_mhz;
		bool ok=false;
		const int mhz=QInputDialog::getInt(
		    this,
		    tr("CPU 周波数"),
		    tr("MHz:"),
		    initial_mhz,
		    1,
		    100,
		    1,
		    &ok);
		if(ok)
		{
			setCpuFrequencyMhz(mhz);
		}
		syncMenuChecks();
	});

	operationMenu->addSeparator();

	gameport_menu_=operationMenu->addMenu(tr("ジョイパッド…"));
	gameport0_group_=new QActionGroup(this);
	gameport0_menu_=gameport_menu_->addMenu(QString());
	gameport1_group_=new QActionGroup(this);
	gameport1_menu_=gameport_menu_->addMenu(QString());
	connect(gameport0_group_,&QActionGroup::triggered,this,[this](QAction *action){
		if(nullptr!=action)
		{
			setGamePort(0,action->data().toUInt());
		}
	});
	connect(gameport1_group_,&QActionGroup::triggered,this,[this](QAction *action){
		if(nullptr!=action)
		{
			setGamePort(1,action->data().toUInt());
		}
	});
	connect(operationMenu,&QMenu::aboutToShow,this,&MainWindow::syncGamePortMenus);
	syncGamePortMenus();

	operationMenu->addSeparator();

	cdrom_speed_menu_=operationMenu->addMenu(QString());
	cdrom_speed_group_=new QActionGroup(this);
	cdrom_speed_group_->setExclusive(true);
	const struct {int speed; const char *label;} cd_presets[]={
	    {0,"標準"},{2,"2倍"},{4,"4倍"},{8,"8倍"},{16,"最大"},
	};
	for(const auto &preset : cd_presets)
	{
		auto *action=cdrom_speed_menu_->addAction(tr(preset.label));
		action->setCheckable(true);
		action->setData(preset.speed);
		cdrom_speed_group_->addAction(action);
		connect(action,&QAction::triggered,this,[this,speed=preset.speed]{
			setCdSpeed(speed);
		});
	}
	syncCdSpeedMenuTitle();

	sprite_menu_=operationMenu->addMenu(QString());
	sprite_dma_group_=new QActionGroup(this);
	sprite_dma_group_->setExclusive(true);
	const struct {int mode; const char *label;} sprite_presets[]={
	    {0,"標準"},{1,"倍速"},{2,"最大"},
	};
	for(const auto &preset : sprite_presets)
	{
		auto *action=sprite_menu_->addAction(tr(preset.label));
		action->setCheckable(true);
		action->setData(preset.mode);
		sprite_dma_group_->addAction(action);
		connect(action,&QAction::triggered,this,[this,mode=preset.mode]{
			setSpriteTransferMode(mode);
		});
	}
	syncSpriteMenuTitle();
	syncFastModeMenu();

	fullscreen_action_=operationMenu->addAction(tr("全画面切替(&F)"));
	fullscreen_action_->setCheckable(true);
	fullscreen_action_->setChecked(isFullScreen());
	addAction(fullscreen_action_);
	connect(fullscreen_action_,&QAction::triggered,this,&MainWindow::toggleFullScreen);

	operationMenu->addSeparator();

	auto *quitAction=operationMenu->addAction(tr("終了(&Q)"));
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction,&QAction::triggered,this,&QWidget::close);

	auto *diskMenu=menuBar()->addMenu(tr("ディスク(&D)"));
	disk_menu_=diskMenu;
	open_cd_action_=diskMenu->addAction(tr("CDイメージを開く(&O)…"));
	connect(open_cd_action_,&QAction::triggered,this,&MainWindow::openCdImage);
	auto *ejectCdAction=diskMenu->addAction(tr("CDを取り出す(&E)"));
	connect(ejectCdAction,&QAction::triggered,this,[this]{
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(controller_,"ejectCd",Qt::QueuedConnection);
		}
	});
	cd_recent_menu_=diskMenu->addMenu(tr("最近のファイルを開く(&R)"));
	connect(cd_recent_menu_,&QMenu::aboutToShow,this,&MainWindow::rebuildRecentCdMenu);

	diskMenu->addSeparator();
	for(int drive=0; drive<2; ++drive)
	{
		if(0<drive)
		{
			diskMenu->addSeparator();
		}
		open_fd_action_[drive]=diskMenu->addAction(tr("FDイメージを開く(%1)…").arg(drive));
		connect(open_fd_action_[drive],&QAction::triggered,this,[this,drive]{
			openFdImage(drive);
		});
		auto *eject_fd_action=diskMenu->addAction(tr("FDを取り出す"));
		connect(eject_fd_action,&QAction::triggered,this,[this,drive]{
			if(nullptr!=controller_)
			{
				QMetaObject::invokeMethod(
				    controller_,
				    "ejectFd",
				    Qt::QueuedConnection,
				    Q_ARG(int,drive));
			}
		});
		fd_recent_menu_[drive]=diskMenu->addMenu(tr("最近のファイルを開く"));
		connect(fd_recent_menu_[drive],&QMenu::aboutToShow,this,[this,drive]{
			rebuildRecentFdMenu(drive);
		});
		fd_write_protect_[drive]=diskMenu->addAction(tr("書き込み禁止"));
		fd_write_protect_[drive]->setCheckable(true);
		fd_write_protect_[drive]->setChecked(TownsQtSettings::fdWriteProtect(drive));
		connect(fd_write_protect_[drive],&QAction::toggled,this,[this,drive](bool checked){
			if(syncing_fd_write_protect_menu_ || nullptr==controller_)
			{
				return;
			}
			QMetaObject::invokeMethod(
			    controller_,
			    "setFdWriteProtect",
			    Qt::QueuedConnection,
			    Q_ARG(int,drive),
			    Q_ARG(bool,checked));
		});
	}
	diskMenu->addSeparator();
	auto *create_blank_fd_action=diskMenu->addAction(tr("ブランクFDイメージ作成"));
	connect(create_blank_fd_action,&QAction::triggered,this,&MainWindow::createBlankFdImage);
	connect(disk_menu_,&QMenu::aboutToShow,this,&MainWindow::syncFdWriteProtectMenuChecks);

	auto *toolsMenu=menuBar()->addMenu(tr("ツール(&T)"));
	auto *settingsAction=toolsMenu->addAction(tr("設定(&S)…"));
	connect(settingsAction,&QAction::triggered,this,&MainWindow::openSettingsDialog);
	toolsMenu->addSeparator();
	drive_access_action_=toolsMenu->addAction(tr("ドライブアクセス"));
	drive_access_action_->setCheckable(true);
	drive_access_action_->setChecked(TownsQtSettings::showDriveAccessOverlay());
	connect(drive_access_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowDriveAccessOverlay(enabled);
		applyDriveAccessVisibility();
	});
	midi_monitor_action_=toolsMenu->addAction(tr("MIDI モニタ"));
	midi_monitor_action_->setCheckable(true);
	midi_monitor_action_->setChecked(TownsQtSettings::midiMonitor());
	connect(midi_monitor_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setMidiMonitor(enabled);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setMidiMonitor",
			    Qt::QueuedConnection,
			    Q_ARG(bool,enabled));
		}
	});
	auto *driveDebugMenu=toolsMenu->addMenu(tr("デバッグ表示"));
	drive_access_debug_action_=driveDebugMenu->addAction(tr("ランプ状態"));
	drive_access_debug_action_->setCheckable(true);
	drive_access_debug_action_->setChecked(TownsQtSettings::showDriveAccessDebug());
	connect(drive_access_debug_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowDriveAccessDebug(enabled);
		applyDriveAccessVisibility();
		if(enabled)
		{
			updateDriveAccessDebugDisplay();
		}
	});
	mouse_debug_action_=driveDebugMenu->addAction(tr("マウス統合座標"));
	mouse_debug_action_->setCheckable(true);
	mouse_debug_action_->setChecked(TownsQtSettings::showMouseIntegrationDebug());
	connect(mouse_debug_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowMouseIntegrationDebug(enabled);
		applyMouseDebugVisibility();
		if(enabled)
		{
			updateMouseDebugDisplay();
		}
	});

	auto *helpMenu=menuBar()->addMenu(tr("ヘルプ(&H)"));
	auto *aboutAction=helpMenu->addAction(AboutTsugaruTitleText());
	connect(aboutAction,&QAction::triggered,this,&MainWindow::showAboutDialog);
}

void MainWindow::setCpuFastModeEnabled(bool enabled)
{
	TownsQtSettings::setCpuFastModeEnabled(enabled);
	syncFastModeMenu();
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "applyCpuFastMode",
		    Qt::QueuedConnection,
		    Q_ARG(bool,enabled));
	}
}

void MainWindow::setCpuFrequencyMhz(int mhz)
{
	TownsQtSettings::setCpuFrequencyMhz(mhz);
	syncMenuChecks();
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "setCpuFrequencyMhz",
		    Qt::QueuedConnection,
		    Q_ARG(int,mhz));
	}
}

void MainWindow::setCdSpeed(int speed)
{
	speed=std::max(0,speed);
	TownsQtSettings::setCdSpeed(speed);
	syncCdSpeedMenuTitle();
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "setCdSpeed",
		    Qt::QueuedConnection,
		    Q_ARG(int,speed));
	}
}

void MainWindow::setSpriteTransferMode(int mode)
{
	mode=std::clamp(mode,0,2);
	TownsQtSettings::setSpriteTransferMode(mode);
	syncSpriteMenuTitle();
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "applyDisplayOptions",
		    Qt::BlockingQueuedConnection,
		    Q_ARG(bool,TownsQtSettings::damperWireLine()),
		    Q_ARG(bool,TownsQtSettings::scanLineEffectIn15KHz()),
		    Q_ARG(int,mode));
	}
}

void MainWindow::setGamePort(int port,unsigned int emu)
{
	port=std::clamp(port,0,1);
	TownsQtSettings::setGamePort(port,emu);
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "applyPeripheralSettings",
		    Qt::QueuedConnection,
		    Q_ARG(unsigned int,TownsQtSettings::gamePort(0)),
		    Q_ARG(unsigned int,TownsQtSettings::gamePort(1)),
		    Q_ARG(int,TownsQtSettings::maxButtonHoldTimeMs(0,0)),
		    Q_ARG(int,TownsQtSettings::maxButtonHoldTimeMs(0,1)),
		    Q_ARG(int,TownsQtSettings::mouseIntegrationSpeed()),
		    Q_ARG(bool,TownsQtSettings::considerVRAMOffsetInMouseIntegration()),
		    Q_ARG(bool,TownsQtSettings::differentialMouseIntegration()),
		    Q_ARG(int,TownsQtSettings::mouseMinX()),
		    Q_ARG(int,TownsQtSettings::mouseMinY()),
		    Q_ARG(int,TownsQtSettings::mouseMaxX()),
		    Q_ARG(int,TownsQtSettings::mouseMaxY()));
	}
	syncGamePortMenus();
}

void MainWindow::syncGamePortMenus()
{
	QMenu *menus[2]={gameport0_menu_,gameport1_menu_};
	QActionGroup *groups[2]={gameport0_group_,gameport1_group_};
	const unsigned int fallbacks[2]={TOWNS_GAMEPORTEMU_PHYSICAL0,TOWNS_GAMEPORTEMU_MOUSE};
	for(int port=0; port<2; ++port)
	{
		if(nullptr==menus[port] || nullptr==groups[port])
		{
			continue;
		}
		unsigned int emu=TownsQtSettings::gamePort(port);
		TownsQtGamePortOptions::PopulateMenu(menus[port],groups[port],emu);
		if(!groups[port]->checkedAction())
		{
			emu=fallbacks[port];
			TownsQtGamePortOptions::PopulateMenu(menus[port],groups[port],emu);
		}
		menus[port]->menuAction()->setText(
		    QStringLiteral("%1  %2").arg(port).arg(TownsQtGamePortOptions::LabelForEmu(emu)));
	}
}

void MainWindow::syncMenuChecks()
{
	syncFastModeMenu();
	if(nullptr!=cpu_clock_group_)
	{
		const int mhz=TownsQtSettings::cpuFrequencyMhz();
		for(QAction *action : cpu_clock_group_->actions())
		{
			if(action==cpu_clock_custom_action_)
			{
				continue;
			}
			action->setChecked(action->data().toInt()==mhz);
		}
		if(nullptr!=cpu_clock_custom_action_)
		{
			cpu_clock_custom_action_->setChecked(!IsCpuFrequencyPreset(mhz));
		}
	}
	if(nullptr!=sprite_dma_group_)
	{
		const int mode=TownsQtSettings::spriteTransferMode();
		for(QAction *action : sprite_dma_group_->actions())
		{
			action->setChecked(action->data().toInt()==mode);
		}
		syncSpriteMenuTitle();
	}
	if(nullptr!=cdrom_speed_group_)
	{
		const int speed=TownsQtSettings::cdSpeed();
		for(QAction *action : cdrom_speed_group_->actions())
		{
			action->setChecked(action->data().toInt()==speed);
		}
		syncCdSpeedMenuTitle();
	}
	if(nullptr!=fullscreen_action_)
	{
		fullscreen_action_->setChecked(fullscreen_ || isFullScreen());
	}
	applyDriveAccessVisibility();
	applyMouseDebugVisibility();
}

void MainWindow::syncFastModeMenu()
{
	const bool supports_fast=TownsQtModelGroupSupportsFastMode(TownsQtSettings::modelGroupIndex());
	if(!supports_fast && TownsQtSettings::cpuFastModeEnabled())
	{
		TownsQtSettings::setCpuFastModeEnabled(false);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "applyCpuFastMode",
			    Qt::QueuedConnection,
			    Q_ARG(bool,false));
		}
	}

	const bool fast_mode=TownsQtSettings::cpuFastModeEnabled();
	if(nullptr!=fast_mode_action_)
	{
		QSignalBlocker blocker(fast_mode_action_);
		fast_mode_action_->setChecked(fast_mode);
		fast_mode_action_->setEnabled(supports_fast);
	}
	if(nullptr!=cpu_clock_group_)
	{
		for(QAction *action : cpu_clock_group_->actions())
		{
			action->setEnabled(supports_fast && fast_mode);
		}
	}
}

void MainWindow::syncGuestFastModeFromEmulator()
{
	if(nullptr==controller_)
	{
		syncFastModeMenu();
		return;
	}
	bool fast_mode=TownsQtSettings::cpuFastModeEnabled();
	QMetaObject::invokeMethod(
	    controller_,
	    "fastModeLamp",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(bool,fast_mode));
	onGuestFastModeLampChanged(fast_mode);
}

void MainWindow::onGuestFastModeLampChanged(bool fast_mode)
{
	if(nullptr!=fast_mode_action_)
	{
		QSignalBlocker blocker(fast_mode_action_);
		fast_mode_action_->setChecked(fast_mode);
	}
	if(TownsQtSettings::cpuFastModeEnabled()!=fast_mode)
	{
		TownsQtSettings::setCpuFastModeEnabled(fast_mode);
	}
	syncFastModeMenu();
}

void MainWindow::syncCdSpeedMenuTitle()
{
	if(nullptr==cdrom_speed_menu_)
	{
		return;
	}
	static const char *labels[]={"標準","2倍","4倍","8倍","最大"};
	const int speed=TownsQtSettings::cdSpeed();
	const char *label="標準";
	switch(speed)
	{
	case 0:
		label=labels[0];
		break;
	case 2:
		label=labels[1];
		break;
	case 4:
		label=labels[2];
		break;
	case 8:
		label=labels[3];
		break;
	default:
		if(16<=speed)
		{
			label=labels[4];
		}
		break;
	}
	cdrom_speed_menu_->menuAction()->setText(
	    tr("CDROM速度  %1").arg(tr(label)));
}

void MainWindow::syncSpriteMenuTitle()
{
	if(nullptr==sprite_menu_)
	{
		return;
	}
	const int mode=std::clamp(TownsQtSettings::spriteTransferMode(),0,2);
	static const char *labels[]={"標準","倍速","最大"};
	sprite_menu_->menuAction()->setText(
	    tr("スプライト転送  %1").arg(tr(labels[mode])));
}

void MainWindow::openSettingsDialog()
{
	SettingsDialog::Values initial;
	initial.cpuFrequencyMhz=TownsQtSettings::cpuFrequencyMhz();
	initial.memSizeInMB=TownsQtSettings::memSizeInMB();
	initial.cpuHighFidelity=TownsQtSettings::cpuHighFidelity();
	initial.pretend386DX=TownsQtSettings::pretend386DX();
	initial.useFPU=TownsQtSettings::useFPU();
	initial.fastScsi=TownsQtSettings::fastScsi();
	initial.midiBoard=TownsQtSettings::midiBoard();
	initial.modelGroupIndex=TownsQtSettings::modelGroupIndex();
	initial.displayScale=TownsQtSettings::displayScale();
	initial.autoScaling=TownsQtSettings::autoScaling();
	initial.maintainAspect=TownsQtSettings::maintainAspect();
	initial.damperWireLine=TownsQtSettings::damperWireLine();
	initial.scanLineEffectIn15KHz=TownsQtSettings::scanLineEffectIn15KHz();
	initial.fullscreenVsync=TownsQtSettings::fullscreenVsync();
	initial.spriteTransferMode=TownsQtSettings::spriteTransferMode();
	initial.pcmResampleHighQuality=TownsQtSettings::pcmResampleHighQuality();
	initial.audioBackend=TownsQtSettings::audioBackend();
	initial.audioDevice=TownsQtSettings::audioDevice();
	initial.fmVolumePercent=TownsQtSettings::fmVolumePercent();
	initial.pcmVolumePercent=TownsQtSettings::pcmVolumePercent();
	initial.cddaVolumePercent=TownsQtSettings::cddaVolumePercent();
	initial.midiVolumePercent=TownsQtSettings::midiVolumePercent();
	initial.midiSoundFont=TownsQtSettings::midiSoundFont();
	initial.midiOutput=TownsQtSettings::midiOutput();
	initial.pcmLpfEnabled=TownsQtSettings::pcmLpfEnabled();
	initial.pcmLpfCutoffHz=TownsQtSettings::pcmLpfCutoffHz();
	initial.waylandIdleInhibit=TownsQtSettings::waylandIdleInhibit();
	initial.gamePort0=TownsQtSettings::gamePort(0);
	initial.gamePort1=TownsQtSettings::gamePort(1);
	initial.maxButtonHoldTimeMs0=TownsQtSettings::maxButtonHoldTimeMs(0,0);
	initial.maxButtonHoldTimeMs1=TownsQtSettings::maxButtonHoldTimeMs(0,1);
	initial.mouseIntegrationSpeed=TownsQtSettings::mouseIntegrationSpeed();
	initial.considerVRAMOffsetInMouseIntegration=TownsQtSettings::considerVRAMOffsetInMouseIntegration();
	initial.differentialMouseIntegration=TownsQtSettings::differentialMouseIntegration();
	initial.snapMouseIntegration=TownsQtSettings::snapMouseIntegration();
	initial.snapMouseWarmupFrames=TownsQtSettings::snapMouseWarmupFrames();
	initial.mouseMinX=TownsQtSettings::mouseMinX();
	initial.mouseMinY=TownsQtSettings::mouseMinY();
	initial.mouseMaxX=TownsQtSettings::mouseMaxX();
	initial.mouseMaxY=TownsQtSettings::mouseMaxY();
	initial.appSpecificSetting=TownsQtSettings::appSpecificSetting();

	const QString rom_dir=argv_.ROMPath.empty() ?
	    TownsQtPaths::romsDir() :
	    QString::fromStdString(argv_.ROMPath);
	SettingsDialog dlg(initial,rom_dir,this);
	connect(&dlg,&SettingsDialog::settingsApplied,this,&MainWindow::applySettings);
	if(QDialog::Accepted!=dlg.exec())
	{
		return;
	}
	applySettings(dlg.values());
}

void MainWindow::showAboutDialog()
{
	QDialog dlg(this);
	dlg.setWindowTitle(AboutTsugaruTitleText());

	auto *root=new QVBoxLayout(&dlg);
	auto *row=new QHBoxLayout;
	row->setSpacing(16);

	auto *iconLabel=new QLabel(&dlg);
	iconLabel->setScaledContents(false);
	iconLabel->setAlignment(Qt::AlignTop|Qt::AlignHCenter);
	{
		QPixmap iconPix(QStringLiteral(":/icons/tsugaru_128.png"));
		const qreal dpr=dlg.devicePixelRatioF();
		// 1:1 device pixels — do not resample; DPR marks logical size.
		iconPix.setDevicePixelRatio(dpr);
		iconLabel->setPixmap(iconPix);
		iconLabel->setFixedSize(
		    qRound(static_cast<qreal>(iconPix.width())/dpr),
		    qRound(static_cast<qreal>(iconPix.height())/dpr));
	}
	row->addWidget(iconLabel,0,Qt::AlignTop);

	auto *textLabel=new QLabel(&dlg);
	textLabel->setTextFormat(Qt::RichText);
	textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	textLabel->setOpenExternalLinks(true);
	textLabel->setAlignment(Qt::AlignLeft|Qt::AlignTop);
	textLabel->setText(QStringLiteral(
	    "<div style=\"line-height:1.35\">"
	    "<b>Tsugaru \"津軽\"</b><br>"
	    "FM TOWNS / Marty エミュレータ<br>"
	    "for Linux (Qt) v20260522-qt 0.5.0"
	    "<br><br>"
	    "<a href=\"https://github.com/3d4m0t0/TOWNSEMU\">https://github.com/3d4m0t0/TOWNSEMU</a>"
	    "<br><br>"
	    "%1<br>"
	    "<a href=\"https://github.com/captainys/TOWNSEMU\">https://github.com/captainys/TOWNSEMU</a>"
	    "</div>")
	    .arg(QCoreApplication::translate("MainWindow","オリジナル")));
	row->addWidget(textLabel,1);
	root->addLayout(row);

	auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok,&dlg);
	connect(buttons,&QDialogButtonBox::accepted,&dlg,&QDialog::accept);
	root->addWidget(buttons);

	dlg.exec();
}

void MainWindow::applySettings(const SettingsDialog::Values &values)
{
	const unsigned int prev_app_specific=argv_.appSpecificSetting;
	const int prev_model_group=TownsQtSettings::modelGroupIndex();
	SettingsDialog::Values effective=values;
	const QString rom_dir=argv_.ROMPath.empty() ?
	    TownsQtPaths::romsDir() :
	    QString::fromStdString(argv_.ROMPath);
	const TownsQtSysRomProfile sys_rom_profile=TownsQtRomAvailability::ClassifySysRom(rom_dir);
	const bool marty_ex_rom=TownsQtRomAvailability::MartyExRomPresent(rom_dir);
	if(!TownsQtRomAvailability::ModelGroupAllowedForSysRom(
	       effective.modelGroupIndex,
	       sys_rom_profile,
	       marty_ex_rom))
	{
		effective.modelGroupIndex=
		    TownsQtRomAvailability::PreferredModelGroupForSysRom(sys_rom_profile);
		statusBar()->showMessage(
		    tr("SYS ROM と合わないモデルが選ばれていたため、既定のモデルに戻しました。"),
		    8000);
	}
	const bool model_changed=(effective.modelGroupIndex!=prev_model_group);
	const QString prev_audio_backend=TownsQtSettings::audioBackend();
	const QString prev_audio_device=TownsQtSettings::audioDevice();
	const bool needs_emu_restart=
	    (prev_app_specific!=effective.appSpecificSetting) ||
	    MachineSettingsNeedRestart(effective);
	TownsQtSettings::setDisplayScale(effective.displayScale);
	TownsQtSettings::setAutoScaling(effective.autoScaling);
	TownsQtSettings::setMaintainAspect(effective.maintainAspect);
	TownsQtSettings::setFullscreenVsync(effective.fullscreenVsync);
	TownsQtSettings::setPcmResampleHighQuality(effective.pcmResampleHighQuality);
	TownsQtSettings::setAudioBackend(effective.audioBackend);
	TownsQtSettings::setAudioDevice(effective.audioDevice);
	TownsQtSettings::setFmVolumePercent(effective.fmVolumePercent);
	TownsQtSettings::setPcmVolumePercent(effective.pcmVolumePercent);
	TownsQtSettings::setCddaVolumePercent(effective.cddaVolumePercent);
	TownsQtSettings::setPcmLpfEnabled(effective.pcmLpfEnabled);
	TownsQtSettings::setPcmLpfCutoffHz(effective.pcmLpfCutoffHz);
	TownsQtSettings::setWaylandIdleInhibit(effective.waylandIdleInhibit);
	TownsQtSettings::setModelGroupIndex(effective.modelGroupIndex);
	TownsQtSettings::setMemSizeInMB(
	    std::min(effective.memSizeInMB,TownsQtModelGroupMaxMemMb(effective.modelGroupIndex)));
	TownsQtSettings::setCpuHighFidelity(effective.cpuHighFidelity);
	TownsQtSettings::setPretend386DX(effective.pretend386DX);
	TownsQtSettings::setUseFPU(effective.useFPU);
	TownsQtSettings::setFastScsi(effective.fastScsi);
	TownsQtSettings::setMidiBoard(effective.midiBoard);
	argv_.nMidiCards=effective.midiBoard ? 1 : 0;
	TownsQtSettings::setMidiSoundFont(effective.midiSoundFont);
	TownsQtSettings::setMidiVolumePercent(effective.midiVolumePercent);
	TownsQtSettings::setMidiOutput(effective.midiOutput);
#if defined(__linux__)
	{
		const QByteArray sf_utf8=effective.midiSoundFont.toUtf8();
		MidiFluidSynthHost::SetSoundFontPath(sf_utf8.constData());
		MidiFluidSynthHost::SetMasterVolumePercent(effective.midiVolumePercent);
	}
#endif
	TownsQtSettings::setDamperWireLine(effective.damperWireLine);
	TownsQtSettings::setScanLineEffectIn15KHz(effective.scanLineEffectIn15KHz);
	if(model_changed)
	{
		TownsQtSettings::setSpriteTransferMode(0);
		TownsQtSettings::setCdSpeed(TownsQtModelGroupDefaultCdSpeed(effective.modelGroupIndex));
		if(!TownsQtModelGroupSupportsFastMode(effective.modelGroupIndex))
		{
			TownsQtSettings::setCpuFastModeEnabled(false);
		}
		QFile::remove(TownsQtPaths::cmosFilePath());
	}
	else
	{
		TownsQtSettings::setSpriteTransferMode(effective.spriteTransferMode);
	}
	TownsQtSettings::setGamePort(0,effective.gamePort0);
	TownsQtSettings::setGamePort(1,effective.gamePort1);
	TownsQtSettings::setMaxButtonHoldTimeMs(0,0,effective.maxButtonHoldTimeMs0);
	TownsQtSettings::setMaxButtonHoldTimeMs(0,1,effective.maxButtonHoldTimeMs1);
	TownsQtSettings::setMaxButtonHoldTimeMs(1,0,effective.maxButtonHoldTimeMs0);
	TownsQtSettings::setMaxButtonHoldTimeMs(1,1,effective.maxButtonHoldTimeMs1);
	TownsQtSettings::setMouseIntegrationSpeed(effective.mouseIntegrationSpeed);
	TownsQtSettings::setConsiderVRAMOffsetInMouseIntegration(effective.considerVRAMOffsetInMouseIntegration);
	TownsQtSettings::setDifferentialMouseIntegration(effective.differentialMouseIntegration);
	TownsQtSettings::setSnapMouseIntegration(effective.snapMouseIntegration);
	TownsQtSettings::setSnapMouseWarmupFrames(effective.snapMouseWarmupFrames);
	TownsQtSettings::setMouseMinX(effective.mouseMinX);
	TownsQtSettings::setMouseMinY(effective.mouseMinY);
	TownsQtSettings::setMouseMaxX(effective.mouseMaxX);
	TownsQtSettings::setMouseMaxY(effective.mouseMaxY);
	TownsQtSettings::setAppSpecificSetting(effective.appSpecificSetting);
	applyWindowScale(effective.displayScale,effective.autoScaling,effective.maintainAspect);
	view_->setFullscreenVsync(effective.fullscreenVsync && isFullScreen());
	applyFullscreenVsync();
	syncWaylandIdleInhibit();
	syncMenuChecks();
	syncGamePortMenus();
	if(!needs_emu_restart && model_changed)
	{
		setCdSpeed(TownsQtSettings::cdSpeed());
		setSpriteTransferMode(0);
		if(!TownsQtModelGroupSupportsFastMode(effective.modelGroupIndex) && nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "applyCpuFastMode",
			    Qt::QueuedConnection,
			    Q_ARG(bool,false));
		}
	}

	if(!needs_emu_restart && nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		const int fm_vol=TownsQtSettings::fmChipVolume();
		const int pcm_vol=TownsQtSettings::pcmChipVolume();
		QMetaObject::invokeMethod(
		    controller_,
		    "applyAudioVolumes",
		    Qt::QueuedConnection,
		    Q_ARG(int,fm_vol),
		    Q_ARG(int,pcm_vol),
		    Q_ARG(int,effective.cddaVolumePercent),
		    Q_ARG(bool,effective.pcmLpfEnabled),
		    Q_ARG(int,effective.pcmLpfCutoffHz),
		    Q_ARG(bool,effective.pcmResampleHighQuality));
		const bool audio_output_changed=
		    prev_audio_backend!=effective.audioBackend ||
		    prev_audio_device!=effective.audioDevice;
		if(audio_output_changed)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "restartAudioOutput",
			    Qt::QueuedConnection);
		}
		QMetaObject::invokeMethod(
		    controller_,
		    "applyMidiBoard",
		    Qt::QueuedConnection,
		    Q_ARG(bool,effective.midiBoard));
	}
	if(nullptr!=controller_ && !needs_emu_restart && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "applyDisplayOptions",
		    Qt::QueuedConnection,
		    Q_ARG(bool,effective.damperWireLine),
		    Q_ARG(bool,effective.scanLineEffectIn15KHz),
		    Q_ARG(int,TownsQtSettings::spriteTransferMode()));
		QMetaObject::invokeMethod(
		    controller_,
		    "applyPeripheralSettings",
		    Qt::QueuedConnection,
		    Q_ARG(unsigned int,effective.gamePort0),
		    Q_ARG(unsigned int,effective.gamePort1),
		    Q_ARG(int,effective.maxButtonHoldTimeMs0),
		    Q_ARG(int,effective.maxButtonHoldTimeMs1),
		    Q_ARG(int,effective.mouseIntegrationSpeed),
		    Q_ARG(bool,effective.considerVRAMOffsetInMouseIntegration),
		    Q_ARG(bool,effective.differentialMouseIntegration),
		    Q_ARG(int,effective.mouseMinX),
		    Q_ARG(int,effective.mouseMinY),
		    Q_ARG(int,effective.mouseMaxX),
		    Q_ARG(int,effective.mouseMaxY));
		QMetaObject::invokeMethod(
		    controller_,
		    "applySnapMouseSettings",
		    Qt::QueuedConnection,
		    Q_ARG(bool,effective.snapMouseIntegration),
		    Q_ARG(int,effective.snapMouseWarmupFrames));
		syncDifferentialMouseCursor();
	}
	if(needs_emu_restart)
	{
		argv_.appSpecificSetting=effective.appSpecificSetting;
		if(emu_restarting_)
		{
			emu_restart_pending_=true;
		}
		else
		{
			QTimer::singleShot(0,this,[this]{
				scheduleRestartEmulator();
			});
		}
	}
}

void MainWindow::openCdImage()
{
	QString startDir;
	if(!cd_path_.isEmpty())
	{
		startDir=QFileInfo(cd_path_).absolutePath();
	}
	else if(!argv_.cdImgFName.empty())
	{
		startDir=QFileInfo(QString::fromStdString(argv_.cdImgFName)).absolutePath();
	}
	else
	{
		startDir=TownsQtPaths::configDir();
	}

	const QString path=QFileDialog::getOpenFileName(
	    this,
	    tr("CDイメージを開く"),
	    startDir,
	    tr("CD images (*.cue *.iso *.bin *.mds *.chd);;All files (*)"));
	if(path.isEmpty() || nullptr==controller_)
	{
		return;
	}
	Q_EMIT cdLoadRequested(path);
}

void MainWindow::openFdImage(int drive)
{
	drive=std::clamp(drive,0,1);
	QString startDir;
	if(!fd_path_[drive].isEmpty())
	{
		startDir=QFileInfo(fd_path_[drive]).absolutePath();
	}
	else if(!argv_.fdImgFName[drive].empty())
	{
		startDir=QFileInfo(QString::fromStdString(argv_.fdImgFName[drive])).absolutePath();
	}
	else
	{
		startDir=TownsQtPaths::configDir();
	}

	const QString path=QFileDialog::getOpenFileName(
	    this,
	    tr("FDイメージを開く(%1)").arg(drive),
	    startDir,
	    tr("Floppy images (*.d77 *.xdf *.hdm *.fdd *.bin);;All files (*)"));
	if(path.isEmpty() || nullptr==controller_)
	{
		return;
	}
	Q_EMIT fdLoadRequested(drive,path);
}

void MainWindow::createBlankFdImage()
{
	if(nullptr==controller_)
	{
		return;
	}
	const QString path=CreateBlankFdImagePath();
	if(path.isEmpty())
	{
		QMessageBox::warning(
		    this,
		    tr("ブランクFDイメージ作成"),
		    tr("FDイメージの作成に失敗しました。"));
		return;
	}
	Q_EMIT fdLoadRequested(0,path);
}

void MainWindow::rebuildRecentCdMenu()
{
	if(nullptr==cd_recent_menu_)
	{
		return;
	}
	cd_recent_menu_->clear();
	const QStringList paths=TownsQtSettings::recentCdImagePaths();
	for(const QString &path : paths)
	{
		const QFileInfo info(path);
		auto *action=cd_recent_menu_->addAction(info.fileName().isEmpty() ? path : info.fileName());
		action->setToolTip(path);
		action->setData(path);
		action->setEnabled(QFile::exists(path));
		connect(action,&QAction::triggered,this,[this,path]{
			if(nullptr!=controller_)
			{
				Q_EMIT cdLoadRequested(path);
			}
		});
	}
	if(!paths.isEmpty())
	{
		cd_recent_menu_->addSeparator();
	}
	auto *clear_action=cd_recent_menu_->addAction(tr("リストを消す"));
	connect(clear_action,&QAction::triggered,this,&MainWindow::clearRecentCdList);
}

void MainWindow::rebuildRecentFdMenu(int drive)
{
	drive=std::clamp(drive,0,1);
	if(nullptr==fd_recent_menu_[drive])
	{
		return;
	}
	fd_recent_menu_[drive]->clear();
	const QStringList paths=TownsQtSettings::recentFdImagePaths(drive);
	for(const QString &path : paths)
	{
		const QFileInfo info(path);
		auto *action=fd_recent_menu_[drive]->addAction(info.fileName().isEmpty() ? path : info.fileName());
		action->setToolTip(path);
		action->setData(path);
		action->setEnabled(QFile::exists(path));
		connect(action,&QAction::triggered,this,[this,drive,path]{
			if(nullptr!=controller_)
			{
				Q_EMIT fdLoadRequested(drive,path);
			}
		});
	}
	if(!paths.isEmpty())
	{
		fd_recent_menu_[drive]->addSeparator();
	}
	auto *clear_action=fd_recent_menu_[drive]->addAction(tr("リストを消す"));
	connect(clear_action,&QAction::triggered,this,[this,drive]{
		clearRecentFdList(drive);
	});
}

void MainWindow::clearRecentCdList()
{
	TownsQtSettings::clearRecentCdImagePaths();
	rebuildRecentCdMenu();
}

void MainWindow::clearRecentFdList(int drive)
{
	TownsQtSettings::clearRecentFdImagePaths(drive);
	rebuildRecentFdMenu(drive);
}

void MainWindow::syncFdWriteProtectMenuChecks()
{
	if(nullptr==controller_)
	{
		return;
	}
	syncing_fd_write_protect_menu_=true;
	for(int drive=0; drive<2; ++drive)
	{
		if(nullptr==fd_write_protect_[drive])
		{
			continue;
		}
		bool write_protect=TownsQtSettings::fdWriteProtect(drive);
		QMetaObject::invokeMethod(
		    controller_,
		    "fdWriteProtected",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,write_protect),
		    Q_ARG(int,drive));
		fd_write_protect_[drive]->setChecked(write_protect);
	}
	syncing_fd_write_protect_menu_=false;
}

void MainWindow::updateOpenCdMenuLabel()
{
	if(nullptr==open_cd_action_)
	{
		return;
	}
	if(cd_path_.isEmpty())
	{
		open_cd_action_->setText(tr("CDイメージを開く(&O)…"));
		open_cd_action_->setToolTip(QString());
	}
	else
	{
		const QFileInfo info(cd_path_);
		open_cd_action_->setText(info.fileName().isEmpty() ? cd_path_ : info.fileName());
		open_cd_action_->setToolTip(cd_path_);
	}
}

void MainWindow::updateOpenFdMenuLabel(int drive)
{
	drive=std::clamp(drive,0,1);
	if(nullptr==open_fd_action_[drive])
	{
		return;
	}
	if(fd_path_[drive].isEmpty())
	{
		open_fd_action_[drive]->setText(tr("FDイメージを開く(%1)…").arg(drive));
		open_fd_action_[drive]->setToolTip(QString());
	}
	else
	{
		const QFileInfo info(fd_path_[drive]);
		open_fd_action_[drive]->setText(info.fileName().isEmpty() ? fd_path_[drive] : info.fileName());
		open_fd_action_[drive]->setToolTip(fd_path_[drive]);
	}
}

void MainWindow::onCdPathChanged(const QString &path)
{
	cd_path_=path;
	updateOpenCdMenuLabel();
	if(path.isEmpty())
	{
		statusBar()->showMessage(tr("CDを取り出しました"),3000);
	}
	else
	{
		statusBar()->showMessage(tr("CD: %1").arg(path),5000);
	}
}

void MainWindow::onFdPathChanged(int drive,const QString &path)
{
	drive=std::clamp(drive,0,1);
	fd_path_[drive]=path;
	updateOpenFdMenuLabel(drive);
	if(path.isEmpty())
	{
		statusBar()->showMessage(tr("FD%1を取り出しました").arg(drive),3000);
	}
	else
	{
		statusBar()->showMessage(tr("FD%1: %2").arg(drive).arg(path),5000);
	}
}

void MainWindow::onFdWriteProtectChanged(int drive,bool write_protect)
{
	drive=std::clamp(drive,0,1);
	if(nullptr==fd_write_protect_[drive])
	{
		return;
	}
	syncing_fd_write_protect_menu_=true;
	fd_write_protect_[drive]->setChecked(write_protect);
	syncing_fd_write_protect_menu_=false;
}

void MainWindow::onPollTimer()
{
	if(nullptr!=view_)
	{
		view_->pollMousePosition();
	}
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(controller_,&EmulatorController::pollWindow,Qt::BlockingQueuedConnection);
		if(nullptr!=view_)
		{
			drive_access_status_=controller_->driveAccessStatus();
			view_->updateDriveAccessIndicators(drive_access_status_);
		}
	}
	const bool was_differential=cached_differential_integration_;
	cached_differential_integration_=queryDifferentialMouseIntegration();
	if(was_differential!=cached_differential_integration_)
	{
		if(cached_differential_integration_)
		{
			host_cursor_hide_by_click_=false;
		}
		updateBlankCursor();
		if(!cached_differential_integration_)
		{
			inputQueue_.CancelCursorWarp();
		}
	}
	updateBlankCursor();
	if(fullscreen_ && !cached_differential_integration_)
	{
		updateFullscreenNormalIntegrationChrome();
	}
	updateMouseDebugDisplay();
	updateDriveAccessDebugDisplay();
}

void MainWindow::applyDriveAccessVisibility()
{
	const bool overlay=TownsQtSettings::showDriveAccessOverlay();
	if(nullptr!=view_)
	{
		view_->setDriveAccessOverlayEnabled(overlay);
	}
	if(nullptr!=drive_access_action_ && drive_access_action_->isChecked()!=overlay)
	{
		drive_access_action_->blockSignals(true);
		drive_access_action_->setChecked(overlay);
		drive_access_action_->blockSignals(false);
	}

	const bool debug=TownsQtSettings::showDriveAccessDebug();
	if(nullptr!=drive_debug_label_)
	{
		drive_debug_label_->setVisible(debug);
	}
	if(nullptr!=drive_access_debug_action_ && drive_access_debug_action_->isChecked()!=debug)
	{
		drive_access_debug_action_->blockSignals(true);
		drive_access_debug_action_->setChecked(debug);
		drive_access_debug_action_->blockSignals(false);
	}
}

void MainWindow::updateDriveAccessDebugDisplay()
{
	if(nullptr==drive_debug_label_ || !TownsQtSettings::showDriveAccessDebug())
	{
		return;
	}

	auto lampText=[](bool busy)->QString
	{
		return busy ? QStringLiteral("BUSY") : QStringLiteral("IDL");
	};

	QString text=QStringLiteral("CD:%1").arg(lampText(drive_access_status_.cdAccessLamp));
	for(int fd=0; fd<2; ++fd)
	{
		text+=QStringLiteral(" FD%1:%2").arg(fd).arg(lampText(drive_access_status_.fdAccessLamp[fd]));
	}
	for(int hdd=0; hdd<6; ++hdd)
	{
		text+=QStringLiteral(" HDD%1:%2").arg(hdd).arg(lampText(drive_access_status_.scsiAccessLamp[hdd]));
	}
	drive_debug_label_->setText(text);
}

void MainWindow::applyMouseDebugVisibility()
{
	const bool show=TownsQtSettings::showMouseIntegrationDebug();
	if(nullptr!=mouse_debug_label_)
	{
		mouse_debug_label_->setVisible(show);
	}
	if(nullptr!=mouse_debug_action_ && mouse_debug_action_->isChecked()!=show)
	{
		mouse_debug_action_->blockSignals(true);
		mouse_debug_action_->setChecked(show);
		mouse_debug_action_->blockSignals(false);
	}
}

void MainWindow::updateMouseDebugDisplay()
{
	if(nullptr==mouse_debug_label_ || !TownsQtSettings::showMouseIntegrationDebug())
	{
		return;
	}

	int host_x=0,host_y=0;
	if(nullptr!=view_)
	{
		const QPoint host=view_->hostMouseEmuCoords();
		host_x=host.x();
		host_y=host.y();
	}

	QString guest_text=QStringLiteral("-,-");
	QString mos_text=QStringLiteral("-,-");
	QString tbios_text=QStringLiteral("-,-");
	QString meta_text;
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		QVariantMap guest;
		QMetaObject::invokeMethod(
		    controller_,
		    "guestMouseCoords",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(QVariantMap,guest));
		if(guest.value(QStringLiteral("valid")).toBool())
		{
			guest_text=QStringLiteral("%1,%2")
			               .arg(guest.value(QStringLiteral("x")).toInt())
			               .arg(guest.value(QStringLiteral("y")).toInt());
		}
		mos_text=QStringLiteral("%1,%2")
		             .arg(guest.value(QStringLiteral("mos_x")).toInt())
		             .arg(guest.value(QStringLiteral("mos_y")).toInt());
		tbios_text=QStringLiteral("%1,%2")
		               .arg(guest.value(QStringLiteral("tbios_x")).toInt())
		               .arg(guest.value(QStringLiteral("tbios_y")).toInt());
		const unsigned int app_value=guest.value(QStringLiteral("app_specific")).toUInt();
		const auto &app_profile=TownsQtAppProfileAt(TownsQtAppProfileIndexForApp(app_value));
		meta_text=QStringLiteral("BIOS:%1 TB:%2 %3 W:%4")
		              .arg(guest.value(QStringLiteral("mouse_bios")).toBool() ? 1 : 0)
		              .arg(guest.value(QStringLiteral("tbios_version")).toUInt())
		              .arg(QString::fromUtf8(app_profile.label))
		              .arg(guest.value(QStringLiteral("snap_warmup")).toInt());
	}

	mouse_debug_label_->setText(
	    tr("ホスト:%1,%2  ゲスト:%3  MOS:%4  TBIOS:%5  %6")
	        .arg(host_x)
	        .arg(host_y)
	        .arg(guest_text)
	        .arg(mos_text)
	        .arg(tbios_text)
	        .arg(meta_text));
}

void MainWindow::onFrameReady()
{
	if(!isVisible())
	{
		show();
	}
	if(cached_differential_integration_)
	{
		processPendingMouseWarp();
	}
}

void MainWindow::onStatsUpdated(double fps,double emu_hz,int queue_depth,int capture_queue_depth,int present_lag)
{
	setWindowTitle(tr("TownsQt — %1 FPS | %2 Hz | P:%3 C:%4 lag:%5")
	                   .arg(fps,0,'f',1)
	                   .arg(emu_hz,0,'f',2)
	                   .arg(queue_depth)
	                   .arg(capture_queue_depth)
	                   .arg(present_lag));
}

void MainWindow::onFailed(const QString &message)
{
	statusBar()->showMessage(message,0);
	QMessageBox::critical(this,tr("TownsQt"),message);
}

void MainWindow::onControllerFinished()
{
	if(emu_restarting_)
	{
		return;
	}
	poll_timer_.stop();
	statusBar()->showMessage(tr("エミュレータを停止しました"),3000);
}

void MainWindow::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);
	if(!emu_started_)
	{
		emu_started_=true;
		startEmulator();
	}
	if(!fullscreen_)
	{
		ensureMenuBarDocked();
	}
	syncWaylandIdleInhibit();
}

void MainWindow::changeEvent(QEvent *event)
{
	QMainWindow::changeEvent(event);
	if(QEvent::WindowStateChange==event->type())
	{
		const bool fs=isFullScreen();
		if(nullptr!=fullscreen_action_)
		{
			fullscreen_action_->setChecked(fs);
		}
		if(fs!=fullscreen_)
		{
			if(fs)
			{
				windowed_geometry_=geometry();
				fullscreen_=true;
				applyFullscreenLayout();
			}
			else
			{
				fullscreen_=false;
				applyWindowedLayout();
			}
		}
		applyFullscreenVsync();
		syncWaylandIdleInhibit();
	}
	else if(QEvent::ActivationChange==event->type())
	{
		syncDifferentialMouseCursor();
		if(!isActiveWindow())
		{
			inputQueue_.CancelCursorWarp();
		}
	}
}

bool MainWindow::eventFilter(QObject *watched,QEvent *event)
{
	if(QEvent::KeyPress==event->type() && isActiveWindow() && nullptr==QApplication::activeModalWidget())
	{
		const auto *key_event=static_cast<const QKeyEvent *>(event);
		if(!key_event->isAutoRepeat())
		{
			const QWidget *widget=qobject_cast<const QWidget *>(watched);
			if(nullptr!=widget && (widget==this || isAncestorOf(widget)))
			{
				const bool alt_enter=
				    (Qt::AltModifier==(key_event->modifiers()&Qt::KeyboardModifierMask)) &&
				    (Qt::Key_Return==key_event->key() || Qt::Key_Enter==key_event->key());
				if(alt_enter || Qt::Key_F11==key_event->key())
				{
					toggleFullScreen();
					return true;
				}
				if(Qt::Key_Escape==key_event->key() && host_cursor_hide_by_click_ && !fullscreen_)
				{
					releaseHostCursorHide();
				}
			}
		}
	}
	if(fullscreen_ && QEvent::MouseMove==event->type())
	{
		const QPoint global=static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
		if(isMouseInsideWindow(global) && isSignificantFullscreenMouseMove(global))
		{
			last_fullscreen_mouse_global_=global;
			have_last_fullscreen_mouse_global_=true;
			if(cached_differential_integration_)
			{
				noteFullscreenMouseActivity(global);
			}
			else
			{
				updateFullscreenNormalIntegrationChrome();
				updateBlankCursor();
			}
		}
	}
	return false;
}

void MainWindow::toggleFullScreen()
{
	const bool enter_fullscreen=!(fullscreen_ || isFullScreen());
	if(enter_fullscreen)
	{
		windowed_geometry_=geometry();
		fullscreen_=true;
		applyFullscreenLayout();
	}
	else
	{
		fullscreen_=false;
		applyWindowedLayout();
	}
	if(nullptr!=fullscreen_action_)
	{
		fullscreen_action_->setChecked(fullscreen_);
	}
	applyFullscreenVsync();
	syncWaylandIdleInhibit();
}

void MainWindow::applyFullscreenLayout()
{
	if(statusBar())
	{
		statusBar()->hide();
	}
	if(nullptr!=fullscreen_chrome_hide_timer_)
	{
		fullscreen_chrome_hide_timer_->stop();
	}
	host_cursor_hide_by_click_=false;
	cached_differential_integration_=queryDifferentialMouseIntegration();
	ensureMenuBarDocked();
	have_last_fullscreen_mouse_global_=false;
	last_fullscreen_mouse_move_ms_=0;
	last_fullscreen_menubar_show_ms_=0;
	showFullscreenCursor();
	if(menuBar())
	{
		menuBar()->hide();
	}
	showFullScreen();
	applyWindowScale(
	    TownsQtSettings::displayScale(),
	    TownsQtSettings::autoScaling(),
	    TownsQtSettings::maintainAspect());
	if(nullptr!=view_)
	{
		view_->setFocus();
	}
	if(cached_differential_integration_)
	{
		scheduleFullscreenChromeHide();
	}
	else
	{
		updateFullscreenNormalIntegrationChrome();
		updateBlankCursor();
	}
}

void MainWindow::applyWindowedLayout()
{
	if(nullptr!=fullscreen_chrome_hide_timer_)
	{
		fullscreen_chrome_hide_timer_->stop();
	}
	have_last_fullscreen_mouse_global_=false;
	last_fullscreen_mouse_move_ms_=0;
	last_fullscreen_menubar_show_ms_=0;
	showFullscreenCursor();
	ensureMenuBarDocked();
	showNormal();
	if(!windowed_geometry_.isNull())
	{
		setGeometry(windowed_geometry_);
	}
	applyWindowScale(
	    TownsQtSettings::displayScale(),
	    TownsQtSettings::autoScaling(),
	    TownsQtSettings::maintainAspect());
	if(statusBar())
	{
		statusBar()->show();
	}
	if(nullptr!=view_)
	{
		view_->setFocus();
	}
}

void MainWindow::scheduleFullscreenChromeHide()
{
	if(!fullscreen_ || nullptr==fullscreen_chrome_hide_timer_)
	{
		return;
	}
	fullscreen_chrome_hide_timer_->start(kFullscreenChromeHideMs);
}

bool MainWindow::isSignificantFullscreenMouseMove(const QPoint &global_pos) const
{
	if(!have_last_fullscreen_mouse_global_)
	{
		return true;
	}
	const QPoint delta=global_pos-last_fullscreen_mouse_global_;
	return (delta.x()*delta.x()+delta.y()*delta.y())>=
	       kFullscreenMouseMoveThresholdPx*kFullscreenMouseMoveThresholdPx;
}

void MainWindow::showFullscreenCursor()
{
	if(!fullscreen_cursor_hidden_)
	{
		return;
	}
	fullscreen_cursor_hidden_=false;
	updateBlankCursor();
}

void MainWindow::hideFullscreenCursor()
{
	if(!fullscreen_ || fullscreen_cursor_hidden_)
	{
		return;
	}
	fullscreen_cursor_hidden_=true;
	updateBlankCursor();
}

void MainWindow::ensureMenuBarDocked()
{
	QMenuBar *mb=menuBar();
	if(nullptr==mb)
	{
		return;
	}
	if(mb->parentWidget()!=this)
	{
		mb->hide();
		setMenuBar(mb);
	}
	mb->setMouseTracking(true);
	if(!fullscreen_)
	{
		mb->show();
	}
}

void MainWindow::noteFullscreenMouseActivity(const QPoint &global_pos)
{
	if(!fullscreen_)
	{
		return;
	}

	const qint64 now=QDateTime::currentMSecsSinceEpoch();
	last_fullscreen_mouse_move_ms_=now;
	last_fullscreen_mouse_global_=global_pos;
	have_last_fullscreen_mouse_global_=true;
	showFullscreenCursor();

	QMenuBar *mb=menuBar();
	if(nullptr==mb)
	{
		return;
	}
	ensureMenuBarDocked();
	if(!mb->isVisible())
	{
		mb->show();
		last_fullscreen_menubar_show_ms_=now;
	}
	scheduleFullscreenChromeHide();
}

void MainWindow::hideFullscreenChrome()
{
	if(!fullscreen_)
	{
		return;
	}
	if(isAnyMenuVisible())
	{
		scheduleFullscreenChromeHide();
		return;
	}

	const qint64 now=QDateTime::currentMSecsSinceEpoch();
	if(now-last_fullscreen_mouse_move_ms_<kFullscreenChromeHideMs)
	{
		scheduleFullscreenChromeHide();
		return;
	}
	if(now-last_fullscreen_menubar_show_ms_<kFullscreenChromeToggleMs)
	{
		scheduleFullscreenChromeHide();
		return;
	}

	QMenuBar *mb=menuBar();
	if(nullptr!=mb && mb->isVisible())
	{
		mb->hide();
	}
	if(cached_differential_integration_)
	{
		hideFullscreenCursor();
	}
	if(nullptr!=fullscreen_chrome_hide_timer_)
	{
		fullscreen_chrome_hide_timer_->stop();
	}
}

bool MainWindow::isMouseInsideWindow(const QPoint &global_pos) const
{
	if(!isVisible())
	{
		return false;
	}
	return rect().contains(mapFromGlobal(global_pos));
}

void MainWindow::processPendingMouseWarp()
{
	if(!cached_differential_integration_ ||
	   !isActiveWindow() ||
	   nullptr!=QApplication::activeModalWidget() ||
	   nullptr==view_)
	{
		inputQueue_.CancelCursorWarp();
		return;
	}

	const QPoint local=view_->hostCursorInView();
	if(!view_->rect().contains(local))
	{
		inputQueue_.CancelCursorWarp();
		return;
	}

	int view_x=0,view_y=0;
	if(true!=inputQueue_.TakeCursorWarp(view_x,view_y))
	{
		return;
	}
	QCursor::setPos(view_->mapToGlobal(QPoint(view_x,view_y)));
}

bool MainWindow::queryDifferentialMouseIntegration() const
{
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		bool enabled=false;
		QMetaObject::invokeMethod(
		    controller_,
		    "differentialMouseIntegration",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,enabled));
		return enabled;
	}
	return TownsQtSettings::differentialMouseIntegration();
}

void MainWindow::noteEmuPictureClicked()
{
	if(cached_differential_integration_ || fullscreen_)
	{
		return;
	}
	host_cursor_hide_by_click_=true;
	updateBlankCursor();
}

void MainWindow::releaseHostCursorHide()
{
	if(!host_cursor_hide_by_click_)
	{
		return;
	}
	host_cursor_hide_by_click_=false;
	updateBlankCursor();
}

void MainWindow::syncDifferentialMouseCursor()
{
	cached_differential_integration_=queryDifferentialMouseIntegration();
	updateBlankCursor();
}

void MainWindow::updateBlankCursor()
{
	const bool active=
	    isVisible() &&
	    isActiveWindow() &&
	    nullptr==QApplication::activeModalWidget();
	const bool differential=cached_differential_integration_ && active;

	bool want_blank=differential;
	if(!differential && active && nullptr!=view_)
	{
		const QPoint view_pos=view_->hostCursorInView();
		if(fullscreen_)
		{
			if(isCursorNearFullscreenMenu())
			{
				want_blank=false;
			}
			else if(view_->isPointOnEmuPicture(view_pos))
			{
				want_blank=true;
			}
		}
		else if(host_cursor_hide_by_click_ &&
		        view_->isPointOnEmuPicture(view_pos) &&
		        !isCursorOverUiChrome())
		{
			want_blank=true;
		}
	}

	if(want_blank==host_cursor_blank_)
	{
		return;
	}
	host_cursor_blank_=want_blank;
	if(nullptr!=view_)
	{
		view_->setHostCursorBlank(want_blank);
	}
}

void MainWindow::updateFullscreenNormalIntegrationChrome()
{
	if(!fullscreen_ || cached_differential_integration_)
	{
		return;
	}

	if(isCursorNearFullscreenMenu())
	{
		if(nullptr!=fullscreen_chrome_hide_timer_)
		{
			fullscreen_chrome_hide_timer_->stop();
		}
		ensureMenuBarDocked();
		if(QMenuBar *mb=menuBar())
		{
			if(!mb->isVisible())
			{
				mb->show();
			}
		}
		return;
	}

	if(nullptr==view_)
	{
		return;
	}

	const QPoint view_pos=view_->hostCursorInView();
	if(!view_->isPointOnEmuPicture(view_pos))
	{
		return;
	}

	if(nullptr!=fullscreen_chrome_hide_timer_)
	{
		fullscreen_chrome_hide_timer_->stop();
	}
	if(QMenuBar *mb=menuBar())
	{
		if(mb->isVisible())
		{
			mb->hide();
		}
	}
}

bool MainWindow::isCursorNearFullscreenMenu() const
{
	if(isAnyMenuVisible())
	{
		return true;
	}

	QPoint global;
	if(QGuiApplication::platformName()==QLatin1String("wayland"))
	{
		const QWidget *top=window();
		if(nullptr==top)
		{
			return false;
		}
		global=top->mapToGlobal(QCursor::pos());
	}
	else
	{
		const QScreen *scr=screen();
		global=(nullptr!=scr) ? QCursor::pos(scr) : QCursor::pos();
	}

	if(QMenuBar *mb=menuBar())
	{
		if(mb->isVisible() && mb->rect().contains(mb->mapFromGlobal(global)))
		{
			return true;
		}
		const QPoint local=mapFromGlobal(global);
		const int zone_h=std::max(mb->sizeHint().height(),24);
		if(0<=local.y() && local.y()<zone_h)
		{
			return true;
		}
	}
	return false;
}

bool MainWindow::isCursorOverUiChrome() const
{
	if(isAnyMenuVisible())
	{
		return true;
	}

	QPoint global;
	if(QGuiApplication::platformName()==QLatin1String("wayland"))
	{
		const QWidget *top=window();
		if(nullptr==top)
		{
			return false;
		}
		global=top->mapToGlobal(QCursor::pos());
	}
	else
	{
		const QScreen *scr=screen();
		global=(nullptr!=scr) ? QCursor::pos(scr) : QCursor::pos();
	}

	if(QMenuBar *mb=menuBar())
	{
		if(mb->isVisible() && mb->rect().contains(mb->mapFromGlobal(global)))
		{
			return true;
		}
	}
	if(QStatusBar *sb=statusBar())
	{
		if(sb->isVisible() && sb->rect().contains(sb->mapFromGlobal(global)))
		{
			return true;
		}
	}
	return false;
}

bool MainWindow::isAnyMenuVisible() const
{
	if(nullptr==menuBar())
	{
		return false;
	}
	QList<const QMenu *> pending;
	for(QAction *action : menuBar()->actions())
	{
		if(QMenu *menu=action->menu())
		{
			pending.append(menu);
		}
	}
	while(!pending.isEmpty())
	{
		const QMenu *menu=pending.takeFirst();
		if(menu->isVisible())
		{
			return true;
		}
		for(const QAction *action : menu->actions())
		{
			if(const QMenu *sub=action->menu())
			{
				pending.append(sub);
			}
		}
	}
	return false;
}

void MainWindow::connectFullscreenMenuHooks()
{
	if(nullptr==menuBar())
	{
		return;
	}
	QList<QMenu *> pending;
	for(QAction *action : menuBar()->actions())
	{
		if(QMenu *menu=action->menu())
		{
			pending.append(menu);
		}
	}
	while(!pending.isEmpty())
	{
		QMenu *menu=pending.takeFirst();
		connect(menu,&QMenu::aboutToHide,this,[this]{
			if(fullscreen_)
			{
				scheduleFullscreenChromeHide();
			}
		});
		for(QAction *action : menu->actions())
		{
			if(QMenu *sub=action->menu())
			{
				pending.append(sub);
			}
		}
	}
}

void MainWindow::applyWindowScale(int scale,bool auto_scaling,bool maintain_aspect)
{
	scale=std::max(1,scale);
	argv_.scaling=static_cast<unsigned int>(scale)*100U;
	argv_.autoScaling=auto_scaling;
	argv_.maintainAspect=maintain_aspect;

	const bool effective_auto_scaling=
	    (fullscreen_ || isFullScreen()) ? true : auto_scaling;

	const int content_w=640*scale;
	const int content_h=480*scale;
	const int chrome_w=16;
	const int chrome_h=menuBar()->sizeHint().height()+statusBar()->sizeHint().height()+16;
	const int window_w=content_w+chrome_w;
	const int window_h=content_h+chrome_h;

	if(nullptr!=view_)
	{
		view_->setMinimumSize(0,0);
		view_->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
		view_->setVideoOptions(scale,effective_auto_scaling,maintain_aspect);
		view_->setMinimumSize(content_w,content_h);
		view_->updateGeometry();
	}

	if(!fullscreen_ && !isFullScreen())
	{
		// Clear stale minimum sizes so x2->x1 can shrink the window.
		setMinimumSize(0,0);
		setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
		setMinimumSize(window_w,window_h);
		resize(window_w,window_h);
		windowed_geometry_=geometry();
	}
}

void MainWindow::applyFullscreenVsync()
{
	if(nullptr==view_)
	{
		return;
	}
	const bool enable=TownsQtSettings::fullscreenVsync() && isFullScreen();
	view_->setFullscreenVsync(enable);
}

void MainWindow::syncWaylandIdleInhibit()
{
	const bool want=TownsQtSettings::waylandIdleInhibit() && isVisible() && !isMinimized();
	TownsQtWaylandIdleInhibit::Apply(want ? windowHandle() : nullptr,want);
}

void MainWindow::cleanupStoppedEmulator(EmulatorController *stopping)
{
	if(nullptr!=stopping)
	{
		disconnect(stopping,nullptr,emu_thread_,nullptr);
		stopping->moveToThread(thread());
		delete stopping;
	}
	if(nullptr!=emu_thread_)
	{
		if(emu_thread_->isRunning())
		{
			emu_thread_->quit();
			emu_thread_->wait(30000);
		}
		delete emu_thread_;
		emu_thread_=nullptr;
	}
}

void MainWindow::completeEmulatorStop()
{
	emu_stop_in_progress_=false;
	std::function<void()> done=std::move(emu_stop_done_);
	emu_stop_done_=nullptr;
	if(done)
	{
		done();
	}
}

void MainWindow::stopEmulatorAsync(const std::function<void()> &on_stopped)
{
	if(emu_stop_in_progress_)
	{
		const std::function<void()> prev=std::move(emu_stop_done_);
		emu_stop_done_=[prev,on_stopped]{
			if(prev)
			{
				prev();
			}
			if(on_stopped)
			{
				on_stopped();
			}
		};
		return;
	}
	emu_stop_in_progress_=true;
	emu_stop_done_=on_stopped;

	poll_timer_.stop();
	EmulatorController *const stopping=controller_;
	controller_=nullptr;
	teardownEmulatorConnections();

	auto finishStop=[this,stopping]{
		cleanupStoppedEmulator(stopping);
		completeEmulatorStop();
	};

	if(nullptr==stopping && (nullptr==emu_thread_ || !emu_thread_->isRunning()))
	{
		finishStop();
		return;
	}

	if(nullptr!=stopping)
	{
		stopping->requestStop();
	}

	if(nullptr==emu_thread_ || !emu_thread_->isRunning())
	{
		finishStop();
		return;
	}

	connect(
	    emu_thread_,
	    &QThread::finished,
	    this,
	    finishStop,
	    Qt::SingleShotConnection);
}

void MainWindow::stopEmulator()
{
	if(nullptr==emu_thread_ && nullptr==controller_)
	{
		return;
	}
	QEventLoop loop;
	QTimer timeout;
	timeout.setSingleShot(true);
	timeout.setInterval(30000);
	connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
	stopEmulatorAsync([&loop]{
		loop.quit();
	});
	timeout.start();
	loop.exec();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	stopEmulator();
	QMainWindow::closeEvent(event);
}
