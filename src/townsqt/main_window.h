#pragma once

#include <QMainWindow>
#include <QRect>
#include <QThread>
#include <QTimer>
#include <QVariantMap>

#include <functional>

#include "emu_view.h"
#include "qt_input_queue.h"
#include "settings_dialog.h"
#include "shared_rgba_framebuffer.h"
#include "townsargv.h"
#include "townsdef.h"
#include "outside_world.h"

class QAction;
class QActionGroup;
class EmulatorController;
class QLabel;
class QMenu;
class DebugTextWindow;
class CdromMonitorWindow;
class MouseCoordScanWindow;
class AudioMixerDialog;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(const TownsARGV &argv,int scale=1,QWidget *parent=nullptr);
	~MainWindow() override;

protected:
	void closeEvent(QCloseEvent *event) override;
	void changeEvent(QEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	bool eventFilter(QObject *watched,QEvent *event) override;

private Q_SLOTS:
	void onPollTimer();
	void onFrameReady();
	void onControllerFinished();
	void onFailed(const QString &message);
	void onStatsUpdated(double fps,double emu_hz,int queue_depth,int capture_queue_depth,int present_lag);
	void onGuestFastModeLampChanged(bool fast_mode);
	void openCdImage();
	void openFdImage(int drive);
	void createBlankFdImage();
	void onCdPathChanged(const QString &path);
	void onFdPathChanged(int drive,const QString &path);
	void onFdWriteProtectChanged(int drive,bool write_protect);
	void rebuildRecentCdMenu();
	void rebuildRecentFdMenu(int drive);
	void clearRecentCdList();
	void clearRecentFdList();
	void syncFdWriteProtectMenuChecks();
	void syncFdDriveMenus();
	void syncEjectMenus();
	void openSettingsDialog();
	void openHddSettingsDialog();
	void openAudioMixerDialog();
	void openMouseCoordProfileEditor();
	void cancelMouseCoordScan();
	void stopMouseCoordScanSession();
	void applySettings(const SettingsDialog::Values &values);
	void applyAudioMixerVolumes(int fm_percent,int pcm_percent,int cdda_percent,int midi_percent);
	void onDiscProfileStateChanged();
	void toggleFullScreen();
	void showAboutDialog();
	void loadStateSlotFromMenu(int slot);
	void saveStateSlotFromMenu(int slot);

Q_SIGNALS:
	void fdLoadRequested(int drive,const QString &path);

private:
	void setupMenuBar();
	void setupEmulatorConnections();
	void startEmulator();
	void restartEmulator();
	/*! Resolve CD (pending / argv / last), fingerprint profile, override argv before boot. */
	void prepareArgvForNextBoot(void);
	/*! Load fp_*.ini machine map for a disc image without an EMU core. */
	bool loadDiscProfileOverrideForPath(const QString &cdPath);
	void clearDiscProfileOverride(void);
	/*! Live CD image change (state save → eject → mount).
	    Profiled disc → retarget edit paths and restart; unprofiled/eject → keep prior edit target. */
	void requestCdImageChange(const QString &path);
	void fillDiscProfileSettings(SettingsDialog::Values &values) const;
	/*! Sync edit target from the mounted disc.
	    If the disc has no profile and retainOverrideIfUnprofiled, leave prior profile
	    CMOS/settings target unchanged (multi-disc install). */
	void applyRuntimeDiscProfileOverrides(bool retainOverrideIfUnprofiled=true);
	void applyDiscProfileOverridesToArgv();
	/*! Profile hd0..hd6 keys → argv_.scsiImg (skips CD-ROM SCSI slots). */
	void applyDiscProfileHddOverridesToArgv();
	/*! Conf baseline then profile HDD overrides. Returns true if any slot path/type changed. */
	bool syncArgvHardDiskFromSettingsAndProfile();
	bool runtimeUseDiscProfile() const;
	void teardownEmulatorConnections();
	void stopEmulator();
	void stopEmulatorAsync(const std::function<void()> &on_stopped);
	/*! On app exit only (not emulator restart): save state0_XXXXXXXX.TState before teardown. */
	void maybeSaveDiscStateSaveBeforeStop(EmulatorController *controller);
	void cleanupStoppedEmulator(EmulatorController *stopping);
	void completeEmulatorStop();
	void scheduleRestartEmulator();
	void applyDisplayVsync();
	void applyWindowScale(int scale);
	QSize computeWindowedSizeForScale(int scale) const;
	int maxDisplayScale() const;
	int maxDisplayScaleForFullscreen() const;
	void setDisplayScale(int scale);
	void bumpDisplayScale(int delta);
	void syncDisplayScaleMenu();
	void syncWaylandIdleInhibit();
	void syncWaylandRelativePointer();
	void syncMenuChecks();
	void syncCpuClockMenuActions();
	void syncFastModeMenu();
	void syncGuestFastModeFromEmulator();
	void syncCdSpeedMenuTitle();
	/*! When a disc profile is loaded, Operation-menu clock writes into that profile. */
	bool applyCpuClockViaDiscProfile(bool fast_mode,int mhz);
	void setCpuFastModeEnabled(bool enabled);
	void setCpuFrequencyMhz(int mhz);
	void setCdSpeed(int speed);
	void setSpriteTransferMode(int mode);
	void setGamePort(int port,unsigned int emu);
	void syncGamePortMenus();
	void syncSpriteMenuTitle();
	void applyFullscreenLayout();
	void applyWindowedLayout();
	void scheduleFullscreenChromeHide();
	void hideFullscreenChrome();
	void noteFullscreenMouseActivity(const QPoint &global_pos);
	void showFullscreenCursor();
	void hideFullscreenCursor();
	void ensureMenuBarDocked();
	void connectFullscreenMenuHooks();
	void processPendingMouseWarp();
	void syncDifferentialMouseCursor();
	void updateBlankCursor();
	void noteEmuPictureClicked();
	void refreshMouseUiState();
	void updateMouseModeIndicator();
	void updateMouseFailsafeFromActivity();
	void updateFullscreenNormalIntegrationChrome();
	void updateWindowTitle();
	void updateProfileEnabledIndicator();
	void updateMouseDebugDisplay();
	void applyMouseDebugVisibility();
	void ensureMouseDebugWindow();
	void updateMouseCoordScanDisplay();
	void applyMouseCoordScanVisibility();
	void ensureMouseCoordScanWindow();
	void updateMidiMonitorDisplay();
	void applyMidiMonitorVisibility();
	void ensureMidiMonitorWindow();
	void updateCdromMonitorDisplay();
	void applyCdromMonitorVisibility();
	void ensureCdromMonitorWindow();
	void updateAppMonitorDisplay();
	void applyAppMonitorVisibility();
	void ensureAppMonitorWindow();
	void updateCpuDebugDisplay();
	void applyCpuDebugVisibility();
	void ensureCpuDebugWindow();
	void applyDriveAccessVisibility();
	DriveAccessPresence currentDriveAccessPresence() const;
	void updateOpenCdMenuLabel();
	void updateOpenFdMenuLabel(int drive);
	bool queryDifferentialMouseIntegration() const;
	QVariantMap queryMouseUiState() const;
	bool isMouseInsideWindow(const QPoint &global_pos) const;
	bool shouldCaptureHostMouse() const;
	bool mouseCoordScanActive() const;
	void raiseMainWindowForMouseCoordSession();
	void stopMouseCoordScanByEsc();
	void releaseDifferentialMouseCaptureByEsc();
	/*! Focus check for keeping an already-active differential Wayland capture. */
	bool shouldKeepDifferentialWaylandCapture() const;
	bool isCursorOverUiChrome() const;
	bool isCursorNearFullscreenMenu() const;
	bool isSignificantFullscreenMouseMove(const QPoint &global_pos) const;
	bool isAnyMenuVisible() const;

	TownsARGV argv_;
	SharedRgbaFramebuffer framebuffer_;
	QtInputQueue inputQueue_;
	EmuView *view_=nullptr;
	EmulatorController *controller_=nullptr;
	QThread *emu_thread_=nullptr;
	QTimer poll_timer_;
	QString cd_path_;
	/*! Canonical CD path used at the previous emulator boot (restart state-save policy). */
	QString cd_path_at_last_boot_;
	/*! CD path to mount on the next startEmulator (set by open/recent before stop→start). */
	QString pending_boot_cd_path_;
	QString fd_path_[2];
	bool fd_drive_available_[2]={true,true};

	QMenu *disk_menu_=nullptr;
	QMenu *cdrom_menu_=nullptr;
	QMenu *cd_recent_menu_=nullptr;
	QAction *open_cd_action_=nullptr;
	QAction *eject_cd_action_=nullptr;
	QMenu *fd_recent_menu_[2]={nullptr,nullptr};
	QAction *open_fd_action_[2]={nullptr,nullptr};
	QAction *eject_fd_action_[2]={nullptr,nullptr};
	QAction *fd_write_protect_[2]={nullptr,nullptr};
	bool syncing_fd_write_protect_menu_=false;

	QAction *cpu_compat_action_=nullptr;
	QActionGroup *cpu_clock_group_=nullptr;
	QAction *cpu_clock_custom_action_=nullptr;
	QMenu *gameport_menu_=nullptr;
	QMenu *gameport0_menu_=nullptr;
	QMenu *gameport1_menu_=nullptr;
	QActionGroup *gameport0_group_=nullptr;
	QActionGroup *gameport1_group_=nullptr;
	QMenu *cdrom_speed_menu_=nullptr;
	QActionGroup *cdrom_speed_group_=nullptr;
	QMenu *sprite_menu_=nullptr;
	QActionGroup *sprite_dma_group_=nullptr;
	QAction *fullscreen_action_=nullptr;
	QMenu *display_scale_menu_=nullptr;
	QActionGroup *display_scale_group_=nullptr;
	QAction *scale_down_action_=nullptr;
	QAction *scale_up_action_=nullptr;
	QAction *drive_access_action_=nullptr;
	QAction *fps_display_action_=nullptr;
	QAction *midi_monitor_action_=nullptr;
	QAction *cdrom_monitor_action_=nullptr;
	QAction *app_monitor_action_=nullptr;
	QAction *mouse_debug_action_=nullptr;
	QAction *cpu_debug_action_=nullptr;
	QTimer *fullscreen_chrome_hide_timer_=nullptr;
	DebugTextWindow *mouse_debug_window_=nullptr;
	MouseCoordScanWindow *mouse_coord_scan_window_=nullptr;
	/*! Non-owning; set while SettingsDialog::exec() is nested. */
	SettingsDialog *active_settings_dialog_=nullptr;
	/*! Settings ↔ Memory scan are exclusive; restore Settings after scan. */
	bool pending_focus_mouse_integration_tab_=false;
	bool pending_apply_game_cursor_from_scan_=false;
	bool have_deferred_settings_values_=false;
	bool deferred_settings_dirty_=false;
	SettingsDialog::Values deferred_settings_values_;
	QVariantMap deferred_mouse_coord_profile_;
	DebugTextWindow *midi_monitor_window_=nullptr;
	CdromMonitorWindow *cdrom_monitor_window_=nullptr;
	DebugTextWindow *app_monitor_window_=nullptr;
	DebugTextWindow *cpu_debug_window_=nullptr;
	AudioMixerDialog *audio_mixer_dialog_=nullptr;

	bool fullscreen_=false;
	bool fullscreen_cursor_hidden_=false;
	bool cached_differential_integration_=false;
	bool cached_mouse_bios_active_=false;
	bool cached_mouse_capture_released_=false;
	bool cached_mouse_profile_loaded_=false;
	bool cached_mouse_profile_apply_=false;
	int cached_integration_mode_=0;
	bool cached_disc_profile_loaded_=false;
	unsigned int cached_disc_fingerprint_hash32_=0;
	bool disc_profile_override_active_=false;
	QVariantMap disc_profile_machine_override_;
	QLabel *profile_enabled_label_=nullptr;
	QLabel *mouse_mode_label_=nullptr;
	QTimer *mouse_mode_alternate_timer_=nullptr;
	int mouse_mode_category_=-1;
	int mouse_mode_integration_for_buttons_=-1;
	int mouse_mode_phase_=0;
	bool host_cursor_blank_=false;
	bool mouse_failsafe_show_cursor_=false;
	bool wayland_capture_want_=false;
	QString wayland_capture_method_;
	qint64 last_emu_activity_ms_=0;
	bool have_last_fullscreen_mouse_global_=false;
	qint64 last_fullscreen_mouse_move_ms_=0;
	qint64 last_fullscreen_menubar_show_ms_=0;
	QPoint last_fullscreen_mouse_global_;
	int mouse_coord_scan_ui_div_=0;
	QString last_mouse_coord_disc_base_;
	QRect windowed_geometry_;
	QSize intended_window_size_;
	bool allow_window_resize_=false;
	Outside_World::StatusBarInfo drive_access_status_{};
	bool have_window_stats_=false;
	double last_stats_fps_=0.0;
	double last_stats_emu_hz_=0.0;
	int last_stats_queue_depth_=0;
	int last_stats_capture_queue_depth_=0;
	int last_stats_present_lag_=0;

	bool emu_restarting_=false;
	bool emu_restart_pending_=false;
	bool emu_started_=false;
	bool emu_stop_in_progress_=false;
	/*! Re-read QSettings into argv_ on the next prepareArgvForNextBoot (restarts only). */
	bool refresh_argv_from_settings_on_next_boot_=false;
	std::function<void()> emu_stop_done_;

	static constexpr int kFullscreenChromeHideMs=5000;
	static constexpr int kFullscreenChromeToggleMs=250;
	static constexpr int kFullscreenMouseMoveThresholdPx=5;
};
