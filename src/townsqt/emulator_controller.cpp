#include "emulator_controller.h"

#undef slots

#include "qt_command_thread.h"
#include "qt_outside_world.h"
#include "cdrom.h"
#include "discimg.h"
#include "towns.h"
#include "townsdef.h"
#include "qt_sync_sound.h"
#include "townsqt_paths.h"
#include "townsqt_disc_statesave.h"
#include "townsqt_cpu_profile.h"
#include "townsqt_model_profile.h"
#include "townsqt_settings.h"
#include "townsdef.h"
#include "townsthread.h"
#include "townscmos_util.h"
#include "townscommandutil.h"
#include "miscutil.h"

#include "fssimplewindow_connection.h"
#include "cpputil.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimer>
#include <QVariantMap>
#include <QVariantList>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
MouseCoordWriteScan::MachineSettings MachineSettingsFromVariantMap(const QVariantMap &machine)
{
	MouseCoordWriteScan::MachineSettings m;
	auto takeInt=[&](const char *key,bool &has,int &dst,int lo,int hi){
		if(true!=machine.contains(QString::fromLatin1(key)))
		{
			return;
		}
		has=true;
		dst=std::clamp(machine.value(QString::fromLatin1(key)).toInt(),lo,hi);
	};
	auto takeBool=[&](const char *key,bool &has,bool &dst){
		if(true!=machine.contains(QString::fromLatin1(key)))
		{
			return;
		}
		has=true;
		dst=machine.value(QString::fromLatin1(key)).toBool();
	};
	auto takeUInt=[&](const char *key,bool &has,unsigned int &dst){
		if(true!=machine.contains(QString::fromLatin1(key)))
		{
			return;
		}
		has=true;
		dst=machine.value(QString::fromLatin1(key)).toUInt();
	};
	takeInt("frequency_mhz",m.hasFrequencyMhz,m.frequencyMhz,1,100);
	takeInt("custom_frequency_mhz",m.hasCustomFrequencyMhz,m.customFrequencyMhz,33,60);
	takeBool("fast_mode",m.hasFastMode,m.fastMode);
	takeInt("mem_size_mb",m.hasMemSizeInMB,m.memSizeInMB,1,64);
	takeUInt("gameport0",m.hasGamePort0,m.gamePort0);
	takeUInt("gameport1",m.hasGamePort1,m.gamePort1);
	takeInt("max_button_hold_ms0",m.hasMaxButtonHoldMs0,m.maxButtonHoldMs0,0,9999);
	takeInt("max_button_hold_ms1",m.hasMaxButtonHoldMs1,m.maxButtonHoldMs1,0,9999);
	if(true==machine.contains(QStringLiteral("model_group")))
	{
		m.hasModelGroup=true;
		m.modelGroup=machine.value(QStringLiteral("model_group")).toString().trimmed().toStdString();
		const int idx=TownsQtModelGroupIndexForId(QString::fromStdString(m.modelGroup));
		m.hasCpu=true;
		m.cpu=TownsQtCpuKindId(TownsQtCpuKindFromTownsType(TownsQtModelGroupTownsType(idx)));
	}
	else if(true==machine.contains(QStringLiteral("cpu")))
	{
		m.hasCpu=true;
		m.cpu=machine.value(QStringLiteral("cpu")).toString().trimmed().toStdString();
	}
	else if(true==machine.contains(QStringLiteral("model_group_index")))
	{
		// Legacy fp_*.ini: convert model group index → id + CPU.
		const int model_index=std::clamp(
		    machine.value(QStringLiteral("model_group_index")).toInt(),
		    0,
		    TownsQtModelGroupCount()-1);
		m.hasModelGroup=true;
		m.modelGroup=TownsQtModelGroupId(model_index).toStdString();
		m.hasModelGroupIndex=true;
		m.modelGroupIndex=model_index;
		const TownsQtCpuKind kind=TownsQtCpuKindFromTownsType(
		    TownsQtModelGroupTownsType(model_index));
		m.hasCpu=true;
		m.cpu=TownsQtCpuKindId(kind);
	}
	takeBool("cpu_high_fidelity",m.hasCpuHighFidelity,m.cpuHighFidelity);
	takeBool("pretend_386dx",m.hasPretend386DX,m.pretend386DX);
	takeBool("use_fpu",m.hasUseFPU,m.useFPU);
	takeBool("fast_scsi",m.hasFastScsi,m.fastScsi);
	takeBool("fast_fd",m.hasFastFd,m.fastFd);
	takeBool("midi_board",m.hasMidiBoard,m.midiBoard);
	takeBool("single_drive",m.hasSingleDrive,m.singleDrive);
	takeBool("high_res_crtc",m.hasHighResCrtc,m.highResCrtc);
	takeBool("high_res_pcm",m.hasHighResPcm,m.highResPcm);
	takeInt("cd_speed",m.hasCdSpeed,m.cdSpeed,0,64);
	takeInt("sprite_transfer",m.hasSpriteTransfer,m.spriteTransfer,0,2);
	auto takeFd=[&](const char *key,int drive){
		if(true!=machine.contains(QString::fromLatin1(key)))
		{
			return;
		}
		m.hasFdImg[drive]=true;
		m.fdImg[drive]=machine.value(QString::fromLatin1(key)).toString().trimmed().toStdString();
	};
	takeFd("fd0",0);
	takeFd("fd1",1);
	for(int hd=0; hd<MouseCoordWriteScan::MachineSettings::kHddImgCount; ++hd)
	{
		const QString key=QStringLiteral("hd%1").arg(hd);
		if(true!=machine.contains(key))
		{
			continue;
		}
		m.hasHddImg[hd]=true;
		m.hddImg[hd]=machine.value(key).toString().trimmed().toStdString();
	}
	if(true==machine.contains(QStringLiteral("boot_key")))
	{
		m.hasBootKeyComb=true;
		m.bootKeyComb=TownsStrToKeyComb(
		    machine.value(QStringLiteral("boot_key")).toString().trimmed().toStdString());
	}
	return m;
}
}

struct EmulatorController::Impl
{
	QtOutsideWorld *outside_world=nullptr;
	Outside_World::Sound *sound=nullptr;
	Outside_World::WindowInterface *window=nullptr;
	TownsThread townsThread;
	QtCommandThread cmdThread;
	std::thread vmThread;
	std::thread uiThread;
	std::mutex pending_state_save_mutex;
	std::string pending_state_save_path;
	std::atomic<bool> pending_state_save_requested{false};
	std::atomic<bool> pending_state_save_completed{false};
	std::atomic<bool> pending_state_save_success{false};
};

EmulatorController::EmulatorController(const TownsARGV &argv,
                                       SharedRgbaFramebuffer *framebuffer,
                                       QtInputQueue *inputQueue,
                                       QObject *parent)
	: QObject(parent),
	  impl_(new Impl),
	  argv_(argv),
	  framebuffer_(framebuffer),
	  inputQueue_(inputQueue)
{
}

EmulatorController::~EmulatorController()
{
	requestStop();
	if(impl_->vmThread.joinable())
	{
		impl_->vmThread.join();
	}
	if(impl_->uiThread.joinable())
	{
		impl_->uiThread.join();
	}
	delete impl_->outside_world;
	delete impl_;
}

void EmulatorController::run()
{
	running_.store(true,std::memory_order_relaxed);

	auto emitFinishedFromEmuThread=[this]{
		// moveToThread must run on the object's current thread (this emu QThread).
		// cleanupStoppedEmulator() deletes us from the UI thread after QThread::finished.
		if(QCoreApplication *app=QCoreApplication::instance())
		{
			moveToThread(app->thread());
		}
		Q_EMIT finished();
	};

	impl_->outside_world=new QtOutsideWorld(inputQueue_,framebuffer_);
	impl_->outside_world->hostLogPrefix="Tsugaru_QT: ";
	impl_->sound=impl_->outside_world->CreateSound();
	if(auto *qt_sound=dynamic_cast<QtSyncSoundConnection *>(impl_->sound))
	{
		const int cdda_percent=TownsQtSettings::cddaVolumePercent();
		const float cdda_vol=static_cast<float>(cdda_percent)/100.0f;
		qt_sound->CDDASetVolume(cdda_vol,cdda_vol);
	}
	impl_->window=impl_->outside_world->CreateWindowInterface();

	auto runWithTowns=[&](auto &towns){
		towns_=static_cast<FMTownsCommon *>(&towns);
		towns.mouseCoordWriteScan.SetProfileDirectory(
		    TownsQtPaths::profilesDir().toStdString());
		towns.var.useDiscProfiles=TownsQtSettings::useDiscProfiles();
		// A/C boot: CD mounted by Setup → profile → optional state save (path not kept in memory).
		const std::string startupStateFName=towns.var.startUpStateFName;
		towns.var.startUpStateFName.clear();
		argv_.startUpStateFName.clear();
		{
			const std::string disc=towns.cdrom.state.GetDisc().fName;
			if(true!=disc.empty())
			{
				towns.mouseCoordWriteScan.SetProfileDirectory(
				    TownsQtPaths::profilesDir().toStdString());
				towns.mouseCoordWriteScan.TryLoadForDisc(disc);
			}
		}
		applyAudioVolumes(
		    TownsQtSettings::fmChipVolume(),
		    TownsQtSettings::pcmChipVolume(),
		    TownsQtSettings::cddaVolumePercent(),
		    TownsQtSettings::pcmLpfEnabled(),
		    TownsQtSettings::pcmLpfCutoffHz(),
		    TownsQtSettings::pcmResampleHighQuality());
		if(true==argv_.debugger || true==TownsQtSettings::showCpuDebug())
		{
			towns.EnableDebugger();
		}
		else
		{
			towns.DisableDebugger();
		}

		impl_->window->Start();
		impl_->window->ClearVMClosedFlag();
		impl_->outside_world->Start();

		bool startupStateLoaded=false;
		if(""!=startupStateFName)
		{
			if(true==towns.LoadState(startupStateFName))
			{
				startupStateLoaded=true;
				if(nullptr!=framebuffer_)
				{
					framebuffer_->ClearQueue();
				}
				if(nullptr!=impl_->window)
				{
					impl_->window->ClearPendingCaptures();
				}
				last_frame_serial_=0;
				has_presented_frame_=false;
			}
			else
			{
				std::cerr << "Tsugaru_QT: Failed to load startup state "
				          << startupStateFName << std::endl;
			}
		}

		QMetaObject::invokeMethod(this,[this](){
			Q_EMIT discProfileStateChanged();
		},Qt::QueuedConnection);

		applyPeripheralSettings(
		    TownsQtSettings::gamePort(0),
		    TownsQtSettings::gamePort(1),
		    TownsQtSettings::maxButtonHoldTimeMs(0,0),
		    TownsQtSettings::maxButtonHoldTimeMs(0,1),
		    TownsQtSettings::mouseIntegrationSpeed(),
		    TownsQtSettings::considerVRAMOffsetInMouseIntegration(),
		    TownsQtSettings::autoDifferentialOnMosUnused(),
		    TownsQtSettings::mouseMinX(),
		    TownsQtSettings::mouseMinY(),
		    TownsQtSettings::mouseMaxX(),
		    TownsQtSettings::mouseMaxY());
		if(true!=startupStateLoaded)
		{
			applyCpuFastMode(TownsQtSettings::cpuFastModeEnabled());
			// BIOS may rewrite wait/CMOS after Start; re-assert host preference.
			const bool prefer_fast=TownsQtSettings::cpuFastModeEnabled();
			QTimer::singleShot(0,this,[this,prefer_fast]{
				applyCpuFastModeLive(prefer_fast);
			});
			QTimer::singleShot(250,this,[this,prefer_fast]{
				applyCpuFastModeLive(prefer_fast);
			});
		}
		setCdSpeed(TownsQtSettings::cdSpeed());
		applyDisplayOptions(
		    TownsQtSettings::damperWireLine(),
		    TownsQtSettings::scanLineEffectIn15KHz(),
		    TownsQtSettings::spriteTransferMode());
		applyCddaCacheSettings(
		    TownsQtSettings::cddaCacheDuringDataRead(),
		    TownsQtSettings::cddaCachePostReadGraceSec());
		if(nullptr!=towns_)
		{
			last_fast_mode_lamp_revision_=towns_->var.fastModeLampRevision.load(std::memory_order_acquire);
			last_fast_mode_lamp_=towns_->FASTModeLamp();
			towns_->midi.midiMonitor=TownsQtSettings::midiMonitor();
			towns_->cdrom.var.debugMonitorCommandWrite=TownsQtSettings::cdromMonitor();
		}
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);

		if(""!=argv_.cdImgFName)
		{
			cd_path_=QString::fromStdString(argv_.cdImgFName);
			TownsQtSettings::setLastCdImagePath(cd_path_);
			Q_EMIT cdPathChanged(cd_path_);
		}
		for(int drive=0; drive<2; ++drive)
		{
			if(""!=argv_.fdImgFName[drive])
			{
				fd_path_[drive]=QString::fromStdString(argv_.fdImgFName[drive]);
				TownsQtSettings::setLastFdImagePath(drive,fd_path_[drive]);
				Q_EMIT fdPathChanged(drive,fd_path_[drive]);
			}
			else
			{
				fd_path_[drive].clear();
				Q_EMIT fdPathChanged(drive,fd_path_[drive]);
			}
		}

		impl_->uiThread=std::thread(
		    &QtCommandThread::Run,&impl_->cmdThread,&impl_->townsThread,&towns,&argv_,impl_->outside_world);

		impl_->townsThread.SetOnPauseTick([this](FMTownsCommon &townsRef){
			runPendingStateSaveOnVmThread(townsRef);
		});

		impl_->vmThread=std::thread([this,&towns]{
			impl_->townsThread.VMStart(&towns,impl_->outside_world,&impl_->cmdThread);
			impl_->townsThread.VMMainLoop(&towns,impl_->outside_world,impl_->sound,impl_->window,&impl_->cmdThread);
			impl_->townsThread.VMEnd(&towns,impl_->outside_world,&impl_->cmdThread);
			QMetaObject::invokeMethod(this,"onVmFinished",Qt::QueuedConnection);
		});

		// FD images are mounted via argv_.fdImgFName before boot
		// (MainWindow::prepareArgvForNextBoot — profile override or lastFd).

		QEventLoop loop;
		QTimer wait_timer;
		wait_timer.setInterval(50);
		QObject::connect(&wait_timer,&QTimer::timeout,this,[&]{
			if(!running_.load(std::memory_order_relaxed))
			{
				loop.quit();
			}
		});
		wait_timer.start();
		loop.exec();

		if(impl_->vmThread.joinable())
		{
			impl_->vmThread.join();
		}
		if(impl_->uiThread.joinable())
		{
			impl_->uiThread.join();
		}

		towns_=nullptr;
		impl_->outside_world->DeleteWindowInterface(impl_->window);
		impl_->window=nullptr;
		impl_->outside_world->DeleteSound(impl_->sound);
		impl_->sound=nullptr;
	};

	if(i486DXCommon::HIGH_FIDELITY==argv_.CPUFidelityLevel)
	{
		auto towns=std::make_unique<FMTownsTemplate<i486DXHighFidelity>>();
		if(true!=FMTownsCommon::Setup(*towns,impl_->outside_world,impl_->window,argv_))
		{
			Q_EMIT failed(QStringLiteral("Failed to set up FM TOWNS (ROM missing or invalid)."));
			emitFinishedFromEmuThread();
			return;
		}
		runWithTowns(*towns);
	}
	else
	{
		auto towns=std::make_unique<FMTownsTemplate<i486DXDefaultFidelity>>();
		if(true!=FMTownsCommon::Setup(*towns,impl_->outside_world,impl_->window,argv_))
		{
			Q_EMIT failed(QStringLiteral("Failed to set up FM TOWNS (ROM missing or invalid)."));
			emitFinishedFromEmuThread();
			return;
		}
		runWithTowns(*towns);
	}

	emitFinishedFromEmuThread();
}

void EmulatorController::requestStop()
{
	impl_->townsThread.SetRunMode(TownsThread::RUNMODE_EXIT);
	if(nullptr!=impl_->outside_world)
	{
		impl_->cmdThread.EnqueueCommand(*impl_->outside_world,"QUIT");
	}
	running_.store(false,std::memory_order_relaxed);
}

void EmulatorController::resetMachine()
{
	const bool preserve_fast=
	    (nullptr!=towns_) ? towns_->FASTModeLamp() : TownsQtSettings::cpuFastModeEnabled();
	TownsQtSettings::setCpuFastModeEnabled(preserve_fast);
	if(nullptr!=towns_)
	{
		// BIOS reads CMOS FAST_MODE after Reset; keep it aligned with the host setting.
		towns_->physMem.state.CMOSRAM[TOWNS_CMOSRAM_FASTMODE_FLAG]=preserve_fast ? 1 : 0;
	}
	if(nullptr!=impl_->outside_world)
	{
		if(true==impl_->outside_world->snapMouseIntegration)
		{
			impl_->outside_world->snapMouseWarmupFrames=TownsQtSettings::snapMouseWarmupFrames();
			impl_->outside_world->ResetSnapMouseWarmup();
		}
		impl_->cmdThread.EnqueueCommand(*impl_->outside_world,"RESET");
	}
	// Re-apply after Reset/BIOS may rewrite wait control from CMOS.
	QTimer::singleShot(0,this,[this,preserve_fast]{
		applyCpuFastMode(preserve_fast);
	});
	QTimer::singleShot(250,this,[this,preserve_fast]{
		applyCpuFastMode(preserve_fast);
	});
}

namespace
{
bool WriteStateSaveFileForTowns(FMTownsCommon &towns,const std::string &stdPath)
{
	const std::string tmpPath=stdPath+".tmp";
	if(true!=towns.SaveState(tmpPath))
	{
		(void)QFile::remove(QString::fromStdString(tmpPath));
		return false;
	}
	QFile::remove(QString::fromStdString(stdPath));
	if(true!=QFile::rename(QString::fromStdString(tmpPath),QString::fromStdString(stdPath)))
	{
		std::cerr << "Tsugaru_QT: Failed to replace state save " << stdPath << std::endl;
		(void)QFile::remove(QString::fromStdString(tmpPath));
		return false;
	}
	return true;
}
}

void EmulatorController::loadCdImage(const QString &path)
{
	loadCdImageInternal(path);
}

void EmulatorController::loadCdImageInternal(const QString &path)
{
	if(path.isEmpty() || nullptr==impl_->outside_world)
	{
		return;
	}

	const QString canonical=QFileInfo(path).canonicalFilePath();
	const QString usePath=canonical.isEmpty() ? path : canonical;
	cd_path_=usePath;
	TownsQtSettings::addRecentCdImagePath(cd_path_);
	Q_EMIT cdPathChanged(cd_path_);

	std::string cmd="CDLOAD \"";
	cmd+=usePath.toUtf8().constData();
	cmd+='\"';
	impl_->cmdThread.EnqueueCommand(*impl_->outside_world,cmd);
	// Soft CDLOAD path (rare); profile apply is live peripherals only — no restart.
	QMetaObject::invokeMethod(this,[this](){
		QTimer::singleShot(200,this,[this](){
			Q_EMIT discProfileStateChanged();
		});
	},Qt::QueuedConnection);
}

void EmulatorController::ejectCd()
{
	if(nullptr==impl_->outside_world || nullptr==towns_)
	{
		return;
	}
	const bool had_disc=!towns_->cdrom.state.GetDisc().fName.empty();
	// B: profile state save → CD unmount.
	if(true==had_disc)
	{
		(void)saveDiscStateSaveIfProfiled(false);
		towns_->cdrom.StopCDDA();
		towns_->cdrom.Eject();
		towns_->mouseCoordWriteScan.ClearActiveProfile();
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
	}
	else
	{
		impl_->cmdThread.EnqueueCommand(*impl_->outside_world,"CDEJECT");
	}
	cd_path_.clear();
	TownsQtSettings::clearLastCdImagePath();
	Q_EMIT cdPathChanged(cd_path_);
	QMetaObject::invokeMethod(this,[this](){
		QTimer::singleShot(100,this,[this](){
			Q_EMIT discProfileStateChanged();
		});
	},Qt::QueuedConnection);
}

bool EmulatorController::swapCdImage(const QString &path)
{
	if(path.isEmpty() || nullptr==impl_->outside_world || nullptr==towns_)
	{
		return false;
	}
	const QString canonical=QFileInfo(path).canonicalFilePath();
	const QString usePath=canonical.isEmpty() ? path : canonical;
	if(true!=QFile::exists(usePath))
	{
		return false;
	}

	const bool had_disc=!towns_->cdrom.state.GetDisc().fName.empty();
	// B (old disc) then A mount (new disc; state save apply only on next VM boot).
	if(true==had_disc)
	{
		(void)saveDiscStateSaveIfProfiled(false);
		towns_->cdrom.StopCDDA();
		towns_->cdrom.Eject();
		towns_->mouseCoordWriteScan.ClearActiveProfile();
	}
	else
	{
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_PAUSE);
		(void)impl_->townsThread.WaitForHostPauseAcknowledged(2000);
	}

	const std::string imgPath=usePath.toStdString();
	const auto err=towns_->cdrom.LoadDiscImage(imgPath);
	if(DiscImage::ERROR_NOERROR!=err)
	{
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
		return false;
	}

	towns_->mouseCoordWriteScan.SetProfileDirectory(
	    TownsQtPaths::profilesDir().toStdString());
	towns_->mouseCoordWriteScan.ClearActiveProfile();
	towns_->mouseCoordWriteScan.TryLoadForDisc(imgPath);
	impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);

	cd_path_=usePath;
	TownsQtSettings::addRecentCdImagePath(cd_path_);
	TownsQtSettings::setLastCdImagePath(cd_path_);
	Q_EMIT cdPathChanged(cd_path_);
	QMetaObject::invokeMethod(this,[this](){
		QTimer::singleShot(100,this,[this](){
			Q_EMIT discProfileStateChanged();
		});
	},Qt::QueuedConnection);
	return true;
}

void EmulatorController::loadFdImage(int drive,const QString &path)
{
	drive=std::clamp(drive,0,1);
	if(path.isEmpty() || nullptr==impl_->outside_world)
	{
		return;
	}
	if(true!=fdDriveAvailable(drive))
	{
		return;
	}

	const QString canonical=QFileInfo(path).canonicalFilePath();
	const QString usePath=canonical.isEmpty() ? path : canonical;
	fd_path_[drive]=usePath;
	TownsQtSettings::addRecentFdImagePath(drive,usePath);
	Q_EMIT fdPathChanged(drive,fd_path_[drive]);

	std::string cmd=(0==drive) ? "FD0LOAD \"" : "FD1LOAD \"";
	cmd+=usePath.toUtf8().constData();
	cmd+='\"';
	impl_->cmdThread.EnqueueCommand(*impl_->outside_world,cmd);

	const bool write_protect=TownsQtSettings::fdWriteProtect(drive);
	if(write_protect)
	{
		// CUI names are FD0WP / FD1WP (not FD0WRITEPROTECT).
		impl_->cmdThread.EnqueueCommand(
		    *impl_->outside_world,
		    (0==drive) ? "FD0WP" : "FD1WP");
	}
	Q_EMIT fdWriteProtectChanged(drive,write_protect);
	persistFdMountsToDiscProfile();
}

void EmulatorController::ejectFd(int drive)
{
	drive=std::clamp(drive,0,1);
	if(nullptr!=impl_->outside_world)
	{
		fd_path_[drive].clear();
		TownsQtSettings::clearLastFdImagePath(drive);
		Q_EMIT fdPathChanged(drive,fd_path_[drive]);
		impl_->cmdThread.EnqueueCommand(
		    *impl_->outside_world,
		    (0==drive) ? "FD0EJECT" : "FD1EJECT");
		persistFdMountsToDiscProfile();
	}
}

void EmulatorController::persistFdMountsToDiscProfile(void)
{
	if(nullptr==towns_ || true!=TownsQtSettings::useDiscProfiles())
	{
		return;
	}
	if(true!=towns_->mouseCoordWriteScan.DiscProfileLoaded())
	{
		return;
	}
	if(true!=towns_->mouseCoordWriteScan.ApplyAndSaveFdMounts(
	       fd_path_[0].toStdString(),
	       fd_path_[1].toStdString()))
	{
		return;
	}
	Q_EMIT discProfileStateChanged();
}

bool EmulatorController::persistHddMountsToDiscProfile(const QStringList &hdd_paths)
{
	if(nullptr==towns_ || true!=TownsQtSettings::useDiscProfiles())
	{
		return false;
	}
	if(true!=towns_->mouseCoordWriteScan.DiscProfileLoaded())
	{
		return false;
	}
	std::string paths[MouseCoordWriteScan::MachineSettings::kHddImgCount];
	const int n=std::min(
	    static_cast<int>(hdd_paths.size()),
	    MouseCoordWriteScan::MachineSettings::kHddImgCount);
	for(int hd=0; hd<n; ++hd)
	{
		paths[hd]=hdd_paths.at(hd).trimmed().toStdString();
	}
	if(true!=towns_->mouseCoordWriteScan.ApplyAndSaveHddMounts(paths,n))
	{
		return false;
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

void EmulatorController::setFdWriteProtect(int drive,bool write_protect)
{
	drive=std::clamp(drive,0,1);
	TownsQtSettings::setFdWriteProtect(drive,write_protect);
	if(nullptr!=towns_)
	{
		towns_->fdc.SetWriteProtect(drive,write_protect);
	}
	else if(nullptr!=impl_->outside_world)
	{
		const char *cmd=nullptr;
		if(0==drive)
		{
			cmd=write_protect ? "FD0WP" : "FD0UP";
		}
		else
		{
			cmd=write_protect ? "FD1WP" : "FD1UP";
		}
		impl_->cmdThread.EnqueueCommand(*impl_->outside_world,cmd);
	}
	Q_EMIT fdWriteProtectChanged(drive,write_protect);
}

bool EmulatorController::fdWriteProtected(int drive)
{
	drive=std::clamp(drive,0,1);
	if(nullptr!=towns_)
	{
		const auto &drv=towns_->fdc.state.drive[drive];
		if(0<=drv.imgFileNum && drv.imgFileNum<DiskDrive::NUM_DRIVES)
		{
			return towns_->fdc.imgFile[drv.imgFileNum].img.WriteProtected(drv.diskIndex);
		}
		return false;
	}
	return TownsQtSettings::fdWriteProtect(drive);
}

bool EmulatorController::QueryFdDriveAvailable(int drive,const FMTownsCommon *towns)
{
	drive=std::clamp(drive,0,1);
	if(0==drive)
	{
		return true;
	}
	if(TownsQtCpuKindIsMarty(TownsQtSettings::cpuKind()))
	{
		return false;
	}

	const unsigned int cmos_index=
	    (TOWNSIO_CMOS_SINGLE_DRIVE_MODE-TOWNSIO_CMOS_BASE)/2;
	if(cmos_index>=TOWNS_CMOS_SIZE)
	{
		return true;
	}

	if(nullptr!=towns)
	{
		// Non-zero = Towns CMOS single-drive mode (FD1 unavailable).
		return 0==towns->physMem.state.CMOSRAM[cmos_index];
	}

	// Emulator not running yet: peek the CMOS file selected for this boot.
	return true!=QueryCmosSingleDrive(nullptr);
}

bool EmulatorController::fdDriveAvailable(int drive) const
{
	return QueryFdDriveAvailable(drive,towns_);
}

void EmulatorController::applyMidiBoard(bool enabled)
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	argv_.nMidiCards=enabled ? 1 : 0;
	if(nullptr==towns_)
	{
		return;
	}
	towns_->var.configuredMidiCards=argv_.nMidiCards;
	towns_->midi.EnableCards(argv_.nMidiCards);
}

namespace
{
constexpr unsigned int CmosSingleDriveIndex(void)
{
	return TownsCmos::kSingleDriveIndex;
}

QVariantMap DriveSettingsToVariantMap(bool singleDrive,
                                      const TownsCmos::DriveAssignEntry letters[TownsCmos::kDriveLetterCount])
{
	QVariantMap m;
	m.insert(QStringLiteral("single_drive"),singleDrive);
	QVariantList list;
	list.reserve(TownsCmos::kDriveLetterCount);
	for(int i=0; i<TownsCmos::kDriveLetterCount; ++i)
	{
		QVariantMap e;
		e.insert(QStringLiteral("type"),static_cast<int>(letters[i].type));
		e.insert(QStringLiteral("unit"),static_cast<int>(letters[i].unit));
		list.push_back(e);
	}
	m.insert(QStringLiteral("letters"),list);
	return m;
}

bool VariantListToDriveAssign(const QVariantList &letters,
                              TownsCmos::DriveAssignEntry out[TownsCmos::kDriveLetterCount])
{
	for(int i=0; i<TownsCmos::kDriveLetterCount; ++i)
	{
		out[i].type=TownsCmos::kTypeUnassigned;
		out[i].unit=TownsCmos::kTypeUnassigned;
	}
	const int n=std::min(static_cast<int>(letters.size()),TownsCmos::kDriveLetterCount);
	for(int i=0; i<n; ++i)
	{
		const QVariantMap e=letters.at(i).toMap();
		int type=e.value(QStringLiteral("type"),255).toInt();
		int unit=e.value(QStringLiteral("unit"),255).toInt();
		if(type<0 || type>255 || unit<0 || unit>255)
		{
			return false;
		}
		if(255!=type &&
		   TownsCmos::kTypeFd!=type &&
		   TownsCmos::kTypeScsi!=type &&
		   TownsCmos::kTypeRom!=type)
		{
			return false;
		}
		out[i].type=static_cast<unsigned char>(type);
		out[i].unit=static_cast<unsigned char>(unit);
		if(TownsCmos::kTypeUnassigned==out[i].type)
		{
			out[i].unit=TownsCmos::kTypeUnassigned;
		}
	}
	return true;
}
}

QString EmulatorController::activeCmosFilePath() const
{
	if(true!=argv_.CMOSFName.empty())
	{
		return QString::fromStdString(argv_.CMOSFName);
	}
	return TownsQtPaths::cmosFilePath();
}

bool EmulatorController::readCmosBuffer(unsigned char *buf) const
{
	if(nullptr==buf)
	{
		return false;
	}
	if(nullptr!=towns_)
	{
		std::memcpy(buf,towns_->physMem.state.CMOSRAM,TOWNS_CMOS_SIZE);
		return true;
	}
	QFile f(activeCmosFilePath());
	if(!f.open(QIODevice::ReadOnly))
	{
		return false;
	}
	const QByteArray data=f.read(TOWNS_CMOS_SIZE);
	if(data.size()!=TOWNS_CMOS_SIZE)
	{
		return false;
	}
	std::memcpy(buf,data.constData(),TOWNS_CMOS_SIZE);
	return true;
}

bool EmulatorController::writeCmosBuffer(const unsigned char *buf)
{
	if(nullptr==buf)
	{
		return false;
	}
	if(nullptr!=towns_)
	{
		std::memcpy(towns_->physMem.state.CMOSRAM,buf,TOWNS_CMOS_SIZE);
	}
	const QString path=activeCmosFilePath();
	if(path.isEmpty())
	{
		return nullptr!=towns_;
	}
	QSaveFile out(path);
	if(!out.open(QIODevice::WriteOnly))
	{
		return false;
	}
	if(TOWNS_CMOS_SIZE!=out.write(reinterpret_cast<const char *>(buf),TOWNS_CMOS_SIZE))
	{
		return false;
	}
	return out.commit();
}

bool EmulatorController::QueryCmosSingleDrive(const FMTownsCommon *towns,const QString &cmosPath)
{
	const unsigned int cmos_index=CmosSingleDriveIndex();
	if(cmos_index>=TOWNS_CMOS_SIZE)
	{
		return false;
	}
	if(nullptr!=towns)
	{
		return 0!=towns->physMem.state.CMOSRAM[cmos_index];
	}
	const QString path=cmosPath.isEmpty() ? TownsQtPaths::cmosFilePath() : cmosPath;
	QFile cmos_file(path);
	if(!cmos_file.open(QIODevice::ReadOnly))
	{
		return false;
	}
	const QByteArray cmos=cmos_file.read(static_cast<int>(cmos_index)+1);
	if(cmos.size()<=static_cast<int>(cmos_index))
	{
		return false;
	}
	return 0!=static_cast<unsigned char>(cmos.at(static_cast<int>(cmos_index)));
}

bool EmulatorController::cmosSingleDrive() const
{
	if(nullptr!=towns_)
	{
		return QueryCmosSingleDrive(towns_);
	}
	if(true!=argv_.CMOSFName.empty())
	{
		return QueryCmosSingleDrive(nullptr,QString::fromStdString(argv_.CMOSFName));
	}
	return QueryCmosSingleDrive(nullptr);
}

QVariantMap EmulatorController::cmosDriveSettings() const
{
	unsigned char buf[TOWNS_CMOS_SIZE];
	TownsCmos::DriveAssignEntry letters[TownsCmos::kDriveLetterCount];
	if(true!=readCmosBuffer(buf))
	{
		for(int i=0; i<TownsCmos::kDriveLetterCount; ++i)
		{
			letters[i].type=TownsCmos::kTypeUnassigned;
			letters[i].unit=TownsCmos::kTypeUnassigned;
		}
		return DriveSettingsToVariantMap(false,letters);
	}
	TownsCmos::GetDriveAssign(buf,letters);
	return DriveSettingsToVariantMap(TownsCmos::GetSingleDriveMode(buf),letters);
}

bool EmulatorController::applyCmosDriveSettings(bool single_drive,const QVariantList &letters)
{
	TownsCmos::DriveAssignEntry assign[TownsCmos::kDriveLetterCount];
	if(true!=VariantListToDriveAssign(letters,assign))
	{
		return false;
	}
	unsigned char buf[TOWNS_CMOS_SIZE];
	if(true!=readCmosBuffer(buf))
	{
		// No CMOS yet: start from factory default image.
		std::memcpy(buf,FMTownsCommon::defCMOS,TOWNS_CMOS_SIZE);
	}
	TownsCmos::ApplyDriveSettings(buf,single_drive,assign);
	return writeCmosBuffer(buf);
}

bool EmulatorController::assignDriveLetterScsi(int letter_index,int scsi_unit)
{
	if(letter_index<0 || letter_index>=TownsCmos::kDriveLetterCount ||
	   scsi_unit<0 || scsi_unit>255)
	{
		return false;
	}
	unsigned char buf[TOWNS_CMOS_SIZE];
	if(true!=readCmosBuffer(buf))
	{
		std::memcpy(buf,FMTownsCommon::defCMOS,TOWNS_CMOS_SIZE);
	}
	TownsCmos::DriveAssignEntry letters[TownsCmos::kDriveLetterCount];
	TownsCmos::GetDriveAssign(buf,letters);
	letters[letter_index].type=TownsCmos::kTypeScsi;
	letters[letter_index].unit=static_cast<unsigned char>(scsi_unit);
	TownsCmos::ApplyDriveSettings(buf,TownsCmos::GetSingleDriveMode(buf),letters);
	return writeCmosBuffer(buf);
}

void EmulatorController::PersistSingleDriveToCmosFile(bool enabled)
{
	// Best-effort file patch when called without a controller instance.
	QFile f(TownsQtPaths::cmosFilePath());
	if(!f.open(QIODevice::ReadWrite))
	{
		return;
	}
	QByteArray data=f.read(TOWNS_CMOS_SIZE);
	if(data.size()!=TOWNS_CMOS_SIZE)
	{
		return;
	}
	auto *buf=reinterpret_cast<unsigned char *>(data.data());
	TownsCmos::SetSingleDriveMode(buf,enabled);
	if(!f.seek(0))
	{
		return;
	}
	(void)f.write(data);
	f.flush();
}

void EmulatorController::applySingleDrive(bool enabled)
{
	unsigned char buf[TOWNS_CMOS_SIZE];
	if(true!=readCmosBuffer(buf))
	{
		std::memcpy(buf,FMTownsCommon::defCMOS,TOWNS_CMOS_SIZE);
	}
	TownsCmos::SetSingleDriveMode(buf,enabled);
	(void)writeCmosBuffer(buf);
}

void EmulatorController::setMidiMonitor(bool enabled)
{
	if(nullptr!=towns_)
	{
		towns_->midi.midiMonitor=enabled;
	}
}

QStringList EmulatorController::takeMidiMonitorLines()
{
	QStringList lines;
	if(nullptr==towns_)
	{
		return lines;
	}
	for(const auto &line : towns_->midi.TakeMonitorLines())
	{
		lines<<QString::fromStdString(line);
	}
	return lines;
}

void EmulatorController::setCdromMonitor(bool enabled)
{
	if(nullptr!=towns_)
	{
		towns_->cdrom.var.debugMonitorCommandWrite=enabled;
	}
}

QStringList EmulatorController::takeCdromMonitorLines()
{
	QStringList lines;
	if(nullptr==towns_)
	{
		return lines;
	}
	for(const auto &line : towns_->cdrom.TakeMonitorLines())
	{
		lines<<QString::fromStdString(line);
	}
	return lines;
}

QStringList EmulatorController::takeAppMonitorLines()
{
	QStringList lines;
	if(nullptr==towns_)
	{
		return lines;
	}
	for(const auto &line : towns_->mouseCoordWriteScan.TakeMonitorLines())
	{
		lines<<QString::fromStdString(line);
	}
	return lines;
}

void EmulatorController::setCpuDebugMonitor(bool enabled)
{
	cpu_debug_ui_enabled_=enabled;
	if(nullptr==towns_)
	{
		return;
	}
	if(enabled || true==argv_.debugger)
	{
		towns_->EnableDebugger();
	}
	else
	{
		towns_->DisableDebugger();
	}
}

void EmulatorController::setVmPaused(bool paused)
{
	if(true==paused)
	{
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_PAUSE);
	}
	else if(TownsThread::RUNMODE_PAUSE==impl_->townsThread.GetRunMode())
	{
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
		if(nullptr!=towns_)
		{
			towns_->SetDebugBreakFlag(false);
		}
	}
}

bool EmulatorController::vmPaused() const
{
	return TownsThread::RUNMODE_PAUSE==impl_->townsThread.GetRunMode();
}

QString EmulatorController::dumpGuestMemory(const QString &addrSpec,unsigned int length) const
{
	QString out;
	if(nullptr==towns_ || addrSpec.trimmed().isEmpty())
	{
		return out;
	}
	length=std::clamp(length,1u,4096u);

	try
	{
		auto &cpu=towns_->CPU();
		auto farPtr=cmdutil::MakeFarPointer(addrSpec.trimmed().toStdString(),cpu);
		if(i486DXCommon::FarPointer::NO_SEG==farPtr.SEG)
		{
			// Bare hex → linear address (common for physical-looking dumps).
			farPtr.SEG=i486DXCommon::FarPointer::LINEAR_ADDR;
		}
		out+=QStringLiteral("Dump %1  length=%2\n")
		         .arg(addrSpec.trimmed())
		         .arg(length);
		for(const auto &line : miscutil::MakeMemDump(cpu,towns_->mem,farPtr,length,/*shiftJIS=*/false))
		{
			out+=QString::fromStdString(line);
			out+=QLatin1Char('\n');
		}
	}
	catch(const std::exception &e)
	{
		out=QStringLiteral("Dump failed: %1").arg(QString::fromUtf8(e.what()));
	}
	catch(...)
	{
		out=QStringLiteral("Dump failed.");
	}
	return out;
}

QString EmulatorController::cpuDebugSnapshot() const
{
	QString out;
	if(nullptr==towns_)
	{
		return out;
	}

	try
	{
		auto &cpu=towns_->CPU();
		auto &debugger=towns_->debugger;

		out+=QStringLiteral("=== Registers ===\n");
		for(const auto &line : cpu.GetStateText())
		{
			out+=QString::fromStdString(line);
			out+=QLatin1Char('\n');
		}

		out+=QStringLiteral("\n=== Current instruction ===\n");
		if(nullptr!=cpu.debuggerPtr)
		{
			try
			{
				i486DXCommon::InstructionAndOperand instOp;
				MemoryAccess::ConstMemoryWindow emptyMemWindow;
				// Racy vs VM thread: may see a torn instruction.  Catch and keep going.
				cpu.DebugFetchInstruction(emptyMemWindow,instOp,towns_->mem);
				const std::string disasm=cpu.Disassemble(
				    instOp.inst,
				    instOp.op1,
				    instOp.op2,
				    cpu.state.CS(),
				    cpu.state.EIP,
				    towns_->mem,
				    debugger.GetSymTable(),
				    debugger.GetIOTable());
				out+=QString::fromStdString(disasm);
				out+=QLatin1Char('\n');
			}
			catch(const std::exception &e)
			{
				out+=QStringLiteral("(disassembly unavailable: %1)\n").arg(QString::fromUtf8(e.what()));
			}
			catch(...)
			{
				out+=QStringLiteral("(disassembly unavailable: torn CPU state)\n");
			}
		}
		else
		{
			out+=QStringLiteral("(debugger not attached — open this window to enable)\n");
		}

		out+=QStringLiteral("\n=== CS:EIP history (newest first) ===\n");
		constexpr unsigned int kHistorySteps=64;
		const auto hist=debugger.GetCSEIPLog(kHistorySteps);
		const auto &symTable=debugger.GetSymTable();
		for(auto iter=hist.rbegin(); iter!=hist.rend(); ++iter)
		{
			if(0==iter->SEG && 0==iter->OFFSET && 0==iter->count)
			{
				continue;
			}
			out+=QString::fromStdString(
			    cpputil::Ustox(iter->SEG)+":"+cpputil::Uitox(iter->OFFSET)+
			    "  SS="+cpputil::Ustox(iter->SS)+
			    "  ESP="+cpputil::Uitox(iter->ESP));
			if(1<iter->count)
			{
				out+=QStringLiteral(" (%1)").arg(static_cast<qulonglong>(iter->count));
			}
			if(const auto *sym=symTable.Find(iter->SEG,iter->OFFSET))
			{
				out+=QLatin1Char(' ');
				out+=QString::fromStdString(sym->Format());
			}
			out+=QLatin1Char('\n');
		}

		out+=QStringLiteral("\n=== Call stack ===\n");
		const auto stack=debugger.GetCallStackText(cpu);
		constexpr size_t kMaxStackLines=48;
		const size_t start=(stack.size()>kMaxStackLines) ? (stack.size()-kMaxStackLines) : 0;
		if(stack.empty())
		{
			out+=QStringLiteral("(empty)\n");
		}
		else
		{
			if(0<start)
			{
				out+=QStringLiteral("... (%1 older frames omitted)\n").arg(static_cast<qulonglong>(start));
			}
			for(size_t i=start; i<stack.size(); ++i)
			{
				out+=QString::fromStdString(stack[i]);
				out+=QLatin1Char('\n');
			}
		}
	}
	catch(const std::exception &e)
	{
		out+=QStringLiteral("\n(cpu debug snapshot failed: %1)\n").arg(QString::fromUtf8(e.what()));
	}
	catch(...)
	{
		out+=QStringLiteral("\n(cpu debug snapshot failed)\n");
	}

	return out;
}

void EmulatorController::restartAudioOutput()
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	if(nullptr==impl_->sound)
	{
		return;
	}
	if(auto *qt_sound=dynamic_cast<QtSyncSoundConnection *>(impl_->sound))
	{
		qt_sound->RequestRestartOutput();
	}
}

void EmulatorController::setCpuFrequencyMhz(int mhz)
{
	TownsQtSettings::setCpuFrequencyMhz(mhz);
	applyCpuFrequencyMhzLive(TownsQtSettings::cpuFrequencyMhz());
}

void EmulatorController::applyCpuFrequencyMhzLive(int mhz)
{
	mhz=std::clamp(mhz,1,100);
	if(nullptr!=towns_)
	{
		towns_->state.fastModeFreq=mhz;
		// 16MHz FAST ↔ higher FAST switches VRAM 3WS ↔ 0WS.
		if(true==towns_->FASTModeLamp())
		{
			towns_->SetFastModeMemoryWait();
		}
		towns_->AdjustMachineSpeedForMemoryWait();
	}
}

void EmulatorController::applyCpuFastMode(bool enabled)
{
	TownsQtSettings::setCpuFastModeEnabled(enabled);
	applyCpuFastModeLive(enabled);
}

void EmulatorController::applyCpuFastModeLive(bool enabled)
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	if(nullptr==towns_)
	{
		return;
	}
	towns_->physMem.state.CMOSRAM[TOWNS_CMOSRAM_FASTMODE_FLAG]=enabled ? 1 : 0;
	if(true==enabled)
	{
		towns_->SetFastModeMemoryWait();
	}
	else
	{
		towns_->SetCompatibleMemoryWait();
	}
	towns_->AdjustMachineSpeedForMemoryWait();
}

void EmulatorController::setCdSpeed(int speed)
{
	speed=std::max(0,speed);
	TownsQtSettings::setCdSpeed(speed);
	applyCdSpeedLive(speed);
}

void EmulatorController::applyCdSpeedLive(int speed)
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	speed=std::max(0,speed);
	if(nullptr==towns_)
	{
		return;
	}
	if(0==speed)
	{
		towns_->cdrom.state.readSectorTime=TownsCDROM::DEFAULT_READ_SECTOR_TIME;
		towns_->cdrom.state.maxSeekTime=TownsCDROM::DEFAULT_SEEK_TIME;
	}
	else
	{
		towns_->cdrom.state.readSectorTime=TOWNS_CD_READ_SECTOR_TIME_1X/static_cast<unsigned int>(speed);
		towns_->cdrom.state.maxSeekTime=TOWNS_CD_SEEK_TIME_1X/static_cast<unsigned int>(speed);
	}
}

void EmulatorController::applyAudioVolumes(int fm_chip_volume,int pcm_chip_volume,int cdda_volume_percent,bool pcm_lpf_enabled,int pcm_lpf_cutoff_hz,bool pcm_resample_hq)
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	fm_chip_volume=std::clamp(fm_chip_volume,0,8192);
	pcm_chip_volume=std::clamp(pcm_chip_volume,0,8192);
	cdda_volume_percent=std::clamp(cdda_volume_percent,0,100);
	pcm_lpf_cutoff_hz=std::clamp(pcm_lpf_cutoff_hz,200,20000);
	if(nullptr!=towns_)
	{
		towns_->sound.state.ym2612.state.volume=fm_chip_volume;
		towns_->sound.state.rf5c68.state.volume=pcm_chip_volume;
		towns_->sound.state.rf5c68.SetHostLpf(pcm_lpf_enabled,pcm_lpf_cutoff_hz);
		towns_->sound.state.rf5c68.SetResampleHighQuality(pcm_resample_hq);
		towns_->sound.SetCDDAUserGain(static_cast<float>(cdda_volume_percent)/100.0f);
	}
	if(nullptr!=impl_->sound)
	{
		if(auto *qt_sound=dynamic_cast<QtSyncSoundConnection *>(impl_->sound))
		{
			// Host CDDA slider; emulated CDDA is mixed in ProcessSound via SetCDDAUserGain
			// and guest electric-volume.  CDDASetVolume only affects the unused legacy
			// QtSyncSoundConnection::CDDAPlay / FillAudio path.
			const float cdda_vol=static_cast<float>(cdda_volume_percent)/100.0f;
			qt_sound->CDDASetVolume(cdda_vol,cdda_vol);
		}
	}
}

void EmulatorController::applyPeripheralSettings(unsigned int game_port0,
                                                 unsigned int game_port1,
                                                 int max_button_hold_ms0,
                                                 int max_button_hold_ms1,
                                                 int mouse_integration_speed,
                                                 bool consider_vram_offset_in_mouse_integration,
                                                 bool auto_differential_on_mos_unused,
                                                 int mouse_min_x,
                                                 int mouse_min_y,
                                                 int mouse_max_x,
                                                 int mouse_max_y)
{
	if(nullptr==towns_ || nullptr==impl_->outside_world)
	{
		return;
	}

		impl_->outside_world->SetDifferentialMouseIntegrationPreference(
		    TownsQtSettings::differentialMouseIntegration(),towns_);
		impl_->outside_world->autoDifferentialOnMosUnused=auto_differential_on_mos_unused;
		impl_->outside_world->snapMouseIntegration=TownsQtSettings::snapMouseIntegration();
		impl_->outside_world->snapMouseWarmupFrames=TownsQtSettings::snapMouseWarmupFrames();
		if(true==impl_->outside_world->snapMouseIntegration)
		{
			impl_->outside_world->ResetSnapMouseWarmup();
		}
		else
		{
			impl_->outside_world->snapMouseWarmupRemaining=0;
		}
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);

	towns_->state.mouseIntegrationSpeed=static_cast<unsigned int>(std::clamp(mouse_integration_speed,32,256));
	towns_->var.considerVRAMOffsetInMouseIntegration=consider_vram_offset_in_mouse_integration;
	towns_->var.mouseMinX=mouse_min_x;
	towns_->var.mouseMinY=mouse_min_y;
	towns_->var.mouseMaxX=mouse_max_x;
	towns_->var.mouseMaxY=mouse_max_y;

	const unsigned int game_ports[2]={game_port0,game_port1};
	const long long int hold_ns[2]={
	    static_cast<long long int>(std::max(0,max_button_hold_ms0))*1000000LL,
	    static_cast<long long int>(std::max(0,max_button_hold_ms1))*1000000LL,
	};
	for(int port=0; port<2; ++port)
	{
		impl_->outside_world->gamePort[port]=game_ports[port];
		towns_->gameport.state.ports[port].device=
		    TownsGamePort::EmulationTypeToDeviceType(game_ports[port]);
		towns_->gameport.state.ports[port].maxButtonHoldTime[0]=hold_ns[0];
		towns_->gameport.state.ports[port].maxButtonHoldTime[1]=hold_ns[1];
	}
	impl_->outside_world->CacheGamePadIndicesThatNeedUpdates();
}

bool EmulatorController::differentialMouseIntegration() const
{
	if(nullptr!=impl_->outside_world)
	{
		// Runtime path (may auto-force after Mouse BIOS stop); not the settings preference alone.
		return impl_->outside_world->effectiveDifferentialMouseIntegration;
	}
	return TownsQtSettings::differentialMouseIntegration();
}

QVariantMap EmulatorController::mouseUiState() const
{
	QVariantMap result;
	result[QStringLiteral("diff")]=false;
	result[QStringLiteral("mos")]=false;
	result[QStringLiteral("soft_ok")]=false;
	result[QStringLiteral("capture_released")]=false;
	result[QStringLiteral("feeding")]=true;
	result[QStringLiteral("failsafe")]=false;
	result[QStringLiteral("pref_diff")]=TownsQtSettings::differentialMouseIntegration();
	if(nullptr==impl_->outside_world)
	{
		return result;
	}
	const auto &ow=*impl_->outside_world;
	result[QStringLiteral("diff")]=ow.effectiveDifferentialMouseIntegration;
	result[QStringLiteral("mos")]=ow.debugMouseBIOSActive;
	result[QStringLiteral("soft_ok")]=false;
	result[QStringLiteral("capture_released")]=ow.mouseCaptureReleased_;
	result[QStringLiteral("feeding")]=ow.mouseFeedingEnabled_;
	result[QStringLiteral("failsafe")]=ow.mouseFailsafeShowHostCursor_;
	result[QStringLiteral("pref_diff")]=ow.differentialMouseIntegration;
	result[QStringLiteral("profile_loaded")]=false;
	result[QStringLiteral("profile_apply")]=false;
	result[QStringLiteral("disc_profile_loaded")]=false;
	// Effective (running) mode — not merely what the profile file stores.
	result[QStringLiteral("integration_mode")]=MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL;
	if(nullptr!=towns_)
	{
		result[QStringLiteral("profile_loaded")]=towns_->mouseCoordWriteScan.ProfileLoaded();
		result[QStringLiteral("profile_apply")]=towns_->var.mouseCoordProfileApply;
		result[QStringLiteral("disc_profile_loaded")]=
		    towns_->mouseCoordWriteScan.DiscProfileLoaded();
		// Capture-released keeps effectiveDifferential=false while still on the
		// differential path (feeding paused). Prefer that over MOS absolute labels.
		if(true==ow.effectiveDifferentialMouseIntegration ||
		   true==ow.mouseCaptureReleased_)
		{
			result[QStringLiteral("integration_mode")]=
			    MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL;
		}
		else if(true==towns_->var.mouseCoordProfileApply)
		{
			// Disc-profile poke / gameport-feedback path is actually applying.
			result[QStringLiteral("integration_mode")]=
			    towns_->mouseCoordWriteScan.GetActiveProfile().integrationMode;
		}
		else if(true==towns_->state.mouseBIOSActive)
		{
			// System MOS absolute / snap (TMENU and other soft-cursor UI).
			result[QStringLiteral("integration_mode")]=
			    MouseCoordWriteScan::INTEGRATION_MOS;
		}
		else
		{
			// MOS never up / already stopped — treat as capture path for the UI.
			result[QStringLiteral("integration_mode")]=
			    MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL;
		}
		unsigned int px=0,py=0;
		int mx=0,my=0;
		result[QStringLiteral("soft_ok")]=
		    towns_->mouseCoordWriteScan.GetSoftCursorSnapshot(px,py,mx,my);
		std::string appName;
		unsigned int appHash=0;
		towns_->mouseCoordWriteScan.GetActiveAppExec(appName,appHash);
		result[QStringLiteral("app_exec_name")]=QString::fromStdString(appName);
		result[QStringLiteral("app_exec_hash")]=static_cast<uint>(appHash);
		result[QStringLiteral("app_exec_matched")]=
		    towns_->mouseCoordWriteScan.AppExecMatched();
		if(true==towns_->mouseCoordWriteScan.ProfileLoaded())
		{
			const auto p=towns_->mouseCoordWriteScan.GetActiveProfile();
			result[QStringLiteral("app_exec_bound")]=p.HasAppExecBind();
			result[QStringLiteral("app_exec_bind_name")]=
			    QString::fromStdString(p.appExecName);
			result[QStringLiteral("app_exec_bind_hash")]=
			    static_cast<uint>(p.appExecHash32);
		}
		else
		{
			result[QStringLiteral("app_exec_bound")]=false;
			result[QStringLiteral("app_exec_bind_name")]=QString();
			result[QStringLiteral("app_exec_bind_hash")]=0u;
		}
	}
	return result;
}

void EmulatorController::resumeMouseCapture()
{
	if(nullptr==impl_->outside_world || nullptr==towns_)
	{
		return;
	}
	impl_->outside_world->ResumeMouseCapture(*towns_);
}

void EmulatorController::releaseMouseCapture()
{
	if(nullptr==impl_->outside_world || nullptr==towns_)
	{
		return;
	}
	impl_->outside_world->ReleaseMouseCapture(*towns_);
}

void EmulatorController::setMouseFailsafeShowHostCursor(bool show)
{
	if(nullptr==impl_->outside_world)
	{
		return;
	}
	impl_->outside_world->SetMouseFailsafeShowHostCursor(show);
}

void EmulatorController::setSnapMouseIntegration(bool enabled)
{
	applySnapMouseSettings(enabled,TownsQtSettings::snapMouseWarmupFrames());
}

void EmulatorController::applySnapMouseSettings(bool enabled,int warmup_frames)
{
	warmup_frames=std::clamp(warmup_frames,0,600);
	TownsQtSettings::setSnapMouseIntegration(enabled);
	TownsQtSettings::setSnapMouseWarmupFrames(warmup_frames);
	if(nullptr!=impl_->outside_world)
	{
		impl_->outside_world->snapMouseIntegration=enabled;
		impl_->outside_world->snapMouseWarmupFrames=warmup_frames;
		if(enabled)
		{
			impl_->outside_world->ResetSnapMouseWarmup();
		}
		else
		{
			impl_->outside_world->snapMouseWarmupRemaining=0;
		}
	}
}

bool EmulatorController::snapMouseIntegration() const
{
	if(nullptr!=impl_->outside_world)
	{
		return impl_->outside_world->snapMouseIntegration;
	}
	return TownsQtSettings::snapMouseIntegration();
}

void EmulatorController::applyCddaCacheSettings(bool enabled,int post_read_grace_sec)
{
	post_read_grace_sec=std::clamp(post_read_grace_sec,1,60);
	TownsQtSettings::setCddaCacheDuringDataRead(enabled);
	TownsQtSettings::setCddaCachePostReadGraceSec(post_read_grace_sec);
	if(nullptr==towns_)
	{
		return;
	}
	towns_->cdrom.var.cddaCacheDuringDataRead=enabled;
	towns_->cdrom.var.cddaCachePostReadGraceSec=static_cast<unsigned int>(post_read_grace_sec);
	if(true!=enabled)
	{
		towns_->cdrom.state.CDDAAudioOutput=false;
	}
}

bool EmulatorController::saveStateToFile(const QString &path,bool resume_run_after)
{
	if(nullptr==towns_ || path.isEmpty())
	{
		return false;
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		return false;
	}

	const int prior_mode=impl_->townsThread.GetRunMode();
	impl_->townsThread.ClearHostPauseAcknowledged();
	impl_->townsThread.SetRunMode(TownsThread::RUNMODE_PAUSE);
	if(true!=impl_->townsThread.WaitForHostPauseAcknowledged(5000))
	{
		std::cerr << "Tsugaru_QT: VM pause timeout before state save" << std::endl;
		if(TownsThread::RUNMODE_RUN==prior_mode)
		{
			impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
		}
		return false;
	}

	// Same thread model as LoadState: VM is paused; write from this side.
	// Do not mutate CRTC before save (mode-specific regs/sifter must be preserved).
	const bool ok=WriteStateSaveFileForTowns(*towns_,path.toStdString());
	if(true!=ok)
	{
		std::cerr << "Tsugaru_QT: Failed to save state " << path.toStdString() << std::endl;
	}

	if(true==resume_run_after && TownsThread::RUNMODE_RUN==prior_mode)
	{
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
	}
	return ok;
}

void EmulatorController::runPendingStateSaveOnVmThread(FMTownsCommon &towns)
{
	if(true!=impl_->pending_state_save_requested.load(std::memory_order_acquire))
	{
		return;
	}
	std::string path;
	{
		std::lock_guard<std::mutex> lock(impl_->pending_state_save_mutex);
		if(true!=impl_->pending_state_save_requested)
		{
			return;
		}
		path=impl_->pending_state_save_path;
	}
	const bool ok=WriteStateSaveFileForTowns(towns,path);
	impl_->pending_state_save_success.store(ok,std::memory_order_release);
	impl_->pending_state_save_requested.store(false,std::memory_order_release);
	impl_->pending_state_save_completed.store(true,std::memory_order_release);
}

bool EmulatorController::loadStateFromFile(const QString &path)
{
	if(nullptr==towns_ || path.isEmpty() || true!=QFile::exists(path))
	{
		return false;
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		return false;
	}
	const int prior_mode=impl_->townsThread.GetRunMode();
	impl_->townsThread.SetRunMode(TownsThread::RUNMODE_PAUSE);
	if(true!=impl_->townsThread.WaitForHostPauseAcknowledged(5000))
	{
		std::cerr << "Tsugaru_QT: VM pause timeout before state load" << std::endl;
		if(TownsThread::RUNMODE_RUN==prior_mode)
		{
			impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
		}
		return false;
	}
	const bool ok=towns_->LoadState(path.toStdString());
	if(true!=ok)
	{
		std::cerr << "Tsugaru_QT: Failed to load state " << path.toStdString() << std::endl;
	}
	else
	{
		// Discard frames stamped with the pre-load townsTime so PresentOneDueFrame
		// is not stuck behind a larger vsync_index after the clock jumps.
		if(nullptr!=framebuffer_)
		{
			framebuffer_->ClearQueue();
		}
		if(nullptr!=impl_->window)
		{
			impl_->window->ClearPendingCaptures();
		}
		last_frame_serial_=0;
		has_presented_frame_=false;
	}
	if(TownsThread::RUNMODE_RUN==prior_mode)
	{
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);
	}
	if(true==ok)
	{
		QMetaObject::invokeMethod(this,[this](){
			Q_EMIT discProfileStateChanged();
			Q_EMIT frameReady();
		},Qt::QueuedConnection);
	}
	return ok;
}

bool EmulatorController::loadStateSlot(int slot)
{
	if(nullptr==towns_ || slot<0 || 9<slot)
	{
		return false;
	}
	QString path;
	if(0==slot)
	{
		const std::string disc=towns_->cdrom.state.GetDisc().fName;
		if(true==disc.empty())
		{
			return false;
		}
		path=TownsQtDiscStateSave::ResumeStatePathForDisc(QString::fromStdString(disc));
	}
	else
	{
		path=TownsQtDiscStateSave::ManualStateSlotPath(slot);
	}
	return loadStateFromFile(path);
}

bool EmulatorController::saveStateSlot(int slot)
{
	if(slot<1 || 9<slot)
	{
		return false;
	}
	const QString path=TownsQtDiscStateSave::ManualStateSlotPath(slot);
	if(path.isEmpty())
	{
		return false;
	}
	return saveStateToFile(path,true);
}

bool EmulatorController::saveDiscStateSaveIfProfiled(bool resume_run_after)
{
	if(true!=TownsQtSettings::autoResumeEnabled())
	{
		return false;
	}
	if(nullptr==towns_ || true!=towns_->var.useDiscProfiles)
	{
		return false;
	}
	const std::string discPath=towns_->cdrom.state.GetDisc().fName;
	if(true==discPath.empty())
	{
		return false;
	}
	unsigned int fingerprint=0;
	if(true==towns_->mouseCoordWriteScan.DiscProfileLoaded())
	{
		const auto profile=towns_->mouseCoordWriteScan.GetActiveProfile();
		if(true==profile.HasFingerprint())
		{
			fingerprint=profile.discFingerprintHash32;
		}
	}
	if(0==fingerprint)
	{
		const auto &disc=towns_->cdrom.state.GetDisc();
		DiscIdentity discId=disc.ComputeIdentity(false);
		if(true!=discId.hasFingerprint)
		{
			discId=disc.ComputeIdentity(true);
		}
		if(true!=discId.hasFingerprint)
		{
			return false;
		}
		fingerprint=discId.fingerprintHash32;
	}
	if(true!=TownsQtDiscStateSave::ProfileExistsForFingerprint(fingerprint))
	{
		return false;
	}
	if(true!=TownsQtPaths::ensureLayout())
	{
		return false;
	}
	const QString statePath=TownsQtDiscStateSave::PathForFingerprint(fingerprint);
	if(statePath.isEmpty())
	{
		return false;
	}
	return saveStateToFile(statePath,resume_run_after);
}

void EmulatorController::applyDisplayOptions(bool damperWireLine,bool scanLineEffectIn15KHz,int spriteTransferMode)
{
	spriteTransferMode=std::clamp(spriteTransferMode,0,2);
	TownsQtSettings::setDamperWireLine(damperWireLine);
	TownsQtSettings::setScanLineEffectIn15KHz(scanLineEffectIn15KHz);
	TownsQtSettings::setSpriteTransferMode(spriteTransferMode);
	if(nullptr!=towns_)
	{
		towns_->var.damperWireLine=damperWireLine;
		towns_->var.scanLineEffectIn15KHz=scanLineEffectIn15KHz;
	}
	applySpriteTransferModeLive(spriteTransferMode);
}

void EmulatorController::applySpriteTransferModeLive(int spriteTransferMode)
{
	spriteTransferMode=std::clamp(spriteTransferMode,0,2);
	if(nullptr!=towns_)
	{
		towns_->var.spriteTransferMode=static_cast<unsigned int>(spriteTransferMode);
		towns_->ApplySpriteTransferTime();
	}
}

void EmulatorController::presentDueFrames()
{
	if(nullptr==framebuffer_ || nullptr==towns_)
	{
		return;
	}

	const uint64_t due_index=towns_->GetObserverTownsTimeNs()/TOWNS_RENDERING_FREQUENCY;
	uint64_t presented_vsync_index=0;
	if(true!=framebuffer_->PresentOneDueFrame(due_index,&presented_vsync_index))
	{
		return;
	}

	const uint64_t serial=framebuffer_->Serial();
	if(serial==last_frame_serial_)
	{
		return;
	}

	last_frame_serial_=serial;
	last_presented_vsync_index_=presented_vsync_index;
	has_presented_frame_=true;
	++stats_frame_count_;
	if(nullptr!=impl_->window)
	{
		impl_->window->winThr.newImageRendered=false;
	}
	Q_EMIT frameReady();
}

void EmulatorController::pollWindow()
{
	// Modal UI loops use QueuedConnection; coalesce if the timer outruns us.
	if(true==poll_window_busy_.exchange(true,std::memory_order_acq_rel))
	{
		return;
	}
	struct ClearBusy
	{
		std::atomic<bool> &busy;
		~ClearBusy(){busy.store(false,std::memory_order_release);}
	} clear{poll_window_busy_};

	if(nullptr==impl_->window)
	{
		return;
	}
	impl_->window->Interval();

	if(nullptr!=framebuffer_ && nullptr!=towns_)
	{
		presentDueFrames();
		if(1<framebuffer_->QueueDepth())
		{
			presentDueFrames();
		}
		updateStats();

		const uint32_t revision=towns_->var.fastModeLampRevision.load(std::memory_order_acquire);
		if(revision!=last_fast_mode_lamp_revision_)
		{
			last_fast_mode_lamp_revision_=revision;
			const bool fast_mode=towns_->FASTModeLamp();
			if(fast_mode!=last_fast_mode_lamp_)
			{
				last_fast_mode_lamp_=fast_mode;
				Q_EMIT fastModeLampChanged(fast_mode);
			}
		}
	}

	if(true==impl_->window->CheckVMClosed())
	{
		requestStop();
	}
}

bool EmulatorController::fastModeLamp() const
{
	if(nullptr==towns_)
	{
		return TownsQtSettings::cpuFastModeEnabled();
	}
	return towns_->FASTModeLamp();
}

Outside_World::StatusBarInfo EmulatorController::driveAccessStatus() const
{
	Outside_World::StatusBarInfo info;
	if(nullptr==impl_->window)
	{
		return info;
	}
	auto *window_conn=dynamic_cast<FsSimpleWindowConnection::WindowConnection *>(impl_->window);
	if(nullptr==window_conn)
	{
		return info;
	}
	std::lock_guard<std::mutex> lock(window_conn->renderingLock);
	info=window_conn->sharedEx.statusBarInfo;
	return info;
}

QVariantMap EmulatorController::guestMouseCoords() const
{
	QVariantMap result;
	if(nullptr==impl_->outside_world)
	{
		return result;
	}
	const auto &ow=*impl_->outside_world;
	result[QStringLiteral("valid")]=ow.debugGuestValid;
	result[QStringLiteral("x")]=ow.debugGuestMx;
	result[QStringLiteral("y")]=ow.debugGuestMy;
	result[QStringLiteral("mos_x")]=ow.debugMosMx;
	result[QStringLiteral("mos_y")]=ow.debugMosMy;
	result[QStringLiteral("tbios_x")]=ow.debugTbiosMx;
	result[QStringLiteral("tbios_y")]=ow.debugTbiosMy;
	result[QStringLiteral("mouse_bios")]=ow.debugMouseBIOSActive;
	result[QStringLiteral("diff_eff")]=ow.debugEffectiveDifferential;
	result[QStringLiteral("diff_forced")]=ow.debugForcedDifferentialByMouseBIOSStop;
	result[QStringLiteral("diff_mos_unused")]=ow.debugForcedDifferentialByMosUnused;
	result[QStringLiteral("mos_probe")]=ow.debugMosUsageObserving;
	result[QStringLiteral("mos_reads")]=static_cast<uint>(ow.debugMosCoordAppReads);
	result[QStringLiteral("gp_packets")]=static_cast<uint>(ow.debugGameportMousePackets);
	result[QStringLiteral("capture_released")]=ow.debugMouseCaptureReleased;
	result[QStringLiteral("feeding")]=ow.debugMouseFeedingEnabled;
	if(nullptr!=towns_)
	{
		std::string appName;
		unsigned int appHash=0;
		towns_->mouseCoordWriteScan.GetActiveAppExec(appName,appHash);
		result[QStringLiteral("app_exec_name")]=QString::fromStdString(appName);
		result[QStringLiteral("app_exec_hash")]=static_cast<uint>(appHash);
		result[QStringLiteral("app_exec_matched")]=
		    towns_->mouseCoordWriteScan.AppExecMatched();
		result[QStringLiteral("profile_apply")]=towns_->var.mouseCoordProfileApply;
		if(true==towns_->mouseCoordWriteScan.ProfileLoaded())
		{
			result[QStringLiteral("app_exec_bound")]=
			    towns_->mouseCoordWriteScan.GetActiveProfile().HasAppExecBind();
		}
		else
		{
			result[QStringLiteral("app_exec_bound")]=false;
		}
	}
	result[QStringLiteral("tbios_version")]=static_cast<uint>(ow.debugTBIOSVersion);
	result[QStringLiteral("app_specific")]=static_cast<uint>(ow.debugAppSpecific);
	result[QStringLiteral("mos_work")]=static_cast<uint>(ow.debugMosWorkPhysAddr);
	result[QStringLiteral("tbios_off")]=static_cast<uint>(ow.debugTbiosMouseInfoOffset);
	result[QStringLiteral("snap_warmup")]=ow.debugSnapWarmupRemaining;
	result[QStringLiteral("raw_x")]=ow.debugRawHostMx;
	result[QStringLiteral("raw_y")]=ow.debugRawHostMy;
	result[QStringLiteral("ctrl_x")]=ow.debugCtrlMx;
	result[QStringLiteral("ctrl_y")]=ow.debugCtrlMy;
	result[QStringLiteral("org_x")]=ow.debugOriginX;
	result[QStringLiteral("org_y")]=ow.debugOriginY;
	result[QStringLiteral("zoom_x")]=ow.debugZoom2xX;
	result[QStringLiteral("zoom_y")]=ow.debugZoom2xY;
	result[QStringLiteral("page")]=ow.debugMousePage;
	result[QStringLiteral("hw_defined")]=ow.debugHwCursorDefined;
	result[QStringLiteral("hw_x")]=ow.debugHwCursorX;
	result[QStringLiteral("hw_y")]=ow.debugHwCursorY;
	result[QStringLiteral("hskip")]=ow.debugHSkip1X;
	result[QStringLiteral("spr_h")]=ow.debugSpriteHOffset;
	result[QStringLiteral("spr_v")]=ow.debugSpriteVOffset;
	result[QStringLiteral("spr_cx")]=ow.debugSpriteCursorX;
	result[QStringLiteral("spr_cy")]=ow.debugSpriteCursorY;
	result[QStringLiteral("spr_n")]=ow.debugSpriteCursorCount;
	result[QStringLiteral("spr_nx")]=ow.debugSpriteNearestX;
	result[QStringLiteral("spr_ny")]=ow.debugSpriteNearestY;
	result[QStringLiteral("spr_hx")]=ow.debugSpriteHalfX;
	result[QStringLiteral("spr_hy")]=ow.debugSpriteHalfY;
	result[QStringLiteral("spen")]=ow.debugSpriteSpen;
	result[QStringLiteral("voff_x")]=ow.debugVramOffsetX;
	result[QStringLiteral("voff_y")]=ow.debugVramOffsetY;
	result[QStringLiteral("voff1_x")]=ow.debugVramOffsetX1;
	result[QStringLiteral("voff1_y")]=ow.debugVramOffsetY1;
	result[QStringLiteral("fa0_0")]=ow.debugFa0_0;
	result[QStringLiteral("fa0_1")]=ow.debugFa0_1;
	result[QStringLiteral("hot_x")]=ow.debugMouseInfoHotX;
	result[QStringLiteral("hot_y")]=ow.debugMouseInfoHotY;
	result[QStringLiteral("cp_x")]=ow.debugCursorDrawX;
	result[QStringLiteral("cp_y")]=ow.debugCursorDrawY;
	result[QStringLiteral("org0_x")]=ow.debugOrg0X;
	result[QStringLiteral("org1_x")]=ow.debugOrg1X;
	result[QStringLiteral("hskip0")]=ow.debugHSkip0;
	result[QStringLiteral("hskip1")]=ow.debugHSkip1;
	result[QStringLiteral("single_page")]=ow.debugSinglePage;
	result[QStringLiteral("show0")]=ow.debugShowPage0;
	result[QStringLiteral("show1")]=ow.debugShowPage1;
	result[QStringLiteral("sysrom")]=QString::fromStdString(ow.debugSysRomVersion);
	result[QStringLiteral("tbios_id")]=QString::fromStdString(ow.debugTbiosId);
	result[QStringLiteral("tbios_date")]=QString::fromStdString(ow.debugTbiosDate);
	result[QStringLiteral("tos")]=QString::fromStdString(ow.debugTosVersion);
	result[QStringLiteral("mi_words")]=QString::fromStdString(ow.debugMouseInfoWords);
	result[QStringLiteral("mo_words")]=QString::fromStdString(ow.debugMosWorkWords);
	result[QStringLiteral("zoom0_x")]=ow.debugZoom0X;
	result[QStringLiteral("zoom0_y")]=ow.debugZoom0Y;
	result[QStringLiteral("zoom1_x")]=ow.debugZoom1X;
	result[QStringLiteral("zoom1_y")]=ow.debugZoom1Y;
	result[QStringLiteral("psize0_x")]=ow.debugPageSize0X;
	result[QStringLiteral("psize1_x")]=ow.debugPageSize1X;
	result[QStringLiteral("snap_valid")]=ow.debugMouseSnapValid;
	result[QStringLiteral("snap_applied")]=ow.debugMouseSnapApplied;
	result[QStringLiteral("xor_repair")]=ow.debugMouseInfoRepair;
	result[QStringLiteral("mi_prev_x")]=ow.debugMiPrevX;
	result[QStringLiteral("mi_prev_y")]=ow.debugMiPrevY;
	result[QStringLiteral("mi_paint_x")]=ow.debugMiPaintX;
	result[QStringLiteral("mi_paint_y")]=ow.debugMiPaintY;
	return result;
}

QVariantList EmulatorController::mouseCoordWriteScanCandidates() const
{
	QVariantList list;
	if(nullptr==towns_)
	{
		return list;
	}
	towns_->mouseCoordWriteScan.RefreshWatchedValues();
	const auto cands=towns_->mouseCoordWriteScan.GetTopCandidates();
	for(const auto &c : cands)
	{
		QVariantMap row;
		row[QStringLiteral("phys")]=static_cast<uint>(c.physAddr);
		row[QStringLiteral("size")]=static_cast<uint>(c.size);
		row[QStringLiteral("value")]=static_cast<uint>(c.lastValue);
		row[QStringLiteral("score_x")]=c.scoreX;
		row[QStringLiteral("score_y")]=c.scoreY;
		row[QStringLiteral("reject")]=c.rejectScore;
		row[QStringLiteral("in_range")]=static_cast<uint>(c.inRangeHits);
		row[QStringLiteral("hits")]=static_cast<uint>(c.hits);
		row[QStringLiteral("soft")]=c.knownSoftCursor;
		row[QStringLiteral("shadow")]=c.appShadow;
		row[QStringLiteral("shadow_hits")]=static_cast<uint>(c.appShadowHits);
		row[QStringLiteral("motion")]=c.motionPulse;
		row[QStringLiteral("screen_draw")]=c.screenDraw;
		row[QStringLiteral("screen_draw_hits")]=static_cast<uint>(c.screenDrawHits);
		row[QStringLiteral("motion_corr")]=c.motionCorr;
		row[QStringLiteral("motion_corr_hits")]=static_cast<uint>(c.motionCorrHits);
		row[QStringLiteral("from_profile")]=c.fromProfile;
		row[QStringLiteral("profile_x")]=c.profileAxisX;
		row[QStringLiteral("profile_y")]=c.profileAxisY;
		row[QStringLiteral("user_watch")]=c.userWatch;
		row[QStringLiteral("user_chase")]=c.userChase;
		row[QStringLiteral("res_range_fit")]=c.resRangeFit;
		row[QStringLiteral("res_range_fit_x")]=c.resRangeFitX;
		row[QStringLiteral("res_range_fit_y")]=c.resRangeFitY;
		row[QStringLiteral("min")]=static_cast<uint>(c.minValue);
		row[QStringLiteral("max")]=static_cast<uint>(c.maxValue);
		row[QStringLiteral("has_range")]=c.hasRange;
		row[QStringLiteral("cs")]=static_cast<uint>(c.cs);
		row[QStringLiteral("eip")]=static_cast<uint>(c.eip);
		list.push_back(row);
	}
	return list;
}

void EmulatorController::setMouseCoordWriteScanEnabled(bool enabled)
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.SetEnabled(enabled);
	if(nullptr!=impl_->outside_world)
	{
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);
	}
}

void EmulatorController::setMouseCoordForceCapture(bool enabled)
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->var.mouseCoordForceCapture=enabled;
	if(nullptr!=impl_->outside_world)
	{
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);
		if(true==enabled)
		{
			impl_->outside_world->ResumeMouseCapture(*towns_);
		}
	}
}

void EmulatorController::setMouseCoordWriteScanPaused(bool paused)
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.paused=paused;
}

void EmulatorController::startMouseCoordCalibration()
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.StartCalibration();
}

void EmulatorController::stopMouseCoordCalibration()
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.StopCalibration();
}

bool EmulatorController::mouseCoordCalibrating() const
{
	if(nullptr==towns_)
	{
		return false;
	}
	return towns_->mouseCoordWriteScan.IsCalibrating();
}

void EmulatorController::setMouseCoordWatchPhys(const QVariantList &physList)
{
	if(nullptr==towns_)
	{
		return;
	}
	std::vector<unsigned int> phys;
	phys.reserve((size_t)physList.size());
	for(const QVariant &v : physList)
	{
		const unsigned int p=v.toUInt();
		if(0!=p)
		{
			phys.push_back(p);
		}
	}
	towns_->mouseCoordWriteScan.SetUserWatchPhys(phys);
}

void EmulatorController::setMouseCoordChasePhys(const QVariantList &physList)
{
	if(nullptr==towns_)
	{
		return;
	}
	std::vector<unsigned int> phys;
	phys.reserve((size_t)physList.size());
	for(const QVariant &v : physList)
	{
		const unsigned int p=v.toUInt();
		if(0!=p)
		{
			phys.push_back(p);
		}
	}
	towns_->mouseCoordWriteScan.SetUserChasePhys(phys);
}

void EmulatorController::clearMouseCoordWriteScanCandidates()
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.ClearCandidates();
}

void EmulatorController::clearMouseCoordWriteScanRanges()
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.ClearCandidateRanges();
}

void EmulatorController::keepOnlyMouseCoordWriteScanCandidates(const QVariantList &physList)
{
	if(nullptr==towns_)
	{
		return;
	}
	std::vector<unsigned int> keep;
	keep.reserve((size_t)physList.size());
	for(const QVariant &v : physList)
	{
		const unsigned int p=v.toUInt();
		if(0!=p)
		{
			keep.push_back(p);
		}
	}
	towns_->mouseCoordWriteScan.KeepOnlyCandidates(keep);
}

void EmulatorController::selectMouseCoordWriteScanCandidate(unsigned int physAddr,unsigned int size)
{
	if(nullptr==towns_)
	{
		return;
	}
	towns_->mouseCoordWriteScan.SelectCandidate(physAddr,size);
}

unsigned int EmulatorController::chaseMouseCoordSource(unsigned int physAddr)
{
	if(nullptr==towns_ || 0==physAddr)
	{
		return 0;
	}
	return towns_->mouseCoordWriteScan.ChaseSourceOf(physAddr);
}

QVariantList EmulatorController::takeMouseCoordFollowedSources()
{
	QVariantList out;
	if(nullptr==towns_)
	{
		return out;
	}
	unsigned int src[8]={};
	unsigned int n=0;
	towns_->mouseCoordWriteScan.TakeFollowedSources(src,n);
	for(unsigned int i=0; i<n; ++i)
	{
		out.append(static_cast<uint>(src[i]));
	}
	return out;
}

QVariantList EmulatorController::takeMouseCoordClearedChase()
{
	QVariantList out;
	if(nullptr==towns_)
	{
		return out;
	}
	unsigned int src[8]={};
	unsigned int n=0;
	towns_->mouseCoordWriteScan.TakeClearedChase(src,n);
	for(unsigned int i=0; i<n; ++i)
	{
		out.append(static_cast<uint>(src[i]));
	}
	return out;
}

QByteArray EmulatorController::fetchPhysBytes(unsigned int physAddr,unsigned int length) const
{
	QByteArray out;
	if(nullptr==towns_ || 0==length || 4096<length)
	{
		return out;
	}
	// Host-side read: don't let the MOS usage probe count these as app reads.
	towns_->var.suppressMosCoordReadProbe=true;
	out.resize(static_cast<int>(length));
	for(unsigned int i=0; i<length; ++i)
	{
		out[static_cast<int>(i)]=
		    static_cast<char>(towns_->mem.FetchByte(physAddr+i)&0xFF);
	}
	towns_->var.suppressMosCoordReadProbe=false;
	return out;
}

QVariantMap EmulatorController::mouseCoordWriteScanState() const
{
	QVariantMap result;
	if(nullptr==towns_)
	{
		return result;
	}
	result[QStringLiteral("enabled")]=towns_->var.mouseCoordWriteScanEnabled;
	result[QStringLiteral("force_capture")]=towns_->var.mouseCoordForceCapture;
	{
		const std::string discPath=towns_->cdrom.state.GetDisc().fName;
		if(true!=discPath.empty())
		{
			result[QStringLiteral("current_disc_base")]=
			    QString::fromStdString(cpputil::GetBaseName(discPath));
			const auto sz=cpputil::FileSize(discPath);
			if(0<sz)
			{
				result[QStringLiteral("current_disc_size")]=qulonglong(sz);
			}
		}
		// Prefer cached identity.  Never scan ISO sectors on the UI poll path while
		// CDDA prefetch / MODE may own the disc (contention → empty sectors / bad status).
		const DiscIdentity discId=towns_->cdrom.state.GetDisc().ComputeIdentity(false);
		result[QStringLiteral("disc_has_iso9660")]=discId.hasIso9660;
		result[QStringLiteral("disc_has_content_id")]=discId.hasContentId;
		result[QStringLiteral("disc_volume_label")]=QString::fromStdString(discId.volumeLabel);
		result[QStringLiteral("disc_system_id")]=QString::fromStdString(discId.systemIdentifier);
		result[QStringLiteral("disc_content_key")]=QString::fromStdString(discId.contentKey);
		result[QStringLiteral("disc_content_hash")]=QString::fromStdString(discId.contentHashHex);
		result[QStringLiteral("disc_content_hash32")]=discId.contentHash32;
		result[QStringLiteral("disc_toc_hash")]=QString::fromStdString(discId.tocHashHex);
		result[QStringLiteral("disc_toc_hash32")]=discId.tocHash32;
		result[QStringLiteral("disc_has_fingerprint")]=discId.hasFingerprint;
		result[QStringLiteral("disc_fingerprint_hash")]=QString::fromStdString(discId.fingerprintHashHex);
		result[QStringLiteral("disc_fingerprint_hash32")]=discId.fingerprintHash32;
		result[QStringLiteral("disc_num_audio_tracks")]=discId.numAudioTracks;
		result[QStringLiteral("disc_num_data_tracks")]=discId.numDataTracks;
		result[QStringLiteral("disc_pvd_hsg")]=discId.pvdSectorHSG;
		result[QStringLiteral("disc_num_tracks")]=discId.numTracks;
		result[QStringLiteral("disc_num_sectors")]=discId.numSectors;
	}
	result[QStringLiteral("mos")]=towns_->state.mouseBIOSActive;
	result[QStringLiteral("calibrating")]=towns_->mouseCoordWriteScan.IsCalibrating();
	result[QStringLiteral("paused")]=towns_->mouseCoordWriteScan.paused;
	result[QStringLiteral("selected_phys")]=static_cast<uint>(towns_->mouseCoordWriteScan.SelectedPhysAddr());
	result[QStringLiteral("selected_size")]=static_cast<uint>(towns_->mouseCoordWriteScan.SelectedSize());
	result[QStringLiteral("arm")]=static_cast<uint>(towns_->mouseCoordWriteScan.ArmCount());
	result[QStringLiteral("arm_io")]=static_cast<uint>(towns_->mouseCoordWriteScan.IoArmCount());
	result[QStringLiteral("arm_tbios")]=static_cast<uint>(towns_->mouseCoordWriteScan.TbiosIoArmCount());
	result[QStringLiteral("arm_shadow")]=static_cast<uint>(towns_->mouseCoordWriteScan.ShadowArmCount());
	result[QStringLiteral("arm_bios")]=static_cast<uint>(towns_->mouseCoordWriteScan.BiosPathArmCount());
	result[QStringLiteral("stores")]=static_cast<uint>(towns_->mouseCoordWriteScan.StoreEventCount());
	result[QStringLiteral("remaining")]=static_cast<uint>(towns_->mouseCoordWriteScan.TraceRemaining());
	result[QStringLiteral("gp_reads")]=static_cast<uint>(towns_->state.gameportMouseReadCount);
	result[QStringLiteral("mos_app")]=static_cast<uint>(towns_->state.mosCoordAppReadCount);
	result[QStringLiteral("mos_sys")]=static_cast<uint>(towns_->state.mosCoordTbiosSelfReadCount);
	result[QStringLiteral("mos_bios")]=static_cast<uint>(towns_->state.mosBIOSAppCallCount);
	result[QStringLiteral("gp_dx")]=towns_->mouseCoordWriteScan.PendingGpDx();
	result[QStringLiteral("gp_dy")]=towns_->mouseCoordWriteScan.PendingGpDy();
	result[QStringLiteral("cand")]=static_cast<uint>(towns_->mouseCoordWriteScan.CandidateCount());
	{
		const int hostX=towns_->var.lastKnownMouseX;
		const int hostY=towns_->var.lastKnownMouseY;
		result[QStringLiteral("host_x")]=hostX;
		result[QStringLiteral("host_y")]=hostY;

		// What ControlMouse actually aims at (not merely "profile loaded").
		int integX=hostX;
		int integY=hostY;
		const bool apply=towns_->var.mouseCoordProfileApply;
		if(true==apply)
		{
			towns_->mouseCoordWriteScan.MapHostToProfileCoords(integX,integY);
			const auto p=towns_->mouseCoordWriteScan.GetActiveProfile();
			integX+=p.offsetX;
			integY+=p.offsetY;
		}
		else
		{
			int originX=0,originY=0,zoom2xX=2,zoom2xY=2,page=0;
			towns_->TransformHostMouseForIntegration(
			    hostX,hostY,integX,integY,originX,originY,zoom2xX,zoom2xY,page);
		}
		result[QStringLiteral("integ_x")]=integX;
		result[QStringLiteral("integ_y")]=integY;
		result[QStringLiteral("integ_via_profile")]=apply;

		// Profile mapped (always, for comparison — may differ from integ when apply=0).
		int mapX=hostX;
		int mapY=hostY;
		if(true==towns_->mouseCoordWriteScan.ProfileLoaded())
		{
			towns_->mouseCoordWriteScan.MapHostToProfileCoords(mapX,mapY);
		}
		result[QStringLiteral("host_mapped_x")]=mapX;
		result[QStringLiteral("host_mapped_y")]=mapY;

		int screenW=0,screenH=0;
		const bool scrOk=towns_->mouseCoordWriteScan.TryGuestScreenSize(screenW,screenH);
		result[QStringLiteral("screen_ok")]=scrOk;
		result[QStringLiteral("screen_w")]=screenW;
		result[QStringLiteral("screen_h")]=screenH;
		{
			unsigned int page=0;
			if(true!=towns_->crtc.InSinglePageMode())
			{
				page=towns_->state.mouseDisplayPage;
			}
			const auto origin=towns_->crtc.GetPageOriginOnMonitor((unsigned char)page);
			result[QStringLiteral("origin_x")]=origin.x();
			result[QStringLiteral("origin_y")]=origin.y();
		}
	}
	{
		unsigned int px=0,py=0;
		int mx=0,my=0;
		const bool ok=towns_->mouseCoordWriteScan.GetSoftCursorSnapshot(px,py,mx,my);
		result[QStringLiteral("soft_ok")]=ok;
		result[QStringLiteral("soft_px")]=static_cast<uint>(px);
		result[QStringLiteral("soft_py")]=static_cast<uint>(py);
		result[QStringLiteral("soft_x")]=mx;
		result[QStringLiteral("soft_y")]=my;
		int shX=0,shY=0;
		const bool shOk=towns_->mouseCoordWriteScan.TryReadAppShadowCoords(shX,shY);
		result[QStringLiteral("shadow_ok")]=shOk;
		result[QStringLiteral("shadow_x")]=shX;
		result[QStringLiteral("shadow_y")]=shY;
		unsigned int shPx=0,shPy=0;
		towns_->mouseCoordWriteScan.TryReadAppShadowPhys(shPx,shPy);
		result[QStringLiteral("shadow_px")]=static_cast<uint>(shPx);
		result[QStringLiteral("shadow_py")]=static_cast<uint>(shPy);
		int drX=0,drY=0;
		const bool drOk=towns_->mouseCoordWriteScan.TryReadScreenDrawCoords(drX,drY);
		result[QStringLiteral("draw_ok")]=drOk;
		result[QStringLiteral("draw_x")]=drX;
		result[QStringLiteral("draw_y")]=drY;
		unsigned int drPx=0,drPy=0;
		towns_->mouseCoordWriteScan.TryReadScreenDrawPhys(drPx,drPy);
		result[QStringLiteral("draw_px")]=static_cast<uint>(drPx);
		result[QStringLiteral("draw_py")]=static_cast<uint>(drPy);
		unsigned int mnX=0,mxX=0,mnY=0,mxY=0;
		const bool rangeOk=towns_->mouseCoordWriteScan.GetSoftCursorRange(mnX,mxX,mnY,mxY);
		result[QStringLiteral("soft_range_ok")]=rangeOk;
		result[QStringLiteral("soft_min_x")]=static_cast<uint>(mnX);
		result[QStringLiteral("soft_max_x")]=static_cast<uint>(mxX);
		result[QStringLiteral("soft_min_y")]=static_cast<uint>(mnY);
		result[QStringLiteral("soft_max_y")]=static_cast<uint>(mxY);
	}
	{
		const bool mouseLoaded=towns_->mouseCoordWriteScan.ProfileLoaded();
		const bool discLoaded=towns_->mouseCoordWriteScan.DiscProfileLoaded();
		const unsigned int discFingerprintHash=
		    result.value(QStringLiteral("disc_fingerprint_hash32")).toUInt();
		const bool discHasFingerprint=
		    result.value(QStringLiteral("disc_has_fingerprint")).toBool();
		bool profileForCurrentDisc=discLoaded;
		if(true==discLoaded)
		{
			const auto p=towns_->mouseCoordWriteScan.GetActiveProfile();
			if(true==p.HasFingerprint() && true==discHasFingerprint &&
			   0!=discFingerprintHash)
			{
				profileForCurrentDisc=
				    (p.discFingerprintHash32==discFingerprintHash);
			}
		}
		result[QStringLiteral("disc_profile_loaded")]=profileForCurrentDisc;
		result[QStringLiteral("profile_loaded")]=profileForCurrentDisc && mouseLoaded;
		result[QStringLiteral("profile_apply")]=towns_->var.mouseCoordProfileApply;
		result[QStringLiteral("use_disc_profiles")]=towns_->var.useDiscProfiles;
		if(true==profileForCurrentDisc)
		{
			const auto p=towns_->mouseCoordWriteScan.GetActiveProfile();
			result[QStringLiteral("prof_file")]=QString::fromStdString(
			    towns_->mouseCoordWriteScan.ActiveProfileFileName());
			if(true==p.machine.hasMemSizeInMB)
			{
				result[QStringLiteral("prof_mem_size_mb")]=p.machine.memSizeInMB;
			}
			if(true==p.machine.hasFrequencyMhz)
			{
				result[QStringLiteral("prof_frequency_mhz")]=p.machine.frequencyMhz;
			}
			if(true==p.machine.hasCustomFrequencyMhz)
			{
				result[QStringLiteral("prof_custom_frequency_mhz")]=p.machine.customFrequencyMhz;
			}
			if(true==p.machine.hasFastMode)
			{
				result[QStringLiteral("prof_fast_mode")]=p.machine.fastMode;
			}
			else if(true==p.machine.hasFrequencyMhz)
			{
				// Legacy profile: frequency without fast_mode → treat as FAST.
				result[QStringLiteral("prof_fast_mode")]=true;
			}
			if(true==p.machine.hasGamePort0)
			{
				result[QStringLiteral("prof_gameport0")]=static_cast<uint>(p.machine.gamePort0);
			}
			if(true==p.machine.hasGamePort1)
			{
				result[QStringLiteral("prof_gameport1")]=static_cast<uint>(p.machine.gamePort1);
			}
			if(true==p.machine.hasMaxButtonHoldMs0)
			{
				result[QStringLiteral("prof_max_button_hold_ms0")]=p.machine.maxButtonHoldMs0;
			}
			if(true==p.machine.hasMaxButtonHoldMs1)
			{
				result[QStringLiteral("prof_max_button_hold_ms1")]=p.machine.maxButtonHoldMs1;
			}
			if(true==p.machine.hasModelGroup)
			{
				result[QStringLiteral("prof_model_group")]=
				    QString::fromStdString(p.machine.modelGroup);
			}
			else if(true==p.machine.hasModelGroupIndex)
			{
				result[QStringLiteral("prof_model_group")]=
				    TownsQtModelGroupId(p.machine.modelGroupIndex);
			}
			if(true==p.machine.hasCpu)
			{
				result[QStringLiteral("prof_cpu")]=
				    QString::fromStdString(p.machine.cpu);
			}
			else if(true==p.machine.hasModelGroup || true==p.machine.hasModelGroupIndex)
			{
				const int idx=true==p.machine.hasModelGroup ?
				    TownsQtModelGroupIndexForId(QString::fromStdString(p.machine.modelGroup)) :
				    p.machine.modelGroupIndex;
				const TownsQtCpuKind kind=TownsQtCpuKindFromTownsType(
				    TownsQtModelGroupTownsType(idx));
				result[QStringLiteral("prof_cpu")]=
				    QString::fromLatin1(TownsQtCpuKindId(kind));
			}
			if(true==p.machine.hasCpuHighFidelity)
			{
				result[QStringLiteral("prof_cpu_high_fidelity")]=p.machine.cpuHighFidelity;
			}
			if(true==p.machine.hasPretend386DX)
			{
				result[QStringLiteral("prof_pretend_386dx")]=p.machine.pretend386DX;
			}
			if(true==p.machine.hasUseFPU)
			{
				result[QStringLiteral("prof_use_fpu")]=p.machine.useFPU;
			}
			if(true==p.machine.hasFastScsi)
			{
				result[QStringLiteral("prof_fast_scsi")]=p.machine.fastScsi;
			}
			if(true==p.machine.hasFastFd)
			{
				result[QStringLiteral("prof_fast_fd")]=p.machine.fastFd;
			}
			if(true==p.machine.hasMidiBoard)
			{
				result[QStringLiteral("prof_midi_board")]=p.machine.midiBoard;
			}
			if(true==p.machine.hasSingleDrive)
			{
				result[QStringLiteral("prof_single_drive")]=p.machine.singleDrive;
			}
			if(true==p.machine.hasFdImg[0])
			{
				result[QStringLiteral("prof_fd0")]=QString::fromStdString(p.machine.fdImg[0]);
			}
			if(true==p.machine.hasFdImg[1])
			{
				result[QStringLiteral("prof_fd1")]=QString::fromStdString(p.machine.fdImg[1]);
			}
			for(int hd=0; hd<MouseCoordWriteScan::MachineSettings::kHddImgCount; ++hd)
			{
				if(true==p.machine.hasHddImg[hd])
				{
					result[QStringLiteral("prof_hd%1").arg(hd)]=
					    QString::fromStdString(p.machine.hddImg[hd]);
				}
			}
			if(true==p.machine.hasBootKeyComb)
			{
				result[QStringLiteral("prof_boot_key")]=
				    QString::fromStdString(TownsKeyCombToStr(p.machine.bootKeyComb));
			}
			if(true==p.machine.hasHighResCrtc)
			{
				result[QStringLiteral("prof_high_res_crtc")]=p.machine.highResCrtc;
			}
			if(true==p.machine.hasHighResPcm)
			{
				result[QStringLiteral("prof_high_res_pcm")]=p.machine.highResPcm;
			}
			result[QStringLiteral("prof_has_mouse")]=p.HasMouseIntegration();
			result[QStringLiteral("prof_verified")]=p.verified;
			// Always expose the stored mouse operation type for the Settings editor,
			// even when HasMouseIntegration() is false (e.g. empty DW/GF slots).
			result[QStringLiteral("prof_integration_mode")]=p.integrationMode;
			result[QStringLiteral("prof_feedback_only")]=p.feedbackOnly;
			result[QStringLiteral("prof_enabled")]=p.enabled;
			result[QStringLiteral("prof_app_exec_name")]=
			    QString::fromStdString(p.appExecName);
			result[QStringLiteral("prof_app_exec_hash")]=
			    static_cast<uint>(p.appExecHash32);
		}
		{
			std::string appName;
			unsigned int appHash=0;
			towns_->mouseCoordWriteScan.GetActiveAppExec(appName,appHash);
			result[QStringLiteral("current_app_exec_name")]=
			    QString::fromStdString(appName);
			result[QStringLiteral("current_app_exec_hash")]=
			    static_cast<uint>(appHash);
			result[QStringLiteral("app_exec_matched")]=
			    towns_->mouseCoordWriteScan.AppExecMatched();
		}
		if(true==profileForCurrentDisc && true==mouseLoaded)
		{
			const auto p=towns_->mouseCoordWriteScan.GetActiveProfile();
			result[QStringLiteral("prof_px")]=static_cast<uint>(p.physX);
			result[QStringLiteral("prof_py")]=static_cast<uint>(p.physY);
			result[QStringLiteral("prof_num_pairs")]=
			    static_cast<uint>(p.NumPairs());
			for(uint i=0; i<MouseCoordWriteScan::MAX_COORD_PAIRS; ++i)
			{
				const auto &pr=p.pair[i];
				const QString base=QStringLiteral("prof_pair%1").arg(i);
				result[base+QStringLiteral("_x")]=static_cast<uint>(pr.physX);
				result[base+QStringLiteral("_y")]=static_cast<uint>(pr.physY);
				if(true==pr.hasDsOff)
				{
					result[base+QStringLiteral("_ds_off_x")]=static_cast<uint>(pr.dsOffX);
					result[base+QStringLiteral("_ds_off_y")]=static_cast<uint>(pr.dsOffY);
					result[base+QStringLiteral("_ds_sel")]=static_cast<uint>(pr.dsSelector);
					result[base+QStringLiteral("_has_ds_off")]=true;
				}
				result[base+QStringLiteral("_bias_x")]=pr.biasX;
				result[base+QStringLiteral("_bias_y")]=pr.biasY;
				result[base+QStringLiteral("_scale_x")]=pr.scaleX;
				result[base+QStringLiteral("_scale_y")]=pr.scaleY;
				if(true==pr.hasRangeX)
				{
					result[base+QStringLiteral("_min_x")]=pr.rangeMinX;
					result[base+QStringLiteral("_max_x")]=pr.rangeMaxX;
				}
				if(true==pr.hasRangeY)
				{
					result[base+QStringLiteral("_min_y")]=pr.rangeMinY;
					result[base+QStringLiteral("_max_y")]=pr.rangeMaxY;
				}
				if(true==pr.Valid())
				{
					result[base+QStringLiteral("_val_x")]=
					    (int)(short)towns_->mem.FetchWord(pr.physX);
					result[base+QStringLiteral("_val_y")]=
					    (int)(short)towns_->mem.FetchWord(pr.physY);
				}
			}
			result[QStringLiteral("direct_write_count")]=
			    static_cast<uint>(towns_->mouseCoordWriteScan.DirectWriteCount());
			result[QStringLiteral("direct_write_ok")]=
			    towns_->mouseCoordWriteScan.LastDirectWriteOk();
			result[QStringLiteral("app_guard_blocks")]=
			    static_cast<uint>(towns_->mouseCoordWriteScan.AppStoreGuardBlockCount());
			result[QStringLiteral("app_guard_on")]=
			    towns_->mem.storeGuardActive;
			int tX=0,tY=0;
			towns_->mouseCoordWriteScan.GetLastDirectWrite(tX,tY);
			result[QStringLiteral("direct_target_x")]=tX;
			result[QStringLiteral("direct_target_y")]=tY;
			// Who else writes the target words — tells copies apart from
			// engine-owned state that re-writes itself (e.g. back to screen center).
			auto guestKeyName=[](uint i)->QString
			{
				if(MouseCoordWriteScan::TARGET_SOFT_X==i)
				{
					return QStringLiteral("guest_soft_x");
				}
				if(MouseCoordWriteScan::TARGET_SOFT_Y==i)
				{
					return QStringLiteral("guest_soft_y");
				}
				const uint p=i-MouseCoordWriteScan::TARGET_PAIR_BASE;
				return QStringLiteral("guest_p%1%2").arg(p/2).arg(0==(p&1) ? 'x' : 'y');
			};
			for(uint i=0; i<MouseCoordWriteScan::NUM_TARGET_WRITE; ++i)
			{
				const auto w=towns_->mouseCoordWriteScan.GetGuestTargetWrite(i);
				const QString key=guestKeyName(i);
				result[key+QStringLiteral("_n")]=static_cast<uint>(w.count);
				result[key+QStringLiteral("_v")]=(int)(short)(w.lastValue&0xffff);
				result[key+QStringLiteral("_cs")]=static_cast<uint>(w.cs);
				result[key+QStringLiteral("_eip")]=static_cast<uint>(w.eip);
			}
			{
				const auto tr=towns_->mouseCoordWriteScan.GetGuestWriterTrace();
				result[QStringLiteral("guest_writer_valid")]=tr.valid;
				if(true==tr.valid)
				{
					QString wn=QStringLiteral("?");
					if(true==tr.fromChase)
					{
						wn=QStringLiteral("chase");
					}
					else if(MouseCoordWriteScan::TARGET_SOFT_X==tr.which)
					{
						wn=QStringLiteral("softX");
					}
					else if(MouseCoordWriteScan::TARGET_SOFT_Y==tr.which)
					{
						wn=QStringLiteral("softY");
					}
					else if(tr.which<MouseCoordWriteScan::NUM_TARGET_WRITE)
					{
						const uint p=tr.which-MouseCoordWriteScan::TARGET_PAIR_BASE;
						wn=QStringLiteral("pair%1%2").arg(p/2).arg(0==(p&1) ? 'X' : 'Y');
					}
					result[QStringLiteral("guest_writer_which")]=wn;
					result[QStringLiteral("guest_writer_from_chase")]=tr.fromChase;
					result[QStringLiteral("guest_writer_phys")]=static_cast<uint>(tr.physAddr);
					result[QStringLiteral("guest_writer_val")]=(int)(short)(tr.value&0xffff);
					result[QStringLiteral("guest_writer_cs")]=static_cast<uint>(tr.cs);
					result[QStringLiteral("guest_writer_eip")]=static_cast<uint>(tr.eip);
					result[QStringLiteral("guest_writer_eax")]=static_cast<uint>(tr.eax);
					result[QStringLiteral("guest_writer_ebx")]=static_cast<uint>(tr.ebx);
					result[QStringLiteral("guest_writer_ecx")]=static_cast<uint>(tr.ecx);
					result[QStringLiteral("guest_writer_edx")]=static_cast<uint>(tr.edx);
					result[QStringLiteral("guest_writer_esi")]=static_cast<uint>(tr.esi);
					result[QStringLiteral("guest_writer_edi")]=static_cast<uint>(tr.edi);
					result[QStringLiteral("guest_writer_ebp")]=static_cast<uint>(tr.ebp);
					result[QStringLiteral("guest_writer_esp")]=static_cast<uint>(tr.esp);
					result[QStringLiteral("guest_writer_ds")]=static_cast<uint>(tr.ds);
					result[QStringLiteral("guest_writer_es")]=static_cast<uint>(tr.es);
					result[QStringLiteral("guest_writer_ss")]=static_cast<uint>(tr.ss);
					result[QStringLiteral("guest_writer_ds_base")]=static_cast<uint>(tr.dsBase);
					result[QStringLiteral("guest_writer_es_base")]=static_cast<uint>(tr.esBase);
					result[QStringLiteral("guest_writer_ss_base")]=static_cast<uint>(tr.ssBase);
					result[QStringLiteral("guest_writer_implied_base")]=
					    static_cast<uint>(tr.impliedDsBase);
					result[QStringLiteral("guest_writer_has_implied_base")]=tr.hasImpliedDsBase;
					result[QStringLiteral("guest_writer_store_moffs")]=
					    static_cast<uint>(tr.storeMoffs);
					result[QStringLiteral("guest_writer_has_src")]=tr.hasSrc;
					result[QStringLiteral("guest_writer_src_moffs")]=
					    static_cast<uint>(tr.srcMoffs);
					result[QStringLiteral("guest_writer_src_phys")]=
					    static_cast<uint>(tr.srcPhys);
					result[QStringLiteral("guest_writer_src_val")]=tr.srcValue;
					result[QStringLiteral("guest_writer_copy_summary")]=
					    QString::fromStdString(tr.copySummary);
					result[QStringLiteral("guest_writer_disasm")]=
					    QString::fromStdString(tr.disasm);
					QString nearStr;
					for(int i=0; i<12; ++i)
					{
						if(0!=i)
						{
							nearStr+=QLatin1Char(' ');
						}
						const unsigned int p=
						    (tr.physAddr>=8u) ? (tr.physAddr-8u+(unsigned)i*2u)
						                      : ((unsigned)i*2u);
						nearStr+=QStringLiteral("%1=%2")
						             .arg(p,5,16,QLatin1Char('0'))
						             .arg(tr.nearWords[i],4,16,QLatin1Char('0'));
					}
					result[QStringLiteral("guest_writer_near")]=nearStr;
				}
			}
			{
				unsigned int chase[8]={};
				unsigned int nChase=0;
				towns_->mouseCoordWriteScan.GetChasePhys(chase,nChase);
				result[QStringLiteral("guest_chase_n")]=static_cast<uint>(nChase);
				for(uint i=0; i<nChase && i<8; ++i)
				{
					result[QStringLiteral("guest_chase_%1").arg(i)]=
					    static_cast<uint>(chase[i]);
				}
			}
			result[QStringLiteral("prof_offset_x")]=p.offsetX;
			result[QStringLiteral("prof_offset_y")]=p.offsetY;
			result[QStringLiteral("prof_scale_x")]=p.pair[0].scaleX;
			result[QStringLiteral("prof_scale_y")]=p.pair[0].scaleY;
			result[QStringLiteral("prof_invert_x")]=p.invertX;
			result[QStringLiteral("prof_invert_y")]=p.invertY;
			result[QStringLiteral("prof_wait_feedback")]=p.waitFeedback;
			result[QStringLiteral("prof_stop_soft_write")]=p.stopSoftWrite;
			result[QStringLiteral("prof_verified")]=p.verified;
			result[QStringLiteral("prof_cd")]=
			    true==p.HasFingerprint()
			        ? QStringLiteral("fp_%1")
			              .arg(p.discFingerprintHash32,8,16,QLatin1Char('0'))
			        : (true==p.HasContentId()
			               ? QString::fromStdString(p.discVolumeLabel+"|"+p.discSystemId)
			               : QString::fromStdString(p.cdBasename));
			result[QStringLiteral("prof_disc_volume")]=QString::fromStdString(p.discVolumeLabel);
			result[QStringLiteral("prof_disc_system")]=QString::fromStdString(p.discSystemId);
			result[QStringLiteral("prof_disc_content_hash32")]=p.discContentHash32;
			result[QStringLiteral("prof_disc_fingerprint_hash32")]=p.discFingerprintHash32;
			result[QStringLiteral("prof_file")]=QString::fromStdString(
			    towns_->mouseCoordWriteScan.ActiveProfileFileName());
		}
	}
	return result;
}

QVariantMap EmulatorController::captureDsRelativeFromPhys(
    unsigned int physX,unsigned int physY) const
{
	QVariantMap out;
	if(nullptr==towns_ || 0==physX || 0==physY)
	{
		return out;
	}
	unsigned int offX=0,offY=0,selX=0,selY=0;
	if(true!=towns_->mouseCoordWriteScan.CaptureDsRelativeFromPhys(physX,offX,selX) ||
	   true!=towns_->mouseCoordWriteScan.CaptureDsRelativeFromPhys(physY,offY,selY))
	{
		return out;
	}
	out.insert(QStringLiteral("ds_off_x"),static_cast<uint>(offX));
	out.insert(QStringLiteral("ds_off_y"),static_cast<uint>(offY));
	out.insert(QStringLiteral("ds_sel"),static_cast<uint>((0!=selX) ? selX : selY));
	out.insert(QStringLiteral("ok"),true);
	return out;
}

bool EmulatorController::captureMouseCoordProfileFromSoftCursor()
{
	if(nullptr==towns_)
	{
		return false;
	}
	if(true!=towns_->mouseCoordWriteScan.CaptureSoftCursorProfile())
	{
		return false;
	}
	return towns_->mouseCoordWriteScan.SaveActiveProfile();
}

bool EmulatorController::saveMouseCoordProfile()
{
	if(nullptr==towns_)
	{
		return false;
	}
	return towns_->mouseCoordWriteScan.SaveActiveProfile();
}

bool EmulatorController::resetMouseCoordProfile()
{
	if(nullptr==towns_)
	{
		return false;
	}
	return towns_->mouseCoordWriteScan.ResetProfileSettings();
}

bool EmulatorController::applyMouseCoordProfile(const QVariantMap &profile)
{
	if(nullptr==towns_)
	{
		return false;
	}
	// Preserve [machine] and disc identity from the active disc profile.
	MouseCoordWriteScan::Profile p=towns_->mouseCoordWriteScan.GetActiveProfile();
	for(auto &pr : p.pair)
	{
		pr=MouseCoordWriteScan::CoordPair();
	}
	if(profile.contains(QStringLiteral("prof_px")))
	{
		p.physX=profile.value(QStringLiteral("prof_px")).toUInt();
	}
	if(profile.contains(QStringLiteral("prof_py")))
	{
		p.physY=profile.value(QStringLiteral("prof_py")).toUInt();
	}
	for(uint i=0; i<MouseCoordWriteScan::MAX_COORD_PAIRS; ++i)
	{
		const QString base=QStringLiteral("prof_pair%1").arg(i);
		auto &pr=p.pair[i];
		if(profile.contains(base+QStringLiteral("_x")))
		{
			pr.physX=profile.value(base+QStringLiteral("_x")).toUInt();
		}
		if(profile.contains(base+QStringLiteral("_y")))
		{
			pr.physY=profile.value(base+QStringLiteral("_y")).toUInt();
		}
		if(profile.contains(base+QStringLiteral("_ds_off_x")) ||
		   profile.contains(base+QStringLiteral("_ds_off_y")))
		{
			pr.dsOffX=profile.value(base+QStringLiteral("_ds_off_x")).toUInt();
			pr.dsOffY=profile.value(base+QStringLiteral("_ds_off_y")).toUInt();
			pr.hasDsOff=true;
			pr.dsSelector=profile.value(base+QStringLiteral("_ds_sel")).toUInt();
		}
		else if(true==pr.Valid())
		{
			// Capture DS-relative offsets when absolute phys was set without offs.
			unsigned int offX=0,offY=0,selX=0,selY=0;
			if(true==towns_->mouseCoordWriteScan.CaptureDsRelativeFromPhys(
			       pr.physX,offX,selX) &&
			   true==towns_->mouseCoordWriteScan.CaptureDsRelativeFromPhys(
			       pr.physY,offY,selY))
			{
				pr.dsOffX=offX;
				pr.dsOffY=offY;
				pr.dsSelector=(0!=selX) ? selX : selY;
				pr.hasDsOff=true;
			}
		}
		if(profile.contains(base+QStringLiteral("_bias_x")))
		{
			pr.biasX=profile.value(base+QStringLiteral("_bias_x")).toInt();
		}
		if(profile.contains(base+QStringLiteral("_bias_y")))
		{
			pr.biasY=profile.value(base+QStringLiteral("_bias_y")).toInt();
		}
		if(profile.contains(base+QStringLiteral("_scale_x")))
		{
			pr.scaleX=profile.value(base+QStringLiteral("_scale_x")).toInt();
		}
		if(profile.contains(base+QStringLiteral("_scale_y")))
		{
			pr.scaleY=profile.value(base+QStringLiteral("_scale_y")).toInt();
		}
		if(profile.contains(base+QStringLiteral("_min_x")))
		{
			pr.rangeMinX=profile.value(base+QStringLiteral("_min_x")).toInt();
			pr.rangeMaxX=profile.value(base+QStringLiteral("_max_x"),pr.rangeMinX).toInt();
			pr.hasRangeX=true;
		}
		if(profile.contains(base+QStringLiteral("_min_y")))
		{
			pr.rangeMinY=profile.value(base+QStringLiteral("_min_y")).toInt();
			pr.rangeMaxY=profile.value(base+QStringLiteral("_max_y"),pr.rangeMinY).toInt();
			pr.hasRangeY=true;
		}
	}
	if(profile.contains(QStringLiteral("prof_integration_mode")))
	{
		const int mode=profile.value(QStringLiteral("prof_integration_mode")).toInt();
		if(MouseCoordWriteScan::INTEGRATION_DIFFERENTIAL==mode ||
		   MouseCoordWriteScan::INTEGRATION_MOS==mode ||
		   MouseCoordWriteScan::INTEGRATION_DIRECT_WRITE==mode ||
		   MouseCoordWriteScan::INTEGRATION_GAME_FEEDBACK==mode ||
		   MouseCoordWriteScan::INTEGRATION_AUTO==mode)
		{
			p.integrationMode=mode;
		}
	}
	else if(profile.contains(QStringLiteral("prof_feedback_only")) ||
	        profile.contains(QStringLiteral("prof_enabled")))
	{
		if(profile.contains(QStringLiteral("prof_feedback_only")))
		{
			p.feedbackOnly=profile.value(QStringLiteral("prof_feedback_only")).toBool();
		}
		if(profile.contains(QStringLiteral("prof_enabled")))
		{
			p.enabled=profile.value(QStringLiteral("prof_enabled")).toBool();
		}
		p.DeriveModeFromLegacyFlags();
	}
	p.SyncLegacyFlagsFromMode();
	if(profile.contains(QStringLiteral("prof_offset_x")))
	{
		p.offsetX=profile.value(QStringLiteral("prof_offset_x")).toInt();
	}
	if(profile.contains(QStringLiteral("prof_offset_y")))
	{
		p.offsetY=profile.value(QStringLiteral("prof_offset_y")).toInt();
	}
	if(profile.contains(QStringLiteral("prof_scale_x")))
	{
		p.scaleX=profile.value(QStringLiteral("prof_scale_x")).toInt();
		if(true!=profile.contains(QStringLiteral("prof_pair0_scale_x")))
		{
			p.pair[0].scaleX=p.scaleX;
		}
	}
	if(profile.contains(QStringLiteral("prof_scale_y")))
	{
		p.scaleY=profile.value(QStringLiteral("prof_scale_y")).toInt();
		if(true!=profile.contains(QStringLiteral("prof_pair0_scale_y")))
		{
			p.pair[0].scaleY=p.scaleY;
		}
	}
	p.scaleX=p.pair[0].scaleX;
	p.scaleY=p.pair[0].scaleY;
	if(profile.contains(QStringLiteral("prof_invert_x")))
	{
		p.invertX=profile.value(QStringLiteral("prof_invert_x")).toBool();
	}
	if(profile.contains(QStringLiteral("prof_invert_y")))
	{
		p.invertY=profile.value(QStringLiteral("prof_invert_y")).toBool();
	}
	if(profile.contains(QStringLiteral("prof_wait_feedback")))
	{
		p.waitFeedback=profile.value(QStringLiteral("prof_wait_feedback")).toBool();
	}
	if(profile.contains(QStringLiteral("prof_stop_soft_write")))
	{
		p.stopSoftWrite=profile.value(QStringLiteral("prof_stop_soft_write")).toBool();
	}
	if(profile.contains(QStringLiteral("prof_app_exec_name")))
	{
		// Keep path separators; core NormalizeDosExecPath runs on save/load in INI.
		p.appExecName=
		    profile.value(QStringLiteral("prof_app_exec_name")).toString().trimmed().toUpper().toStdString();
		for(char &c : p.appExecName)
		{
			if('/'==c)
			{
				c='\\';
			}
		}
	}
	if(profile.contains(QStringLiteral("prof_app_exec_hash")))
	{
		p.appExecHash32=profile.value(QStringLiteral("prof_app_exec_hash")).toUInt();
	}
	if(profile.contains(QStringLiteral("prof_verified")))
	{
		p.verified=profile.value(QStringLiteral("prof_verified")).toBool();
	}
	else
	{
		p.verified=true;
	}
	p.SyncLegacyFlagsFromMode();
	// Empty Phys (Clear) is allowed for app-specific modes; runtime apply
	// simply stays idle until pairs are configured.
	for(auto &pr : p.pair)
	{
		if(true!=pr.Configured())
		{
			pr.physX=0;
			pr.physY=0;
			pr.hasDsOff=false;
			pr.dsOffX=0;
			pr.dsOffY=0;
			pr.dsSelector=0;
			pr.hasRangeX=false;
			pr.hasRangeY=false;
			pr.rangeMinX=0;
			pr.rangeMaxX=0;
			pr.rangeMinY=0;
			pr.rangeMaxY=0;
		}
	}
	if(true!=p.HasMouseIntegration())
	{
		return false;
	}
	// Soft phys is unused at runtime — clear so saves do not keep stale IDs.
	p.physX=0;
	p.physY=0;
	const std::string discPath=towns_->cdrom.state.GetDisc().fName;
	if(true!=discPath.empty())
	{
		p.cdBasename=cpputil::GetBaseName(discPath);
		const auto sz=cpputil::FileSize(discPath);
		if(0<sz)
		{
			p.cdSize=(unsigned long long)sz;
		}
		const DiscIdentity discId=towns_->cdrom.state.GetDisc().ComputeIdentity(true);
		if(true==discId.hasContentId)
		{
			p.discVolumeLabel=discId.volumeLabel;
			p.discSystemId=discId.systemIdentifier;
			p.discContentHash32=discId.contentHash32;
		}
		if(true==discId.hasFingerprint)
		{
			p.discFingerprintHash32=discId.fingerprintHash32;
		}
	}
	towns_->mouseCoordWriteScan.SetActiveProfile(p);
	return true;
}

bool EmulatorController::applyAndSaveMouseCoordProfile(const QVariantMap &profile)
{
	if(true!=applyMouseCoordProfile(profile))
	{
		return false;
	}
	if(true!=towns_->mouseCoordWriteScan.SaveActiveProfile())
	{
		return false;
	}
	if(nullptr!=impl_->outside_world)
	{
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

bool EmulatorController::bindCurrentAppExecToMouseProfile(void)
{
	if(nullptr==towns_)
	{
		return false;
	}
	if(true!=towns_->mouseCoordWriteScan.BindActiveAppExecToProfile())
	{
		return false;
	}
	if(true!=towns_->mouseCoordWriteScan.SaveActiveProfile())
	{
		return false;
	}
	if(nullptr!=impl_->outside_world)
	{
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

bool EmulatorController::createDiscProfile(const QVariantMap &machine)
{
	if(nullptr==towns_)
	{
		return false;
	}
	MouseCoordWriteScan::MachineSettings m=MachineSettingsFromVariantMap(machine);
	if(true!=towns_->mouseCoordWriteScan.CreateProfileForCurrentDisc(m))
	{
		return false;
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

bool EmulatorController::deleteDiscProfile(void)
{
	if(nullptr==towns_)
	{
		return false;
	}
	if(true!=towns_->mouseCoordWriteScan.DeleteProfileForCurrentDisc())
	{
		return false;
	}
	if(nullptr!=impl_->outside_world)
	{
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

bool EmulatorController::saveDiscMachineProfile(const QVariantMap &machine)
{
	if(nullptr==towns_)
	{
		return false;
	}
	MouseCoordWriteScan::MachineSettings m=MachineSettingsFromVariantMap(machine);
	if(true!=towns_->mouseCoordWriteScan.ApplyAndSaveMachineSettings(m))
	{
		return false;
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

bool EmulatorController::updateDiscMachineClock(bool fastMode,int frequencyMhz,int customFrequencyMhz)
{
	if(nullptr==towns_)
	{
		return false;
	}
	frequencyMhz=std::clamp(frequencyMhz,1,100);
	customFrequencyMhz=std::clamp(customFrequencyMhz,33,60);
	if(true!=towns_->mouseCoordWriteScan.MergeAndSaveMachineClock(
	       fastMode,frequencyMhz,customFrequencyMhz))
	{
		return false;
	}
	Q_EMIT discProfileStateChanged();
	return true;
}

void EmulatorController::setUseDiscProfiles(bool enabled)
{
	if(nullptr!=towns_)
	{
		towns_->var.useDiscProfiles=enabled;
	}
	if(nullptr!=impl_->outside_world && nullptr!=towns_)
	{
		impl_->outside_world->UpdateEffectiveDifferentialMouseIntegration(*towns_);
	}
	Q_EMIT discProfileStateChanged();
}

void EmulatorController::updateStats()
{
	if(nullptr==towns_ || nullptr==framebuffer_ || nullptr==impl_->window)
	{
		return;
	}

	if(true!=stats_timer_started_)
	{
		stats_timer_.start();
		stats_towns_time0_=towns_->GetObserverTownsTimeNs();
		stats_frame_count_=0;
		stats_timer_started_=true;
	}
	if(kStatsWindowMs>stats_timer_.elapsed())
	{
		return;
	}

	const double sec=stats_timer_.elapsed()/1000.0;
	const double fps=0.0<sec ? static_cast<double>(stats_frame_count_)/sec : 0.0;
	const uint64_t towns_now=towns_->GetObserverTownsTimeNs();
	const double emu_hz=0.0<sec ?
	    static_cast<double>(towns_now-stats_towns_time0_)/static_cast<double>(TOWNS_RENDERING_FREQUENCY)/sec :
	    0.0;
	if(stats_fps_ema_<=0.0)
	{
		stats_fps_ema_=fps;
		stats_hz_ema_=emu_hz;
	}
	else
	{
		stats_fps_ema_=kStatsEmaAlpha*fps+(1.0-kStatsEmaAlpha)*stats_fps_ema_;
		stats_hz_ema_=kStatsEmaAlpha*emu_hz+(1.0-kStatsEmaAlpha)*stats_hz_ema_;
	}
	const int queue_depth=static_cast<int>(framebuffer_->QueueDepth());
	const int capture_queue_depth=static_cast<int>(impl_->window->VmCaptureQueueDepth());
	const uint64_t due_index=towns_->GetObserverTownsTimeNs()/TOWNS_RENDERING_FREQUENCY;
	const uint64_t front_vsync_index=framebuffer_->FrontVsyncIndex();
	int present_lag=0;
	if(0<front_vsync_index)
	{
		present_lag=static_cast<int>(due_index-front_vsync_index);
	}
	else if(true==has_presented_frame_)
	{
		present_lag=static_cast<int>(due_index-last_presented_vsync_index_);
	}
	Q_EMIT statsUpdated(stats_fps_ema_,stats_hz_ema_,queue_depth,capture_queue_depth,present_lag);
	stats_timer_.restart();
	stats_frame_count_=0;
	stats_towns_time0_=towns_now;
}

void EmulatorController::onVmFinished()
{
	running_.store(false,std::memory_order_relaxed);
}
