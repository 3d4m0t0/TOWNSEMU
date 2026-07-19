#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <atomic>

#include "townsargv.h"
#include "outside_world.h"

class FMTownsCommon;
class QtInputQueue;
class SharedRgbaFramebuffer;

class EmulatorController : public QObject
{
	Q_OBJECT

public:
	explicit EmulatorController(const TownsARGV &argv,
	                            SharedRgbaFramebuffer *framebuffer,
	                            QtInputQueue *inputQueue,
	                            QObject *parent=nullptr);
	~EmulatorController() override;

public Q_SLOTS:
	void run();
	void requestStop();
	void resetMachine();
	void loadCdImage(const QString &path);
	void ejectCd();
	void loadFdImage(int drive,const QString &path);
	void ejectFd(int drive);
	void setFdWriteProtect(int drive,bool write_protect);
	Q_INVOKABLE bool fdWriteProtected(int drive);
	Q_INVOKABLE bool fdDriveAvailable(int drive) const;
	/*! FD0 always (non-Marty); FD1 disabled in CMOS single-drive mode. */
	static bool QueryFdDriveAvailable(int drive,const FMTownsCommon *towns);
	void pollWindow();
	Q_INVOKABLE QVariantMap guestMouseCoords() const;
	Q_INVOKABLE bool fastModeLamp() const;
	Outside_World::StatusBarInfo driveAccessStatus() const;
	void setCpuFrequencyMhz(int mhz);
	void applyCpuFastMode(bool enabled);
	void setCdSpeed(int speed);
	void applyDisplayOptions(bool damperWireLine,bool scanLineEffectIn15KHz,int spriteTransferMode);
	void applyAudioVolumes(int fm_chip_volume,int pcm_chip_volume,int cdda_volume_percent,bool pcm_lpf_enabled,int pcm_lpf_cutoff_hz,bool pcm_resample_hq);
	void applyMidiBoard(bool enabled);
	void setMidiMonitor(bool enabled);
	Q_INVOKABLE QStringList takeMidiMonitorLines();
	void setCpuDebugMonitor(bool enabled);
	Q_INVOKABLE QString cpuDebugSnapshot() const;
	void restartAudioOutput(void);
	void applyPeripheralSettings(unsigned int game_port0,
	                             unsigned int game_port1,
	                             int max_button_hold_ms0,
	                             int max_button_hold_ms1,
	                             int mouse_integration_speed,
	                             bool consider_vram_offset_in_mouse_integration,
	                             bool auto_differential_on_mouse_bios_stop,
	                             int mouse_min_x,
	                             int mouse_min_y,
	                             int mouse_max_x,
	                             int mouse_max_y);
	Q_INVOKABLE bool differentialMouseIntegration() const;
	/*! Runtime mouse UI flags: diff, mos, capture_released, feeding, failsafe. */
	Q_INVOKABLE QVariantMap mouseUiState() const;
	Q_INVOKABLE void resumeMouseCapture();
	Q_INVOKABLE void setMouseFailsafeShowHostCursor(bool show);
	Q_INVOKABLE void setSnapMouseIntegration(bool enabled);
	Q_INVOKABLE void applySnapMouseSettings(bool enabled,int warmup_frames);
	Q_INVOKABLE bool snapMouseIntegration() const;

Q_SIGNALS:
	void frameReady();
	void statsUpdated(double fps,double emu_hz,int queue_depth,int capture_queue_depth,int present_lag);
	void finished();
	void failed(const QString &message);
	void cdPathChanged(const QString &path);
	void fdPathChanged(int drive,const QString &path);
	void fdWriteProtectChanged(int drive,bool write_protect);
	void fastModeLampChanged(bool fast_mode);

private Q_SLOTS:
	void onVmFinished();

private:
	struct Impl;
	Impl *impl_=nullptr;
	TownsARGV argv_;
	SharedRgbaFramebuffer *framebuffer_=nullptr;
	QtInputQueue *inputQueue_=nullptr;
	std::atomic<bool> running_{false};
	uint64_t last_frame_serial_=0;
	QString cd_path_;
	QString fd_path_[2];

	FMTownsCommon *towns_=nullptr;
	QElapsedTimer stats_timer_;
	int stats_frame_count_=0;
	uint64_t stats_towns_time0_=0;
	bool stats_timer_started_=false;
	double stats_fps_ema_=0.0;
	double stats_hz_ema_=0.0;
	static constexpr int kStatsWindowMs=2000;
	static constexpr double kStatsEmaAlpha=0.3;

	uint32_t last_fast_mode_lamp_revision_=0;
	bool last_fast_mode_lamp_=false;
	bool cpu_debug_ui_enabled_=false;

	uint64_t last_presented_vsync_index_=0;
	bool has_presented_frame_=false;

	void presentDueFrames();
	void updateStats();
	void loadCdImageInternal(const QString &path,bool auto_mount_fd0);
};
