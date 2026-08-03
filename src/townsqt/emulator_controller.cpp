#include "emulator_controller.h"

#undef slots

#include "qt_command_thread.h"
#include "qt_outside_world.h"
#include "cdrom.h"
#include "towns.h"
#include "townsdef.h"
#include "qt_sync_sound.h"
#include "townsqt_paths.h"
#include "townsqt_model_profile.h"
#include "townsqt_settings.h"
#include "townsdef.h"
#include "townsthread.h"

#include "fssimplewindow_connection.h"
#include "cpputil.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

namespace
{
const QStringList &FdImageSuffixPriority()
{
	static const QStringList suffixes{
	    QStringLiteral("d77"),
	    QStringLiteral("d88"),
	    QStringLiteral("rdd"),
	    QStringLiteral("xdf"),
	    QStringLiteral("hdm"),
	    QStringLiteral("fdd")};
	return suffixes;
}

int FirstMediaTagPosition(const QString &stem)
{
	const int parenthesis=stem.indexOf(QLatin1Char('('));
	const int bracket=stem.indexOf(QLatin1Char('['));
	if(parenthesis<0)
	{
		return bracket;
	}
	if(bracket<0)
	{
		return parenthesis;
	}
	return std::min(parenthesis,bracket);
}

QString MediaTitle(const QString &stem)
{
	const int tag_pos=FirstMediaTagPosition(stem);
	return (tag_pos<0 ? stem : stem.left(tag_pos)).trimmed();
}

int MatchingFdStemRank(const QString &stem,const QString &title)
{
	if(!stem.startsWith(title,Qt::CaseInsensitive))
	{
		return -1;
	}
	const QString remainder=stem.mid(title.size()).trimmed();
	if(0==remainder.compare(QStringLiteral("(USER)"),Qt::CaseInsensitive) ||
	   0==remainder.compare(QStringLiteral("[USER]"),Qt::CaseInsensitive))
	{
		return 0;
	}
	if(remainder.isEmpty())
	{
		return 1;
	}
	if((remainder.startsWith(QLatin1Char('(')) && remainder.endsWith(QLatin1Char(')'))) ||
	   (remainder.startsWith(QLatin1Char('[')) && remainder.endsWith(QLatin1Char(']'))))
	{
		return 2;
	}
	return -1;
}

QString MatchingAutoFdImage(const QString &cd_path)
{
	const QFileInfo cd_info(cd_path);
	const QString cd_stem=cd_info.completeBaseName();
	if(cd_stem.isEmpty())
	{
		return {};
	}

	// First priority: an exact CD-basename match in TownsQt's blank-FD directory.
	const QFileInfoList blank_files=QDir(TownsQtPaths::blankFdDir()).entryInfoList(
	    QDir::Files|QDir::Readable,
	    QDir::Name|QDir::IgnoreCase);
	for(const QString &suffix : FdImageSuffixPriority())
	{
		for(const QFileInfo &file : blank_files)
		{
			if(0==file.completeBaseName().compare(cd_stem,Qt::CaseInsensitive) &&
			   0==file.suffix().compare(suffix,Qt::CaseInsensitive))
			{
				return file.absoluteFilePath();
			}
		}
	}

	// Fallback beside the CD image.  Tags are optional on either side:
	// Game(CD).cue, Game[CD].cue, or Game.cue can match
	// Game(USER).d77, Game[USER].d77, or Game.d77.
	const QString title=MediaTitle(cd_stem);
	if(title.isEmpty())
	{
		return {};
	}

	const QFileInfoList sibling_files=cd_info.dir().entryInfoList(
	    QDir::Files|QDir::Readable,
	    QDir::Name|QDir::IgnoreCase);
	for(const QString &suffix : FdImageSuffixPriority())
	{
		QString best_match;
		int best_rank=3;
		for(const QFileInfo &file : sibling_files)
		{
			if(0!=file.suffix().compare(suffix,Qt::CaseInsensitive))
			{
				continue;
			}
			if(file.absoluteFilePath()==cd_info.absoluteFilePath())
			{
				continue;
			}
			const QString fd_stem=file.completeBaseName();
			const int rank=MatchingFdStemRank(fd_stem,title);
			if(rank<0 || best_rank<=rank)
			{
				continue;
			}
			best_match=file.absoluteFilePath();
			best_rank=rank;
			if(0==rank)
			{
				break;
			}
		}
		if(!best_match.isEmpty())
		{
			return best_match;
		}
	}
	return {};
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
		applyPeripheralSettings(
		    TownsQtSettings::gamePort(0),
		    TownsQtSettings::gamePort(1),
		    TownsQtSettings::maxButtonHoldTimeMs(0,0),
		    TownsQtSettings::maxButtonHoldTimeMs(0,1),
		    TownsQtSettings::mouseIntegrationSpeed(),
		    TownsQtSettings::considerVRAMOffsetInMouseIntegration(),
		    TownsQtSettings::autoDifferentialOnMouseBIOSStop(),
		    TownsQtSettings::mouseMinX(),
		    TownsQtSettings::mouseMinY(),
		    TownsQtSettings::mouseMaxX(),
		    TownsQtSettings::mouseMaxY());
		applyCpuFastMode(TownsQtSettings::cpuFastModeEnabled());
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

		impl_->uiThread=std::thread(
		    &QtCommandThread::Run,&impl_->cmdThread,&impl_->townsThread,&towns,&argv_,impl_->outside_world);

		impl_->vmThread=std::thread([this,&towns]{
			impl_->townsThread.VMStart(&towns,impl_->outside_world,&impl_->cmdThread);
			impl_->townsThread.VMMainLoop(&towns,impl_->outside_world,impl_->sound,impl_->window,&impl_->cmdThread);
			impl_->townsThread.VMEnd(&towns,impl_->outside_world,&impl_->cmdThread);
			QMetaObject::invokeMethod(this,"onVmFinished",Qt::QueuedConnection);
		});

		const bool explicit_fd0=(""!=argv_.fdImgFName[0]);
		QString startup_cd_path=QString::fromStdString(argv_.cdImgFName);
		if(startup_cd_path.isEmpty())
		{
			const QString saved_cd=TownsQtSettings::lastCdImagePath();
			if(QFile::exists(saved_cd))
			{
				startup_cd_path=saved_cd;
			}
		}
		const QString startup_auto_fd0=
		    explicit_fd0 ? QString{} : MatchingAutoFdImage(startup_cd_path);

		if(""==argv_.cdImgFName)
		{
			const QString saved_cd=TownsQtSettings::lastCdImagePath();
			if(!saved_cd.isEmpty() && QFile::exists(saved_cd))
			{
				QTimer::singleShot(0,this,[this,saved_cd,explicit_fd0]{
					loadCdImageInternal(saved_cd,!explicit_fd0);
				});
			}
		}
		else if(!startup_auto_fd0.isEmpty())
		{
			QTimer::singleShot(0,this,[this,startup_auto_fd0]{
				loadFdImage(0,startup_auto_fd0);
			});
		}
		for(int drive=0; drive<2; ++drive)
		{
			if(""!=argv_.fdImgFName[drive])
			{
				continue;
			}
			if(0==drive && !startup_auto_fd0.isEmpty())
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

void EmulatorController::loadCdImage(const QString &path)
{
	loadCdImageInternal(path,true);
}

void EmulatorController::loadCdImageInternal(const QString &path,bool auto_mount_fd0)
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

	if(auto_mount_fd0)
	{
		const QString matching_fd=MatchingAutoFdImage(usePath);
		if(!matching_fd.isEmpty())
		{
			loadFdImage(0,matching_fd);
		}
	}
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
	if(TownsQtModelGroupIsMarty(TownsQtSettings::modelGroupIndex()))
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

	// Emulator not running yet: peek saved CMOS (default is dual-drive).
	QFile cmos_file(TownsQtPaths::cmosFilePath());
	if(!cmos_file.open(QIODevice::ReadOnly))
	{
		return true;
	}
	const QByteArray cmos=cmos_file.read(static_cast<int>(cmos_index)+1);
	if(cmos.size()<=static_cast<int>(cmos_index))
	{
		return true;
	}
	return 0==static_cast<unsigned char>(cmos.at(static_cast<int>(cmos_index)));
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
                                                 bool auto_differential_on_mouse_bios_stop,
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
		impl_->outside_world->autoDifferentialOnMouseBIOSStop=auto_differential_on_mouse_bios_stop;
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
	result[QStringLiteral("capture_released")]=ow.mouseCaptureReleased_;
	result[QStringLiteral("feeding")]=ow.mouseFeedingEnabled_;
	result[QStringLiteral("failsafe")]=ow.mouseFailsafeShowHostCursor_;
	result[QStringLiteral("pref_diff")]=ow.differentialMouseIntegration;
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
