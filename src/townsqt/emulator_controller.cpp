#include "emulator_controller.h"

#undef slots

#include "qt_command_thread.h"
#include "qt_outside_world.h"
#include "cdrom.h"
#include "towns.h"
#include "townsdef.h"
#include "qt_sync_sound.h"
#include "townsqt_settings.h"
#include "townsdef.h"
#include "townsthread.h"

#include "fssimplewindow_connection.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

struct EmulatorController::Impl
{
	QtOutsideWorld *outside_world=nullptr;
	Outside_World::Sound *sound=nullptr;
	Outside_World::WindowInterface *window=nullptr;
	TownsThread townsThread;
	QtCommandThread cmdThread;
	std::thread vmThread;
	std::thread uiThread;
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

	impl_->outside_world=new QtOutsideWorld(inputQueue_,framebuffer_);
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
		applyAudioVolumes(
		    TownsQtSettings::fmChipVolume(),
		    TownsQtSettings::pcmChipVolume(),
		    TownsQtSettings::cddaVolumePercent(),
		    TownsQtSettings::pcmLpfEnabled(),
		    TownsQtSettings::pcmLpfCutoffHz(),
		    TownsQtSettings::pcmResampleHighQuality());
		if(true==argv_.debugger)
		{
			towns.EnableDebugger();
		}
		else
		{
			towns.DisableDebugger();
		}

		impl_->window->Start();
		impl_->window->ClearVMClosedFlag();
		applyPeripheralSettings(
		    TownsQtSettings::gamePort(0),
		    TownsQtSettings::gamePort(1),
		    TownsQtSettings::maxButtonHoldTimeMs(0,0),
		    TownsQtSettings::maxButtonHoldTimeMs(0,1),
		    TownsQtSettings::mouseIntegrationSpeed(),
		    TownsQtSettings::considerVRAMOffsetInMouseIntegration(),
		    TownsQtSettings::differentialMouseIntegration(),
		    TownsQtSettings::mouseMinX(),
		    TownsQtSettings::mouseMinY(),
		    TownsQtSettings::mouseMaxX(),
		    TownsQtSettings::mouseMaxY());
		applyCpuFastMode(TownsQtSettings::cpuFastModeEnabled());
		setCdSpeed(TownsQtSettings::cdSpeed());
		if(nullptr!=towns_)
		{
			last_fast_mode_lamp_revision_=towns_->var.fastModeLampRevision.load(std::memory_order_acquire);
			last_fast_mode_lamp_=towns_->FASTModeLamp();
			towns_->midi.midiMonitor=TownsQtSettings::midiMonitor();
		}
		impl_->townsThread.SetRunMode(TownsThread::RUNMODE_RUN);

		if(""!=argv_.cdImgFName)
		{
			cd_path_=QString::fromStdString(argv_.cdImgFName);
			TownsQtSettings::setLastCdImagePath(cd_path_);
			Q_EMIT cdPathChanged(cd_path_);
		}

		impl_->uiThread=std::thread(
		    &QtCommandThread::Run,&impl_->cmdThread,&impl_->townsThread,&towns,&argv_,impl_->outside_world);

		impl_->vmThread=std::thread([this,&towns]{
			impl_->townsThread.VMStart(&towns,impl_->outside_world,&impl_->cmdThread);
			impl_->townsThread.VMMainLoop(&towns,impl_->outside_world,impl_->sound,impl_->window,&impl_->cmdThread);
			impl_->townsThread.VMEnd(&towns,impl_->outside_world,&impl_->cmdThread);
			QMetaObject::invokeMethod(this,"onVmFinished",Qt::QueuedConnection);
		});

		if(""==argv_.cdImgFName)
		{
			const QString saved_cd=TownsQtSettings::lastCdImagePath();
			if(!saved_cd.isEmpty() && QFile::exists(saved_cd))
			{
				QTimer::singleShot(0,this,[this,saved_cd]{
					loadCdImage(saved_cd);
				});
			}
		}
		for(int drive=0; drive<2; ++drive)
		{
			if(""!=argv_.fdImgFName[drive])
			{
				continue;
			}
			const QString saved_fd=TownsQtSettings::lastFdImagePath(drive);
			if(!saved_fd.isEmpty() && QFile::exists(saved_fd))
			{
				QTimer::singleShot(0,this,[this,drive,saved_fd]{
					loadFdImage(drive,saved_fd);
				});
			}
		}

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
			Q_EMIT finished();
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
			Q_EMIT finished();
			return;
		}
		runWithTowns(*towns);
	}

	Q_EMIT finished();
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

void EmulatorController::loadCdImage(const QString &path)
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
}

void EmulatorController::ejectCd()
{
	if(nullptr!=impl_->outside_world)
	{
		cd_path_.clear();
		TownsQtSettings::clearLastCdImagePath();
		Q_EMIT cdPathChanged(cd_path_);
		impl_->cmdThread.EnqueueCommand(*impl_->outside_world,"CDEJECT");
	}
}

void EmulatorController::loadFdImage(int drive,const QString &path)
{
	drive=std::clamp(drive,0,1);
	if(path.isEmpty() || nullptr==impl_->outside_world)
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
		impl_->cmdThread.EnqueueCommand(
		    *impl_->outside_world,
		    (0==drive) ? "FD0WRITEPROTECT" : "FD1WRITEPROTECT");
	}
	Q_EMIT fdWriteProtectChanged(drive,write_protect);
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
	}
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
			cmd=write_protect ? "FD0WRITEPROTECT" : "FD0WRITEUNPROTECT";
		}
		else
		{
			cmd=write_protect ? "FD1WRITEPROTECT" : "FD1WRITEUNPROTECT";
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

void EmulatorController::setMidiMonitor(bool enabled)
{
	if(nullptr!=towns_)
	{
		towns_->midi.midiMonitor=enabled;
	}
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
	const int preset=TownsQtSettings::cpuFrequencyMhz();
	if(nullptr!=towns_)
	{
		towns_->state.fastModeFreq=preset;
		if(true==TownsQtSettings::cpuFastModeEnabled())
		{
			towns_->state.currentFreq=preset;
		}
	}
}

void EmulatorController::applyCpuFastMode(bool enabled)
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	TownsQtSettings::setCpuFastModeEnabled(enabled);
	if(nullptr==towns_)
	{
		return;
	}
	towns_->physMem.state.CMOSRAM[TOWNS_CMOSRAM_FASTMODE_FLAG]=enabled ? 1 : 0;
	if(true==enabled)
	{
		towns_->state.mainRAMWait=0;
		towns_->state.VRAMWait=0;
	}
	else
	{
		towns_->state.mainRAMWait=6;
		towns_->state.VRAMWait=6;
	}
	towns_->AdjustMachineSpeedForMemoryWait();
}

void EmulatorController::setCdSpeed(int speed)
{
	if(!running_.load(std::memory_order_relaxed))
	{
		return;
	}
	speed=std::max(0,speed);
	TownsQtSettings::setCdSpeed(speed);
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
                                                 bool differential_mouse_integration,
                                                 int mouse_min_x,
                                                 int mouse_min_y,
                                                 int mouse_max_x,
                                                 int mouse_max_y)
{
	if(nullptr==towns_ || nullptr==impl_->outside_world)
	{
		return;
	}

		impl_->outside_world->differentialMouseIntegration=differential_mouse_integration;
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
		return impl_->outside_world->differentialMouseIntegration;
	}
	return TownsQtSettings::differentialMouseIntegration();
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
	result[QStringLiteral("tbios_version")]=static_cast<uint>(ow.debugTBIOSVersion);
	result[QStringLiteral("app_specific")]=static_cast<uint>(ow.debugAppSpecific);
	result[QStringLiteral("mos_work")]=static_cast<uint>(ow.debugMosWorkPhysAddr);
	result[QStringLiteral("tbios_off")]=static_cast<uint>(ow.debugTbiosMouseInfoOffset);
	result[QStringLiteral("snap_warmup")]=ow.debugSnapWarmupRemaining;
	return result;
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
