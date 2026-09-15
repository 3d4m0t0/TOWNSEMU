#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

#include "townsqt_cpu_profile.h"

namespace TownsQtSettings
{
QString lastCdImagePath();
void setLastCdImagePath(const QString &path);
void clearLastCdImagePath();
QString workingDirectory();
void setWorkingDirectory(const QString &directory);
/*! Start directory for CD/FD file dialogs: shared working directory, then fallback, then config dir.
 *  Paths under ~/.config/townsqt/blank_fd/ do not update the working directory. */
QString fileDialogStartDirectory(const QString &fallback=QString());
/*! Remember the directory of a CD/FD path chosen in a file dialog (blank_fd excluded). */
void rememberFileDialogPath(const QString &path);

constexpr int kRecentCdImageHistoryMax=20;
constexpr int kRecentFdImageHistoryMax=8;
QStringList recentCdImagePaths();
void addRecentCdImagePath(const QString &path);
void clearRecentCdImagePaths();

/*! Last mounted FD image path per drive (stored in townsqt.conf [media] fd0_image / fd1_image). */
QString lastFdImagePath(int drive);
void setLastFdImagePath(int drive,const QString &path);
void clearLastFdImagePath(int drive);
/*! Shared recent-FD list for FD0 and FD1. */
QStringList recentFdImagePaths();
void addRecentFdImagePath(int drive,const QString &path);
void clearRecentFdImagePaths();
bool fdWriteProtect(int drive);
void setFdWriteProtect(int drive,bool enabled);

bool damperWireLine();
void setDamperWireLine(bool enabled);

bool scanLineEffectIn15KHz();
void setScanLineEffectIn15KHz(bool enabled);

int displayScale();
void setDisplayScale(int scale);
/*! Largest integer scale whose 640×480 window (+ chrome) fits in available_size (DIP / DE-scaled). */
int maxDisplayScaleForAvailableSize(QSize available_size,int chrome_w=0,int chrome_h=0);

bool autoScaling();
void setAutoScaling(bool enabled);

bool maintainAspect();
void setMaintainAspect(bool enabled);

bool fullscreenVsync();
void setFullscreenVsync(bool enabled);

bool windowedVsync();
void setWindowedVsync(bool enabled);

/*! Returns SPRITE_TRANSFER_* (see townsparam.h). */
int spriteTransferMode();
void setSpriteTransferMode(int mode);

int cpuFrequencyMhz();
void setCpuFrequencyMhz(int mhz);

/*! Independent custom FAST-mode frequency (33–60 MHz). Always selectable in the Operations menu. */
int cpuCustomFrequencyMhz();
void setCpuCustomFrequencyMhz(int mhz);

bool cpuFastModeEnabled();
void setCpuFastModeEnabled(bool enabled);

/*! Boot-key combination (BOOT_KEYCOMB_CD / F0 / F1 / H0). Default CD. */
unsigned int bootKeyComb();
void setBootKeyComb(unsigned int keyComb);

/*! CD speed multiplier; 0 = emulator default timing. */
int cdSpeed();
void setCdSpeed(int speed);

int memSizeInMB();
void setMemSizeInMB(int mb);

bool cpuHighFidelity();
void setCpuHighFidelity(bool enabled);

bool pretend386DX();
void setPretend386DX(bool enabled);

bool useFPU();
void setUseFPU(bool enabled);

bool fastScsi();
void setFastScsi(bool enabled);

bool fastFd();
void setFastFd(bool enabled);

bool midiBoard();
void setMidiBoard(bool enabled);

/*! Towns CMOS single-drive mode (FD1 unavailable when enabled). */
bool singleDrive();
void setSingleDrive(bool enabled);

bool highResCrtc();
void setHighResCrtc(bool enabled);

bool highResPcm();
void setHighResPcm(bool enabled);

QString midiSoundFont();
void setMidiSoundFont(const QString &path);

int midiVolumePercent();
void setMidiVolumePercent(int percent);

QString midiOutput();
void setMidiOutput(const QString &output);
/*! ALSA sequencer destination "client:port" when midiOutput is "alsa". */
QString midiAlsaPort();
void setMidiAlsaPort(const QString &port);

int modelGroupIndex();
void setModelGroupIndex(int index);

TownsQtCpuKind cpuKind();
void setCpuKind(TownsQtCpuKind kind);

unsigned int townsType();
void setTownsType(unsigned int towns_type);

bool pcmResampleHighQuality();
void setPcmResampleHighQuality(bool enabled);

QString audioBackend();
void setAudioBackend(const QString &backend);

QString audioDevice();
void setAudioDevice(const QString &device);

int fmVolumePercent();
void setFmVolumePercent(int percent);
int pcmVolumePercent();
void setPcmVolumePercent(int percent);
int cddaVolumePercent();
void setCddaVolumePercent(int percent);
int fmChipVolume();
int pcmChipVolume();

bool pcmLpfEnabled();
void setPcmLpfEnabled(bool enabled);
int pcmLpfCutoffHz();
void setPcmLpfCutoffHz(int hz);

bool waylandIdleInhibit();
void setWaylandIdleInhibit(bool enabled);

unsigned int gamePort(int port);
void setGamePort(int port,unsigned int emu);

int maxButtonHoldTimeMs(int port,int button);
void setMaxButtonHoldTimeMs(int port,int button,int ms);

int mouseIntegrationSpeed();
void setMouseIntegrationSpeed(int speed);

bool considerVRAMOffsetInMouseIntegration();
void setConsiderVRAMOffsetInMouseIntegration(bool enabled);

bool differentialMouseIntegration();
void setDifferentialMouseIntegration(bool enabled);

/*! When MOS is active but unused, automatically switch to differential.
    (Mouse-BIOS-stop → differential is always on and not a setting.) */
bool autoDifferentialOnMosUnused();
void setAutoDifferentialOnMosUnused(bool enabled);

int mouseMinX();
void setMouseMinX(int value);
int mouseMinY();
void setMouseMinY(int value);
int mouseMaxX();
void setMouseMaxX(int value);
int mouseMaxY();
void setMouseMaxY(int value);

unsigned int appSpecificSetting();
void setAppSpecificSetting(unsigned int app_value);

bool showMouseIntegrationDebug();
void setShowMouseIntegrationDebug(bool enabled);

bool showMouseCoordWriteScan();
void setShowMouseCoordWriteScan(bool enabled);

bool showCpuDebug();
void setShowCpuDebug(bool enabled);

bool showDriveAccessOverlay();
void setShowDriveAccessOverlay(bool enabled);

/*! Window-title FPS / queue / lag line (Tools → FPS display). Default on. */
bool showFpsDisplay();
void setShowFpsDisplay(bool enabled);

bool midiMonitor();
void setMidiMonitor(bool enabled);

bool cdromMonitor();
void setCdromMonitor(bool enabled);

bool appMonitor();
void setAppMonitor(bool enabled);

bool snapMouseIntegration();
void setSnapMouseIntegration(bool enabled);

/*! Frames of gradual integration before snap mode (ini: function/snap_mouse_warmup_frames). */
int snapMouseWarmupFrames();
void setSnapMouseWarmupFrames(int frames);

/*! Bulk-prefetch CDDA and keep playing through data reads. */
bool cddaCacheDuringDataRead();
void setCddaCacheDuringDataRead(bool enabled);
/*! Seconds until playback is considered finished after a data-read burst. */
int cddaCachePostReadGraceSec();
void setCddaCachePostReadGraceSec(int sec);

/*! Per-disc fp_*.ini machine/mouse overrides (always on). */
bool useDiscProfiles();
void setUseDiscProfiles(bool enabled);

/*! Auto-save state0_XXXXXXXX.TState on eject/exit; auto-load on next mount (Features). */
bool autoResumeEnabled();
void setAutoResumeEnabled(bool enabled);

/*! zlib-compress .TState on save (Features). Load always accepts compressed and legacy. Default off. */
bool stateDataCompressionEnabled();
void setStateDataCompressionEnabled(bool enabled);

/*! Hard-disk images for SCSI IDs 0..6 (TownsStartParameters::MAX_NUM_SCSI_DEVICES). */
constexpr int kHddSlotCount=7;
bool hddEnabled(int slot);
void setHddEnabled(int slot,bool enabled);
QString hddImagePath(int slot);
void setHddImagePath(int slot,const QString &path);
}
