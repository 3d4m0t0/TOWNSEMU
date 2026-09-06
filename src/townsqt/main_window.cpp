#include "main_window.h"

#include "debug_text_window.h"
#include "cdrom_monitor_window.h"
#include "mouse_coord_scan_window.h"
#include "mouse_coord_profile_page.h"
#include "mouse_coord_write_scan.h"
#include "audio_mixer_dialog.h"
#include "emulator_controller.h"
#include "hdd_settings_dialog.h"
#include "townsargv.h"
#include "townsqt_argv_from_settings.h"
#include "townsqt_cpu_profile.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_disc_statesave.h"
#include "townsqt_rom_availability.h"
#include "townsqt_app_profile.h"
#include "townsqt_settings.h"
#include "i486.h"
#include "townsqt_version.h"
#include "townsqt_wayland_idle_inhibit.h"
#include "townsqt_wayland_relative_pointer.h"

#if defined(__linux__)
#include "linux/midi_alsa_seq_host.h"
#include "linux/midi_backend_probe.h"
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
#include <QEventLoop>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QGuiApplication>
#include <QWindow>
#include <QHBoxLayout>
#include <iostream>
#include <QCursor>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QEventLoop>
#include <QPixmap>
#include <QScreen>
#include <QShowEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
QVariantMap DiscMachineMapFromBasics(const SettingsDialog::Values &v)
{
	QVariantMap m;
	m.insert(QStringLiteral("frequency_mhz"),v.cpuFrequencyMhz);
	m.insert(QStringLiteral("custom_frequency_mhz"),v.cpuCustomFrequencyMhz);
	m.insert(QStringLiteral("fast_mode"),v.cpuFastMode);
	m.insert(QStringLiteral("mem_size_mb"),v.memSizeInMB);
	m.insert(QStringLiteral("gameport0"),v.gamePort0);
	m.insert(QStringLiteral("gameport1"),v.gamePort1);
	m.insert(QStringLiteral("max_button_hold_ms0"),v.maxButtonHoldTimeMs0);
	m.insert(QStringLiteral("max_button_hold_ms1"),v.maxButtonHoldTimeMs1);
	m.insert(QStringLiteral("cpu_high_fidelity"),v.cpuHighFidelity);
	m.insert(QStringLiteral("pretend_386dx"),v.pretend386DX);
	m.insert(QStringLiteral("use_fpu"),v.useFPU);
	m.insert(QStringLiteral("fast_scsi"),v.fastScsi);
	m.insert(QStringLiteral("fast_fd"),v.fastFd);
	m.insert(QStringLiteral("midi_board"),v.midiBoard);
	return m;
}

QVariantMap DiscMachineMapFromProfile(const SettingsDialog::Values &v)
{
	QVariantMap m;
	m.insert(QStringLiteral("frequency_mhz"),v.profileCpuFrequencyMhz);
	m.insert(QStringLiteral("custom_frequency_mhz"),v.profileCpuCustomFrequencyMhz);
	m.insert(QStringLiteral("fast_mode"),v.profileCpuFastMode);
	m.insert(QStringLiteral("mem_size_mb"),v.profileMemSizeInMB);
	m.insert(QStringLiteral("gameport0"),v.profileGamePort0);
	m.insert(QStringLiteral("gameport1"),v.profileGamePort1);
	m.insert(QStringLiteral("max_button_hold_ms0"),v.profileMaxButtonHoldTimeMs0);
	m.insert(QStringLiteral("max_button_hold_ms1"),v.profileMaxButtonHoldTimeMs1);
	m.insert(QStringLiteral("cpu_high_fidelity"),v.profileCpuHighFidelity);
	m.insert(QStringLiteral("pretend_386dx"),v.profilePretend386DX);
	m.insert(QStringLiteral("use_fpu"),v.profileUseFPU);
	m.insert(QStringLiteral("fast_scsi"),v.profileFastScsi);
	m.insert(QStringLiteral("fast_fd"),v.profileFastFd);
	m.insert(QStringLiteral("midi_board"),v.profileMidiBoard);
	return m;
}

QVariantMap DiscMachineMapFromScanProfile(const MouseCoordWriteScan::Profile &p)
{
	QVariantMap m;
	if(true==p.machine.hasFrequencyMhz)
	{
		m.insert(QStringLiteral("frequency_mhz"),p.machine.frequencyMhz);
	}
	if(true==p.machine.hasCustomFrequencyMhz)
	{
		m.insert(QStringLiteral("custom_frequency_mhz"),p.machine.customFrequencyMhz);
	}
	if(true==p.machine.hasFastMode)
	{
		m.insert(QStringLiteral("fast_mode"),p.machine.fastMode);
	}
	if(true==p.machine.hasMemSizeInMB)
	{
		m.insert(QStringLiteral("mem_size_mb"),p.machine.memSizeInMB);
	}
	if(true==p.machine.hasGamePort0)
	{
		m.insert(QStringLiteral("gameport0"),static_cast<uint>(p.machine.gamePort0));
	}
	if(true==p.machine.hasGamePort1)
	{
		m.insert(QStringLiteral("gameport1"),static_cast<uint>(p.machine.gamePort1));
	}
	if(true==p.machine.hasMaxButtonHoldMs0)
	{
		m.insert(QStringLiteral("max_button_hold_ms0"),p.machine.maxButtonHoldMs0);
	}
	if(true==p.machine.hasMaxButtonHoldMs1)
	{
		m.insert(QStringLiteral("max_button_hold_ms1"),p.machine.maxButtonHoldMs1);
	}
	if(true==p.machine.hasCpuHighFidelity)
	{
		m.insert(QStringLiteral("cpu_high_fidelity"),p.machine.cpuHighFidelity);
	}
	if(true==p.machine.hasPretend386DX)
	{
		m.insert(QStringLiteral("pretend_386dx"),p.machine.pretend386DX);
	}
	if(true==p.machine.hasUseFPU)
	{
		m.insert(QStringLiteral("use_fpu"),p.machine.useFPU);
	}
	if(true==p.machine.hasFastScsi)
	{
		m.insert(QStringLiteral("fast_scsi"),p.machine.fastScsi);
	}
	if(true==p.machine.hasFastFd)
	{
		m.insert(QStringLiteral("fast_fd"),p.machine.fastFd);
	}
	if(true==p.machine.hasMidiBoard)
	{
		m.insert(QStringLiteral("midi_board"),p.machine.midiBoard);
	}
	if(true==p.machine.hasFdImg[0])
	{
		m.insert(QStringLiteral("fd0"),QString::fromStdString(p.machine.fdImg[0]));
	}
	if(true==p.machine.hasFdImg[1])
	{
		m.insert(QStringLiteral("fd1"),QString::fromStdString(p.machine.fdImg[1]));
	}
	return m;
}

void InsertCurrentFdMounts(QVariantMap &m,const QString fdPath[2])
{
	m.insert(QStringLiteral("fd0"),fdPath[0]);
	m.insert(QStringLiteral("fd1"),fdPath[1]);
}

/*! Mouse integration needs a resolvable soft cursor — not mouseBIOSActive alone.
    Soft X/Y stay empty when soft_ok is false even if the MOS flag is still set. */
bool MosIntegrationAvailable(const QVariantMap &state)
{
	return true==state.value(QStringLiteral("mos")).toBool() &&
	       true==state.value(QStringLiteral("soft_ok")).toBool();
}
}

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

bool MachineSettingsNeedRestart(const SettingsDialog::Values &values,const TownsARGV &argv)
{
	if(values.cpuKind!=TownsQtSettings::cpuKind())
	{
		return true;
	}
	if(values.modelGroupIndex!=TownsQtSettings::modelGroupIndex())
	{
		return true;
	}
	// Compare against the running machine (argv), not townsqt.conf globals.
	// Disc-profile mem/fidelity often differ from globals permanently — that must not
	// force a restart when only mouse integration (or other live fields) change.
	const int runningMem=static_cast<int>(argv.memSizeInMB);
	const bool runningHighFidelity=
	    (i486DXCommon::HIGH_FIDELITY==argv.CPUFidelityLevel);
	const bool runningPretend=argv.pretend386DX;
	const bool runningFpu=argv.useFPU;
	const bool runningFastScsi=argv.fastSCSI;
	const bool runningFastFd=argv.fastFD;
	if(true==values.discProfileAvailable)
	{
		if(values.profileMemSizeInMB!=runningMem)
		{
			return true;
		}
		if(values.profileCpuHighFidelity!=runningHighFidelity)
		{
			return true;
		}
		if(values.profilePretend386DX!=runningPretend)
		{
			return true;
		}
		if(values.profileUseFPU!=runningFpu)
		{
			return true;
		}
		if(values.profileFastScsi!=runningFastScsi)
		{
			return true;
		}
		if(values.profileFastFd!=runningFastFd)
		{
			return true;
		}
	}
	else
	{
		if(values.memSizeInMB!=runningMem)
		{
			return true;
		}
		if(values.cpuHighFidelity!=runningHighFidelity)
		{
			return true;
		}
		if(values.pretend386DX!=runningPretend)
		{
			return true;
		}
		if(values.useFPU!=runningFpu)
		{
			return true;
		}
		if(values.fastScsi!=runningFastScsi)
		{
			return true;
		}
		if(values.fastFd!=runningFastFd)
		{
			return true;
		}
	}
	if(values.highResCrtc!=argv.highResAvailable)
	{
		return true;
	}
	if(values.highResPcm!=argv.highResPCM)
	{
		return true;
	}
	if(values.midiOutput!=TownsQtSettings::midiOutput())
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
	view_->setVideoOptions(scale,false,true);
	view_->setMinimumSize(640*scale,480*scale);
	view_->setMaximumSize(640*scale,480*scale);
	setCentralWidget(view_);

	setupMenuBar();
	// Size follows EMU scale; user drag-resize is rejected in resizeEvent (no setFixedSize).
	setWindowFlag(Qt::WindowMaximizeButtonHint,false);
	statusBar()->setSizeGripEnabled(false);

	profile_enabled_label_=new QLabel(this);
	profile_enabled_label_->setTextFormat(Qt::PlainText);
	statusBar()->addWidget(profile_enabled_label_);
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
	applyMouseCoordScanVisibility();
	applyMidiMonitorVisibility();
	applyCdromMonitorVisibility();
	applyAppMonitorVisibility();
	applyCpuDebugVisibility();
	if(nullptr!=view_)
	{
		view_->setDriveAccessOverlayEnabled(TownsQtSettings::showDriveAccessOverlay());
	}

	// Size after status-bar chrome exists so width/height match EMU + menu + status (no padding gaps).
	applyWindowScale(std::clamp(scale,1,maxDisplayScale()));

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
	connect(controller_,&EmulatorController::discProfileStateChanged,this,&MainWindow::onDiscProfileStateChanged);
	connect(controller_,&EmulatorController::fdPathChanged,this,&MainWindow::onFdPathChanged);
	connect(controller_,&EmulatorController::fdWriteProtectChanged,this,&MainWindow::onFdWriteProtectChanged);
	connect(controller_,&EmulatorController::fastModeLampChanged,this,&MainWindow::onGuestFastModeLampChanged);
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
	prepareArgvForNextBoot();
#if defined(__linux__)
	{
		const QByteArray sf_utf8=TownsQtSettings::midiSoundFont().toUtf8();
		MidiFluidSynthHost::SetSoundFontPath(sf_utf8.constData());
		MidiFluidSynthHost::SetMasterVolumePercent(TownsQtSettings::midiVolumePercent());
		const QString midi_out=TownsQtSettings::midiOutput();
		if(midi_out==QStringLiteral("alsa"))
		{
			MidiBackendProbe::SetUserPreference(MidiBackendProbe::Kind::AlsaSeq);
			const QByteArray port_utf8=TownsQtSettings::midiAlsaPort().toUtf8();
			MidiAlsaSeqHost::SetDestination(port_utf8.constData());
		}
		else if(midi_out==QStringLiteral("fluidsynth"))
		{
			MidiBackendProbe::SetUserPreference(MidiBackendProbe::Kind::FluidSynth);
			MidiAlsaSeqHost::SetDestination(nullptr);
		}
		else
		{
			MidiBackendProbe::SetUserPreference(MidiBackendProbe::Kind::None);
			MidiAlsaSeqHost::SetDestination(nullptr);
		}
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

void MainWindow::clearDiscProfileOverride(void)
{
	cached_disc_profile_loaded_=false;
	cached_disc_fingerprint_hash32_=0;
	disc_profile_override_active_=false;
	disc_profile_machine_override_.clear();
}

bool MainWindow::loadDiscProfileOverrideForPath(const QString &cdPath)
{
	clearDiscProfileOverride();
	if(cdPath.isEmpty())
	{
		return false;
	}
	MouseCoordWriteScan scan;
	scan.SetProfileDirectory(TownsQtPaths::profilesDir().toStdString());
	if(true!=scan.TryLoadForDisc(cdPath.toStdString()))
	{
		return false;
	}
	cached_disc_profile_loaded_=scan.DiscProfileLoaded();
	cached_disc_fingerprint_hash32_=scan.GetActiveProfile().discFingerprintHash32;
	const QVariantMap machine=DiscMachineMapFromScanProfile(scan.GetActiveProfile());
	disc_profile_override_active_=cached_disc_profile_loaded_ && !machine.isEmpty();
	disc_profile_machine_override_=disc_profile_override_active_ ? machine : QVariantMap();
	return cached_disc_profile_loaded_;
}

void MainWindow::prepareArgvForNextBoot(void)
{
	const bool refreshedFromSettings=refresh_argv_from_settings_on_next_boot_;
	if(true==refreshedFromSettings)
	{
		TownsQtArgvFromSettings::Apply(argv_);
		refresh_argv_from_settings_on_next_boot_=false;
		// Restart: discard previous session FD mounts; profile / lastFd re-apply below.
		argv_.fdImgFName[0].clear();
		argv_.fdImgFName[1].clear();
	}

	QString cdPath=pending_boot_cd_path_;
	pending_boot_cd_path_.clear();
	if(cdPath.isEmpty())
	{
		cdPath=TownsQtSettings::lastCdImagePath();
	}
	if(cdPath.isEmpty() && !cd_path_.isEmpty())
	{
		cdPath=cd_path_;
	}
	if(cdPath.isEmpty() && !argv_.cdImgFName.empty())
	{
		cdPath=QString::fromStdString(argv_.cdImgFName);
	}
	if(!cdPath.isEmpty())
	{
		const QString canonical=QFileInfo(cdPath).canonicalFilePath();
		if(!canonical.isEmpty())
		{
			cdPath=canonical;
		}
	}
	if(!cdPath.isEmpty() && QFile::exists(cdPath))
	{
		// A/C: mount CD → profile → state-save path for this boot only (not cached in argv after apply).
		argv_.cdImgFName=cdPath.toStdString();
		cd_path_=cdPath;
		TownsQtSettings::setLastCdImagePath(cdPath);
		loadDiscProfileOverrideForPath(cdPath);
	}
	else
	{
		argv_.cdImgFName.clear();
		clearDiscProfileOverride();
	}
	argv_.startUpStateFName.clear();
	if(!cdPath.isEmpty() && true==TownsQtSettings::autoResumeEnabled())
	{
		bool apply_startup_state_save=true;
		// Manual restart with the same CD: cold boot from saved state, not state save.
		if(true==refreshedFromSettings && !cd_path_at_last_boot_.isEmpty())
		{
			apply_startup_state_save=(cd_path_at_last_boot_!=cdPath);
		}
		else if(true==refreshedFromSettings)
		{
			apply_startup_state_save=false;
		}
		if(true==apply_startup_state_save)
		{
			const QString statePath=TownsQtDiscStateSave::StartupStateSavePathForDisc(cdPath);
			if(!statePath.isEmpty())
			{
				argv_.startUpStateFName=statePath.toStdString();
			}
		}
	}
	if(!cdPath.isEmpty())
	{
		cd_path_at_last_boot_=cdPath;
	}
	else
	{
		cd_path_at_last_boot_.clear();
	}
	applyDiscProfileOverridesToArgv();

	for(int drive=0; drive<2; ++drive)
	{
		const char *key=(0==drive) ? "fd0" : "fd1";
		if(true==disc_profile_override_active_ &&
		   disc_profile_machine_override_.contains(QString::fromLatin1(key)))
		{
			continue;
		}
		if(!argv_.fdImgFName[drive].empty())
		{
			fd_path_[drive]=QString::fromStdString(argv_.fdImgFName[drive]);
			continue;
		}
		const QString saved=TownsQtSettings::lastFdImagePath(drive);
		if(!saved.isEmpty() && QFile::exists(saved))
		{
			const QString canonical=QFileInfo(saved).canonicalFilePath();
			const QString usePath=canonical.isEmpty() ? saved : canonical;
			argv_.fdImgFName[drive]=usePath.toStdString();
			fd_path_[drive]=usePath;
		}
		else
		{
			fd_path_[drive].clear();
		}
	}

	updateOpenCdMenuLabel();
	updateOpenFdMenuLabel(0);
	updateOpenFdMenuLabel(1);
	syncEjectMenus();
	updateWindowTitle();
}

void MainWindow::requestCdImageChange(const QString &path)
{
	if(path.isEmpty())
	{
		return;
	}
	const QString canonical=QFileInfo(path).canonicalFilePath();
	const QString usePath=canonical.isEmpty() ? path : canonical;
	if(!QFile::exists(usePath))
	{
		return;
	}
	TownsQtSettings::rememberFileDialogPath(usePath);
	TownsQtSettings::addRecentCdImagePath(usePath);
	rebuildRecentCdMenu();

	// Live CD swap: state save → eject → mount.  Do not restart the emulator.
	if(nullptr!=emu_thread_ && emu_thread_->isRunning() && nullptr!=controller_)
	{
		pending_boot_cd_path_.clear();
		bool swapped=false;
		QMetaObject::invokeMethod(
		    controller_,
		    "swapCdImage",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,swapped),
		    Q_ARG(QString,usePath));
		if(true!=swapped)
		{
			statusBar()->showMessage(tr("Failed to load CD image"),5000);
			return;
		}
		cd_path_=usePath;
		argv_.cdImgFName=usePath.toStdString();
		TownsQtSettings::setLastCdImagePath(usePath);
		updateOpenCdMenuLabel();
		syncEjectMenus();
		applyRuntimeDiscProfileOverrides();
		statusBar()->showMessage(tr("CD: %1").arg(QFileInfo(usePath).fileName()),5000);
		return;
	}
	TownsQtSettings::setLastCdImagePath(usePath);
	cd_path_=usePath;
	argv_.cdImgFName=usePath.toStdString();
	updateOpenCdMenuLabel();
	syncEjectMenus();
	pending_boot_cd_path_=usePath;
	if(true==emu_restarting_)
	{
		emu_restart_pending_=true;
		return;
	}
	if(true==emu_started_)
	{
		refresh_argv_from_settings_on_next_boot_=true;
		startEmulator();
	}
}

void MainWindow::scheduleRestartEmulator()
{
	if(emu_restarting_)
	{
		emu_restart_pending_=true;
		return;
	}
	emu_restarting_=true;
	refresh_argv_from_settings_on_next_boot_=true;
	statusBar()->showMessage(tr("Restarting the emulator…"));
	stopEmulatorAsync([this]{
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
	delete mouse_coord_scan_window_;
	mouse_coord_scan_window_=nullptr;
	stopEmulator();
}

void MainWindow::setupMenuBar()
{
	auto *operationMenu=menuBar()->addMenu(tr("&Operation"));

	auto *restartAction=operationMenu->addAction(tr("&Restart"));
	restartAction->setShortcut(QKeySequence(Qt::Key_F12));
	restartAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(restartAction,&QAction::triggered,this,[this]{
		restartEmulator();
	});

	operationMenu->addSeparator();

	cpu_clock_group_=new QActionGroup(this);
	cpu_clock_group_->setExclusive(true);
	cpu_compat_action_=operationMenu->addAction(tr("Compatible"));
	cpu_compat_action_->setCheckable(true);
	cpu_compat_action_->setData(-1);
	cpu_clock_group_->addAction(cpu_compat_action_);
	connect(cpu_compat_action_,&QAction::triggered,this,[this]{
		setCpuFastModeEnabled(false);
	});
	const struct {int mhz; const char *label;} cpu_presets[]={
	    {16,"16MHz"},{20,"20MHz"},{25,"25MHz"},
	};
	for(const auto &preset : cpu_presets)
	{
		auto *action=operationMenu->addAction(QString::fromUtf8(preset.label));
		action->setCheckable(true);
		action->setData(preset.mhz);
		cpu_clock_group_->addAction(action);
		connect(action,&QAction::triggered,this,[this,mhz=preset.mhz]{
			setCpuFrequencyMhz(mhz);
			setCpuFastModeEnabled(true);
		});
	}
	// Non-preset frequency from Settings (e.g. 33MHz): selection only, not editable here.
	cpu_clock_custom_action_=operationMenu->addAction(QString());
	cpu_clock_custom_action_->setCheckable(true);
	cpu_clock_custom_action_->setVisible(true);
	cpu_clock_group_->addAction(cpu_clock_custom_action_);
	connect(cpu_clock_custom_action_,&QAction::triggered,this,[this]{
		setCpuFrequencyMhz(TownsQtSettings::cpuCustomFrequencyMhz());
		setCpuFastModeEnabled(true);
	});
	connect(operationMenu,&QMenu::aboutToShow,this,&MainWindow::syncGuestFastModeFromEmulator);
	syncCpuClockMenuActions();

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

	display_scale_menu_=operationMenu->addMenu(tr("Window scale"));
	display_scale_group_=new QActionGroup(this);
	display_scale_group_->setExclusive(true);
	connect(display_scale_group_,&QActionGroup::triggered,this,[this](QAction *action){
		if(nullptr!=action)
		{
			setDisplayScale(action->data().toInt());
		}
	});
	connect(operationMenu,&QMenu::aboutToShow,this,&MainWindow::syncDisplayScaleMenu);
	scale_down_action_=new QAction(tr("Decrease window scale"),this);
	scale_up_action_=new QAction(tr("Increase window scale"),this);
	scale_down_action_->setShortcuts({
	    QKeySequence(Qt::ALT|Qt::Key_Less),
	    QKeySequence(Qt::ALT|Qt::Key_Comma),
	});
	scale_up_action_->setShortcuts({
	    QKeySequence(Qt::ALT|Qt::Key_Greater),
	    QKeySequence(Qt::ALT|Qt::Key_Period),
	});
	addAction(scale_down_action_);
	addAction(scale_up_action_);
	connect(scale_down_action_,&QAction::triggered,this,[this]{
		bumpDisplayScale(-1);
	});
	connect(scale_up_action_,&QAction::triggered,this,[this]{
		bumpDisplayScale(1);
	});
	syncDisplayScaleMenu();

	fullscreen_action_=operationMenu->addAction(tr("Toggle &fullscreen"));
	fullscreen_action_->setCheckable(true);
	fullscreen_action_->setChecked(isFullScreen());
	addAction(fullscreen_action_);
	connect(fullscreen_action_,&QAction::triggered,this,&MainWindow::toggleFullScreen);

	operationMenu->addSeparator();

	auto *quitAction=operationMenu->addAction(tr("&Quit"));
	quitAction->setShortcut(QKeySequence::Quit);
	connect(quitAction,&QAction::triggered,this,&QWidget::close);

	auto *cdromMenu=menuBar()->addMenu(tr("&CD-ROM"));
	cdrom_menu_=cdromMenu;
	open_cd_action_=cdromMenu->addAction(tr("&Open CD image…"));
	connect(open_cd_action_,&QAction::triggered,this,&MainWindow::openCdImage);
	eject_cd_action_=cdromMenu->addAction(tr("&Eject CD"));
	connect(eject_cd_action_,&QAction::triggered,this,[this]{
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(controller_,"ejectCd",Qt::BlockingQueuedConnection);
		}
	});
	cd_recent_menu_=cdromMenu->addMenu(tr("Open &recent files"));
	connect(cd_recent_menu_,&QMenu::aboutToShow,this,&MainWindow::rebuildRecentCdMenu);
	connect(cdrom_menu_,&QMenu::aboutToShow,this,&MainWindow::syncEjectMenus);

	auto *diskMenu=menuBar()->addMenu(tr("&Disk"));
	disk_menu_=diskMenu;
	auto *hdd_settings_action=diskMenu->addAction(tr("Hard disk drive settings…"));
	connect(hdd_settings_action,&QAction::triggered,this,&MainWindow::openHddSettingsDialog);
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
	auto *stateMenu=toolsMenu->addMenu(tr("&State"));
	auto *stateLoadMenu=stateMenu->addMenu(tr("&Load"));
	for(int slot=0; slot<=9; ++slot)
	{
		auto *loadSlotAction=stateLoadMenu->addAction(tr("Slot %1").arg(slot));
		connect(loadSlotAction,&QAction::triggered,this,[this,slot]{
			loadStateSlotFromMenu(slot);
		});
	}
	auto *stateSaveMenu=stateMenu->addMenu(tr("&Save"));
	for(int slot=1; slot<=9; ++slot)
	{
		auto *saveSlotAction=stateSaveMenu->addAction(tr("Slot %1").arg(slot));
		connect(saveSlotAction,&QAction::triggered,this,[this,slot]{
			saveStateSlotFromMenu(slot);
		});
	}
	toolsMenu->addSeparator();
	auto *audioMixerAction=toolsMenu->addAction(tr("Audio mixer…"));
	connect(audioMixerAction,&QAction::triggered,this,&MainWindow::openAudioMixerDialog);
	drive_access_action_=toolsMenu->addAction(tr("Drive access"));
	drive_access_action_->setCheckable(true);
	drive_access_action_->setChecked(TownsQtSettings::showDriveAccessOverlay());
	connect(drive_access_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowDriveAccessOverlay(enabled);
		applyDriveAccessVisibility();
	});
	fps_display_action_=toolsMenu->addAction(tr("FPS display"));
	fps_display_action_->setCheckable(true);
	fps_display_action_->setChecked(TownsQtSettings::showFpsDisplay());
	fps_display_action_->setToolTip(
	    tr("Show FPS, emulation Hz, and present-queue stats in the window title."));
	connect(fps_display_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setShowFpsDisplay(enabled);
		updateWindowTitle();
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
	cdrom_monitor_action_=debugMenu->addAction(tr("CD-ROM monitor"));
	cdrom_monitor_action_->setCheckable(true);
	cdrom_monitor_action_->setChecked(TownsQtSettings::cdromMonitor());
	cdrom_monitor_action_->setToolTip(
	    tr("Log CD-ROM commands, CDDA play/stop, and data-sector access."));
	connect(cdrom_monitor_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setCdromMonitor(enabled);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setCdromMonitor",
			    Qt::QueuedConnection,
			    Q_ARG(bool,enabled));
		}
		applyCdromMonitorVisibility();
		if(enabled)
		{
			updateCdromMonitorDisplay();
		}
	});
	app_monitor_action_=debugMenu->addAction(tr("APP monitor"));
	app_monitor_action_->setCheckable(true);
	app_monitor_action_->setChecked(TownsQtSettings::appMonitor());
	app_monitor_action_->setToolTip(
	    tr("Log guest EXE/EXP identity, MOS session, in-game phase, and mouse apply."));
	connect(app_monitor_action_,&QAction::toggled,this,[this](bool enabled){
		TownsQtSettings::setAppMonitor(enabled);
		applyAppMonitorVisibility();
		if(enabled)
		{
			updateAppMonitorDisplay();
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
	const int mhz=[&]()->int{
		if(true==disc_profile_override_active_ &&
		   disc_profile_machine_override_.contains(QStringLiteral("frequency_mhz")))
		{
			return std::clamp(
			    disc_profile_machine_override_.value(QStringLiteral("frequency_mhz")).toInt(),
			    1,
			    100);
		}
		return TownsQtSettings::cpuFrequencyMhz();
	}();
	if(true==applyCpuClockViaDiscProfile(enabled,mhz))
	{
		return;
	}
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
	mhz=std::clamp(mhz,1,100);
	if(true==applyCpuClockViaDiscProfile(true,mhz))
	{
		return;
	}
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

bool MainWindow::applyCpuClockViaDiscProfile(bool fast_mode,int mhz)
{
	if(true!=cached_disc_profile_loaded_ ||
	   nullptr==controller_ ||
	   nullptr==emu_thread_ ||
	   true!=emu_thread_->isRunning())
	{
		return false;
	}
	mhz=std::clamp(mhz,1,100);
	int custom_mhz=TownsQtSettings::cpuCustomFrequencyMhz();
	if(true==disc_profile_override_active_ &&
	   disc_profile_machine_override_.contains(QStringLiteral("custom_frequency_mhz")))
	{
		custom_mhz=std::clamp(
		    disc_profile_machine_override_.value(QStringLiteral("custom_frequency_mhz")).toInt(),
		    33,
		    60);
	}
	if(true!=IsCpuFrequencyPreset(mhz))
	{
		custom_mhz=std::clamp(mhz,33,60);
		// Keep the Operation-menu custom label in sync.
		TownsQtSettings::setCpuCustomFrequencyMhz(custom_mhz);
	}

	bool ok=false;
	QMetaObject::invokeMethod(
	    controller_,
	    "updateDiscMachineClock",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(bool,ok),
	    Q_ARG(bool,fast_mode),
	    Q_ARG(int,mhz),
	    Q_ARG(int,custom_mhz));
	if(true!=ok)
	{
		return false;
	}

	disc_profile_machine_override_.insert(QStringLiteral("fast_mode"),fast_mode);
	disc_profile_machine_override_.insert(QStringLiteral("frequency_mhz"),mhz);
	disc_profile_machine_override_.insert(QStringLiteral("custom_frequency_mhz"),custom_mhz);
	disc_profile_override_active_=true;

	QMetaObject::invokeMethod(
	    controller_,
	    "applyCpuFrequencyMhzLive",
	    Qt::QueuedConnection,
	    Q_ARG(int,mhz));
	QMetaObject::invokeMethod(
	    controller_,
	    "applyCpuFastModeLive",
	    Qt::QueuedConnection,
	    Q_ARG(bool,fast_mode));
	syncMenuChecks();
	if(nullptr!=statusBar())
	{
		statusBar()->showMessage(tr("Disc profile clock updated."),2000);
	}
	return true;
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
		    Q_ARG(bool,TownsQtSettings::autoDifferentialOnMosUnused()),
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

void MainWindow::syncCpuClockMenuActions()
{
	if(nullptr==cpu_clock_group_)
	{
		return;
	}
	// Prefer active disc-profile machine overrides for the Operation menu.
	bool fast_mode=TownsQtSettings::cpuFastModeEnabled();
	int mhz=TownsQtSettings::cpuFrequencyMhz();
	int custom_mhz=TownsQtSettings::cpuCustomFrequencyMhz();
	if(true==disc_profile_override_active_)
	{
		const QVariantMap &m=disc_profile_machine_override_;
		if(m.contains(QStringLiteral("fast_mode")))
		{
			fast_mode=m.value(QStringLiteral("fast_mode")).toBool();
		}
		if(m.contains(QStringLiteral("frequency_mhz")))
		{
			mhz=std::clamp(m.value(QStringLiteral("frequency_mhz")).toInt(),1,100);
		}
		if(m.contains(QStringLiteral("custom_frequency_mhz")))
		{
			custom_mhz=std::clamp(m.value(QStringLiteral("custom_frequency_mhz")).toInt(),33,60);
		}
	}
	for(QAction *action : cpu_clock_group_->actions())
	{
		if(action==cpu_clock_custom_action_ || action==cpu_compat_action_)
		{
			continue;
		}
		QSignalBlocker blocker(action);
		action->setEnabled(true);
		action->setChecked(true==fast_mode && action->data().toInt()==mhz);
	}
	if(nullptr!=cpu_compat_action_)
	{
		QSignalBlocker blocker(cpu_compat_action_);
		cpu_compat_action_->setEnabled(true);
		cpu_compat_action_->setChecked(true!=fast_mode);
	}
	if(nullptr==cpu_clock_custom_action_)
	{
		return;
	}
	QSignalBlocker blocker(cpu_clock_custom_action_);
	cpu_clock_custom_action_->setText(QStringLiteral("%1MHz").arg(custom_mhz));
	cpu_clock_custom_action_->setData(custom_mhz);
	cpu_clock_custom_action_->setVisible(true);
	cpu_clock_custom_action_->setEnabled(true);
	cpu_clock_custom_action_->setChecked(true==fast_mode && !IsCpuFrequencyPreset(mhz));
}

void MainWindow::syncMenuChecks()
{
	syncCpuClockMenuActions();
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
	applyMouseCoordScanVisibility();
	applyMidiMonitorVisibility();
	applyCdromMonitorVisibility();
	applyAppMonitorVisibility();
	applyCpuDebugVisibility();
	syncFdDriveMenus();
}

void MainWindow::syncFastModeMenu()
{
	syncCpuClockMenuActions();
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

void MainWindow::onGuestFastModeLampChanged(bool /*fast_mode*/)
{
	// Guest FAST lamp is runtime-only (BIOS/TOS often clear it after boot).
	// Never write it back into townsqt.conf — that wiped the saved clock preference
	// and made Settings reopen on Compatible while frequency_mhz was still correct.
	syncCpuClockMenuActions();
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
	if(nullptr!=cdrom_speed_group_)
	{
		for(QAction *action : cdrom_speed_group_->actions())
		{
			QSignalBlocker blocker(action);
			action->setChecked(action->data().toInt()==speed);
		}
	}
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
	if(nullptr!=sprite_dma_group_)
	{
		for(QAction *action : sprite_dma_group_->actions())
		{
			QSignalBlocker blocker(action);
			action->setChecked(action->data().toInt()==mode);
		}
	}
}

void MainWindow::openAudioMixerDialog()
{
	if(nullptr!=audio_mixer_dialog_)
	{
		audio_mixer_dialog_->raise();
		audio_mixer_dialog_->activateWindow();
		return;
	}
	audio_mixer_dialog_=new AudioMixerDialog(this);
	connect(audio_mixer_dialog_,&AudioMixerDialog::volumesChanged,
	        this,&MainWindow::applyAudioMixerVolumes);
	connect(audio_mixer_dialog_,&QObject::destroyed,this,[this]{
		audio_mixer_dialog_=nullptr;
	});
	audio_mixer_dialog_->show();
}

void MainWindow::applyAudioMixerVolumes(int fm_percent,int pcm_percent,int cdda_percent,int midi_percent)
{
	TownsQtSettings::setFmVolumePercent(fm_percent);
	TownsQtSettings::setPcmVolumePercent(pcm_percent);
	TownsQtSettings::setCddaVolumePercent(cdda_percent);
	TownsQtSettings::setMidiVolumePercent(midi_percent);
#if defined(__linux__)
	MidiFluidSynthHost::SetMasterVolumePercent(midi_percent);
#endif
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "applyAudioVolumes",
		    Qt::QueuedConnection,
		    Q_ARG(int,TownsQtSettings::fmChipVolume()),
		    Q_ARG(int,TownsQtSettings::pcmChipVolume()),
		    Q_ARG(int,cdda_percent),
		    Q_ARG(bool,TownsQtSettings::pcmLpfEnabled()),
		    Q_ARG(int,TownsQtSettings::pcmLpfCutoffHz()),
		    Q_ARG(bool,TownsQtSettings::pcmResampleHighQuality()));
	}
}

void MainWindow::openHddSettingsDialog()
{
	HddSettingsDialog::Slot slots[TownsQtSettings::kHddSlotCount];
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		slots[slot].enabled=TownsQtSettings::hddEnabled(slot);
		slots[slot].path=TownsQtSettings::hddImagePath(slot);
		if(!slots[slot].enabled &&
		   slots[slot].path.isEmpty() &&
		   TownsStartParameters::SCSIIMAGE_HARDDISK==argv_.scsiImg[slot].imageType &&
		   !argv_.scsiImg[slot].imgFName.empty())
		{
			slots[slot].enabled=true;
			slots[slot].path=QString::fromStdString(argv_.scsiImg[slot].imgFName);
		}
	}

	HddSettingsDialog dlg(slots,this);
	if(QDialog::Accepted!=dlg.exec())
	{
		return;
	}

	dlg.copySlotsTo(slots);
	bool changed=false;
	for(int slot=0; slot<TownsQtSettings::kHddSlotCount; ++slot)
	{
		if(slots[slot].enabled!=TownsQtSettings::hddEnabled(slot) ||
		   slots[slot].path!=TownsQtSettings::hddImagePath(slot))
		{
			changed=true;
		}
		TownsQtSettings::setHddEnabled(slot,slots[slot].enabled);
		TownsQtSettings::setHddImagePath(slot,slots[slot].path);
		if(TownsStartParameters::SCSIIMAGE_CDROM==argv_.scsiImg[slot].imageType)
		{
			continue;
		}
		if(slots[slot].enabled && !slots[slot].path.isEmpty())
		{
			argv_.scsiImg[slot].imageType=TownsStartParameters::SCSIIMAGE_HARDDISK;
			argv_.scsiImg[slot].imgFName=slots[slot].path.toStdString();
		}
		else
		{
			argv_.scsiImg[slot].imageType=TownsStartParameters::SCSIIMAGE_NONE;
			argv_.scsiImg[slot].imgFName.clear();
		}
	}
	if(true!=changed)
	{
		return;
	}
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

void MainWindow::openSettingsDialog()
{
	if(true==TownsQtSettings::showMouseCoordWriteScan())
	{
		stopMouseCoordScanSession();
		if(true==have_deferred_settings_values_)
		{
			pending_focus_mouse_integration_tab_=true;
		}
	}

	SettingsDialog::Values initial;
	QVariantMap deferredMouse;
	bool restoreDirty=false;
	bool restoreDeferredMouse=false;
	if(true==have_deferred_settings_values_)
	{
		initial=deferred_settings_values_;
		deferredMouse=deferred_mouse_coord_profile_;
		restoreDirty=deferred_settings_dirty_;
		restoreDeferredMouse=true;
		have_deferred_settings_values_=false;
		deferred_settings_dirty_=false;
		deferred_mouse_coord_profile_=QVariantMap();
	}
	else
	{
		initial.cpuFrequencyMhz=TownsQtSettings::cpuFrequencyMhz();
		initial.cpuCustomFrequencyMhz=TownsQtSettings::cpuCustomFrequencyMhz();
		initial.cpuFastMode=TownsQtSettings::cpuFastModeEnabled();
		initial.memSizeInMB=TownsQtSettings::memSizeInMB();
		initial.cpuHighFidelity=TownsQtSettings::cpuHighFidelity();
		initial.pretend386DX=TownsQtSettings::pretend386DX();
		initial.useFPU=TownsQtSettings::useFPU();
		initial.fastScsi=TownsQtSettings::fastScsi();
		initial.fastFd=TownsQtSettings::fastFd();
		initial.midiBoard=TownsQtSettings::midiBoard();
		initial.highResCrtc=TownsQtSettings::highResCrtc();
		initial.highResPcm=TownsQtSettings::highResPcm();
		initial.cpuKind=TownsQtSettings::cpuKind();
		initial.modelGroupIndex=TownsQtSettings::modelGroupIndex();
		initial.displayScale=TownsQtSettings::displayScale();
		initial.autoScaling=TownsQtSettings::autoScaling();
		initial.maintainAspect=TownsQtSettings::maintainAspect();
		initial.damperWireLine=TownsQtSettings::damperWireLine();
		initial.scanLineEffectIn15KHz=TownsQtSettings::scanLineEffectIn15KHz();
		initial.fullscreenVsync=TownsQtSettings::fullscreenVsync();
		initial.windowedVsync=TownsQtSettings::windowedVsync();
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
		initial.midiAlsaPort=TownsQtSettings::midiAlsaPort();
		initial.pcmLpfEnabled=TownsQtSettings::pcmLpfEnabled();
		initial.pcmLpfCutoffHz=TownsQtSettings::pcmLpfCutoffHz();
		initial.waylandIdleInhibit=TownsQtSettings::waylandIdleInhibit();
		initial.gamePort0=TownsQtSettings::gamePort(0);
		initial.gamePort1=TownsQtSettings::gamePort(1);
		initial.maxButtonHoldTimeMs0=TownsQtSettings::maxButtonHoldTimeMs(0,0);
		initial.maxButtonHoldTimeMs1=TownsQtSettings::maxButtonHoldTimeMs(0,1);
		initial.mouseIntegrationSpeed=TownsQtSettings::mouseIntegrationSpeed();
		initial.considerVRAMOffsetInMouseIntegration=TownsQtSettings::considerVRAMOffsetInMouseIntegration();
		initial.autoDifferentialOnMosUnused=TownsQtSettings::autoDifferentialOnMosUnused();
		initial.snapMouseIntegration=TownsQtSettings::snapMouseIntegration();
		initial.snapMouseWarmupFrames=TownsQtSettings::snapMouseWarmupFrames();
		initial.cddaCacheDuringDataRead=TownsQtSettings::cddaCacheDuringDataRead();
		initial.cddaCachePostReadGraceSec=TownsQtSettings::cddaCachePostReadGraceSec();
		initial.mouseMinX=TownsQtSettings::mouseMinX();
		initial.mouseMinY=TownsQtSettings::mouseMinY();
		initial.mouseMaxX=TownsQtSettings::mouseMaxX();
		initial.mouseMaxY=TownsQtSettings::mouseMaxY();
		initial.appSpecificSetting=TownsQtSettings::appSpecificSetting();
		initial.useDiscProfiles=true;
		initial.autoResumeEnabled=TownsQtSettings::autoResumeEnabled();
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
		fillDiscProfileSettings(initial);
	}

	const QString rom_dir=argv_.ROMPath.empty() ?
	    TownsQtPaths::romsDir() :
	    QString::fromStdString(argv_.ROMPath);
	SettingsDialog dlg(initial,rom_dir,this);
	active_settings_dialog_=&dlg;

	auto queryDiscMouseProfile=[&](){
		QVariantMap mouseState;
		if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "mouseCoordWriteScanState",
			    Qt::BlockingQueuedConnection,
			    Q_RETURN_ARG(QVariantMap,mouseState));
		}
		return mouseState;
	};

	auto persistMouseMap=[&](QVariantMap mouseMap){
		if(true!=dlg.values().discProfileAvailable ||
		   nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
		{
			return;
		}
		bool ok=false;
		// Named map: Q_ARG must not bind to a temporary across BlockingQueuedConnection.
		QMetaObject::invokeMethod(
		    controller_,
		    "applyAndSaveMouseCoordProfile",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,ok),
		    Q_ARG(QVariantMap,mouseMap));
		if(nullptr!=statusBar() && true!=ok)
		{
			statusBar()->showMessage(
			    tr("Could not apply mouse profile (need a disc profile)."),
			    5000);
		}
		refreshMouseUiState();
		updateMouseCoordScanDisplay();
	};

	connect(&dlg,&SettingsDialog::settingsApplied,this,[&](const SettingsDialog::Values &v){
		// Capture before applySettings — machine-profile save emits signals that
		// refresh UI state and must not overwrite the editor selection.
		const QVariantMap mouseMap=dlg.mouseCoordProfile();
		applySettings(v);
		persistMouseMap(mouseMap);
	});
	connect(&dlg,&SettingsDialog::createDiscProfileRequested,this,[this,&dlg,&queryDiscMouseProfile](){
		if(nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
		{
			return;
		}
		const auto v=dlg.values();
		bool ok=false;
		QVariantMap machine=DiscMachineMapFromBasics(v);
		InsertCurrentFdMounts(machine,fd_path_);
		QMetaObject::invokeMethod(
		    controller_,
		    "createDiscProfile",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,ok),
		    Q_ARG(QVariantMap,machine));
		if(true!=ok)
		{
			if(nullptr!=statusBar())
			{
				statusBar()->showMessage(tr("Could not create disc profile."),5000);
			}
			return;
		}
		SettingsDialog::Values refreshed=dlg.values();
		fillDiscProfileSettings(refreshed);
		dlg.setDiscProfileState(refreshed);
		applyRuntimeDiscProfileOverrides();
		{
			QVariantMap mouseState=queryDiscMouseProfile();
			// New disc profile: mouse operation type defaults to Default.
			if(true!=mouseState.contains(QStringLiteral("prof_integration_mode")))
			{
				mouseState.insert(QStringLiteral("prof_integration_mode"),
				                  MouseCoordWriteScan::INTEGRATION_AUTO);
				mouseState.insert(QStringLiteral("prof_enabled"),true);
				mouseState.insert(QStringLiteral("prof_feedback_only"),false);
				mouseState.insert(QStringLiteral("prof_verified"),true);
			}
			dlg.setMouseCoordProfile(mouseState,MosIntegrationAvailable(mouseState));
			dlg.focusBasicsTab();
		}
	});
	connect(&dlg,&SettingsDialog::deleteDiscProfileRequested,this,[this,&dlg,&queryDiscMouseProfile](){
		if(nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
		{
			return;
		}
		bool ok=false;
		QMetaObject::invokeMethod(
		    controller_,
		    "deleteDiscProfile",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,ok));
		if(true!=ok)
		{
			if(nullptr!=statusBar())
			{
				statusBar()->showMessage(tr("Could not delete disc profile."),5000);
			}
			return;
		}
		SettingsDialog::Values refreshed=dlg.values();
		fillDiscProfileSettings(refreshed);
		dlg.setDiscProfileState(refreshed);
		applyRuntimeDiscProfileOverrides();
		dlg.setMouseCoordProfile(queryDiscMouseProfile(),false);
		dlg.focusBasicsTab();
		if(nullptr!=statusBar())
		{
			statusBar()->showMessage(tr("Disc profile deleted."),4000);
		}
	});
	connect(&dlg,&SettingsDialog::openMemoryScanRequested,this,[&](){
		deferred_settings_values_=dlg.values();
		deferred_mouse_coord_profile_=dlg.mouseCoordProfile();
		deferred_settings_dirty_=dlg.applyPending();
		have_deferred_settings_values_=true;
		dlg.done(SettingsDialog::ResultOpenMemoryScan);
	});
	connect(&dlg,&SettingsDialog::bindCurrentAppExecRequested,this,[this,&dlg](){
		if(nullptr==controller_)
		{
			return;
		}
		if(true!=controller_->bindCurrentAppExecToMouseProfile())
		{
			return;
		}
		const QVariantMap mouseState=controller_->mouseCoordWriteScanState();
		dlg.setMouseCoordProfile(mouseState,MosIntegrationAvailable(mouseState));
		if(nullptr!=statusBar())
		{
			statusBar()->showMessage(tr("Bound current EXP to disc profile."),4000);
		}
	});

	{
		QVariantMap mouseState;
		if(true==restoreDeferredMouse)
		{
			mouseState=deferredMouse;
		}
		else
		{
			// Stored disc-profile mouse fields — not mouseUiState() (effective runtime mode).
			mouseState=queryDiscMouseProfile();
			if(true==initial.discProfileAvailable &&
			   true!=mouseState.contains(QStringLiteral("prof_integration_mode")))
			{
				mouseState.insert(QStringLiteral("prof_px"),0u);
				mouseState.insert(QStringLiteral("prof_py"),0u);
				mouseState.insert(QStringLiteral("prof_offset_x"),0);
				mouseState.insert(QStringLiteral("prof_offset_y"),0);
				mouseState.insert(QStringLiteral("prof_scale_x"),1);
				mouseState.insert(QStringLiteral("prof_scale_y"),1);
				mouseState.insert(QStringLiteral("prof_verified"),true);
				mouseState.insert(QStringLiteral("prof_integration_mode"),
				                  MouseCoordWriteScan::INTEGRATION_AUTO);
				mouseState.insert(QStringLiteral("prof_feedback_only"),false);
				mouseState.insert(QStringLiteral("prof_enabled"),true);
				for(uint i=0; i<MouseCoordWriteScan::MAX_COORD_PAIRS; ++i)
				{
					const QString base=QStringLiteral("prof_pair%1").arg(i);
					mouseState.insert(base+QStringLiteral("_x"),0u);
					mouseState.insert(base+QStringLiteral("_y"),0u);
					mouseState.insert(base+QStringLiteral("_bias_x"),0);
					mouseState.insert(base+QStringLiteral("_bias_y"),0);
					mouseState.insert(base+QStringLiteral("_min_x"),0);
					mouseState.insert(base+QStringLiteral("_max_x"),0);
					mouseState.insert(base+QStringLiteral("_min_y"),0);
					mouseState.insert(base+QStringLiteral("_max_y"),0);
				}
			}
		}
		dlg.setMouseCoordProfile(mouseState,MosIntegrationAvailable(mouseState));
	}

	if(true==restoreDirty)
	{
		dlg.setApplyPending(true);
	}

	if(true==pending_focus_mouse_integration_tab_)
	{
		pending_focus_mouse_integration_tab_=false;
		dlg.focusMouseIntegrationTab();
		if(true==pending_apply_game_cursor_from_scan_)
		{
			pending_apply_game_cursor_from_scan_=false;
			QVariantList cands;
			if(nullptr!=controller_)
			{
				QMetaObject::invokeMethod(
				    controller_,
				    "mouseCoordWriteScanCandidates",
				    Qt::BlockingQueuedConnection,
				    Q_RETURN_ARG(QVariantList,cands));
			}
			if(nullptr!=mouse_coord_scan_window_)
			{
				mouse_coord_scan_window_->updateCandidates(cands);
				mouse_coord_scan_window_->applyProfileChecks(cands);
				const auto gameCursor=mouse_coord_scan_window_->selectedGameCursor();
				if(true==gameCursor.valid())
				{
					unsigned int dsOffX=0,dsOffY=0,dsSel=0;
					bool hasDsOff=false;
					if(nullptr!=controller_)
					{
						QVariantMap dsMap;
						QMetaObject::invokeMethod(
						    controller_,
						    "captureDsRelativeFromPhys",
						    Qt::BlockingQueuedConnection,
						    Q_RETURN_ARG(QVariantMap,dsMap),
						    Q_ARG(uint,gameCursor.physX),
						    Q_ARG(uint,gameCursor.physY));
						if(true==dsMap.value(QStringLiteral("ok")).toBool())
						{
							dsOffX=dsMap.value(QStringLiteral("ds_off_x")).toUInt();
							dsOffY=dsMap.value(QStringLiteral("ds_off_y")).toUInt();
							dsSel=dsMap.value(QStringLiteral("ds_sel")).toUInt();
							hasDsOff=true;
						}
					}
					dlg.setGameCursorFromSelection(
					    gameCursor.physX,gameCursor.physY,
					    gameCursor.minX,gameCursor.maxX,
					    gameCursor.minY,gameCursor.maxY,
					    gameCursor.hasRangeX,gameCursor.hasRangeY,
					    dsOffX,dsOffY,dsSel,hasDsOff);
					dlg.setApplyPending(true);
				}
				const auto gameCursor2=mouse_coord_scan_window_->selectedGameCursor2();
				if(true==gameCursor2.valid())
				{
					unsigned int dsOffX=0,dsOffY=0,dsSel=0;
					bool hasDsOff=false;
					if(nullptr!=controller_)
					{
						QVariantMap dsMap;
						QMetaObject::invokeMethod(
						    controller_,
						    "captureDsRelativeFromPhys",
						    Qt::BlockingQueuedConnection,
						    Q_RETURN_ARG(QVariantMap,dsMap),
						    Q_ARG(uint,gameCursor2.physX),
						    Q_ARG(uint,gameCursor2.physY));
						if(true==dsMap.value(QStringLiteral("ok")).toBool())
						{
							dsOffX=dsMap.value(QStringLiteral("ds_off_x")).toUInt();
							dsOffY=dsMap.value(QStringLiteral("ds_off_y")).toUInt();
							dsSel=dsMap.value(QStringLiteral("ds_sel")).toUInt();
							hasDsOff=true;
						}
					}
					dlg.setGameCursor2FromSelection(
					    gameCursor2.physX,gameCursor2.physY,
					    dsOffX,dsOffY,dsSel,hasDsOff);
					dlg.setApplyPending(true);
				}
			}
		}
	}

	const int dlg_result=dlg.exec();
	active_settings_dialog_=nullptr;
	if(SettingsDialog::ResultOpenMemoryScan==dlg_result)
	{
		TownsQtSettings::setShowMouseCoordWriteScan(true);
		applyMouseCoordScanVisibility();
		updateMouseCoordScanDisplay();
		return;
	}
	if(QDialog::Accepted!=dlg_result)
	{
		return;
	}
	const QVariantMap mouseMap=dlg.mouseCoordProfile();
	applySettings(dlg.values());
	persistMouseMap(mouseMap);
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
	const TownsQtCpuKind prev_cpu=TownsQtSettings::cpuKind();
	const int prev_model=TownsQtSettings::modelGroupIndex();
	SettingsDialog::Values effective=values;
	const QString rom_dir=argv_.ROMPath.empty() ?
	    TownsQtPaths::romsDir() :
	    QString::fromStdString(argv_.ROMPath);
	const TownsQtSysRomProfile sys_rom_profile=TownsQtRomAvailability::ClassifySysRom(rom_dir);
	const int sys_rom_level=TownsQtRomAvailability::SysRomTownsOsLevel(rom_dir);
	const bool marty_ex_rom=TownsQtRomAvailability::MartyExRomPresent(rom_dir);
	{
		const TownsQtCpuKind clamped_cpu=TownsQtCpuKindClampToAllowed(
		    effective.cpuKind,
		    sys_rom_profile,
		    sys_rom_level,
		    marty_ex_rom);
		const int clamped_model=TownsQtModelGroupClampToAllowed(
		    effective.modelGroupIndex,
		    clamped_cpu,
		    sys_rom_profile,
		    sys_rom_level,
		    marty_ex_rom);
		if(clamped_cpu!=effective.cpuKind || clamped_model!=effective.modelGroupIndex)
		{
			effective.cpuKind=clamped_cpu;
			effective.modelGroupIndex=clamped_model;
			statusBar()->showMessage(
			    tr("The selected CPU/model did not match the SYS ROM, so it was reset to an allowed value."),
			    8000);
		}
		else
		{
			effective.cpuKind=clamped_cpu;
			effective.modelGroupIndex=clamped_model;
		}
	}
	const bool cpu_changed=(effective.cpuKind!=prev_cpu);
	const bool model_changed=(effective.modelGroupIndex!=prev_model);
	const QString prev_audio_backend=TownsQtSettings::audioBackend();
	const QString prev_audio_device=TownsQtSettings::audioDevice();
	const bool prev_auto_resume=TownsQtSettings::autoResumeEnabled();
	const bool needs_emu_restart=
	    (prev_app_specific!=effective.appSpecificSetting) ||
	    (prev_auto_resume!=effective.autoResumeEnabled) ||
	    MachineSettingsNeedRestart(effective,argv_);
	effective.displayScale=std::clamp(effective.displayScale,1,maxDisplayScale());
	effective.autoScaling=false;
	effective.maintainAspect=true;
	TownsQtSettings::setDisplayScale(effective.displayScale);
	TownsQtSettings::setAutoScaling(false);
	TownsQtSettings::setMaintainAspect(true);
	TownsQtSettings::setFullscreenVsync(effective.fullscreenVsync);
	TownsQtSettings::setWindowedVsync(effective.windowedVsync);
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
	TownsQtSettings::setCpuCustomFrequencyMhz(effective.cpuCustomFrequencyMhz);
	// Basics always update global defaults.  Disc-profile clock is saved separately
	// below; do not route through setCpuFrequencyMhz (that merges into the profile).
	TownsQtSettings::setCpuFrequencyMhz(effective.cpuFrequencyMhz);
	TownsQtSettings::setCpuFastModeEnabled(effective.cpuFastMode);
	argv_.freq=static_cast<unsigned int>(std::clamp(effective.cpuFrequencyMhz,1,100));
	argv_.alwaysBootToFASTMode=effective.cpuFastMode;
	if(nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		const bool live_fast=
		    (true==effective.discProfileAvailable) ? effective.profileCpuFastMode : effective.cpuFastMode;
		const int live_mhz=
		    (true==effective.discProfileAvailable) ? effective.profileCpuFrequencyMhz : effective.cpuFrequencyMhz;
		QMetaObject::invokeMethod(
		    controller_,
		    "applyCpuFrequencyMhzLive",
		    Qt::QueuedConnection,
		    Q_ARG(int,std::clamp(live_mhz,1,100)));
		QMetaObject::invokeMethod(
		    controller_,
		    "applyCpuFastModeLive",
		    Qt::QueuedConnection,
		    Q_ARG(bool,live_fast));
	}
	syncMenuChecks();
	TownsQtSettings::setPretend386DX(effective.pretend386DX);
	TownsQtSettings::setUseFPU(effective.useFPU);
	TownsQtSettings::setFastScsi(effective.fastScsi);
	TownsQtSettings::setFastFd(effective.fastFd);
	TownsQtSettings::setMidiBoard(effective.midiBoard);
	{
		const bool hi=TownsQtModelGroupEffectiveHighRes(effective.modelGroupIndex,rom_dir);
		TownsQtSettings::setHighResCrtc(hi);
		TownsQtSettings::setHighResPcm(hi);
		argv_.highResAvailable=hi;
		argv_.highResPCM=hi;
		argv_.ugGenerationIO=TownsQtModelGroupEffectiveUgGenerationIO(
		    effective.modelGroupIndex,
		    rom_dir);
	}
	argv_.nMidiCards=effective.midiBoard ? 1 : 0;
	TownsQtSettings::setMidiSoundFont(effective.midiSoundFont);
	TownsQtSettings::setMidiVolumePercent(effective.midiVolumePercent);
	TownsQtSettings::setMidiOutput(effective.midiOutput);
	TownsQtSettings::setMidiAlsaPort(effective.midiAlsaPort);
#if defined(__linux__)
	{
		const QByteArray sf_utf8=effective.midiSoundFont.toUtf8();
		MidiFluidSynthHost::SetSoundFontPath(sf_utf8.constData());
		MidiFluidSynthHost::SetMasterVolumePercent(effective.midiVolumePercent);
		if(effective.midiOutput==QStringLiteral("alsa"))
		{
			MidiBackendProbe::SetUserPreference(MidiBackendProbe::Kind::AlsaSeq);
			const QByteArray port_utf8=effective.midiAlsaPort.toUtf8();
			MidiAlsaSeqHost::SetDestination(port_utf8.constData());
		}
		else if(effective.midiOutput==QStringLiteral("fluidsynth"))
		{
			MidiBackendProbe::SetUserPreference(MidiBackendProbe::Kind::FluidSynth);
			MidiAlsaSeqHost::SetDestination(nullptr);
		}
		else
		{
			MidiBackendProbe::SetUserPreference(MidiBackendProbe::Kind::None);
			MidiAlsaSeqHost::SetDestination(nullptr);
		}
	}
#endif
	TownsQtSettings::setDamperWireLine(effective.damperWireLine);
	TownsQtSettings::setScanLineEffectIn15KHz(effective.scanLineEffectIn15KHz);
	if(cpu_changed || model_changed)
	{
		TownsQtSettings::setSpriteTransferMode(0);
		TownsQtSettings::setCdSpeed(TownsQtModelGroupDefaultCdSpeed(effective.modelGroupIndex));
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
	TownsQtSettings::setAutoDifferentialOnMosUnused(effective.autoDifferentialOnMosUnused);
	TownsQtSettings::setSnapMouseIntegration(effective.snapMouseIntegration);
	TownsQtSettings::setSnapMouseWarmupFrames(effective.snapMouseWarmupFrames);
	TownsQtSettings::setCddaCacheDuringDataRead(effective.cddaCacheDuringDataRead);
	TownsQtSettings::setCddaCachePostReadGraceSec(effective.cddaCachePostReadGraceSec);
	TownsQtSettings::setMouseMinX(effective.mouseMinX);
	TownsQtSettings::setMouseMinY(effective.mouseMinY);
	TownsQtSettings::setMouseMaxX(effective.mouseMaxX);
	TownsQtSettings::setMouseMaxY(effective.mouseMaxY);
	TownsQtSettings::setAppSpecificSetting(effective.appSpecificSetting);
	TownsQtSettings::setAutoResumeEnabled(effective.autoResumeEnabled);
	updateWindowTitle();
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
	applyWindowScale(std::clamp(effective.displayScale,1,maxDisplayScale()));
	applyDisplayVsync();
	syncDisplayScaleMenu();
	syncWaylandIdleInhibit();
	syncMenuChecks();
	syncGamePortMenus();
	if(!needs_emu_restart && (cpu_changed || model_changed))
	{
		setCdSpeed(TownsQtSettings::cdSpeed());
		setSpriteTransferMode(0);
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
		    Q_ARG(bool,effective.autoDifferentialOnMosUnused),
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
		QMetaObject::invokeMethod(
		    controller_,
		    "applyCddaCacheSettings",
		    Qt::QueuedConnection,
		    Q_ARG(bool,effective.cddaCacheDuringDataRead),
		    Q_ARG(int,effective.cddaCachePostReadGraceSec));
		syncDifferentialMouseCursor();
	}
	if(true==effective.discProfileAvailable &&
	   nullptr!=controller_ && nullptr!=emu_thread_ && emu_thread_->isRunning())
	{
		bool ok=false;
		QVariantMap machine=DiscMachineMapFromProfile(effective);
		InsertCurrentFdMounts(machine,fd_path_);
		QMetaObject::invokeMethod(
		    controller_,
		    "saveDiscMachineProfile",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(bool,ok),
		    Q_ARG(QVariantMap,machine));
		if(nullptr!=statusBar() && true!=ok)
		{
			statusBar()->showMessage(tr("Could not save disc profile."),5000);
		}
	}
	applyRuntimeDiscProfileOverrides();
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

void MainWindow::fillDiscProfileSettings(SettingsDialog::Values &values) const
{
	values.discMounted=false;
	values.discProfileAvailable=false;
	values.discProfileFileName.clear();
	values.profileCpuFrequencyMhz=values.cpuFrequencyMhz;
	values.profileCpuCustomFrequencyMhz=values.cpuCustomFrequencyMhz;
	values.profileCpuFastMode=values.cpuFastMode;
	values.profileMemSizeInMB=values.memSizeInMB;
	values.profileGamePort0=values.gamePort0;
	values.profileGamePort1=values.gamePort1;
	values.profileMaxButtonHoldTimeMs0=values.maxButtonHoldTimeMs0;
	values.profileMaxButtonHoldTimeMs1=values.maxButtonHoldTimeMs1;
	values.profileCpuHighFidelity=values.cpuHighFidelity;
	values.profilePretend386DX=values.pretend386DX;
	values.profileUseFPU=values.useFPU;
	values.profileFastScsi=values.fastScsi;
	values.profileFastFd=values.fastFd;
	values.profileMidiBoard=values.midiBoard;
	values.profileHasMouseIntegration=false;
	if(nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
	{
		return;
	}
	QVariantMap mouseState;
	QMetaObject::invokeMethod(
	    const_cast<EmulatorController *>(controller_),
	    "mouseCoordWriteScanState",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QVariantMap,mouseState));
	values.discMounted=!mouseState.value(QStringLiteral("current_disc_base")).toString().isEmpty();
	values.discProfileAvailable=mouseState.value(QStringLiteral("disc_profile_loaded")).toBool();
	QString fileName=mouseState.value(QStringLiteral("prof_file")).toString();
	if(fileName.isEmpty())
	{
		fileName=mouseState.value(QStringLiteral("prof_cd")).toString();
	}
	values.discProfileFileName=fileName;
	if(true==values.discProfileAvailable)
	{
		auto takeInt=[&](const char *key,int &dst){
			const QString k=QString::fromLatin1(key);
			if(mouseState.contains(k))
			{
				dst=mouseState.value(k).toInt();
			}
		};
		auto takeUInt=[&](const char *key,unsigned int &dst){
			const QString k=QString::fromLatin1(key);
			if(mouseState.contains(k))
			{
				dst=mouseState.value(k).toUInt();
			}
		};
		auto takeBool=[&](const char *key,bool &dst){
			const QString k=QString::fromLatin1(key);
			if(mouseState.contains(k))
			{
				dst=mouseState.value(k).toBool();
			}
		};
		takeInt("prof_frequency_mhz",values.profileCpuFrequencyMhz);
		takeInt("prof_custom_frequency_mhz",values.profileCpuCustomFrequencyMhz);
		takeBool("prof_fast_mode",values.profileCpuFastMode);
		// Older profiles may store only frequency_mhz; a stored MHz implies FAST.
		if(mouseState.contains(QStringLiteral("prof_frequency_mhz")) &&
		   true!=mouseState.contains(QStringLiteral("prof_fast_mode")))
		{
			values.profileCpuFastMode=true;
		}
		takeInt("prof_mem_size_mb",values.profileMemSizeInMB);
		takeUInt("prof_gameport0",values.profileGamePort0);
		takeUInt("prof_gameport1",values.profileGamePort1);
		takeInt("prof_max_button_hold_ms0",values.profileMaxButtonHoldTimeMs0);
		takeInt("prof_max_button_hold_ms1",values.profileMaxButtonHoldTimeMs1);
		takeBool("prof_cpu_high_fidelity",values.profileCpuHighFidelity);
		takeBool("prof_pretend_386dx",values.profilePretend386DX);
		takeBool("prof_use_fpu",values.profileUseFPU);
		takeBool("prof_fast_scsi",values.profileFastScsi);
		takeBool("prof_fast_fd",values.profileFastFd);
		takeBool("prof_midi_board",values.profileMidiBoard);
		values.profileHasMouseIntegration=
		    mouseState.value(QStringLiteral("prof_has_mouse")).toBool();
	}
}

bool MainWindow::runtimeUseDiscProfile() const
{
	return true==cached_disc_profile_loaded_;
}

void MainWindow::applyDiscProfileOverridesToArgv()
{
	if(true!=disc_profile_override_active_ || disc_profile_machine_override_.isEmpty())
	{
		return;
	}
	const QVariantMap &m=disc_profile_machine_override_;
	if(m.contains(QStringLiteral("frequency_mhz")))
	{
		argv_.freq=static_cast<unsigned int>(
		    std::clamp(m.value(QStringLiteral("frequency_mhz")).toInt(),1,100));
	}
	if(m.contains(QStringLiteral("fast_mode")))
	{
		argv_.alwaysBootToFASTMode=m.value(QStringLiteral("fast_mode")).toBool();
	}
	if(m.contains(QStringLiteral("mem_size_mb")))
	{
		argv_.memSizeInMB=static_cast<unsigned int>(
		    std::clamp(m.value(QStringLiteral("mem_size_mb")).toInt(),1,64));
	}
	if(m.contains(QStringLiteral("cpu_high_fidelity")))
	{
		argv_.CPUFidelityLevel=m.value(QStringLiteral("cpu_high_fidelity")).toBool() ?
		    i486DXCommon::HIGH_FIDELITY :
		    i486DXCommon::MID_FIDELITY;
	}
	if(m.contains(QStringLiteral("pretend_386dx")))
	{
		argv_.pretend386DX=m.value(QStringLiteral("pretend_386dx")).toBool();
	}
	if(m.contains(QStringLiteral("use_fpu")))
	{
		argv_.useFPU=m.value(QStringLiteral("use_fpu")).toBool();
	}
	if(m.contains(QStringLiteral("fast_scsi")))
	{
		argv_.fastSCSI=m.value(QStringLiteral("fast_scsi")).toBool();
	}
	if(m.contains(QStringLiteral("fast_fd")))
	{
		argv_.fastFD=m.value(QStringLiteral("fast_fd")).toBool();
	}
	if(m.contains(QStringLiteral("midi_board")))
	{
		argv_.nMidiCards=m.value(QStringLiteral("midi_board")).toBool() ? 1 : 0;
	}
	if(m.contains(QStringLiteral("gameport0")))
	{
		argv_.gamePort[0]=m.value(QStringLiteral("gameport0")).toUInt();
	}
	if(m.contains(QStringLiteral("gameport1")))
	{
		argv_.gamePort[1]=m.value(QStringLiteral("gameport1")).toUInt();
	}
	if(m.contains(QStringLiteral("max_button_hold_ms0")))
	{
		const long long ns=
		    static_cast<long long>(m.value(QStringLiteral("max_button_hold_ms0")).toInt())*1000000LL;
		argv_.maxButtonHoldTime[0][0]=ns;
		argv_.maxButtonHoldTime[1][0]=ns;
	}
	if(m.contains(QStringLiteral("max_button_hold_ms1")))
	{
		const long long ns=
		    static_cast<long long>(m.value(QStringLiteral("max_button_hold_ms1")).toInt())*1000000LL;
		argv_.maxButtonHoldTime[0][1]=ns;
		argv_.maxButtonHoldTime[1][1]=ns;
	}
	auto applyFd=[&](int drive,const char *key){
		if(true!=m.contains(QString::fromLatin1(key)))
		{
			return;
		}
		QString path=m.value(QString::fromLatin1(key)).toString().trimmed();
		if(!path.isEmpty())
		{
			const QString canonical=QFileInfo(path).canonicalFilePath();
			if(!canonical.isEmpty())
			{
				path=canonical;
			}
			if(true!=QFile::exists(path))
			{
				path.clear();
			}
		}
		if(!path.isEmpty())
		{
			argv_.fdImgFName[drive]=path.toStdString();
			fd_path_[drive]=path;
			TownsQtSettings::setLastFdImagePath(drive,path);
		}
		else
		{
			argv_.fdImgFName[drive].clear();
			fd_path_[drive].clear();
		}
	};
	applyFd(0,"fd0");
	applyFd(1,"fd1");
}

void MainWindow::onDiscProfileStateChanged()
{
	refreshMouseUiState();
	applyRuntimeDiscProfileOverrides();
}

void MainWindow::applyRuntimeDiscProfileOverrides()
{
	if(nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
	{
		return;
	}
	QVariantMap state;
	QMetaObject::invokeMethod(
	    controller_,
	    "mouseCoordWriteScanState",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QVariantMap,state));
	cached_disc_profile_loaded_=state.value(QStringLiteral("disc_profile_loaded")).toBool();
	cached_disc_fingerprint_hash32_=state.value(QStringLiteral("disc_fingerprint_hash32")).toUInt();
	if(0==cached_disc_fingerprint_hash32_)
	{
		cached_disc_fingerprint_hash32_=
		    state.value(QStringLiteral("prof_disc_fingerprint_hash32")).toUInt();
	}
	const bool wantOverride=true==cached_disc_profile_loaded_;

	QVariantMap machine;
	auto copyKey=[&](const char *from,const char *to){
		const QString src=QString::fromLatin1(from);
		if(state.contains(src))
		{
			machine.insert(QString::fromLatin1(to),state.value(src));
		}
	};
	if(true==wantOverride)
	{
		copyKey("prof_frequency_mhz","frequency_mhz");
		copyKey("prof_custom_frequency_mhz","custom_frequency_mhz");
		copyKey("prof_fast_mode","fast_mode");
		copyKey("prof_mem_size_mb","mem_size_mb");
		copyKey("prof_gameport0","gameport0");
		copyKey("prof_gameport1","gameport1");
		copyKey("prof_max_button_hold_ms0","max_button_hold_ms0");
		copyKey("prof_max_button_hold_ms1","max_button_hold_ms1");
		copyKey("prof_cpu_high_fidelity","cpu_high_fidelity");
		copyKey("prof_pretend_386dx","pretend_386dx");
		copyKey("prof_use_fpu","use_fpu");
		copyKey("prof_fast_scsi","fast_scsi");
		copyKey("prof_fast_fd","fast_fd");
		copyKey("prof_midi_board","midi_board");
		copyKey("prof_fd0","fd0");
		copyKey("prof_fd1","fd1");
		// Frequency without fast_mode still means FAST for the Operation menu / apply.
		if(machine.contains(QStringLiteral("frequency_mhz")) &&
		   true!=machine.contains(QStringLiteral("fast_mode")))
		{
			machine.insert(QStringLiteral("fast_mode"),true);
		}
	}

	disc_profile_override_active_=wantOverride && !machine.isEmpty();
	disc_profile_machine_override_=disc_profile_override_active_ ? machine : QVariantMap();

	if(true==disc_profile_override_active_)
	{
		if(machine.contains(QStringLiteral("custom_frequency_mhz")))
		{
			TownsQtSettings::setCpuCustomFrequencyMhz(
			    std::clamp(machine.value(QStringLiteral("custom_frequency_mhz")).toInt(),33,60));
		}
		if(machine.contains(QStringLiteral("frequency_mhz")))
		{
			const int mhz=std::clamp(
			    machine.value(QStringLiteral("frequency_mhz")).toInt(),1,100);
			QMetaObject::invokeMethod(
			    controller_,
			    "applyCpuFrequencyMhzLive",
			    Qt::QueuedConnection,
			    Q_ARG(int,mhz));
		}
		if(machine.contains(QStringLiteral("fast_mode")))
		{
			const bool want_fast=machine.value(QStringLiteral("fast_mode")).toBool();
			QMetaObject::invokeMethod(
			    controller_,
			    "applyCpuFastModeLive",
			    Qt::QueuedConnection,
			    Q_ARG(bool,want_fast));
			// Re-assert after BIOS/CMOS may clear wait states.
			QTimer::singleShot(0,this,[this,want_fast]{
				if(nullptr==controller_)
				{
					return;
				}
				QMetaObject::invokeMethod(
				    controller_,
				    "applyCpuFastModeLive",
				    Qt::QueuedConnection,
				    Q_ARG(bool,want_fast));
			});
			QTimer::singleShot(250,this,[this,want_fast]{
				if(nullptr==controller_)
				{
					return;
				}
				QMetaObject::invokeMethod(
				    controller_,
				    "applyCpuFastModeLive",
				    Qt::QueuedConnection,
				    Q_ARG(bool,want_fast));
			});
		}
	}
	syncCpuClockMenuActions();

	const unsigned int gp0=wantOverride && machine.contains(QStringLiteral("gameport0")) ?
	    machine.value(QStringLiteral("gameport0")).toUInt() :
	    TownsQtSettings::gamePort(0);
	const unsigned int gp1=wantOverride && machine.contains(QStringLiteral("gameport1")) ?
	    machine.value(QStringLiteral("gameport1")).toUInt() :
	    TownsQtSettings::gamePort(1);
	const int hold0=wantOverride && machine.contains(QStringLiteral("max_button_hold_ms0")) ?
	    machine.value(QStringLiteral("max_button_hold_ms0")).toInt() :
	    TownsQtSettings::maxButtonHoldTimeMs(0,0);
	const int hold1=wantOverride && machine.contains(QStringLiteral("max_button_hold_ms1")) ?
	    machine.value(QStringLiteral("max_button_hold_ms1")).toInt() :
	    TownsQtSettings::maxButtonHoldTimeMs(0,1);
	const bool midi=wantOverride && machine.contains(QStringLiteral("midi_board")) ?
	    machine.value(QStringLiteral("midi_board")).toBool() :
	    TownsQtSettings::midiBoard();

	QMetaObject::invokeMethod(
	    controller_,
	    "applyPeripheralSettings",
	    Qt::QueuedConnection,
	    Q_ARG(unsigned int,gp0),
	    Q_ARG(unsigned int,gp1),
	    Q_ARG(int,hold0),
	    Q_ARG(int,hold1),
	    Q_ARG(int,TownsQtSettings::mouseIntegrationSpeed()),
	    Q_ARG(bool,TownsQtSettings::considerVRAMOffsetInMouseIntegration()),
	    Q_ARG(bool,TownsQtSettings::autoDifferentialOnMosUnused()),
	    Q_ARG(int,TownsQtSettings::mouseMinX()),
	    Q_ARG(int,TownsQtSettings::mouseMinY()),
	    Q_ARG(int,TownsQtSettings::mouseMaxX()),
	    Q_ARG(int,TownsQtSettings::mouseMaxY()));
	QMetaObject::invokeMethod(
	    controller_,
	    "applyMidiBoard",
	    Qt::QueuedConnection,
	    Q_ARG(bool,midi));

	updateWindowTitle();
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
	if(path.isEmpty())
	{
		return;
	}
	requestCdImageChange(path);
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
			requestCdImageChange(path);
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
	if(path.isEmpty())
	{
		argv_.cdImgFName.clear();
	}
	else
	{
		argv_.cdImgFName=path.toStdString();
		TownsQtSettings::setLastCdImagePath(path);
	}
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
		if(mouseCoordScanActive())
		{
			view_->pollMousePositionForScan();
		}
		else if(shouldCaptureHostMouse())
		{
			view_->pollMousePosition();
		}
		else
		{
			// Other app / settings dialog / popup has focus: do not feed host cursor.
			inputQueue_.ClearMouseButtons();
		}
	}

	const bool nestedUi=
	    nullptr!=QApplication::activeModalWidget() ||
	    nullptr!=QApplication::activePopupWidget();
	if(nullptr!=controller_)
	{
		if(true==nestedUi)
		{
			// Nested modal/popup loops: never BlockingQueued into the emu thread
			// (deadlock risk).  QueuedConnection still presents frames via frameReady.
			QMetaObject::invokeMethod(
			    controller_,
			    &EmulatorController::pollWindow,
			    Qt::QueuedConnection);
		}
		else
		{
			QMetaObject::invokeMethod(
			    controller_,
			    &EmulatorController::pollWindow,
			    Qt::BlockingQueuedConnection);
			if(nullptr!=view_)
			{
				drive_access_status_=controller_->driveAccessStatus();
				view_->updateDriveAccessIndicators(drive_access_status_,currentDriveAccessPresence());
			}
		}
	}
	if(true==nestedUi)
	{
		return;
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
	updateMouseCoordScanDisplay();
	updateMidiMonitorDisplay();
	updateCdromMonitorDisplay();
	updateAppMonitorDisplay();
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
	if(nullptr!=fps_display_action_)
	{
		const bool fps=TownsQtSettings::showFpsDisplay();
		if(fps_display_action_->isChecked()!=fps)
		{
			fps_display_action_->blockSignals(true);
			fps_display_action_->setChecked(fps);
			fps_display_action_->blockSignals(false);
		}
	}
}

void MainWindow::updateWindowTitle()
{
	QString title=QStringLiteral("Tsugaru_QT");
	if(true==TownsQtSettings::showFpsDisplay() && true==have_window_stats_)
	{
		title+=tr(" — %1 FPS | %2 Hz | P:%3 C:%4 lag:%5")
		           .arg(last_stats_fps_,0,'f',1)
		           .arg(last_stats_emu_hz_,0,'f',2)
		           .arg(last_stats_queue_depth_)
		           .arg(last_stats_capture_queue_depth_)
		           .arg(last_stats_present_lag_);
	}
	setWindowTitle(title);
	updateProfileEnabledIndicator();
}

void MainWindow::updateProfileEnabledIndicator()
{
	if(nullptr==profile_enabled_label_)
	{
		return;
	}
	const bool profileActive=true==cached_disc_profile_loaded_;
	if(true==profileActive)
	{
		profile_enabled_label_->setText(tr("Profile enabled"));
		profile_enabled_label_->setToolTip(tr("A disc profile is loaded for the mounted CD."));
	}
	else
	{
		profile_enabled_label_->clear();
		profile_enabled_label_->setToolTip(QString());
	}
}

DriveAccessPresence MainWindow::currentDriveAccessPresence() const
{
	DriveAccessPresence presence;
	const bool marty=TownsQtCpuKindIsMarty(TownsQtSettings::cpuKind());

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

void MainWindow::ensureMouseCoordScanWindow()
{
	if(nullptr!=mouse_coord_scan_window_)
	{
		return;
	}
	mouse_coord_scan_window_=new MouseCoordScanWindow(nullptr);
	connect(mouse_coord_scan_window_,&DebugTextWindow::windowClosed,this,[this](){
		cancelMouseCoordScan();
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::captureToggled,this,[this](bool on){
		if(nullptr==controller_)
		{
			return;
		}
		QMetaObject::invokeMethod(
		    controller_,
		    "setMouseCoordForceCapture",
		    Qt::BlockingQueuedConnection,
		    Q_ARG(bool,on));
		refreshMouseUiState();
		if(on)
		{
			raiseMainWindowForMouseCoordSession();
			QTimer::singleShot(0,this,[this](){
				raiseMainWindowForMouseCoordSession();
			});
		}
		if(nullptr!=statusBar())
		{
			statusBar()->showMessage(
			    on ? tr("Mouse capture ON — list refresh / prune (profile paused). ESC to stop.")
			       : tr("Mouse capture OFF — profile mouse mode restored."),
			    4000);
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::scanToggled,this,[this](bool on){
		if(nullptr==controller_)
		{
			return;
		}
		QMetaObject::invokeMethod(
		    controller_,
		    "setMouseCoordWriteScanEnabled",
		    Qt::BlockingQueuedConnection,
		    Q_ARG(bool,on));
		if(on)
		{
			raiseMainWindowForMouseCoordSession();
			QTimer::singleShot(0,this,[this](){
				raiseMainWindowForMouseCoordSession();
			});
		}
		refreshMouseUiState();
		if(nullptr!=statusBar())
		{
			statusBar()->showMessage(
			    on ? tr("Scan on — adding candidates (mouse capture ON). ESC to stop.")
			       : tr("Scan off — capture may stay on for prune."),
			    4000);
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::scanStopRequested,this,[this](){
		stopMouseCoordScanByEsc();
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::watchPhysChanged,this,
	        [this](const QVariantList &physList){
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setMouseCoordWatchPhys",
			    Qt::BlockingQueuedConnection,
			    Q_ARG(QVariantList,physList));
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::chasePhysChanged,this,
	        [this](const QVariantList &physList){
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setMouseCoordChasePhys",
			    Qt::BlockingQueuedConnection,
			    Q_ARG(QVariantList,physList));
			for(const QVariant &v : physList)
			{
				const unsigned int phys=v.toUInt();
				if(0==phys)
				{
					continue;
				}
				QMetaObject::invokeMethod(
				    controller_,
				    "chaseMouseCoordSource",
				    Qt::BlockingQueuedConnection,
				    Q_ARG(unsigned int,phys));
			}
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::clearRequested,this,[this](){
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "clearMouseCoordWriteScanCandidates",
			    Qt::BlockingQueuedConnection);
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::clearRangesRequested,this,[this](){
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "clearMouseCoordWriteScanRanges",
			    Qt::BlockingQueuedConnection);
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::keepOnlyRequested,this,
	        [this](const QVariantList &physList){
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "keepOnlyMouseCoordWriteScanCandidates",
			    Qt::BlockingQueuedConnection,
			    Q_ARG(QVariantList,physList));
		}
	});
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::editProfileRequested,this,
	        &MainWindow::openMouseCoordProfileEditor);
	connect(mouse_coord_scan_window_,&MouseCoordScanWindow::cancelRequested,this,
	        &MainWindow::cancelMouseCoordScan);
}

void MainWindow::stopMouseCoordScanSession()
{
	TownsQtSettings::setShowMouseCoordWriteScan(false);
	if(nullptr!=controller_)
	{
		QMetaObject::invokeMethod(
		    controller_,
		    "setMouseCoordWriteScanEnabled",
		    Qt::QueuedConnection,
		    Q_ARG(bool,false));
		QMetaObject::invokeMethod(
		    controller_,
		    "setMouseCoordForceCapture",
		    Qt::QueuedConnection,
		    Q_ARG(bool,false));
	}
	if(nullptr!=mouse_coord_scan_window_)
	{
		mouse_coord_scan_window_->setScanChecked(false);
		mouse_coord_scan_window_->setCaptureChecked(false);
	}
	applyMouseCoordScanVisibility();
}

void MainWindow::cancelMouseCoordScan()
{
	if(true!=TownsQtSettings::showMouseCoordWriteScan() &&
	   (nullptr==mouse_coord_scan_window_ || true!=mouse_coord_scan_window_->isVisible()))
	{
		return;
	}
	stopMouseCoordScanSession();
	pending_focus_mouse_integration_tab_=true;
	pending_apply_game_cursor_from_scan_=false;
	QTimer::singleShot(0,this,&MainWindow::openSettingsDialog);
}

void MainWindow::openMouseCoordProfileEditor()
{
	if(nullptr==controller_)
	{
		return;
	}
	QVariantList cands;
	QMetaObject::invokeMethod(
	    controller_,
	    "mouseCoordWriteScanCandidates",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QVariantList,cands));
	if(nullptr!=mouse_coord_scan_window_)
	{
		mouse_coord_scan_window_->updateCandidates(cands);
		mouse_coord_scan_window_->applyProfileChecks(cands);
	}

	MouseCoordGameCursorSelection gameCursor;
	if(nullptr!=mouse_coord_scan_window_)
	{
		gameCursor=mouse_coord_scan_window_->selectedGameCursor();
	}
	if(true!=gameCursor.valid())
	{
		if(nullptr!=statusBar())
		{
			statusBar()->showMessage(
			    tr("Check one X and one Y candidate, then Update."),
			    4000);
		}
		return;
	}

	stopMouseCoordScanSession();
	pending_focus_mouse_integration_tab_=true;
	pending_apply_game_cursor_from_scan_=true;
	if(nullptr!=statusBar())
	{
		statusBar()->showMessage(
		    tr("Updated Mouse integration setting from selected X/Y."),
		    3000);
	}
	QTimer::singleShot(0,this,&MainWindow::openSettingsDialog);
}

void MainWindow::applyMouseCoordScanVisibility()
{
	const bool show=TownsQtSettings::showMouseCoordWriteScan();
	if(show)
	{
		ensureMouseCoordScanWindow();
		if(nullptr!=mouse_coord_scan_window_)
		{
			mouse_coord_scan_window_->show();
			if(QWindow *main_win=windowHandle())
			{
				if(QWindow *scan_win=mouse_coord_scan_window_->windowHandle())
				{
					scan_win->setTransientParent(main_win);
				}
			}
			mouse_coord_scan_window_->raise();
		}
	}
	else if(nullptr!=mouse_coord_scan_window_)
	{
		mouse_coord_scan_window_->hide();
	}
}

void MainWindow::updateMouseCoordScanDisplay()
{
	if(true!=TownsQtSettings::showMouseCoordWriteScan() || nullptr==controller_)
	{
		return;
	}
	ensureMouseCoordScanWindow();
	if(nullptr==mouse_coord_scan_window_)
	{
		return;
	}
	if(true!=mouse_coord_scan_window_->isVisible())
	{
		return;
	}

	QVariantMap state;
	QMetaObject::invokeMethod(
	    controller_,
	    "mouseCoordWriteScanState",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QVariantMap,state));

	mouse_coord_scan_window_->setScanChecked(
	    state.value(QStringLiteral("enabled")).toBool());
	mouse_coord_scan_window_->setCaptureChecked(
	    state.value(QStringLiteral("force_capture")).toBool());

	if(nullptr!=active_settings_dialog_)
	{
		active_settings_dialog_->setMouseBiosActive(MosIntegrationAvailable(state));
		active_settings_dialog_->setLiveAppExec(
		    state.value(QStringLiteral("current_app_exec_name")).toString(),
		    state.value(QStringLiteral("current_app_exec_hash")).toUInt());
	}

	const QString discBase=state.value(QStringLiteral("current_disc_base")).toString();
	if(true!=discBase.isEmpty())
	{
		if(last_mouse_coord_disc_base_.isEmpty())
		{
			last_mouse_coord_disc_base_=discBase;
		}
		else if(discBase!=last_mouse_coord_disc_base_)
		{
			last_mouse_coord_disc_base_=discBase;
			if(nullptr!=mouse_coord_scan_window_)
			{
				mouse_coord_scan_window_->resetForDiscChange();
			}
			QMetaObject::invokeMethod(
			    controller_,
			    "clearMouseCoordWriteScanCandidates",
			    Qt::BlockingQueuedConnection);
		}
	}

	const bool scanOn=state.value(QStringLiteral("enabled")).toBool();
	if(true==scanOn)
	{
		if(0!=(++mouse_coord_scan_ui_div_&1))
		{
			return;
		}
	}
	else if(0!=(++mouse_coord_scan_ui_div_&3))
	{
		return;
	}

	QVariantList cands;
	QMetaObject::invokeMethod(
	    controller_,
	    "mouseCoordWriteScanCandidates",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QVariantList,cands));
	mouse_coord_scan_window_->updateCandidates(cands);
	mouse_coord_scan_window_->applyProfileChecks(cands);
	{
		QVariantList followed;
		QMetaObject::invokeMethod(
		    controller_,
		    "takeMouseCoordFollowedSources",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(QVariantList,followed));
		mouse_coord_scan_window_->markFollowedSources(followed);

		QVariantList clearedChase;
		QMetaObject::invokeMethod(
		    controller_,
		    "takeMouseCoordClearedChase",
		    Qt::BlockingQueuedConnection,
		    Q_RETURN_ARG(QVariantList,clearedChase));
		mouse_coord_scan_window_->clearChaseFlags(clearedChase);
	}

	QString body;
	body+=QStringLiteral("Mouse coord scan — move mouse to find coordinate addresses\n");
	body+=QStringLiteral("scan=%1  candidates=%2  trace_rem=%3  io_arms=%4\n")
	          .arg(scanOn ? QStringLiteral("on") : QStringLiteral("off"))
	          .arg(state.value(QStringLiteral("cand")).toUInt())
	          .arg(state.value(QStringLiteral("remaining")).toUInt())
	          .arg(state.value(QStringLiteral("arm_io")).toUInt());
	if(true!=scanOn)
	{
		body+=QStringLiteral("Scan = add candidates (+ capture ON).  Capture alone = refresh/prune.\n");
	}
	else if(true==state.value(QStringLiteral("profile_apply")).toBool())
	{
		body+=QStringLiteral("Warning: profile still applying — candidates may not grow.\n");
	}
	body+=QStringLiteral("ESC ends capture and Scan, restores profile mouse mode.\n");
	if(true==state.value(QStringLiteral("disc_id_valid")).toBool())
	{
		body+=QStringLiteral("disc file: %1\n")
		          .arg(state.value(QStringLiteral("current_disc_base")).toString());
		if(true==state.value(QStringLiteral("disc_has_content_id")).toBool())
		{
			body+=QStringLiteral("disc id: %1  hash=%2\n")
			              .arg(state.value(QStringLiteral("disc_content_key")).toString())
			              .arg(state.value(QStringLiteral("disc_content_hash")).toString());
		}
		else if(true==state.value(QStringLiteral("disc_has_iso9660")).toBool())
		{
			body+=QStringLiteral("iso9660: vol=\"%1\"  sys=\"%2\"  (no content id)\n")
			              .arg(state.value(QStringLiteral("disc_volume_label")).toString())
			              .arg(state.value(QStringLiteral("disc_system_id")).toString());
		}
		else
		{
			body+=QStringLiteral("iso9660: (PVD not found)\n");
		}
		if(true==state.value(QStringLiteral("disc_has_fingerprint")).toBool())
		{
			body+=QStringLiteral("fingerprint=%1  audio=%2 data=%3\n")
			              .arg(state.value(QStringLiteral("disc_fingerprint_hash")).toString())
			              .arg(state.value(QStringLiteral("disc_num_audio_tracks")).toUInt())
			              .arg(state.value(QStringLiteral("disc_num_data_tracks")).toUInt());
		}
		else
		{
			body+=QStringLiteral("fingerprint: (unavailable)\n");
		}
		body+=QStringLiteral("toc=%1 (format-dependent, not used for profile match)\n")
		          .arg(state.value(QStringLiteral("disc_toc_hash")).toString());
	}
	mouse_coord_scan_window_->setLiveText(body);
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
		meta_text+=QStringLiteral("\nDiff:%1 Forced:%2 Unu:%3 Pref:%4 CapRel:%5 Feed:%6")
		               .arg(guest.value(QStringLiteral("diff_eff")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("diff_forced")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("diff_mos_unused")).toBool() ? 1 : 0)
		               .arg(TownsQtSettings::differentialMouseIntegration() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("capture_released")).toBool() ? 1 : 0)
		               .arg(guest.value(QStringLiteral("feeding")).toBool() ? 1 : 0);
		{
			const QString appName=guest.value(QStringLiteral("app_exec_name")).toString();
			const unsigned int appHash=guest.value(QStringLiteral("app_exec_hash")).toUInt();
			const int bind=guest.value(QStringLiteral("app_exec_bound")).toBool() ? 1 : 0;
			const int matched=guest.value(QStringLiteral("app_exec_matched")).toBool() ? 1 : 0;
			const int apply=guest.value(QStringLiteral("profile_apply")).toBool() ? 1 : 0;
			meta_text+=QStringLiteral("\nApp:%1 hash=0x%2 Bind:%3 Match:%4 Apply:%5")
			               .arg(true==appName.isEmpty() ? QStringLiteral("-") : appName)
			               .arg(appHash,8,16,QLatin1Char('0'))
			               .arg(bind)
			               .arg(matched)
			               .arg(apply);
		}
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

void MainWindow::ensureCdromMonitorWindow()
{
	if(nullptr!=cdrom_monitor_window_)
	{
		return;
	}
	cdrom_monitor_window_=new CdromMonitorWindow(this);
	connect(cdrom_monitor_window_,&DebugTextWindow::windowClosed,this,[this](){
		TownsQtSettings::setCdromMonitor(false);
		if(nullptr!=controller_)
		{
			QMetaObject::invokeMethod(
			    controller_,
			    "setCdromMonitor",
			    Qt::QueuedConnection,
			    Q_ARG(bool,false));
		}
		applyCdromMonitorVisibility();
	});
}

void MainWindow::applyCdromMonitorVisibility()
{
	const bool show=TownsQtSettings::cdromMonitor();
	if(nullptr!=cdrom_monitor_action_ && cdrom_monitor_action_->isChecked()!=show)
	{
		cdrom_monitor_action_->blockSignals(true);
		cdrom_monitor_action_->setChecked(show);
		cdrom_monitor_action_->blockSignals(false);
	}
	if(show)
	{
		ensureCdromMonitorWindow();
		if(nullptr!=cdrom_monitor_window_)
		{
			cdrom_monitor_window_->show();
			cdrom_monitor_window_->raise();
		}
	}
	else if(nullptr!=cdrom_monitor_window_)
	{
		cdrom_monitor_window_->hide();
	}
}

void MainWindow::updateCdromMonitorDisplay()
{
	if(!TownsQtSettings::cdromMonitor())
	{
		return;
	}
	ensureCdromMonitorWindow();
	if(nullptr==cdrom_monitor_window_ ||
	   nullptr==controller_ ||
	   nullptr==emu_thread_ ||
	   !emu_thread_->isRunning())
	{
		return;
	}

	// Fetch on the emu thread — monitorLines_ are written from the VM thread.
	QStringList lines;
	const bool ok=QMetaObject::invokeMethod(
	    controller_,
	    "takeCdromMonitorLines",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QStringList,lines));
	if(!ok)
	{
		return;
	}
	for(const QString &line : lines)
	{
		cdrom_monitor_window_->appendMonitorLine(line);
	}
}

void MainWindow::ensureAppMonitorWindow()
{
	if(nullptr!=app_monitor_window_)
	{
		return;
	}
	app_monitor_window_=new DebugTextWindow(tr("APP monitor"),this);
	connect(app_monitor_window_,&DebugTextWindow::windowClosed,this,[this](){
		TownsQtSettings::setAppMonitor(false);
		applyAppMonitorVisibility();
	});
}

void MainWindow::applyAppMonitorVisibility()
{
	const bool show=TownsQtSettings::appMonitor();
	if(nullptr!=app_monitor_action_ && app_monitor_action_->isChecked()!=show)
	{
		app_monitor_action_->blockSignals(true);
		app_monitor_action_->setChecked(show);
		app_monitor_action_->blockSignals(false);
	}
	if(show)
	{
		ensureAppMonitorWindow();
		if(nullptr!=app_monitor_window_)
		{
			app_monitor_window_->show();
			app_monitor_window_->raise();
		}
	}
	else if(nullptr!=app_monitor_window_)
	{
		app_monitor_window_->hide();
	}
}

void MainWindow::updateAppMonitorDisplay()
{
	if(!TownsQtSettings::appMonitor())
	{
		return;
	}
	ensureAppMonitorWindow();
	if(nullptr==app_monitor_window_ ||
	   nullptr==controller_ ||
	   nullptr==emu_thread_ ||
	   !emu_thread_->isRunning())
	{
		return;
	}

	QStringList lines;
	const bool ok=QMetaObject::invokeMethod(
	    controller_,
	    "takeAppMonitorLines",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(QStringList,lines));
	if(!ok)
	{
		return;
	}
	for(const QString &line : lines)
	{
		app_monitor_window_->appendLine(line);
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
	// Do not touch last_emu_activity_ms_ here: pollWindow emits stats even while the VM
	// is stalled (e.g. disc I/O).  Only onFrameReady means the emu is actually advancing.
	have_window_stats_=true;
	last_stats_fps_=fps;
	last_stats_emu_hz_=emu_hz;
	last_stats_queue_depth_=queue_depth;
	last_stats_capture_queue_depth_=capture_queue_depth;
	last_stats_present_lag_=present_lag;
	updateWindowTitle();
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
		applyDisplayVsync();
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
		applyDisplayVsync();
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
	if(QEvent::KeyPress==event->type())
	{
		const auto *key_event=static_cast<const QKeyEvent *>(event);
		if(nullptr!=key_event && Qt::Key_Escape==key_event->key() &&
		   true!=key_event->isAutoRepeat())
		{
			if(true==mouseCoordScanActive())
			{
				stopMouseCoordScanByEsc();
				return true;
			}
			if(nullptr!=controller_ &&
			   nullptr!=emu_thread_ && true==emu_thread_->isRunning() &&
			   true==cached_differential_integration_ &&
			   true!=cached_mouse_capture_released_)
			{
				releaseDifferentialMouseCaptureByEsc();
				return true;
			}
		}
	}
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
	applyDisplayVsync();
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
	// Drop size locks so the compositor can take the full screen.
	allow_window_resize_=true;
	intended_window_size_=QSize();
	setMinimumSize(0,0);
	setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
	if(nullptr!=view_)
	{
		view_->setMinimumSize(0,0);
		view_->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
	}
	showFullScreen();
	// Fullscreen: largest integer scale that fits the host display (may exceed windowed max).
	applyWindowScale(maxDisplayScaleForFullscreen());
	allow_window_resize_=false;
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
	allow_window_resize_=true;
	if(!windowed_geometry_.isNull())
	{
		setGeometry(windowed_geometry_);
	}
	applyWindowScale(TownsQtSettings::displayScale());
	allow_window_resize_=false;
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

bool MainWindow::mouseCoordScanActive() const
{
	return TownsQtSettings::showMouseCoordWriteScan() &&
	       nullptr!=mouse_coord_scan_window_ &&
	       (mouse_coord_scan_window_->scanChecked() ||
	        mouse_coord_scan_window_->captureChecked());
}

void MainWindow::raiseMainWindowForMouseCoordSession()
{
	if(true==isMinimized())
	{
		showNormal();
	}
	raise();
	activateWindow();
}

void MainWindow::stopMouseCoordScanByEsc()
{
	if(nullptr==controller_ || nullptr==mouse_coord_scan_window_)
	{
		return;
	}
	QMetaObject::invokeMethod(
	    controller_,
	    "setMouseCoordWriteScanEnabled",
	    Qt::BlockingQueuedConnection,
	    Q_ARG(bool,false));
	QMetaObject::invokeMethod(
	    controller_,
	    "setMouseCoordForceCapture",
	    Qt::BlockingQueuedConnection,
	    Q_ARG(bool,false));
	mouse_coord_scan_window_->setScanChecked(false);
	mouse_coord_scan_window_->setCaptureChecked(false);
	refreshMouseUiState();
	mouse_coord_scan_window_->raise();
	mouse_coord_scan_window_->activateWindow();
	mouse_coord_scan_window_->setFocus(Qt::OtherFocusReason);
	if(nullptr!=statusBar())
	{
		statusBar()->showMessage(
		    tr("Stopped (ESC) — profile mouse mode restored."),4000);
	}
}

void MainWindow::releaseDifferentialMouseCaptureByEsc()
{
	if(nullptr==controller_ ||
	   nullptr==emu_thread_ ||
	   true!=emu_thread_->isRunning() ||
	   true!=cached_differential_integration_ ||
	   true==cached_mouse_capture_released_)
	{
		return;
	}
	QMetaObject::invokeMethod(
	    controller_,
	    "releaseMouseCapture",
	    Qt::BlockingQueuedConnection);
	refreshMouseUiState();
	syncWaylandRelativePointer();
	updateBlankCursor();
	if(nullptr!=statusBar())
	{
		statusBar()->showMessage(tr("Mouse capture released (ESC)."),4000);
	}
}

bool MainWindow::shouldCaptureHostMouse() const
{
	if(!isVisible())
	{
		return false;
	}
	if(nullptr!=QApplication::activeModalWidget() ||
	   nullptr!=QApplication::activePopupWidget())
	{
		return false;
	}
	if(isCursorOverUiChrome())
	{
		return false;
	}
	// Coord scan: keep feeding host motion even when the scan window has focus.
	if(true==mouseCoordScanActive())
	{
		return true;
	}
	return isActiveWindow();
}

bool MainWindow::shouldKeepDifferentialWaylandCapture() const
{
	// Once differential capture is active, do not drop it when the (locked) cursor
	// is reported over chrome — that Start/Stop oscillation freezes input.
	return isVisible() &&
	       !isMinimized() &&
	       isActiveWindow() &&
	       nullptr==QApplication::activeModalWidget() &&
	       nullptr==QApplication::activePopupWidget();
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
		cached_mouse_profile_loaded_=false;
		cached_mouse_profile_apply_=false;
		cached_integration_mode_=MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL;
		cached_disc_profile_loaded_=false;
		if(nullptr!=view_)
		{
			view_->setMouseCaptureReleased(false);
		}
		updateMouseModeIndicator();
		updateWindowTitle();
		return;
	}
	cached_differential_integration_=state.value(QStringLiteral("diff")).toBool();
	cached_mouse_bios_active_=MosIntegrationAvailable(state);
	cached_mouse_capture_released_=state.value(QStringLiteral("capture_released")).toBool();
	cached_mouse_profile_loaded_=state.value(QStringLiteral("profile_loaded")).toBool();
	cached_mouse_profile_apply_=state.value(QStringLiteral("profile_apply")).toBool();
	cached_integration_mode_=state.value(
	    QStringLiteral("integration_mode"),
	    MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL).toInt();
	cached_disc_profile_loaded_=state.value(QStringLiteral("disc_profile_loaded")).toBool();
	cached_disc_fingerprint_hash32_=state.value(QStringLiteral("disc_fingerprint_hash32")).toUInt();
	if(0==cached_disc_fingerprint_hash32_)
	{
		cached_disc_fingerprint_hash32_=
		    state.value(QStringLiteral("prof_disc_fingerprint_hash32")).toUInt();
	}
	if(nullptr!=active_settings_dialog_)
	{
		active_settings_dialog_->setMouseBiosActive(cached_mouse_bios_active_);
		active_settings_dialog_->setLiveAppExec(
		    state.value(QStringLiteral("app_exec_name")).toString(),
		    state.value(QStringLiteral("app_exec_hash")).toUInt());
	}
	if(nullptr!=view_)
	{
		view_->setMouseCaptureReleased(cached_mouse_capture_released_);
	}
	updateMouseModeIndicator();
	updateWindowTitle();
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

	if(true==mouseCoordScanActive())
	{
		const QString escHint=tr("Exit ESC");
		const QString text=(0==mouse_mode_phase_) ? tr("Mouse capture") : escHint;
		mouse_mode_label_->setText(text);
		mouse_mode_label_->setToolTip(
		    tr("Memory scan mouse capture is active. Press ESC to stop Scan and capture."));
		if(-1==mouse_mode_category_ || 3!=mouse_mode_category_)
		{
			inputQueue_.ClearMouseButtons();
			mouse_mode_category_=3;
			mouse_mode_phase_=0;
		}
		return;
	}

	// 0 = integrated (MOS / memory write / game port — see cached_integration_mode_)
	// 1 = captured (differential)
	// 2 = released (differential)
	// Capture-released reports effectiveDifferential=false (feeding off) so the
	// absolute path stays idle; the UI must key off capture_released, not diff.
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
	if(category!=mouse_mode_category_ ||
	   cached_integration_mode_!=mouse_mode_integration_for_buttons_)
	{
		// abs↔diff / MOS↔app-specific / capture on↔off: drop stale presses.
		inputQueue_.ClearMouseButtons();
		mouse_mode_category_=category;
		mouse_mode_integration_for_buttons_=cached_integration_mode_;
		mouse_mode_phase_=0;
	}

	QString text;
	QString tip;
	switch(category)
	{
	case 0:
		// Running absolute path — label matches effective mode from mouseUiState.
		switch(cached_integration_mode_)
		{
		case MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE:
		case MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK:
			text=tr("Mouse integration (app-specific settings)");
			tip=tr("Mouse integration (app-specific settings): follows Default until a bound EXP starts, "
			       "then applies per-app Phys settings for the in-game cursor.");
			break;
		case MouseCoordWriteScan::INTEGRATION_MOS:
		default:
			text=tr("Mouse integration (Mouse BIOS)");
			tip=tr("Mouse integration (Mouse BIOS): forces mouse integration via Mouse BIOS.");
			break;
		}
		break;
	case 1:
		// Mouse capture, captured: alternate the state word with the release hint.
		text=(0==mouse_mode_phase_) ? tr("Mouse capture") : tr("Middle button to release");
		tip=tr("Mouse capture is active. "
		       "Press the middle mouse button to release capture.");
		break;
	default:
		// Mouse capture, released: alternate the state word with the capture hint.
		text=(0==mouse_mode_phase_) ? tr("Mouse capture (released)") : tr("Click to capture");
		tip=tr("Mouse capture released. Click the screen to start capture; "
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
	// Capture resume is for differential only. Absolute/snap clears any stale
	// capture-released flag in UpdateEffectiveDifferentialMouseIntegration.
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
			// Absolute/snap: hide the host cursor over the picture while focused.
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

QSize MainWindow::computeWindowedSizeForScale(int scale) const
{
	scale=std::max(1,scale);
	const int content_w=640*scale;
	const int content_h=480*scale;
	int menu_h=0;
	int status_h=0;
	if(nullptr!=menuBar() && false==menuBar()->isHidden())
	{
		menu_h=std::max(menuBar()->height(),menuBar()->sizeHint().height());
	}
	if(nullptr!=statusBar() && statusBar()->isVisible())
	{
		status_h=std::max(statusBar()->height(),statusBar()->sizeHint().height());
	}
	// Client area only: EMU surface + menu + status. No extra padding (that caused side/bottom gaps).
	return QSize(content_w,content_h+menu_h+status_h);
}

int MainWindow::maxDisplayScale() const
{
	QScreen *screen=this->screen();
	if(nullptr==screen)
	{
		screen=QGuiApplication::primaryScreen();
	}
	if(nullptr==screen)
	{
		return 1;
	}
	// availableGeometry is DIP (DE scale applied). Use sizeHint chrome even if bars are hidden
	// so windowed/fullscreen configurable max stays consistent.
	const QSize avail=screen->availableGeometry().size();
	int menu_h=0;
	int status_h=0;
	if(nullptr!=menuBar())
	{
		menu_h=menuBar()->sizeHint().height();
	}
	if(nullptr!=statusBar())
	{
		status_h=statusBar()->sizeHint().height();
	}
	return TownsQtSettings::maxDisplayScaleForAvailableSize(avail,0,menu_h+status_h);
}

int MainWindow::maxDisplayScaleForFullscreen() const
{
	QScreen *screen=this->screen();
	if(nullptr==screen)
	{
		screen=QGuiApplication::primaryScreen();
	}
	if(nullptr==screen)
	{
		return maxDisplayScale();
	}
	// Fullscreen client is the monitor; no menu/status chrome.
	return TownsQtSettings::maxDisplayScaleForAvailableSize(screen->geometry().size(),0,0);
}

void MainWindow::setDisplayScale(int scale)
{
	const int max_scale=maxDisplayScale();
	scale=std::clamp(scale,1,max_scale);
	TownsQtSettings::setDisplayScale(scale);
	TownsQtSettings::setAutoScaling(false);
	TownsQtSettings::setMaintainAspect(true);
	// Windowed: apply chosen scale. Fullscreen: keep showing host-fit max (preference saved for restore).
	if(!fullscreen_ && !isFullScreen())
	{
		applyWindowScale(scale);
	}
	else
	{
		applyWindowScale(maxDisplayScaleForFullscreen());
	}
	syncDisplayScaleMenu();
}

void MainWindow::bumpDisplayScale(int delta)
{
	setDisplayScale(TownsQtSettings::displayScale()+delta);
}

void MainWindow::syncDisplayScaleMenu()
{
	if(nullptr==display_scale_menu_ || nullptr==display_scale_group_)
	{
		return;
	}
	const int max_scale=maxDisplayScale();
	const int current=std::clamp(TownsQtSettings::displayScale(),1,max_scale);
	if(current!=TownsQtSettings::displayScale())
	{
		TownsQtSettings::setDisplayScale(current);
	}
	const QList<QAction *> stale=display_scale_group_->actions();
	for(QAction *old : stale)
	{
		display_scale_group_->removeAction(old);
	}
	display_scale_menu_->clear();
	for(int scale=1; scale<=max_scale; ++scale)
	{
		auto *action=display_scale_menu_->addAction(tr("%1x").arg(scale));
		action->setCheckable(true);
		action->setData(scale);
		action->setChecked(scale==current);
		display_scale_group_->addAction(action);
	}
	display_scale_menu_->setTitle(tr("Window scale (%1x)").arg(current));
	if(nullptr!=scale_down_action_)
	{
		scale_down_action_->setEnabled(1<current);
	}
	if(nullptr!=scale_up_action_)
	{
		scale_up_action_->setEnabled(current<max_scale);
	}
}

void MainWindow::applyWindowScale(int scale)
{
	const bool windowed=!fullscreen_ && !isFullScreen();
	const int max_scale=windowed ? maxDisplayScale() : maxDisplayScaleForFullscreen();
	if(true==windowed)
	{
		scale=std::clamp(scale,1,max_scale);
	}
	else
	{
		// Fullscreen always renders at the largest integer scale that fits the host display.
		scale=max_scale;
	}
	argv_.scaling=static_cast<unsigned int>(scale)*100U;
	argv_.autoScaling=false;
	argv_.maintainAspect=true;

	const int content_w=640*scale;
	const int content_h=480*scale;

	const bool resume_lock=!allow_window_resize_;
	allow_window_resize_=true;
	setMinimumSize(0,0);
	setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);

	if(nullptr!=view_)
	{
		view_->setMinimumSize(0,0);
		view_->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);
		view_->setVideoOptions(scale,false,true);
		if(true==windowed)
		{
			view_->setMinimumSize(content_w,content_h);
			view_->setMaximumSize(content_w,content_h);
		}
		view_->updateGeometry();
	}

	if(true==windowed)
	{
		if(QLayout *lay=layout())
		{
			lay->activate();
		}
		updateGeometry();
		intended_window_size_=computeWindowedSizeForScale(scale);
		resize(intended_window_size_);
		windowed_geometry_=QRect(pos(),intended_window_size_);
	}
	else
	{
		intended_window_size_=QSize();
	}

	if(true==resume_lock)
	{
		allow_window_resize_=false;
	}
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	if(true==allow_window_resize_ || true==fullscreen_ || true==isFullScreen())
	{
		return;
	}
	if(!intended_window_size_.isValid() || size()==intended_window_size_)
	{
		return;
	}
	// Reject user drag-resize / maximize; keep window matched to EMU scale.
	allow_window_resize_=true;
	resize(intended_window_size_);
	allow_window_resize_=false;
}

void MainWindow::applyDisplayVsync()
{
	if(nullptr==view_)
	{
		return;
	}
	const bool enable=isFullScreen() ?
	    TownsQtSettings::fullscreenVsync() :
	    TownsQtSettings::windowedVsync();
	view_->setDisplayVsync(enable);
}

void MainWindow::syncWaylandIdleInhibit()
{
	const bool want=TownsQtSettings::waylandIdleInhibit() && isVisible() && !isMinimized();
	TownsQtWaylandIdleInhibit::Apply(want ? windowHandle() : nullptr,want);
}

void MainWindow::syncWaylandRelativePointer()
{
	const bool focus_ok=shouldKeepDifferentialWaylandCapture();
	const bool want=
	    cached_differential_integration_ &&
	    !cached_mouse_capture_released_ &&
	    !mouse_failsafe_show_cursor_ &&
	    focus_ok &&
	    (wayland_capture_want_ || shouldCaptureHostMouse());

	if(!want)
	{
		if(wayland_capture_want_)
		{
			TownsQtWaylandRelativePointer::Stop();
			wayland_capture_want_=false;
			wayland_capture_method_.clear();
			inputQueue_.ClearMouseButtons();
		}
		return;
	}

	QWindow *win=windowHandle();
	if(nullptr==win)
	{
		if(wayland_capture_want_)
		{
			TownsQtWaylandRelativePointer::Stop();
			wayland_capture_want_=false;
			wayland_capture_method_.clear();
		}
		return;
	}

	if(wayland_capture_want_ && TownsQtWaylandRelativePointer::Active())
	{
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

	wayland_capture_want_=true;
	wayland_capture_method_=method;
	inputQueue_.ClearMouseButtons();
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

void MainWindow::loadStateSlotFromMenu(int slot)
{
	if(nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
	{
		statusBar()->showMessage(tr("Emulator is not running"),5000);
		return;
	}
	bool ok=false;
	const bool invoked=QMetaObject::invokeMethod(
	    controller_,
	    "loadStateSlot",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(bool,ok),
	    Q_ARG(int,slot));
	if(true!=invoked)
	{
		statusBar()->showMessage(tr("Failed to load state slot %1").arg(slot),5000);
		return;
	}
	if(true==ok)
	{
		statusBar()->showMessage(tr("State slot %1 loaded").arg(slot),5000);
	}
	else if(0==slot)
	{
		statusBar()->showMessage(tr("No resume state for the mounted CD (slot 0)"),5000);
	}
	else
	{
		statusBar()->showMessage(tr("Failed to load state slot %1").arg(slot),5000);
	}
}

void MainWindow::saveStateSlotFromMenu(int slot)
{
	if(nullptr==controller_ || nullptr==emu_thread_ || true!=emu_thread_->isRunning())
	{
		statusBar()->showMessage(tr("Emulator is not running"),5000);
		return;
	}
	bool ok=false;
	const bool invoked=QMetaObject::invokeMethod(
	    controller_,
	    "saveStateSlot",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(bool,ok),
	    Q_ARG(int,slot));
	if(true!=invoked || true!=ok)
	{
		statusBar()->showMessage(tr("Failed to save state slot %1").arg(slot),5000);
		return;
	}
	statusBar()->showMessage(tr("State slot %1 saved").arg(slot),5000);
}

void MainWindow::maybeSaveDiscStateSaveBeforeStop(EmulatorController *controller)
{
	if(nullptr==controller)
	{
		return;
	}
	bool saved=false;
	const bool invoked=QMetaObject::invokeMethod(
	    controller,
	    "saveDiscStateSaveIfProfiled",
	    Qt::BlockingQueuedConnection,
	    Q_RETURN_ARG(bool,saved),
	    Q_ARG(bool,false));
	if(true!=invoked)
	{
		std::cerr << "Tsugaru_QT: saveDiscStateSaveIfProfiled invoke failed" << std::endl;
		return;
	}
	if(true!=saved)
	{
		std::cerr << "Tsugaru_QT: disc state save failed (no profile or write failed)" << std::endl;
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
	if(nullptr!=stopping && true!=emu_restarting_)
	{
		maybeSaveDiscStateSaveBeforeStop(stopping);
	}
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
