#pragma once

#include <QString>
#include <QStringList>

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

constexpr int kRecentFileHistoryMax=8;
QStringList recentCdImagePaths();
void addRecentCdImagePath(const QString &path);
void clearRecentCdImagePaths();

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

bool autoScaling();
void setAutoScaling(bool enabled);

bool maintainAspect();
void setMaintainAspect(bool enabled);

bool fullscreenVsync();
void setFullscreenVsync(bool enabled);

/*! Returns SPRITE_TRANSFER_* (see townsparam.h). */
int spriteTransferMode();
void setSpriteTransferMode(int mode);

int cpuFrequencyMhz();
void setCpuFrequencyMhz(int mhz);

bool cpuFastModeEnabled();
void setCpuFastModeEnabled(bool enabled);

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

QString midiSoundFont();
void setMidiSoundFont(const QString &path);

int midiVolumePercent();
void setMidiVolumePercent(int percent);

QString midiOutput();
void setMidiOutput(const QString &output);

int modelGroupIndex();
void setModelGroupIndex(int index);

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

bool showCpuDebug();
void setShowCpuDebug(bool enabled);

bool showDriveAccessOverlay();
void setShowDriveAccessOverlay(bool enabled);

bool midiMonitor();
void setMidiMonitor(bool enabled);

bool snapMouseIntegration();
void setSnapMouseIntegration(bool enabled);

int snapMouseWarmupFrames();
void setSnapMouseWarmupFrames(int frames);

/*! Hard-disk images for SCSI IDs 0..6 (TownsStartParameters::MAX_NUM_SCSI_DEVICES). */
constexpr int kHddSlotCount=7;
bool hddEnabled(int slot);
void setHddEnabled(int slot,bool enabled);
QString hddImagePath(int slot);
void setHddImagePath(int slot,const QString &path);
}
