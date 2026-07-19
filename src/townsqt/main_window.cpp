#include "main_window.h"

#include "debug_text_window.h"
#include "emulator_controller.h"
#include "townsargv.h"
#include "townsqt_argv_from_settings.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_rom_availability.h"
#include "townsqt_app_profile.h"
#include "townsqt_settings.h"
#include "townsqt_version.h"
#include "townsqt_wayland_idle_inhibit.h"
#include "townsqt_wayland_relative_pointer.h"

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
#include <QSaveFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <iostream>
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
#include <QStringList>
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
	return QCoreApplication::translate("MainWindow","About Tsugaru %1")
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

bool CreateBlankFdImage(const QString &path)
{
	if(true!=TownsQtPaths::ensureLayout())
	{
		return false;
	}

	// Towns 2HD 1232KB blank (same template as CUI -GENFD).
	const auto raw=Get1232KBFloppyDiskImage();
	std::vector<unsigned char> image;
	const QString suffix=QFileInfo(path).suffix().toLower();
	if(QStringLiteral("d77")==suffix)
	{
		D77File d77;
		if(true!=d77.SetRawBinary(raw))
		{
			return false;
		}
		image=d77.MakeD77Image();
	}
	else
	{
		image=raw;
	}
	if(image.empty())
	{
		return false;
	}

	QSaveFile file(path);
	if(!file.open(QIODevice::WriteOnly))
	{
		return false;
	}
	if(image.size()!=static_cast<size_t>(file.write(reinterpret_cast<const char *>(image.data()),static_cast<qint64>(image.size()))))
	{
		file.cancelWriting();
		return false;
	}
	return file.commit();
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
	if(values.fastFd!=TownsQtSettings::fastFd())
	{
		return true;
	}
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		if(values.hdd[slot].enabled!=TownsQtSettings::hddEnabled(slot) ||
		   values.hdd[slot].path!=TownsQtSettings::hddImagePath(slot))
		{
			return true;
		}
	}
	return false;
}
}

MainWindow::MainWindow(const TownsARGV &argv,int scale,QWidget *parent)
	: QMainWindow(parent),argv_(argv)
{
	setWindowTitle(QStringLiteral("Tsugaru_QT"));
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
	const int window_w=content_w+16;
	const int window_h=content_h+menuBar()->sizeHint().height()+statusBar()->sizeHint().height()+16;
	setFixedSize(window_w,window_h);

	statusBar()->showMessage(tr("Starting…"));
	mouse_mode_label_=new QLabel(this);
	mouse_mode_label_->setTextFormat(Qt::PlainText);
	statusBar()->addPermanentWidget(mouse_mode_label_);
	// In the captured/released states the label alternates between the short state word and the
	// operation hint every few seconds to save width.
	mouse_mode_alternate_timer_=new QTimer(this);
	mouse_mode_alternate_timer_->setInterval(5000);
	connect(mouse_mode_alternate_timer_,&QTimer::timeout,this,[this]()
	{
		mouse_mode_phase_^=1;
		updateMouseModeIndicator();
	});
	mouse_mode_alternate_timer_->start();
	updateMouseModeIndicator();
	applyDriveAccessVisibility();
	applyMouseDebugVisibility();
	applyMidiMonitorVisibility();
	applyCpuDebugVisibility();
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
	// CMOS is applied after the VM thread starts; refresh FD1 menu enable state then.
	QTimer::singleShot(500,this,[this]{
		if(nullptr!=emu_thread_ && emu_thread_->isRunning())
		{
			syncFdDriveMenus();
		}
	});
}

void MainWindow::scheduleRestartEmulator()
{
	if(emu_restarting_)
	{
		emu_restart_pending_=true;
		return;
	}
	emu_restarting_=true;
	statusBar()->showMessage(tr("Restarting the emulator…"));
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
		statusBar()->showMessage(tr("Emulator restarted"),5000);
	});
}

void MainWindow::restartEmulator()
{
	scheduleRestartEmulator();
}

MainWindow::~MainWindow()
{
	TownsQtWaylandRelativePointer::Shutdown();
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
	auto *operationMenu=menuBar()->addMenu(tr("&Operation"));

	auto *resetAction=operationMenu->addAction(tr("&Reset"));
	resetAction->setShortcut(QKeySequence(Qt::Key_F12));
	resetAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(resetAction,&QAction::triggered,this,[this]{
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(controller_,"resetMachine",Qt::QueuedConnection);
		}
	});

	operationMenu->addSeparator();

	fast_mode_action_=operationMenu->addAction(tr("Fast mode"));
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
	cpu_clock_custom_action_=operationMenu->addAction(tr("Custom…"));
	cpu_clock_custom_action_->setCheckable(true);
	cpu_clock_custom_action_->setChecked(!IsCpuFrequencyPreset(current_mhz));
	cpu_clock_group_->addAction(cpu_clock_custom_action_);
	connect(cpu_clock_custom_action_,&QAction::triggered,this,[this]{
		const int current_mhz=TownsQtSettings::cpuFrequencyMhz();
		const int initial_mhz=IsCpuFrequencyPreset(current_mhz) ? 25 : current_mhz;
		bool ok=false;
		const int mhz=QInputDialog::getInt(
		    this,
		    tr("CPU frequency"),
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

	gameport_menu_=operationMenu->addMenu(tr("Game pads…"));
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
	    {0,QT_TR_NOOP("Normal")},{2,QT_TR_NOOP("2x")},{4,QT_TR_NOOP("4x")},
	    {8,QT_TR_NOOP("8x")},{16,QT_TR_NOOP("Max")},
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
	    {0,QT_TR_NOOP("Normal")},{1,QT_TR_NOOP("Double")},{2,QT_TR_NOOP("Max")},
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

	fullscreen_action_=operationMenu->addAction(tr("Toggle &fullscreen"));
	fullscreen_action_->setCheckable(true);
	fullscreen_action_->setChecked(isFullScreen());
	addAction(fullscreen_action_);
	connect(fullscreen_action_,&QAction::triggered,this,&MainWindow::toggleFullScreen);

	operationMenu->addSeparator();

	auto *quitAction=operationMenu->addAction(tr("&Quit"));
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction,&QAction::triggered,this,&QWidget::close);

	auto *diskMenu=menuBar()->addMenu(tr("&Disk"));
	disk_menu_=diskMenu;
	open_cd_action_=diskMenu->addAction(tr("&Open CD image…"));
	connect(open_cd_action_,&QAction::triggered,this,&MainWindow::openCdImage);
	eject_cd_action_=diskMenu->addAction(tr("&Eject CD"));
	connect(eject_cd_action_,&QAction::triggered,this,[this]{
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(controller_,"ejectCd",Qt::QueuedConnection);
		}
	});
	cd_recent_menu_=diskMenu->addMenu(tr("Open &recent files"));
	connect(cd_recent_menu_,&QMenu::aboutToShow,this,&MainWindow::rebuildRecentCdMenu);

	diskMenu->addSeparator();
	for(int drive=0; drive<2; ++drive)
	{
		if(0<drive)
		{
			diskMenu->addSeparator();
		}
		open_fd_action_[drive]=diskMenu->addAction(tr("Open FD image (%1)…").arg(drive));
		connect(open_fd_action_[drive],&QAction::triggered,this,[this,drive]{
			openFdImage(drive);
		});
		eject_fd_action_[drive]=diskMenu->addAction(tr("Eject FD"));
		connect(eject_fd_action_[drive],&QAction::triggered,this,[this,drive]{
			if(nullptr!=controller_)
			{
				QMetaObject::invokeMethod(
				    controller_,
				    "ejectFd",
				    Qt::QueuedConnection,
				    Q_ARG(int,drive));
			}
		});
		fd_recent_menu_[drive]=diskMenu->addMenu(tr("Open recent files"));
		connect(fd_recent_menu_[drive],&QMenu::aboutToShow,this,[this,drive]{
			rebuildRecentFdMenu(drive);
		});
		fd_write_protect_[drive]=diskMenu->addAction(tr("Write protect"));
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
	auto *create_blank_fd_action=diskMenu->addAction(tr("Create blank FD image"));
	connect(create_blank_fd_action,&QAction::triggered,this,&MainWindow::createBlankFdImage);
	connect(disk_menu_,&QMenu::aboutToShow,this,[this]{
		syncFdWriteProtectMenuChecks();
		syncFdDriveMenus();
		syncEjectMenus();
	});
	syncFdDriveMenus();
	syncEjectMenus();

	auto *toolsMenu=menuBar()->addMenu(tr("&Tools"));
	auto *settingsAction=toolsMenu->addAction(tr("&Settings…"));
	connect(settingsAction,&QAction::triggered,this,&MainWindow::openSettingsDialog);
	toolsMenu->addSeparator();
	drive_access_action_=toolsMenu->addAction(tr("Drive access"));
	drive_access_action_->setCheckable(true);
	drive_access_action_->setChecked(TownsQtSettings::showDriveAccessOverlay());
	connect(drive_access_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowDriveAccessOverlay(enabled);
		applyDriveAccessVisibility();
	});
	auto *debugMenu=toolsMenu->addMenu(tr("Debug overlay"));
	mouse_debug_action_=debugMenu->addAction(tr("Mouse integration coords"));
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
	midi_monitor_action_=debugMenu->addAction(tr("MIDI monitor"));
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
		applyMidiMonitorVisibility();
		if(enabled)
		{
			updateMidiMonitorDisplay();
		}
	});
	cpu_debug_action_=debugMenu->addAction(tr("CPU / CS:EIP history"));
	cpu_debug_action_->setCheckable(true);
	cpu_debug_action_->setChecked(TownsQtSettings::showCpuDebug());
	cpu_debug_action_->setToolTip(
	    tr("Live CPU registers, current instruction, CS:EIP history, and call stack. Enables the core debugger while open."));
	connect(cpu_debug_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowCpuDebug(enabled);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setCpuDebugMonitor",
			    Qt::QueuedConnection,
			    Q_ARG(bool,enabled));
		}
		applyCpuDebugVisibility();
		if(enabled)
		{
			updateCpuDebugDisplay();
		}
	});

	auto *helpMenu=menuBar()->addMenu(tr("&Help"));
	auto *aboutAction=helpMenu->addAction(AboutTsugaruTitleText());
	connect(aboutAction,&QAction::triggered,this,&MainWindow::showAboutDialog);

	syncMenuChecks();
}

void MainWindow::setCpuFastModeEnabled(bool enabled)
{
	TownsQtSettings::setCpuFastModeEnabled(enabled);
	syncMenuChecks();
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
		    Q_ARG(bool,TownsQtSettings::autoDifferentialOnMouseBIOSStop()),
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
	applyMidiMonitorVisibility();
	applyCpuDebugVisibility();
	syncFdDriveMenus();
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
		syncMenuChecks();
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
	syncMenuChecks();
}

void MainWindow::syncCdSpeedMenuTitle()
{
	if(nullptr==cdrom_speed_menu_)
	{
		return;
	}
	static const char *labels[]={
	    QT_TR_NOOP("Normal"),QT_TR_NOOP("2x"),QT_TR_NOOP("4x"),
	    QT_TR_NOOP("8x"),QT_TR_NOOP("Max"),
	};
	const int speed=TownsQtSettings::cdSpeed();
	const char *label="Normal";
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
	    tr("CD-ROM speed  %1").arg(tr(label)));
}

void MainWindow::syncSpriteMenuTitle()
{
	if(nullptr==sprite_menu_)
	{
		return;
	}
	const int mode=std::clamp(TownsQtSettings::spriteTransferMode(),0,2);
	static const char *labels[]={
	    QT_TR_NOOP("Normal"),QT_TR_NOOP("Double"),QT_TR_NOOP("Max"),
	};
	sprite_menu_->menuAction()->setText(
	    tr("Sprite transfer  %1").arg(tr(labels[mode])));
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
	initial.fastFd=TownsQtSettings::fastFd();
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
	initial.autoDifferentialOnMouseBIOSStop=TownsQtSettings::autoDifferentialOnMouseBIOSStop();
	initial.snapMouseIntegration=TownsQtSettings::snapMouseIntegration();
	initial.snapMouseWarmupFrames=TownsQtSettings::snapMouseWarmupFrames();
	initial.mouseMinX=TownsQtSettings::mouseMinX();
	initial.mouseMinY=TownsQtSettings::mouseMinY();
	initial.mouseMaxX=TownsQtSettings::mouseMaxX();
	initial.mouseMaxY=TownsQtSettings::mouseMaxY();
	initial.appSpecificSetting=TownsQtSettings::appSpecificSetting();
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		initial.hdd[slot].enabled=TownsQtSettings::hddEnabled(slot);
		initial.hdd[slot].path=TownsQtSettings::hddImagePath(slot);
		if(!initial.hdd[slot].enabled &&
		   initial.hdd[slot].path.isEmpty() &&
		   TownsStartParameters::SCSIIMAGE_HARDDISK==argv_.scsiImg[slot].imageType &&
		   !argv_.scsiImg[slot].imgFName.empty())
		{
			initial.hdd[slot].enabled=true;
			initial.hdd[slot].path=QString::fromStdString(argv_.scsiImg[slot].imgFName);
		}
	}

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
	textLabel->setText(tr(
	    "<div style=\"line-height:1.35\">"
	    "<b>Tsugaru \"津軽\"</b><br>"
	    "FM TOWNS / Marty emulator<br>"
	    "for Linux (Qt) %1"
	    "<br><br>"
	    "<a href=\"https://github.com/3d4m0t0/TOWNSEMU\">https://github.com/3d4m0t0/TOWNSEMU</a>"
	    "<br><br>"
	    "%2<br>"
	    "<a href=\"https://github.com/captainys/TOWNSEMU\">https://github.com/captainys/TOWNSEMU</a>"
	    "</div>")
	    .arg(QStringLiteral(TOWNSQT_VERSION),
	         tr("Original")));
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
		    tr("The selected model did not match the SYS ROM, so it was reset to the default."),
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
	TownsQtSettings::setFastFd(effective.fastFd);
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
	TownsQtSettings::setAutoDifferentialOnMouseBIOSStop(effective.autoDifferentialOnMouseBIOSStop);
	TownsQtSettings::setSnapMouseIntegration(effective.snapMouseIntegration);
	TownsQtSettings::setSnapMouseWarmupFrames(effective.snapMouseWarmupFrames);
	TownsQtSettings::setMouseMinX(effective.mouseMinX);
	TownsQtSettings::setMouseMinY(effective.mouseMinY);
	TownsQtSettings::setMouseMaxX(effective.mouseMaxX);
	TownsQtSettings::setMouseMaxY(effective.mouseMaxY);
	TownsQtSettings::setAppSpecificSetting(effective.appSpecificSetting);
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		TownsQtSettings::setHddEnabled(slot,effective.hdd[slot].enabled);
		TownsQtSettings::setHddImagePath(slot,effective.hdd[slot].path);
		if(TownsStartParameters::SCSIIMAGE_CDROM==argv_.scsiImg[slot].imageType)
		{
			continue;
		}
		if(effective.hdd[slot].enabled && !effective.hdd[slot].path.isEmpty())
		{
			argv_.scsiImg[slot].imageType=TownsStartParameters::SCSIIMAGE_HARDDISK;
			argv_.scsiImg[slot].imgFName=effective.hdd[slot].path.toStdString();
		}
		else
		{
			argv_.scsiImg[slot].imageType=TownsStartParameters::SCSIIMAGE_NONE;
			argv_.scsiImg[slot].imgFName.clear();
		}
	}
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
		    Q_ARG(bool,effective.autoDifferentialOnMouseBIOSStop),
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
	const QString startDir=TownsQtSettings::fileDialogStartDirectory(
	    !cd_path_.isEmpty()
	        ? cd_path_
	        : (!argv_.cdImgFName.empty()
	               ? QString::fromStdString(argv_.cdImgFName)
	               : QString()));

	const QString path=QFileDialog::getOpenFileName(
	    this,
	    tr("Open CD image"),
	    startDir,
	    tr("CD images (*.cue *.iso *.bin *.mds *.chd);;All files (*)"));
	if(path.isEmpty() || nullptr==controller_)
	{
		return;
	}
	TownsQtSettings::rememberFileDialogPath(path);
	Q_EMIT cdLoadRequested(path);
}

void MainWindow::openFdImage(int drive)
{
	drive=std::clamp(drive,0,1);
	if(nullptr!=open_fd_action_[drive] && !open_fd_action_[drive]->isEnabled())
	{
		return;
	}
	const QString startDir=TownsQtSettings::fileDialogStartDirectory(
	    !fd_path_[drive].isEmpty()
	        ? fd_path_[drive]
	        : (!argv_.fdImgFName[drive].empty()
	               ? QString::fromStdString(argv_.fdImgFName[drive])
	               : QString()));

	const QString path=QFileDialog::getOpenFileName(
	    this,
	    tr("Open FD image (%1)").arg(drive),
	    startDir,
	    tr("Floppy images (*.d77 *.xdf *.hdm *.fdd *.bin);;All files (*)"));
	if(path.isEmpty() || nullptr==controller_)
	{
		return;
	}
	TownsQtSettings::rememberFileDialogPath(path);
	Q_EMIT fdLoadRequested(drive,path);
}

void MainWindow::createBlankFdImage()
{
	if(nullptr==controller_)
	{
		return;
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		QMessageBox::warning(
		    this,
		    tr("Create blank FD image"),
		    tr("Failed to create the FD image."));
		return;
	}

	QString stem;
	if(!cd_path_.isEmpty())
	{
		stem=QFileInfo(cd_path_).completeBaseName();
	}
	else if(!argv_.cdImgFName.empty())
	{
		stem=QFileInfo(QString::fromStdString(argv_.cdImgFName)).completeBaseName();
	}
	if(stem.isEmpty())
	{
		stem=QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
	}

	QString path=QFileDialog::getSaveFileName(
	    this,
	    tr("Create blank FD image"),
	    TownsQtPaths::blankFdDir()+QStringLiteral("/")+stem+QStringLiteral(".d77"),
	    tr("Floppy images (*.d77 *.xdf *.hdm *.fdd *.bin);;All files (*)"),
	    nullptr,
	    QFileDialog::DontConfirmOverwrite);
	if(path.isEmpty())
	{
		return;
	}
	TownsQtSettings::rememberFileDialogPath(path);

	QString suffix=QFileInfo(path).suffix().toLower();
	if(suffix.isEmpty())
	{
		path+=QStringLiteral(".d77");
		suffix=QStringLiteral("d77");
	}
	static const QStringList supported{
	    QStringLiteral("d77"),
	    QStringLiteral("xdf"),
	    QStringLiteral("hdm"),
	    QStringLiteral("fdd"),
	    QStringLiteral("bin")};
	if(!supported.contains(suffix))
	{
		QMessageBox::warning(
		    this,
		    tr("Create blank FD image"),
		    tr("Unsupported floppy-image extension."));
		return;
	}

	if(QFileInfo::exists(path) &&
	   QMessageBox::Yes!=QMessageBox::question(
	       this,
	       tr("Create blank FD image"),
	       tr("The file already exists. Overwrite it?"),
	       QMessageBox::Yes|QMessageBox::No,
	       QMessageBox::No))
	{
		return;
	}

	if(true!=CreateBlankFdImage(path))
	{
		QMessageBox::warning(
		    this,
		    tr("Create blank FD image"),
		    tr("Failed to create the FD image."));
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
	auto *clear_action=cd_recent_menu_->addAction(tr("Clear list"));
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
	const QStringList paths=TownsQtSettings::recentFdImagePaths();
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
	auto *clear_action=fd_recent_menu_[drive]->addAction(tr("Clear list"));
	connect(clear_action,&QAction::triggered,this,&MainWindow::clearRecentFdList);
}

void MainWindow::clearRecentCdList()
{
	TownsQtSettings::clearRecentCdImagePaths();
	rebuildRecentCdMenu();
}

void MainWindow::clearRecentFdList()
{
	TownsQtSettings::clearRecentFdImagePaths();
	rebuildRecentFdMenu(0);
	rebuildRecentFdMenu(1);
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

void MainWindow::syncFdDriveMenus()
{
	for(int drive=0; drive<2; ++drive)
	{
		bool available=true;
		if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "fdDriveAvailable",
			    Qt::BlockingQueuedConnection,
			    Q_RETURN_ARG(bool,available),
			    Q_ARG(int,drive));
		}
		else
		{
			available=EmulatorController::QueryFdDriveAvailable(drive,nullptr);
		}

		fd_drive_available_[drive]=available;

		if(nullptr!=open_fd_action_[drive])
		{
			open_fd_action_[drive]->setEnabled(available);
		}
		if(nullptr!=eject_fd_action_[drive])
		{
			eject_fd_action_[drive]->setEnabled(available && !fd_path_[drive].isEmpty());
		}
		if(nullptr!=fd_recent_menu_[drive])
		{
			fd_recent_menu_[drive]->setEnabled(available);
		}
		if(nullptr!=fd_write_protect_[drive])
		{
			fd_write_protect_[drive]->setEnabled(available);
		}

		if(!available && !fd_path_[drive].isEmpty() && nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "ejectFd",
			    Qt::QueuedConnection,
			    Q_ARG(int,drive));
		}
	}
}

void MainWindow::syncEjectMenus()
{
	if(nullptr!=eject_cd_action_)
	{
		eject_cd_action_->setEnabled(!cd_path_.isEmpty());
	}
	for(int drive=0; drive<2; ++drive)
	{
		if(nullptr!=eject_fd_action_[drive])
		{
			eject_fd_action_[drive]->setEnabled(
			    fd_drive_available_[drive] && !fd_path_[drive].isEmpty());
		}
	}
}

void MainWindow::updateOpenCdMenuLabel()
{
	if(nullptr==open_cd_action_)
	{
		return;
	}
	if(cd_path_.isEmpty())
	{
		open_cd_action_->setText(tr("&Open CD image…"));
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
		open_fd_action_[drive]->setText(tr("Open FD image (%1)…").arg(drive));
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
	syncEjectMenus();
	if(path.isEmpty())
	{
		statusBar()->showMessage(tr("CD ejected"),3000);
	}
	else
	{
		statusBar()->showMessage(tr("CD: %1").arg(QFileInfo(path).fileName()),5000);
	}
}

void MainWindow::onFdPathChanged(int drive,const QString &path)
{
	drive=std::clamp(drive,0,1);
	fd_path_[drive]=path;
	updateOpenFdMenuLabel(drive);
	syncEjectMenus();
	if(path.isEmpty())
	{
		statusBar()->showMessage(tr("FD%1 ejected").arg(drive),3000);
	}
	else
	{
		statusBar()->showMessage(tr("FD%1: %2").arg(drive).arg(QFileInfo(path).fileName()),5000);
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
		if(shouldCaptureHostMouse())
		{
			view_->pollMousePosition();
		}
		else
		{
			// Other app / settings dialog / popup has focus: do not feed host cursor.
			inputQueue_.ClearMouseButtons();
		}
	}
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(controller_,&EmulatorController::pollWindow,Qt::BlockingQueuedConnection);
		if(nullptr!=view_)
		{
			drive_access_status_=controller_->driveAccessStatus();
			view_->updateDriveAccessIndicators(drive_access_status_,currentDriveAccessPresence());
		}
	}
	const bool was_differential=cached_differential_integration_;
	refreshMouseUiState();
	updateMouseFailsafeFromActivity();
	syncWaylandRelativePointer();
	if(was_differential!=cached_differential_integration_)
	{
		updateBlankCursor();
		if(!cached_differential_integration_ || cached_mouse_capture_released_)
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
	updateMidiMonitorDisplay();
	updateCpuDebugDisplay();
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
}

DriveAccessPresence MainWindow::currentDriveAccessPresence() const
{
	DriveAccessPresence presence;
	const bool marty=TownsQtModelGroupIsMarty(TownsQtSettings::modelGroupIndex());

	// Internal CD-ROM is always present on Towns / Marty hardware.
	presence.cd=true;
	for(int fd=0; fd<2; ++fd)
	{
		presence.fd[fd]=fd_drive_available_[fd];
	}
	if(!marty)
	{
		const int scsi_count=std::min(
		    6,
		    static_cast<int>(TownsStartParameters::MAX_NUM_SCSI_DEVICES));
		for(int hdd=0; hdd<scsi_count; ++hdd)
		{
			presence.hdd[hdd]=
			    (TownsStartParameters::SCSIIMAGE_NONE!=argv_.scsiImg[hdd].imageType) &&
			    !argv_.scsiImg[hdd].imgFName.empty();
		}
	}
	return presence;
}

void MainWindow::ensureMouseDebugWindow()
{
	if(nullptr!=mouse_debug_window_)
	{
		return;
	}
	mouse_debug_window_=new DebugTextWindow(tr("Mouse integration debug"),this);
	connect(mouse_debug_window_,&DebugTextWindow::windowClosed,this,[this](){
		TownsQtSettings::setShowMouseIntegrationDebug(false);
		applyMouseDebugVisibility();
	});
}

void MainWindow::applyMouseDebugVisibility()
{
	const bool show=TownsQtSettings::showMouseIntegrationDebug();
	if(nullptr!=view_)
	{
		view_->setMouseDebugCrosshair(show);
	}
	if(nullptr!=mouse_debug_action_ && mouse_debug_action_->isChecked()!=show)
	{
		mouse_debug_action_->blockSignals(true);
		mouse_debug_action_->setChecked(show);
		mouse_debug_action_->blockSignals(false);
	}
	if(show)
	{
		ensureMouseDebugWindow();
		if(nullptr!=mouse_debug_window_)
		{
			mouse_debug_window_->show();
			mouse_debug_window_->raise();
		}
	}
	else if(nullptr!=mouse_debug_window_)
	{
		mouse_debug_window_->hide();
	}
}

void MainWindow::updateMouseDebugDisplay()
{
	if(!TownsQtSettings::showMouseIntegrationDebug())
	{
		return;
	}
	ensureMouseDebugWindow();
	if(nullptr==mouse_debug_window_)
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
	QString version_text;
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		QVariantMap guest;
		QMetaObject::invokeMethod(
		    controller_,
		    "guestMouseCoords",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(QVariantMap,guest));
		version_text=QStringLiteral("SysROM:%1\nTBIOS:%2 %3 (TB:%4)\nTOS:%5\nMi:%6\nMo:%7")
		                 .arg(guest.value(QStringLiteral("sysrom")).toString())
		                 .arg(guest.value(QStringLiteral("tbios_id")).toString())
		                 .arg(guest.value(QStringLiteral("tbios_date")).toString())
		                 .arg(guest.value(QStringLiteral("tbios_version")).toUInt())
		                 .arg(guest.value(QStringLiteral("tos")).toString())
		                 .arg(guest.value(QStringLiteral("mi_words")).toString())
		                 .arg(guest.value(QStringLiteral("mo_words")).toString());
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
		QString hw_text;
		if(guest.value(QStringLiteral("hw_defined")).toBool())
		{
			hw_text=QStringLiteral(" HW:%1,%2")
			            .arg(guest.value(QStringLiteral("hw_x")).toInt())
			            .arg(guest.value(QStringLiteral("hw_y")).toInt());
		}
		// Keep each .arg() chain to %1..%9 — Qt replaces "%1" inside "%10" otherwise.
		meta_text=QStringLiteral("Raw:%1,%2 Ctrl:%3,%4 Org:%5,%6 Zm:%7,%8 Pg:%9")
		              .arg(guest.value(QStringLiteral("raw_x")).toInt())
		              .arg(guest.value(QStringLiteral("raw_y")).toInt())
		              .arg(guest.value(QStringLiteral("ctrl_x")).toInt())
		              .arg(guest.value(QStringLiteral("ctrl_y")).toInt())
		              .arg(guest.value(QStringLiteral("org_x")).toInt())
		              .arg(guest.value(QStringLiteral("org_y")).toInt())
		              .arg(guest.value(QStringLiteral("zoom_x")).toInt())
		              .arg(guest.value(QStringLiteral("zoom_y")).toInt())
		              .arg(guest.value(QStringLiteral("page")).toInt());
		meta_text+=QStringLiteral("\nBIOS:%1 TB:%2 %3 W:%4 Spr:%5,%6 Sk:%7")
		               .arg(guest.value(QStringLiteral("mouse_bios")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("tbios_version")).toUInt())
		               .arg(QString::fromUtf8(app_profile.label))
		               .arg(guest.value(QStringLiteral("snap_warmup")).toInt())
		               .arg(guest.value(QStringLiteral("spr_h")).toInt())
		               .arg(guest.value(QStringLiteral("spr_v")).toInt())
		               .arg(guest.value(QStringLiteral("hskip")).toInt());
		meta_text+=QStringLiteral("\nDiff:%1 Forced:%2 Pref:%3 CapRel:%4 Feed:%5")
		               .arg(guest.value(QStringLiteral("diff_eff")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("diff_forced")).toBool() ? 1 : 0)
		               .arg(TownsQtSettings::differentialMouseIntegration() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("capture_released")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("feeding")).toBool() ? 1 : 0);
		meta_text+=QStringLiteral("\nSc:%1,%2 n:%3 Sp:%4 Nr:%5,%6 Hs:%7,%8")
		               .arg(guest.value(QStringLiteral("spr_cx")).toInt())
		               .arg(guest.value(QStringLiteral("spr_cy")).toInt())
		               .arg(guest.value(QStringLiteral("spr_n")).toInt())
		               .arg(guest.value(QStringLiteral("spen")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("spr_nx")).toInt())
		               .arg(guest.value(QStringLiteral("spr_ny")).toInt())
		               .arg(guest.value(QStringLiteral("spr_hx")).toInt())
		               .arg(guest.value(QStringLiteral("spr_hy")).toInt());
		meta_text+=QStringLiteral("\nVo:%1,%2 Vo1:%3,%4 Fa0:%5,%6 Hot:%7,%8")
		               .arg(guest.value(QStringLiteral("voff_x")).toInt())
		               .arg(guest.value(QStringLiteral("voff_y")).toInt())
		               .arg(guest.value(QStringLiteral("voff1_x")).toInt())
		               .arg(guest.value(QStringLiteral("voff1_y")).toInt())
		               .arg(guest.value(QStringLiteral("fa0_0")).toInt())
		               .arg(guest.value(QStringLiteral("fa0_1")).toInt())
		               .arg(guest.value(QStringLiteral("hot_x")).toInt())
		               .arg(guest.value(QStringLiteral("hot_y")).toInt());
		meta_text+=QStringLiteral("\nCp:%1,%2")
		               .arg(guest.value(QStringLiteral("cp_x")).toInt())
		               .arg(guest.value(QStringLiteral("cp_y")).toInt());
		meta_text+=QStringLiteral("\nO0:%1 O1:%2 Sk0:%3 Sk1:%4 1p:%5 Sh:%6,%7")
		               .arg(guest.value(QStringLiteral("org0_x")).toInt())
		               .arg(guest.value(QStringLiteral("org1_x")).toInt())
		               .arg(guest.value(QStringLiteral("hskip0")).toInt())
		               .arg(guest.value(QStringLiteral("hskip1")).toInt())
		               .arg(guest.value(QStringLiteral("single_page")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("show0")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("show1")).toBool() ? 1 : 0);
		meta_text+=QStringLiteral("\nZ0:%1,%2 Z1:%3,%4 Sz0:%5 Sz1:%6 HalfOff:%7")
		               .arg(guest.value(QStringLiteral("zoom0_x")).toInt())
		               .arg(guest.value(QStringLiteral("zoom0_y")).toInt())
		               .arg(guest.value(QStringLiteral("zoom1_x")).toInt())
		               .arg(guest.value(QStringLiteral("zoom1_y")).toInt())
		               .arg(guest.value(QStringLiteral("psize0_x")).toInt())
		               .arg(guest.value(QStringLiteral("psize1_x")).toInt())
		               .arg(guest.value(QStringLiteral("ctrl_x")).toInt()/2);
		meta_text+=QStringLiteral("\nSnap:%1,%2 Rpr:%3")
		               .arg(guest.value(QStringLiteral("snap_valid")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("snap_applied")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("xor_repair")).toInt());
		meta_text+=QStringLiteral("\nPv:%1,%2 Pt:%3,%4%5")
		               .arg(guest.value(QStringLiteral("mi_prev_x")).toInt())
		               .arg(guest.value(QStringLiteral("mi_prev_y")).toInt())
		               .arg(guest.value(QStringLiteral("mi_paint_x")).toInt())
		               .arg(guest.value(QStringLiteral("mi_paint_y")).toInt())
		               .arg(hw_text);
	}

	const QString body=QStringLiteral("%1\nHost:%2,%3\nGuest:%4\nMOS:%5\nTBIOS:%6\n%7")
	                       .arg(version_text)
	                       .arg(host_x)
	                       .arg(host_y)
	                       .arg(guest_text)
	                       .arg(mos_text)
	                       .arg(tbios_text)
	                       .arg(meta_text);
	mouse_debug_window_->setLiveText(body);
}

void MainWindow::ensureMidiMonitorWindow()
{
	if(nullptr!=midi_monitor_window_)
	{
		return;
	}
	midi_monitor_window_=new DebugTextWindow(tr("MIDI monitor"),this);
	connect(midi_monitor_window_,&DebugTextWindow::windowClosed,this,[this](){
		TownsQtSettings::setMidiMonitor(false);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setMidiMonitor",
			    Qt::QueuedConnection,
			    Q_ARG(bool,false));
		}
		applyMidiMonitorVisibility();
	});
}

void MainWindow::applyMidiMonitorVisibility()
{
	const bool show=TownsQtSettings::midiMonitor();
	if(nullptr!=midi_monitor_action_ && midi_monitor_action_->isChecked()!=show)
	{
		midi_monitor_action_->blockSignals(true);
		midi_monitor_action_->setChecked(show);
		midi_monitor_action_->blockSignals(false);
	}
	if(show)
	{
		ensureMidiMonitorWindow();
		if(nullptr!=midi_monitor_window_)
		{
			midi_monitor_window_->show();
			midi_monitor_window_->raise();
		}
	}
	else if(nullptr!=midi_monitor_window_)
	{
		midi_monitor_window_->hide();
	}
}

void MainWindow::updateMidiMonitorDisplay()
{
	if(!TownsQtSettings::midiMonitor())
	{
		return;
	}
	ensureMidiMonitorWindow();
	if(nullptr==midi_monitor_window_ ||
	   nullptr==controller_ ||
	   nullptr==emu_thread_ ||
	   !emu_thread_->isRunning())
	{
		return;
	}

	QStringList lines;
	QMetaObject::invokeMethod(
	    controller_,
	    "takeMidiMonitorLines",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QStringList,lines));
	for(const QString &line : lines)
	{
		midi_monitor_window_->appendLine(line);
	}
}

void MainWindow::ensureCpuDebugWindow()
{
	if(nullptr!=cpu_debug_window_)
	{
		return;
	}
	cpu_debug_window_=new DebugTextWindow(tr("CPU / CS:EIP history"),this);
	cpu_debug_window_->resize(820,560);
	connect(cpu_debug_window_,&DebugTextWindow::windowClosed,this,[this](){
		TownsQtSettings::setShowCpuDebug(false);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setCpuDebugMonitor",
			    Qt::QueuedConnection,
			    Q_ARG(bool,false));
		}
		applyCpuDebugVisibility();
	});
}

void MainWindow::applyCpuDebugVisibility()
{
	const bool show=TownsQtSettings::showCpuDebug();
	if(nullptr!=cpu_debug_action_ && cpu_debug_action_->isChecked()!=show)
	{
		cpu_debug_action_->blockSignals(true);
		cpu_debug_action_->setChecked(show);
		cpu_debug_action_->blockSignals(false);
	}
	if(show)
	{
		ensureCpuDebugWindow();
		if(nullptr!=cpu_debug_window_)
		{
			cpu_debug_window_->show();
			cpu_debug_window_->raise();
		}
	}
	else if(nullptr!=cpu_debug_window_)
	{
		cpu_debug_window_->hide();
	}
}

void MainWindow::updateCpuDebugDisplay()
{
	if(!TownsQtSettings::showCpuDebug())
	{
		return;
	}
	ensureCpuDebugWindow();
	if(nullptr==cpu_debug_window_ ||
	   nullptr==controller_ ||
	   nullptr==emu_thread_ ||
	   !emu_thread_->isRunning())
	{
		return;
	}

	// Snapshot is relatively heavy; refresh at ~10 Hz.
	static qint64 s_last_ms=0;
	const qint64 now=QDateTime::currentMSecsSinceEpoch();
	if(now-s_last_ms<100)
	{
		return;
	}
	s_last_ms=now;

	QString text;
	QMetaObject::invokeMethod(
	    controller_,
	    "cpuDebugSnapshot",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QString,text));
	cpu_debug_window_->setLiveText(text);
}

void MainWindow::onFrameReady()
{
	if(!isVisible())
	{
		show();
	}
	last_emu_activity_ms_=QDateTime::currentMSecsSinceEpoch();
	if(cached_differential_integration_ &&
	   !cached_mouse_capture_released_ &&
	   !mouse_failsafe_show_cursor_ &&
	   !inputQueue_.RelativePointerActive())
	{
		processPendingMouseWarp();
	}
}

void MainWindow::onStatsUpdated(double fps,double emu_hz,int queue_depth,int capture_queue_depth,int present_lag)
{
	last_emu_activity_ms_=QDateTime::currentMSecsSinceEpoch();
	setWindowTitle(tr("Tsugaru_QT — %1 FPS | %2 Hz | P:%3 C:%4 lag:%5")
	                   .arg(fps,0,'f',1)
	                   .arg(emu_hz,0,'f',2)
	                   .arg(queue_depth)
	                   .arg(capture_queue_depth)
	                   .arg(present_lag));
}

void MainWindow::onFailed(const QString &message)
{
	statusBar()->showMessage(message,0);
	QMessageBox::critical(this,tr("Tsugaru_QT"),message);
}

void MainWindow::onControllerFinished()
{
	if(emu_restarting_)
	{
		return;
	}
	poll_timer_.stop();
	statusBar()->showMessage(tr("Emulator stopped"),3000);
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
		if(!shouldCaptureHostMouse())
		{
			inputQueue_.CancelCursorWarp();
			inputQueue_.ClearMouseButtons();
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
	host_cursor_blank_=false;
	mouse_failsafe_show_cursor_=false;
	refreshMouseUiState();
	ensureMenuBarDocked();
	have_last_fullscreen_mouse_global_=false;
	last_fullscreen_mouse_move_ms_=0;
	last_fullscreen_menubar_show_ms_=0;
	showFullscreenCursor();
	if(menuBar())
	{
		menuBar()->hide();
	}
	// Drop fixed windowed size so the compositor can take the full screen.
	setMinimumSize(0,0);
	setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
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

bool MainWindow::shouldCaptureHostMouse() const
{
	return isVisible() &&
	       isActiveWindow() &&
	       nullptr==QApplication::activeModalWidget() &&
	       nullptr==QApplication::activePopupWidget() &&
	       !isCursorOverUiChrome();
}

void MainWindow::processPendingMouseWarp()
{
	if(!cached_differential_integration_ ||
	   cached_mouse_capture_released_ ||
	   mouse_failsafe_show_cursor_ ||
	   inputQueue_.RelativePointerActive() ||
	   !shouldCaptureHostMouse() ||
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
	const QVariantMap state=queryMouseUiState();
	if(!state.isEmpty())
	{
		return state.value(QStringLiteral("diff")).toBool();
	}
	return TownsQtSettings::differentialMouseIntegration();
}

QVariantMap MainWindow::queryMouseUiState() const
{
	QVariantMap state;
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "mouseUiState",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(QVariantMap,state));
	}
	return state;
}

void MainWindow::refreshMouseUiState()
{
	const QVariantMap state=queryMouseUiState();
	if(state.isEmpty())
	{
		cached_differential_integration_=TownsQtSettings::differentialMouseIntegration();
		cached_mouse_bios_active_=false;
		cached_mouse_capture_released_=false;
		if(nullptr!=view_)
		{
			view_->setMouseCaptureReleased(false);
		}
		updateMouseModeIndicator();
		return;
	}
	cached_differential_integration_=state.value(QStringLiteral("diff")).toBool();
	cached_mouse_bios_active_=state.value(QStringLiteral("mos")).toBool();
	cached_mouse_capture_released_=state.value(QStringLiteral("capture_released")).toBool();
	if(nullptr!=view_)
	{
		view_->setMouseCaptureReleased(cached_mouse_capture_released_);
	}
	updateMouseModeIndicator();
}

void MainWindow::updateMouseModeIndicator()
{
	if(nullptr==mouse_mode_label_)
	{
		return;
	}
	const bool emu_running=
	    nullptr!=emu_thread_ && emu_thread_->isRunning() && nullptr!=controller_;
	if(true!=emu_running)
	{
		mouse_mode_label_->clear();
		mouse_mode_label_->setToolTip(QString());
		mouse_mode_category_=-1;
		return;
	}

	// 0 = integrated (absolute/snap), 1 = captured (differential), 2 = released (differential).
	// Check capture-released first: while capture is released the runtime reports diff=false
	// (it stops feeding), so keying off diff alone would misread the released state (differential
	// with capture off, e.g. the non-TBIOS forced-differential case) as integrated.
	int category;
	if(true==cached_mouse_capture_released_)
	{
		category=2;
	}
	else if(true==cached_differential_integration_)
	{
		category=1;
	}
	else
	{
		category=0;
	}
	// Restart from the state word whenever the state changes.
	if(category!=mouse_mode_category_)
	{
		mouse_mode_category_=category;
		mouse_mode_phase_=0;
	}

	QString text;
	QString tip;
	switch(category)
	{
	case 0:
		// Absolute/snap: the host pointer maps straight onto the Towns pointer.  Static.
		text=tr("Integrated");
		tip=tr("Mouse integrated: the host pointer controls the Towns pointer directly.");
		break;
	case 1:
		// Differential, captured: alternate the state word with the release hint.
		text=(0==mouse_mode_phase_) ? tr("Captured") : tr("Middle button to release");
		tip=tr("Relative mouse is captured. "
		       "Press the middle mouse button to release capture.");
		break;
	default:
		// Differential, released: alternate the state word with the capture hint.
		text=(0==mouse_mode_phase_) ? tr("Released") : tr("Click to capture");
		tip=tr("Relative mouse. Click the screen to start mouse capture; "
		       "press the middle mouse button to release it.");
		break;
	}
	mouse_mode_label_->setText(text);
	mouse_mode_label_->setToolTip(tip);
}

void MainWindow::updateMouseFailsafeFromActivity()
{
	const bool emu_running=
	    nullptr!=emu_thread_ && emu_thread_->isRunning() && nullptr!=controller_;
	bool want_failsafe=false;
	if(emu_running)
	{
		if(0==last_emu_activity_ms_)
		{
			last_emu_activity_ms_=QDateTime::currentMSecsSinceEpoch();
		}
		else
		{
			const qint64 idle_ms=QDateTime::currentMSecsSinceEpoch()-last_emu_activity_ms_;
			// No frame/stats for 2s while the emu thread is supposedly running.
			want_failsafe=(idle_ms>2000);
		}
	}
	if(want_failsafe==mouse_failsafe_show_cursor_)
	{
		return;
	}
	mouse_failsafe_show_cursor_=want_failsafe;
	if(nullptr!=controller_ && emu_running)
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "setMouseFailsafeShowHostCursor",
		    Qt::QueuedConnection,
		    Q_ARG(bool,want_failsafe));
	}
	updateBlankCursor();
}

void MainWindow::noteEmuPictureClicked()
{
	if(cached_mouse_capture_released_ &&
	   nullptr!=controller_ &&
	   nullptr!=emu_thread_ &&
	   emu_thread_->isRunning())
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "resumeMouseCapture",
		    Qt::QueuedConnection);
		cached_mouse_capture_released_=false;
		if(nullptr!=view_)
		{
			view_->setMouseCaptureReleased(false);
		}
		syncWaylandRelativePointer();
		updateBlankCursor();
		updateMouseModeIndicator();
		return;
	}
	updateBlankCursor();
}

void MainWindow::syncDifferentialMouseCursor()
{
	refreshMouseUiState();
	syncWaylandRelativePointer();
	updateBlankCursor();
}

void MainWindow::updateBlankCursor()
{
	const bool active=shouldCaptureHostMouse();
	// Only the failsafe (VM appears hung) shows the host cursor.  Normal and differential
	// integration hide it over the emulator picture regardless of capture on/off state —
	// including the capture-released "click to start" state of auto-forced differential.
	const bool show_host=mouse_failsafe_show_cursor_;

	bool want_blank=false;
	if(!show_host && active && nullptr!=view_)
	{
		const QPoint view_pos=view_->hostCursorInView();
		if(fullscreen_ && isCursorNearFullscreenMenu())
		{
			want_blank=false;
		}
		else if(cached_differential_integration_)
		{
			want_blank=true;
		}
		else if(view_->isPointOnEmuPicture(view_pos) && !isCursorOverUiChrome())
		{
			// Absolute/snap, or capture-released differential: hide the host cursor
			// over the picture.  The user clicks (blind) anywhere on it to capture.
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
		// Fixed window size: scale/settings may change it, but the user cannot drag-resize.
		setMinimumSize(0,0);
		setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
		setFixedSize(window_w,window_h);
		windowed_geometry_=geometry();
	}
	else
	{
		setMinimumSize(0,0);
		setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
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

void MainWindow::syncWaylandRelativePointer()
{
	const bool want=
	    cached_differential_integration_ &&
	    !cached_mouse_capture_released_ &&
	    !mouse_failsafe_show_cursor_ &&
	    shouldCaptureHostMouse() &&
	    isVisible() &&
	    !isMinimized();

	auto logCapture=[&](bool on,const QString &method)
	{
		std::cout << "Tsugaru_QT: Mouse capture is " << (on ? "ON" : "OFF")
		          << " (method=" << method.toLocal8Bit().constData() << ").\n";
		std::cout.flush();
	};

	if(!want)
	{
		if(wayland_capture_want_)
		{
			logCapture(false,wayland_capture_method_.isEmpty()
			                     ? QStringLiteral("cursor-warp")
			                     : wayland_capture_method_);
		}
		TownsQtWaylandRelativePointer::Stop();
		wayland_capture_want_=false;
		wayland_capture_method_.clear();
		return;
	}

	QWindow *win=windowHandle();
	if(nullptr==win)
	{
		if(wayland_capture_want_)
		{
			logCapture(false,wayland_capture_method_.isEmpty()
			                     ? QStringLiteral("cursor-warp")
			                     : wayland_capture_method_);
		}
		TownsQtWaylandRelativePointer::Stop();
		wayland_capture_want_=false;
		wayland_capture_method_.clear();
		return;
	}

	QString method=QStringLiteral("cursor-warp");
	if(TownsQtWaylandRelativePointer::Active())
	{
		method=QStringLiteral("wayland-relative-pointer");
	}
	else if(TownsQtWaylandRelativePointer::Available() &&
	        TownsQtWaylandRelativePointer::Start(win,&inputQueue_))
	{
		method=QStringLiteral("wayland-relative-pointer");
	}

	if(!wayland_capture_want_ || wayland_capture_method_!=method)
	{
		logCapture(true,method);
	}
	wayland_capture_want_=true;
	wayland_capture_method_=method;
}

void MainWindow::cleanupStoppedEmulator(EmulatorController *stopping)
{
	if(nullptr!=stopping)
	{
		disconnect(stopping,nullptr,emu_thread_,nullptr);
		// Affinity should already be the UI thread (moved at end of EmulatorController::run).
		if(stopping->thread()==thread())
		{
			delete stopping;
		}
		else
		{
			stopping->deleteLater();
		}
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
