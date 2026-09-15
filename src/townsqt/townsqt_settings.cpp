#include "townsqt_settings.h"

#include "townsdef.h"
#include "townsparam.h"
#include "townsqt_app_profile.h"
#include "townsqt_cpu_profile.h"
#include "townsqt_model_profile.h"
#include "townsqt_paths.h"
#include "townsqt_rom_availability.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>

namespace
{
constexpr char kLastCdImageKey[]="media/last_cd_image";
constexpr char kWorkingDirectoryKey[]="media/working_directory";
constexpr char kRecentCdImagesKey[]="media/recent_cd_images";
constexpr char kFd0ImageKey[]="media/fd0_image";
constexpr char kFd1ImageKey[]="media/fd1_image";
constexpr char kLegacyLastFdImageKey[]="media/last_fd_image";
constexpr char kRecentFdImagesKey[]="media/recent_fd_images";
constexpr char kFdWriteProtectKey[]="media/fd_write_protect";
constexpr char kDamperWireLineKey[]="display/damper_wire_line";
constexpr char kScanLine15KKey[]="display/scanline_15k";
constexpr char kDisplayScaleKey[]="display/scale";
constexpr char kSpriteTransferModeKey[]="sprite/transfer_mode";
constexpr char kCpuFrequencyMhzKey[]="cpu/frequency_mhz";
constexpr char kCpuCustomFrequencyMhzKey[]="cpu/custom_frequency_mhz";
constexpr char kCpuFastModeKey[]="cpu/fast_mode";
constexpr char kBootKeyCombKey[]="machine/boot_key";
constexpr char kCdSpeedKey[]="media/cd_speed";
constexpr char kMemSizeMbKey[]="machine/mem_size_mb";
constexpr char kCpuHighFidelityKey[]="cpu/high_fidelity";
constexpr char kPretend386DxKey[]="machine/pretend_386dx";
constexpr char kUseFpuKey[]="machine/use_fpu";
constexpr char kFastScsiKey[]="machine/fast_scsi";
constexpr char kFastFdKey[]="machine/fast_fd";
constexpr char kMidiBoardKey[]="machine/midi_board";
constexpr char kSingleDriveKey[]="machine/single_drive";
constexpr char kHighResCrtcKey[]="machine/high_res_crtc";
constexpr char kHighResPcmKey[]="machine/high_res_pcm";
constexpr char kMidiSoundFontKey[]="midi/soundfont";
constexpr char kMidiVolumePercentKey[]="midi/volume_percent";
constexpr char kMidiOutputKey[]="midi/output";
constexpr char kMidiAlsaPortKey[]="midi/alsa_port";
constexpr char kModelGroupKey[]="machine/model_group";
constexpr char kCpuKindKey[]="machine/cpu";
constexpr char kAutoScalingKey[]="display/auto_scaling";
constexpr char kMaintainAspectKey[]="display/maintain_aspect";
constexpr char kFullscreenVsyncKey[]="display/fullscreen_vsync";
constexpr char kWindowedVsyncKey[]="display/windowed_vsync";
constexpr char kPcmResampleHighQualityKey[]="audio/pcm_resample_hq";
constexpr char kAudioBackendKey[]="audio/backend";
constexpr char kAudioDeviceKey[]="audio/device";
constexpr char kFmVolumePercentKey[]="audio/fm_volume_percent";
constexpr char kPcmVolumePercentKey[]="audio/pcm_volume_percent";
constexpr char kCddaVolumePercentKey[]="audio/cdda_volume_percent";
constexpr char kPcmLpfEnabledKey[]="audio/pcm_lpf_enabled";
constexpr char kPcmLpfCutoffHzKey[]="audio/pcm_lpf_cutoff_hz";
constexpr char kWaylandIdleInhibitKey[]="function/wayland_idle_inhibit";
constexpr char kGamePort0Key[]="peripheral/gameport0";
constexpr char kGamePort1Key[]="peripheral/gameport1";
constexpr char kMaxButtonHoldMsKey[]="peripheral/max_button_hold_ms";
constexpr char kMouseIntegrationSpeedKey[]="peripheral/mouse_integration_speed";
constexpr char kMouseIntegrVramOffsetKey[]="peripheral/mouse_integr_vram_offset";
constexpr char kDifferentialMouseKey[]="peripheral/differential_mouse";
constexpr char kAutoDiffOnMosUnusedKey[]="peripheral/auto_diff_on_mos_unused";
constexpr char kUseDiscProfilesKey[]="function/use_disc_profiles";
constexpr char kAutoResumeKey[]="function/auto_resume";
constexpr char kStateDataCompressionKey[]="function/state_data_compression";
constexpr char kOpenContentBrowserOnStartupKey[]="function/open_content_browser_on_startup";
constexpr char kAutoDiffOnMosUnusedLegacyKey[]="peripheral/auto_diff_on_mouse_bios_stop";
constexpr char kMouseMinXKey[]="peripheral/mouse_min_x";
constexpr char kMouseMinYKey[]="peripheral/mouse_min_y";
constexpr char kMouseMaxXKey[]="peripheral/mouse_max_x";
constexpr char kMouseMaxYKey[]="peripheral/mouse_max_y";
constexpr char kAppSpecificKey[]="app/specific_setting";
constexpr char kMouseIntegrationDebugKey[]="debug/mouse_integration_coords";
constexpr char kMouseCoordWriteScanKey[]="debug/mouse_coord_write_scan";
constexpr char kCpuDebugKey[]="debug/cpu_cseip";
constexpr char kDriveAccessOverlayKey[]="display/drive_access_overlay";
constexpr char kFpsDisplayKey[]="display/fps_display";
constexpr char kMidiMonitorKey[]="debug/midi_monitor";
constexpr char kCdromMonitorKey[]="debug/cdrom_monitor";
constexpr char kAppMonitorKey[]="debug/app_monitor";
constexpr char kSnapMouseIntegrationKey[]="function/snap_mouse_integration";
constexpr char kSnapMouseWarmupFramesKey[]="function/snap_mouse_warmup_frames";
constexpr char kCddaCacheDuringDataReadKey[]="function/cdda_cache_during_data_read";
constexpr char kCddaCachePostReadGraceSecKey[]="function/cdda_cache_post_read_grace_sec";
constexpr char kHddEnabledKeyPrefix[]="hdd/";
constexpr char kHddPathKeySuffix[]="/path";
constexpr char kHddEnabledKeySuffix[]="/enabled";
constexpr char kSnapMouseIntegrationLegacyKey[]="debug/snap_mouse_integration";
constexpr char kSnapMouseWarmupFramesLegacyKey[]="debug/snap_mouse_warmup_frames";
constexpr int kSnapMouseWarmupFramesDefault=10;
constexpr int kCddaCachePostReadGraceSecDefault=1;
constexpr int kCpuFreqDefaultMhz=33;
constexpr int kCpuCustomFreqMinMhz=33;
constexpr int kCpuCustomFreqMaxMhz=60;
constexpr int kCpuCustomFreqDefaultMhz=33;
constexpr int kMemSizeDefaultMb=4;
constexpr int kChipVolumeMax=8192;
constexpr int kFmVolumePercentDefault=50;
constexpr int kPcmVolumePercentDefault=50;
constexpr int kCddaVolumePercentDefault=100;
constexpr int kPcmLpfCutoffHzDefault=8000;

int ClampVolumePercent(int percent)
{
	return std::clamp(percent,0,100);
}

int ChipVolumeFromPercent(int percent)
{
	percent=ClampVolumePercent(percent);
	return percent*kChipVolumeMax/100;
}

bool IsCpuFrequencyPresetMhz(int mhz)
{
	return 16==mhz || 20==mhz || 25==mhz;
}

int ClampCpuCustomFrequencyMhz(int mhz)
{
	return std::clamp(mhz,kCpuCustomFreqMinMhz,kCpuCustomFreqMaxMhz);
}

int ClampCpuFrequencyMhz(int mhz)
{
	if(true==IsCpuFrequencyPresetMhz(mhz))
	{
		return mhz;
	}
	return ClampCpuCustomFrequencyMhz(mhz);
}

int ClampMemSizeMb(int mb)
{
	return std::clamp(mb,1,64);
}

int ClampPcmLpfCutoffHz(int hz)
{
	return std::clamp(hz,200,20000);
}

int ParseSpriteTransferMode(const QString &text,int fallback)
{
	const QString normalized=text.trimmed().toLower();
	if("half"==normalized || "1"==normalized)
	{
		return 1;
	}
	if("unlimited"==normalized || "none"==normalized || "off"==normalized || "2"==normalized)
	{
		return 2;
	}
	if("auto"==normalized || "0"==normalized)
	{
		return 0;
	}
	return fallback;
}

int ClampMouseIntegrationSpeed(int speed)
{
	return std::clamp(speed,32,256);
}

unsigned int ParseGamePortEmu(const QString &text,unsigned int fallback)
{
	if(text.isEmpty())
	{
		return fallback;
	}
	const unsigned int emu=TownsStrToGamePortEmu(text.toStdString());
	if(TOWNS_GAMEPORTEMU_ERROR==emu)
	{
		return fallback;
	}
	return emu;
}

bool IsBlankFdDirectory(const QString &directory)
{
	if(directory.isEmpty())
	{
		return false;
	}
	const QString blank=TownsQtPaths::blankFdDir();
	const QString dir_canon=QFileInfo(directory).canonicalFilePath();
	const QString blank_canon=QFileInfo(blank).canonicalFilePath();
	if(!dir_canon.isEmpty() && !blank_canon.isEmpty())
	{
		return dir_canon==blank_canon;
	}
	return QDir(directory).absolutePath()==QDir(blank).absolutePath();
}

QString FdImageIniKey(int drive)
{
	drive=std::clamp(drive,0,1);
	return QString::fromLatin1(0==drive ? kFd0ImageKey : kFd1ImageKey);
}

QString LegacyFdImageIniKey(int drive)
{
	return QString::fromLatin1(kLegacyLastFdImageKey)+QString::number(drive);
}
}

int TownsQtSettings::spriteTransferMode()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kSpriteTransferModeKey)))
	{
		setSpriteTransferMode(0);
		return 0;
	}
	const QString raw=settings.value(QString::fromLatin1(kSpriteTransferModeKey)).toString();
	return ParseSpriteTransferMode(raw,0);
}

void TownsQtSettings::setSpriteTransferMode(int mode)
{
	mode=std::clamp(mode,0,2);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const char *value="auto";
	if(1==mode)
	{
		value="half";
	}
	else if(2==mode)
	{
		value="unlimited";
	}
	settings.setValue(QString::fromLatin1(kSpriteTransferModeKey),QString::fromLatin1(value));
	settings.sync();
}

int TownsQtSettings::cpuFrequencyMhz()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kCpuFrequencyMhzKey)))
	{
		setCpuFrequencyMhz(kCpuFreqDefaultMhz);
		return kCpuFreqDefaultMhz;
	}
	const int mhz=settings.value(QString::fromLatin1(kCpuFrequencyMhzKey),kCpuFreqDefaultMhz).toInt();
	return ClampCpuFrequencyMhz(mhz);
}

void TownsQtSettings::setCpuFrequencyMhz(int mhz)
{
	mhz=ClampCpuFrequencyMhz(mhz);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCpuFrequencyMhzKey),mhz);
	settings.sync();
}

int TownsQtSettings::cpuCustomFrequencyMhz()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=QString::fromLatin1(kCpuCustomFrequencyMhzKey);
	if(!settings.contains(key))
	{
		const int active=settings.value(
		    QString::fromLatin1(kCpuFrequencyMhzKey),
		    kCpuCustomFreqDefaultMhz).toInt();
		const int seeded=
		    IsCpuFrequencyPresetMhz(active) ? kCpuCustomFreqDefaultMhz : ClampCpuCustomFrequencyMhz(active);
		setCpuCustomFrequencyMhz(seeded);
		return seeded;
	}
	return ClampCpuCustomFrequencyMhz(settings.value(key,kCpuCustomFreqDefaultMhz).toInt());
}

void TownsQtSettings::setCpuCustomFrequencyMhz(int mhz)
{
	mhz=ClampCpuCustomFrequencyMhz(mhz);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCpuCustomFrequencyMhzKey),mhz);
	settings.sync();
}

bool TownsQtSettings::cpuFastModeEnabled()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kCpuFastModeKey),true).toBool();
}

void TownsQtSettings::setCpuFastModeEnabled(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCpuFastModeKey),enabled);
	settings.sync();
}

unsigned int TownsQtSettings::bootKeyComb()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString stored=settings.value(QString::fromLatin1(kBootKeyCombKey),QStringLiteral("CD")).toString();
	const unsigned int key=TownsStrToKeyComb(stored.toStdString());
	if(BOOT_KEYCOMB_CD==key || BOOT_KEYCOMB_F0==key || BOOT_KEYCOMB_F1==key || BOOT_KEYCOMB_H0==key)
	{
		return key;
	}
	return BOOT_KEYCOMB_CD;
}

void TownsQtSettings::setBootKeyComb(unsigned int keyComb)
{
	if(BOOT_KEYCOMB_CD!=keyComb && BOOT_KEYCOMB_F0!=keyComb &&
	   BOOT_KEYCOMB_F1!=keyComb && BOOT_KEYCOMB_H0!=keyComb)
	{
		keyComb=BOOT_KEYCOMB_CD;
	}
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kBootKeyCombKey),QString::fromStdString(TownsKeyCombToStr(keyComb)));
	settings.sync();
}

int TownsQtSettings::cdSpeed()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=QString::fromLatin1(kCdSpeedKey);
	if(!settings.contains(key))
	{
		const int speed=TownsQtCpuKindDefaultCdSpeed(cpuKind());
		setCdSpeed(speed);
		return speed;
	}
	const int speed=settings.value(key,0).toInt();
	return std::max(0,speed);
}

void TownsQtSettings::setCdSpeed(int speed)
{
	speed=std::max(0,speed);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCdSpeedKey),speed);
	settings.sync();
}

int TownsQtSettings::memSizeInMB()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMemSizeMbKey)))
	{
		setMemSizeInMB(kMemSizeDefaultMb);
		return kMemSizeDefaultMb;
	}
	return ClampMemSizeMb(settings.value(QString::fromLatin1(kMemSizeMbKey),kMemSizeDefaultMb).toInt());
}

void TownsQtSettings::setMemSizeInMB(int mb)
{
	mb=ClampMemSizeMb(mb);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMemSizeMbKey),mb);
	settings.sync();
}

bool TownsQtSettings::cpuHighFidelity()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kCpuHighFidelityKey)))
	{
		setCpuHighFidelity(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kCpuHighFidelityKey),false).toBool();
}

void TownsQtSettings::setCpuHighFidelity(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCpuHighFidelityKey),enabled);
	settings.sync();
}

bool TownsQtSettings::pretend386DX()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kPretend386DxKey)))
	{
		setPretend386DX(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kPretend386DxKey),false).toBool();
}

void TownsQtSettings::setPretend386DX(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kPretend386DxKey),enabled);
	settings.sync();
}

bool TownsQtSettings::useFPU()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kUseFpuKey)))
	{
		setUseFPU(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kUseFpuKey),false).toBool();
}

void TownsQtSettings::setUseFPU(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kUseFpuKey),enabled);
	settings.sync();
}

bool TownsQtSettings::fastScsi()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kFastScsiKey)))
	{
		setFastScsi(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kFastScsiKey),false).toBool();
}

void TownsQtSettings::setFastScsi(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kFastScsiKey),enabled);
	settings.sync();
}

bool TownsQtSettings::fastFd()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kFastFdKey)))
	{
		setFastFd(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kFastFdKey),false).toBool();
}

void TownsQtSettings::setFastFd(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kFastFdKey),enabled);
	settings.sync();
}

bool TownsQtSettings::midiBoard()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMidiBoardKey)))
	{
		setMidiBoard(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kMidiBoardKey),false).toBool();
}

void TownsQtSettings::setMidiBoard(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMidiBoardKey),enabled);
	settings.sync();
}

bool TownsQtSettings::singleDrive()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kSingleDriveKey)))
	{
		setSingleDrive(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kSingleDriveKey),false).toBool();
}

void TownsQtSettings::setSingleDrive(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kSingleDriveKey),enabled);
	settings.sync();
}

bool TownsQtSettings::highResCrtc()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kHighResCrtcKey)))
	{
		const bool def=TownsQtRomAvailability::SysRomSupportsHighResCrtc(TownsQtPaths::romsDir());
		setHighResCrtc(def);
		return def;
	}
	return settings.value(QString::fromLatin1(kHighResCrtcKey),true).toBool();
}

void TownsQtSettings::setHighResCrtc(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kHighResCrtcKey),enabled);
	settings.sync();
}

bool TownsQtSettings::highResPcm()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kHighResPcmKey)))
	{
		setHighResPcm(true);
		return true;
	}
	return settings.value(QString::fromLatin1(kHighResPcmKey),true).toBool();
}

void TownsQtSettings::setHighResPcm(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kHighResPcmKey),enabled);
	settings.sync();
}

QString TownsQtSettings::midiSoundFont()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kMidiSoundFontKey)).toString();
}

void TownsQtSettings::setMidiSoundFont(const QString &path)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMidiSoundFontKey),path);
	settings.sync();
}

int TownsQtSettings::midiVolumePercent()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return std::clamp(settings.value(QString::fromLatin1(kMidiVolumePercentKey),100).toInt(),0,100);
}

void TownsQtSettings::setMidiVolumePercent(int percent)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMidiVolumePercentKey),std::clamp(percent,0,100));
	settings.sync();
}

QString TownsQtSettings::midiOutput()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString raw=settings.value(QString::fromLatin1(kMidiOutputKey)).toString().trimmed();
	if(raw.isEmpty() || raw==QStringLiteral("fluidsynth") || raw==QStringLiteral("alsa"))
	{
		return raw;
	}
	// Legacy: destination stored directly in midi/output.
	return QStringLiteral("alsa");
}

void TownsQtSettings::setMidiOutput(const QString &output)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	QString normalized=output.trimmed();
	if(normalized!=QStringLiteral("fluidsynth") && normalized!=QStringLiteral("alsa"))
	{
		if(normalized.contains(QLatin1Char(':')))
		{
			settings.setValue(QString::fromLatin1(kMidiAlsaPortKey),normalized);
			normalized=QStringLiteral("alsa");
		}
		else
		{
			normalized.clear();
		}
	}
	settings.setValue(QString::fromLatin1(kMidiOutputKey),normalized);
	settings.sync();
}

QString TownsQtSettings::midiAlsaPort()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(settings.contains(QString::fromLatin1(kMidiAlsaPortKey)))
	{
		return settings.value(QString::fromLatin1(kMidiAlsaPortKey)).toString();
	}
	const QString raw=settings.value(QString::fromLatin1(kMidiOutputKey)).toString().trimmed();
	if(raw.contains(QLatin1Char(':')) &&
	   raw!=QStringLiteral("fluidsynth") &&
	   raw!=QStringLiteral("alsa"))
	{
		return raw;
	}
	return QString();
}

void TownsQtSettings::setMidiAlsaPort(const QString &port)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMidiAlsaPortKey),port.trimmed());
	settings.sync();
}

int TownsQtSettings::modelGroupIndex()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(settings.contains(QString::fromLatin1(kModelGroupKey)))
	{
		return TownsQtModelGroupIndexForId(
		    settings.value(QString::fromLatin1(kModelGroupKey)).toString());
	}
	// Derive from CPU + SYS-ROM when model_group is absent.
	const QString rom_dir=TownsQtPaths::romsDir();
	return TownsQtModelGroupPreferredForCpuAndSysRom(
	    cpuKind(),
	    TownsQtRomAvailability::ClassifySysRom(rom_dir),
	    TownsQtRomAvailability::SysRomTownsOsLevel(rom_dir),
	    TownsQtRomAvailability::MartyExRomPresent(rom_dir));
}

void TownsQtSettings::setModelGroupIndex(int index)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(
	    QString::fromLatin1(kModelGroupKey),
	    TownsQtModelGroupId(index));
	settings.sync();
	const TownsQtCpuKind kind=TownsQtCpuKindFromTownsType(TownsQtModelGroupTownsType(index));
	settings.setValue(QString::fromLatin1(kCpuKindKey),QString::fromLatin1(TownsQtCpuKindId(kind)));
	settings.sync();
}

TownsQtCpuKind TownsQtSettings::cpuKind()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(settings.contains(QString::fromLatin1(kCpuKindKey)))
	{
		return TownsQtCpuKindFromId(settings.value(QString::fromLatin1(kCpuKindKey)).toString());
	}
	// Migrate legacy model_group → cpu.
	if(settings.contains(QString::fromLatin1(kModelGroupKey)))
	{
		const QString id=settings.value(QString::fromLatin1(kModelGroupKey)).toString();
		const TownsQtCpuKind kind=TownsQtCpuKindFromTownsType(
		    TownsQtModelGroupTownsType(TownsQtModelGroupIndexForId(id)));
		settings.setValue(QString::fromLatin1(kCpuKindKey),QString::fromLatin1(TownsQtCpuKindId(kind)));
		settings.sync();
		return kind;
	}
	settings.setValue(
	    QString::fromLatin1(kCpuKindKey),
	    QString::fromLatin1(TownsQtCpuKindId(TownsQtCpuKindDefault())));
	settings.sync();
	return TownsQtCpuKindDefault();
}

void TownsQtSettings::setCpuKind(TownsQtCpuKind kind)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCpuKindKey),QString::fromLatin1(TownsQtCpuKindId(kind)));
	settings.sync();
	const QString rom_dir=TownsQtPaths::romsDir();
	const int model=TownsQtModelGroupClampToAllowed(
	    modelGroupIndex(),
	    kind,
	    TownsQtRomAvailability::ClassifySysRom(rom_dir),
	    TownsQtRomAvailability::SysRomTownsOsLevel(rom_dir),
	    TownsQtRomAvailability::MartyExRomPresent(rom_dir));
	settings.setValue(QString::fromLatin1(kModelGroupKey),TownsQtModelGroupId(model));
	settings.sync();
}

unsigned int TownsQtSettings::townsType()
{
	return TownsQtModelGroupTownsType(modelGroupIndex());
}

void TownsQtSettings::setTownsType(unsigned int towns_type)
{
	setModelGroupIndex(TownsQtModelGroupIndexForTownsType(towns_type));
}

bool TownsQtSettings::autoScaling()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kAutoScalingKey)))
	{
		setAutoScaling(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kAutoScalingKey),false).toBool();
}

void TownsQtSettings::setAutoScaling(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAutoScalingKey),enabled);
	settings.sync();
}

bool TownsQtSettings::maintainAspect()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMaintainAspectKey)))
	{
		setMaintainAspect(true);
		return true;
	}
	return settings.value(QString::fromLatin1(kMaintainAspectKey),true).toBool();
}

void TownsQtSettings::setMaintainAspect(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMaintainAspectKey),enabled);
	settings.sync();
}

bool TownsQtSettings::fullscreenVsync()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kFullscreenVsyncKey)))
	{
		setFullscreenVsync(true);
		return true;
	}
	return settings.value(QString::fromLatin1(kFullscreenVsyncKey),true).toBool();
}

void TownsQtSettings::setFullscreenVsync(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kFullscreenVsyncKey),enabled);
	settings.sync();
}

bool TownsQtSettings::windowedVsync()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kWindowedVsyncKey),false).toBool();
}

void TownsQtSettings::setWindowedVsync(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kWindowedVsyncKey),enabled);
	settings.sync();
}

bool TownsQtSettings::pcmResampleHighQuality()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kPcmResampleHighQualityKey),false).toBool();
}

void TownsQtSettings::setPcmResampleHighQuality(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kPcmResampleHighQualityKey),enabled);
	settings.sync();
}

QString TownsQtSettings::audioBackend()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kAudioBackendKey)).toString();
}

void TownsQtSettings::setAudioBackend(const QString &backend)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAudioBackendKey),backend);
	settings.sync();
}

QString TownsQtSettings::audioDevice()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kAudioDeviceKey)).toString();
}

void TownsQtSettings::setAudioDevice(const QString &device)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAudioDeviceKey),device);
	settings.sync();
}

int TownsQtSettings::fmVolumePercent()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kFmVolumePercentKey)))
	{
		setFmVolumePercent(kFmVolumePercentDefault);
		return kFmVolumePercentDefault;
	}
	return ClampVolumePercent(settings.value(QString::fromLatin1(kFmVolumePercentKey),kFmVolumePercentDefault).toInt());
}

void TownsQtSettings::setFmVolumePercent(int percent)
{
	percent=ClampVolumePercent(percent);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kFmVolumePercentKey),percent);
	settings.sync();
}

int TownsQtSettings::pcmVolumePercent()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kPcmVolumePercentKey)))
	{
		setPcmVolumePercent(kPcmVolumePercentDefault);
		return kPcmVolumePercentDefault;
	}
	return ClampVolumePercent(settings.value(QString::fromLatin1(kPcmVolumePercentKey),kPcmVolumePercentDefault).toInt());
}

void TownsQtSettings::setPcmVolumePercent(int percent)
{
	percent=ClampVolumePercent(percent);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kPcmVolumePercentKey),percent);
	settings.sync();
}

int TownsQtSettings::cddaVolumePercent()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kCddaVolumePercentKey)))
	{
		setCddaVolumePercent(kCddaVolumePercentDefault);
		return kCddaVolumePercentDefault;
	}
	return ClampVolumePercent(settings.value(QString::fromLatin1(kCddaVolumePercentKey),kCddaVolumePercentDefault).toInt());
}

void TownsQtSettings::setCddaVolumePercent(int percent)
{
	percent=ClampVolumePercent(percent);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCddaVolumePercentKey),percent);
	settings.sync();
}

int TownsQtSettings::fmChipVolume()
{
	return ChipVolumeFromPercent(fmVolumePercent());
}

int TownsQtSettings::pcmChipVolume()
{
	return ChipVolumeFromPercent(pcmVolumePercent());
}

bool TownsQtSettings::pcmLpfEnabled()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kPcmLpfEnabledKey),false).toBool();
}

void TownsQtSettings::setPcmLpfEnabled(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kPcmLpfEnabledKey),enabled);
	settings.sync();
}

int TownsQtSettings::pcmLpfCutoffHz()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kPcmLpfCutoffHzKey)))
	{
		setPcmLpfCutoffHz(kPcmLpfCutoffHzDefault);
		return kPcmLpfCutoffHzDefault;
	}
	return ClampPcmLpfCutoffHz(settings.value(QString::fromLatin1(kPcmLpfCutoffHzKey),kPcmLpfCutoffHzDefault).toInt());
}

void TownsQtSettings::setPcmLpfCutoffHz(int hz)
{
	hz=ClampPcmLpfCutoffHz(hz);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kPcmLpfCutoffHzKey),hz);
	settings.sync();
}

bool TownsQtSettings::waylandIdleInhibit()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kWaylandIdleInhibitKey)))
	{
		setWaylandIdleInhibit(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kWaylandIdleInhibitKey),false).toBool();
}

void TownsQtSettings::setWaylandIdleInhibit(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kWaylandIdleInhibitKey),enabled);
	settings.sync();
}

int TownsQtSettings::displayScale()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kDisplayScaleKey)))
	{
		setDisplayScale(1);
		return 1;
	}
	const int scale=settings.value(QString::fromLatin1(kDisplayScaleKey),1).toInt();
	return std::clamp(scale,1,8);
}

void TownsQtSettings::setDisplayScale(int scale)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kDisplayScaleKey),std::clamp(scale,1,8));
	settings.sync();
}

int TownsQtSettings::maxDisplayScaleForAvailableSize(QSize available_size,int chrome_w,int chrome_h)
{
	chrome_w=std::max(0,chrome_w);
	chrome_h=std::max(0,chrome_h);
	const int avail_w=std::max(0,available_size.width());
	const int avail_h=std::max(0,available_size.height());
	int max_scale=1;
	for(int scale=1; scale<=8; ++scale)
	{
		const int need_w=640*scale+chrome_w;
		const int need_h=480*scale+chrome_h;
		if(need_w<=avail_w && need_h<=avail_h)
		{
			max_scale=scale;
		}
		else
		{
			break;
		}
	}
	return max_scale;
}

bool TownsQtSettings::damperWireLine()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kDamperWireLineKey)))
	{
		setDamperWireLine(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kDamperWireLineKey),false).toBool();
}

void TownsQtSettings::setDamperWireLine(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kDamperWireLineKey),enabled);
	settings.sync();
}

bool TownsQtSettings::scanLineEffectIn15KHz()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kScanLine15KKey)))
	{
		setScanLineEffectIn15KHz(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kScanLine15KKey),false).toBool();
}

void TownsQtSettings::setScanLineEffectIn15KHz(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kScanLine15KKey),enabled);
	settings.sync();
}

QString TownsQtSettings::lastCdImagePath()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kLastCdImageKey)).toString();
}

void TownsQtSettings::setLastCdImagePath(const QString &path)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(path.isEmpty())
	{
		settings.remove(QString::fromLatin1(kLastCdImageKey));
	}
	else
	{
		const QString canonical=QFileInfo(path).canonicalFilePath();
		const QString use_path=canonical.isEmpty() ? path : canonical;
		settings.setValue(QString::fromLatin1(kLastCdImageKey),use_path);
		setWorkingDirectory(QFileInfo(use_path).absolutePath());
	}
	settings.sync();
}

void TownsQtSettings::clearLastCdImagePath()
{
	setLastCdImagePath(QString());
}

QString TownsQtSettings::workingDirectory()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString directory=
	    settings.value(QString::fromLatin1(kWorkingDirectoryKey)).toString();
	if(QFileInfo(directory).isDir() && !IsBlankFdDirectory(directory))
	{
		return directory;
	}

	// Compatibility with configurations created before working_directory existed.
	const QString last_cd=
	    settings.value(QString::fromLatin1(kLastCdImageKey)).toString();
	if(last_cd.isEmpty())
	{
		return {};
	}
	const QString last_cd_directory=QFileInfo(last_cd).absolutePath();
	if(IsBlankFdDirectory(last_cd_directory))
	{
		return {};
	}
	return QFileInfo(last_cd_directory).isDir() ? last_cd_directory : QString{};
}

void TownsQtSettings::setWorkingDirectory(const QString &directory)
{
	if(directory.isEmpty() || !QFileInfo(directory).isDir())
	{
		return;
	}
	// Shared CD/FD working directory must not track blank_fd/.
	if(IsBlankFdDirectory(directory))
	{
		return;
	}
	const QString canonical=QFileInfo(directory).canonicalFilePath();
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(
	    QString::fromLatin1(kWorkingDirectoryKey),
	    canonical.isEmpty() ? QFileInfo(directory).absoluteFilePath() : canonical);
	settings.sync();
}

QString TownsQtSettings::fileDialogStartDirectory(const QString &fallback)
{
	const QString working=workingDirectory();
	if(!working.isEmpty())
	{
		return working;
	}
	if(!fallback.isEmpty())
	{
		if(QFileInfo(fallback).isDir())
		{
			return QFileInfo(fallback).absoluteFilePath();
		}
		const QString directory=QFileInfo(fallback).absolutePath();
		if(QFileInfo(directory).isDir())
		{
			return directory;
		}
	}
	return TownsQtPaths::configDir();
}

void TownsQtSettings::rememberFileDialogPath(const QString &path)
{
	if(path.isEmpty())
	{
		return;
	}
	setWorkingDirectory(QFileInfo(path).absolutePath());
}

QStringList TownsQtSettings::recentCdImagePaths()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	QStringList list=settings.value(QString::fromLatin1(kRecentCdImagesKey)).toStringList();
	QStringList filtered;
	for(const QString &path : list)
	{
		if(!path.isEmpty())
		{
			filtered.push_back(path);
		}
		if(kRecentCdImageHistoryMax<=filtered.size())
		{
			break;
		}
	}
	return filtered;
}

void TownsQtSettings::addRecentCdImagePath(const QString &path)
{
	if(path.isEmpty())
	{
		return;
	}
	const QString canonical=QFileInfo(path).canonicalFilePath();
	const QString usePath=canonical.isEmpty() ? path : canonical;
	QStringList list=recentCdImagePaths();
	list.removeAll(usePath);
	list.prepend(usePath);
	while(kRecentCdImageHistoryMax<list.size())
	{
		list.removeLast();
	}
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kRecentCdImagesKey),list);
	settings.sync();
}

void TownsQtSettings::clearRecentCdImagePaths()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.remove(QString::fromLatin1(kRecentCdImagesKey));
	settings.sync();
}

QString TownsQtSettings::lastFdImagePath(int drive)
{
	drive=std::clamp(drive,0,1);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=FdImageIniKey(drive);
	QString path=settings.value(key).toString();
	if(path.isEmpty())
	{
		path=settings.value(LegacyFdImageIniKey(drive)).toString();
		if(true!=path.isEmpty())
		{
			settings.setValue(key,path);
			settings.remove(LegacyFdImageIniKey(drive));
			settings.sync();
		}
	}
	return path;
}

void TownsQtSettings::setLastFdImagePath(int drive,const QString &path)
{
	drive=std::clamp(drive,0,1);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=FdImageIniKey(drive);
	if(path.isEmpty())
	{
		settings.remove(key);
		settings.remove(LegacyFdImageIniKey(drive));
	}
	else
	{
		const QString canonical=QFileInfo(path).canonicalFilePath();
		const QString use_path=canonical.isEmpty() ? path : canonical;
		settings.setValue(key,use_path);
		settings.remove(LegacyFdImageIniKey(drive));
		setWorkingDirectory(QFileInfo(use_path).absolutePath());
	}
	settings.sync();
}

void TownsQtSettings::clearLastFdImagePath(int drive)
{
	setLastFdImagePath(drive,QString());
}

QStringList TownsQtSettings::recentFdImagePaths()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	QStringList list=settings.value(QString::fromLatin1(kRecentFdImagesKey)).toStringList();

	// Migrate older per-drive recent lists into the shared list once.
	if(list.isEmpty())
	{
		for(int drive=0; drive<2; ++drive)
		{
			const QString legacy_key=
			    QString::fromLatin1(kRecentFdImagesKey)+QString::number(drive);
			const QStringList legacy=settings.value(legacy_key).toStringList();
			for(const QString &path : legacy)
			{
				if(!path.isEmpty() && !list.contains(path))
				{
					list.push_back(path);
				}
			}
		}
		if(!list.isEmpty())
		{
			while(kRecentFdImageHistoryMax<list.size())
			{
				list.removeLast();
			}
			settings.setValue(QString::fromLatin1(kRecentFdImagesKey),list);
			settings.remove(QString::fromLatin1(kRecentFdImagesKey)+QStringLiteral("0"));
			settings.remove(QString::fromLatin1(kRecentFdImagesKey)+QStringLiteral("1"));
			settings.sync();
		}
	}

	QStringList filtered;
	for(const QString &path : list)
	{
		if(!path.isEmpty())
		{
			filtered.push_back(path);
		}
		if(kRecentFdImageHistoryMax<=filtered.size())
		{
			break;
		}
	}
	return filtered;
}

void TownsQtSettings::addRecentFdImagePath(int drive,const QString &path)
{
	drive=std::clamp(drive,0,1);
	if(path.isEmpty())
	{
		return;
	}
	const QString canonical=QFileInfo(path).canonicalFilePath();
	const QString usePath=canonical.isEmpty() ? path : canonical;
	QStringList list=recentFdImagePaths();
	list.removeAll(usePath);
	list.prepend(usePath);
	while(kRecentFdImageHistoryMax<list.size())
	{
		list.removeLast();
	}
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kRecentFdImagesKey),list);
	settings.sync();
}

void TownsQtSettings::clearRecentFdImagePaths()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.remove(QString::fromLatin1(kRecentFdImagesKey));
	settings.remove(QString::fromLatin1(kRecentFdImagesKey)+QStringLiteral("0"));
	settings.remove(QString::fromLatin1(kRecentFdImagesKey)+QStringLiteral("1"));
	settings.sync();
}

bool TownsQtSettings::fdWriteProtect(int drive)
{
	drive=std::clamp(drive,0,1);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=QString::fromLatin1(kFdWriteProtectKey)+QString::number(drive);
	return settings.value(key,false).toBool();
}

void TownsQtSettings::setFdWriteProtect(int drive,bool enabled)
{
	drive=std::clamp(drive,0,1);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=QString::fromLatin1(kFdWriteProtectKey)+QString::number(drive);
	settings.setValue(key,enabled);
	settings.sync();
}

unsigned int TownsQtSettings::gamePort(int port)
{
	port=std::clamp(port,0,1);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const char *key=(0==port) ? kGamePort0Key : kGamePort1Key;
	const unsigned int fallback=(0==port) ? TOWNS_GAMEPORTEMU_PHYSICAL0 : TOWNS_GAMEPORTEMU_MOUSE;
	if(!settings.contains(QString::fromLatin1(key)))
	{
		setGamePort(port,fallback);
		return fallback;
	}
	return ParseGamePortEmu(settings.value(QString::fromLatin1(key)).toString(),fallback);
}

void TownsQtSettings::setGamePort(int port,unsigned int emu)
{
	port=std::clamp(port,0,1);
	if(TOWNS_GAMEPORTEMU_ERROR==emu || TOWNS_GAMEPORTEMU_NUM_DEVICES<=emu)
	{
		emu=TOWNS_GAMEPORTEMU_NONE;
	}
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const char *key=(0==port) ? kGamePort0Key : kGamePort1Key;
	settings.setValue(QString::fromLatin1(key),QString::fromStdString(TownsGamePortEmuToStr(emu)));
	settings.sync();
}

int TownsQtSettings::maxButtonHoldTimeMs(int port,int button)
{
	port=std::clamp(port,0,1);
	button=std::clamp(button,0,1);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=QString::fromLatin1("%1/%2/%3").arg(kMaxButtonHoldMsKey).arg(port).arg(button);
	if(!settings.contains(key))
	{
		setMaxButtonHoldTimeMs(port,button,0);
		return 0;
	}
	return std::max(0,settings.value(key,0).toInt());
}

void TownsQtSettings::setMaxButtonHoldTimeMs(int port,int button,int ms)
{
	port=std::clamp(port,0,1);
	button=std::clamp(button,0,1);
	ms=std::max(0,ms);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	const QString key=QString::fromLatin1("%1/%2/%3").arg(kMaxButtonHoldMsKey).arg(port).arg(button);
	settings.setValue(key,ms);
	settings.sync();
}

int TownsQtSettings::mouseIntegrationSpeed()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMouseIntegrationSpeedKey)))
	{
		setMouseIntegrationSpeed(256);
		return 256;
	}
	return ClampMouseIntegrationSpeed(settings.value(QString::fromLatin1(kMouseIntegrationSpeedKey),256).toInt());
}

void TownsQtSettings::setMouseIntegrationSpeed(int speed)
{
	speed=ClampMouseIntegrationSpeed(speed);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseIntegrationSpeedKey),speed);
	settings.sync();
}

bool TownsQtSettings::considerVRAMOffsetInMouseIntegration()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMouseIntegrVramOffsetKey)))
	{
		setConsiderVRAMOffsetInMouseIntegration(true);
		return true;
	}
	return settings.value(QString::fromLatin1(kMouseIntegrVramOffsetKey),true).toBool();
}

void TownsQtSettings::setConsiderVRAMOffsetInMouseIntegration(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseIntegrVramOffsetKey),enabled);
	settings.sync();
}

bool TownsQtSettings::differentialMouseIntegration()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kDifferentialMouseKey)))
	{
		setDifferentialMouseIntegration(false);
		return false;
	}
	return settings.value(QString::fromLatin1(kDifferentialMouseKey),false).toBool();
}

void TownsQtSettings::setDifferentialMouseIntegration(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kDifferentialMouseKey),enabled);
	settings.sync();
}

bool TownsQtSettings::autoDifferentialOnMosUnused()
{
	// Retired: Default keeps MOS while Mouse BIOS is alive (no unused→capture).
	return false;
}

void TownsQtSettings::setAutoDifferentialOnMosUnused(bool enabled)
{
	Q_UNUSED(enabled);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAutoDiffOnMosUnusedKey),false);
	settings.sync();
}

int TownsQtSettings::mouseMinX()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMouseMinXKey)))
	{
		setMouseMinX(TownsStartParameters::DEFAULT_MOUSE_MINX);
		return TownsStartParameters::DEFAULT_MOUSE_MINX;
	}
	return settings.value(QString::fromLatin1(kMouseMinXKey),TownsStartParameters::DEFAULT_MOUSE_MINX).toInt();
}

void TownsQtSettings::setMouseMinX(int value)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseMinXKey),value);
	settings.sync();
}

int TownsQtSettings::mouseMinY()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMouseMinYKey)))
	{
		setMouseMinY(TownsStartParameters::DEFAULT_MOUSE_MINY);
		return TownsStartParameters::DEFAULT_MOUSE_MINY;
	}
	return settings.value(QString::fromLatin1(kMouseMinYKey),TownsStartParameters::DEFAULT_MOUSE_MINY).toInt();
}

void TownsQtSettings::setMouseMinY(int value)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseMinYKey),value);
	settings.sync();
}

int TownsQtSettings::mouseMaxX()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMouseMaxXKey)))
	{
		setMouseMaxX(TownsStartParameters::DEFAULT_MOUSE_MAXX);
		return TownsStartParameters::DEFAULT_MOUSE_MAXX;
	}
	return settings.value(QString::fromLatin1(kMouseMaxXKey),TownsStartParameters::DEFAULT_MOUSE_MAXX).toInt();
}

void TownsQtSettings::setMouseMaxX(int value)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseMaxXKey),value);
	settings.sync();
}

int TownsQtSettings::mouseMaxY()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kMouseMaxYKey)))
	{
		setMouseMaxY(TownsStartParameters::DEFAULT_MOUSE_MAXY);
		return TownsStartParameters::DEFAULT_MOUSE_MAXY;
	}
	return settings.value(QString::fromLatin1(kMouseMaxYKey),TownsStartParameters::DEFAULT_MOUSE_MAXY).toInt();
}

void TownsQtSettings::setMouseMaxY(int value)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseMaxYKey),value);
	settings.sync();
}

unsigned int TownsQtSettings::appSpecificSetting()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kAppSpecificKey)))
	{
		setAppSpecificSetting(TOWNS_APPSPECIFIC_NONE);
		return TOWNS_APPSPECIFIC_NONE;
	}
	const QString id=settings.value(QString::fromLatin1(kAppSpecificKey)).toString();
	return TownsQtAppProfileApp(TownsQtAppProfileIndexForId(id));
}

void TownsQtSettings::setAppSpecificSetting(unsigned int app_value)
{
	const int index=TownsQtAppProfileIndexForApp(app_value);
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAppSpecificKey),TownsQtAppProfileId(index));
	settings.sync();
}

bool TownsQtSettings::showMouseIntegrationDebug()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kMouseIntegrationDebugKey),false).toBool();
}

void TownsQtSettings::setShowMouseIntegrationDebug(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseIntegrationDebugKey),enabled);
	settings.sync();
}

bool TownsQtSettings::showMouseCoordWriteScan()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kMouseCoordWriteScanKey),false).toBool();
}

void TownsQtSettings::setShowMouseCoordWriteScan(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMouseCoordWriteScanKey),enabled);
	settings.sync();
}

bool TownsQtSettings::showCpuDebug()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kCpuDebugKey),false).toBool();
}

void TownsQtSettings::setShowCpuDebug(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCpuDebugKey),enabled);
	settings.sync();
}

bool TownsQtSettings::showDriveAccessOverlay()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kDriveAccessOverlayKey)))
	{
		return true;
	}
	return settings.value(QString::fromLatin1(kDriveAccessOverlayKey),true).toBool();
}

void TownsQtSettings::setShowDriveAccessOverlay(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kDriveAccessOverlayKey),enabled);
	settings.sync();
}

bool TownsQtSettings::showFpsDisplay()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(!settings.contains(QString::fromLatin1(kFpsDisplayKey)))
	{
		return true;
	}
	return settings.value(QString::fromLatin1(kFpsDisplayKey),true).toBool();
}

void TownsQtSettings::setShowFpsDisplay(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kFpsDisplayKey),enabled);
	settings.sync();
}

bool TownsQtSettings::midiMonitor()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kMidiMonitorKey),false).toBool();
}

void TownsQtSettings::setMidiMonitor(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kMidiMonitorKey),enabled);
	settings.sync();
}

bool TownsQtSettings::cdromMonitor()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kCdromMonitorKey),false).toBool();
}

void TownsQtSettings::setCdromMonitor(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCdromMonitorKey),enabled);
	settings.sync();
}

bool TownsQtSettings::appMonitor()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kAppMonitorKey),false).toBool();
}

void TownsQtSettings::setAppMonitor(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAppMonitorKey),enabled);
	settings.sync();
}

bool TownsQtSettings::snapMouseIntegration()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(settings.contains(QString::fromLatin1(kSnapMouseIntegrationKey)))
	{
		return settings.value(QString::fromLatin1(kSnapMouseIntegrationKey),true).toBool();
	}
	if(settings.contains(QString::fromLatin1(kSnapMouseIntegrationLegacyKey)))
	{
		return settings.value(QString::fromLatin1(kSnapMouseIntegrationLegacyKey),true).toBool();
	}
	return true;
}

void TownsQtSettings::setSnapMouseIntegration(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kSnapMouseIntegrationKey),enabled);
	settings.sync();
}

int TownsQtSettings::snapMouseWarmupFrames()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	if(settings.contains(QString::fromLatin1(kSnapMouseWarmupFramesKey)))
	{
		return std::clamp(
		    settings.value(QString::fromLatin1(kSnapMouseWarmupFramesKey),kSnapMouseWarmupFramesDefault).toInt(),
		    0,600);
	}
	if(settings.contains(QString::fromLatin1(kSnapMouseWarmupFramesLegacyKey)))
	{
		const int frames=std::clamp(
		    settings.value(QString::fromLatin1(kSnapMouseWarmupFramesLegacyKey),kSnapMouseWarmupFramesDefault).toInt(),
		    0,600);
		setSnapMouseWarmupFrames(frames);
		return frames;
	}
	setSnapMouseWarmupFrames(kSnapMouseWarmupFramesDefault);
	return kSnapMouseWarmupFramesDefault;
}

void TownsQtSettings::setSnapMouseWarmupFrames(int frames)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kSnapMouseWarmupFramesKey),std::clamp(frames,0,600));
	settings.sync();
}

bool TownsQtSettings::cddaCacheDuringDataRead()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kCddaCacheDuringDataReadKey),true).toBool();
}

void TownsQtSettings::setCddaCacheDuringDataRead(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCddaCacheDuringDataReadKey),enabled);
	settings.sync();
}

int TownsQtSettings::cddaCachePostReadGraceSec()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return std::clamp(
	    settings.value(QString::fromLatin1(kCddaCachePostReadGraceSecKey),kCddaCachePostReadGraceSecDefault).toInt(),
	    1,60);
}

void TownsQtSettings::setCddaCachePostReadGraceSec(int sec)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kCddaCachePostReadGraceSecKey),std::clamp(sec,1,60));
	settings.sync();
}

bool TownsQtSettings::useDiscProfiles()
{
	return true;
}

void TownsQtSettings::setUseDiscProfiles(bool /*enabled*/)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kUseDiscProfilesKey),true);
	settings.sync();
}

bool TownsQtSettings::autoResumeEnabled()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kAutoResumeKey),true).toBool();
}

void TownsQtSettings::setAutoResumeEnabled(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kAutoResumeKey),enabled);
	settings.sync();
}

bool TownsQtSettings::stateDataCompressionEnabled()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kStateDataCompressionKey),false).toBool();
}

void TownsQtSettings::setStateDataCompressionEnabled(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kStateDataCompressionKey),enabled);
	settings.sync();
}

bool TownsQtSettings::openContentBrowserOnStartup()
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(QString::fromLatin1(kOpenContentBrowserOnStartupKey),false).toBool();
}

void TownsQtSettings::setOpenContentBrowserOnStartup(bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(QString::fromLatin1(kOpenContentBrowserOnStartupKey),enabled);
	settings.sync();
}

namespace
{
int ClampHddSlot(int slot)
{
	return std::clamp(slot,0,TownsQtSettings::kHddSlotCount-1);
}

QString HddEnabledKey(int slot)
{
	return QString::fromLatin1(kHddEnabledKeyPrefix)+QString::number(ClampHddSlot(slot))+
	       QString::fromLatin1(kHddEnabledKeySuffix);
}

QString HddPathKey(int slot)
{
	return QString::fromLatin1(kHddEnabledKeyPrefix)+QString::number(ClampHddSlot(slot))+
	       QString::fromLatin1(kHddPathKeySuffix);
}
}

bool TownsQtSettings::hddEnabled(int slot)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(HddEnabledKey(slot),false).toBool();
}

void TownsQtSettings::setHddEnabled(int slot,bool enabled)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(HddEnabledKey(slot),enabled);
	settings.sync();
}

QString TownsQtSettings::hddImagePath(int slot)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	return settings.value(HddPathKey(slot)).toString();
}

void TownsQtSettings::setHddImagePath(int slot,const QString &path)
{
	QSettings settings(TownsQtPaths::configFilePath(),QSettings::IniFormat);
	settings.setValue(HddPathKey(slot),path);
	settings.sync();
}
