#pragma once

#include <QMainWindow>
#include <QRect>
#include <QThread>
#include <QTimer>

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
	void applySettings(const SettingsDialog::Values &values);
	void toggleFullScreen();
	void showAboutDialog();

Q_SIGNALS:
	void cdLoadRequested(const QString &path);
	void fdLoadRequested(int drive,const QString &path);

private:
	void setupMenuBar();
	void setupEmulatorConnections();
	void startEmulator();
	void restartEmulator();
	void teardownEmulatorConnections();
	void stopEmulator();
	void stopEmulatorAsync(const std::function<void()> &on_stopped);
	void cleanupStoppedEmulator(EmulatorController *stopping);
	void completeEmulatorStop();
	void scheduleRestartEmulator();
	void applyFullscreenVsync();
	void applyWindowScale(int scale,bool auto_scaling,bool maintain_aspect);
	void syncWaylandIdleInhibit();
	void syncWaylandRelativePointer();
	void syncMenuChecks();
	void syncFastModeMenu();
	void syncGuestFastModeFromEmulator();
	void syncCdSpeedMenuTitle();
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
	void updateMouseDebugDisplay();
	void applyMouseDebugVisibility();
	void ensureMouseDebugWindow();
	void updateMidiMonitorDisplay();
	void applyMidiMonitorVisibility();
	void ensureMidiMonitorWindow();
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
	QString fd_path_[2];
	bool fd_drive_available_[2]={true,true};

	QMenu *disk_menu_=nullptr;
	QMenu *cd_recent_menu_=nullptr;
	QAction *open_cd_action_=nullptr;
	QAction *eject_cd_action_=nullptr;
	QMenu *fd_recent_menu_[2]={nullptr,nullptr};
	QAction *open_fd_action_[2]={nullptr,nullptr};
	QAction *eject_fd_action_[2]={nullptr,nullptr};
	QAction *fd_write_protect_[2]={nullptr,nullptr};
	bool syncing_fd_write_protect_menu_=false;

	QAction *fast_mode_action_=nullptr;
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
	QAction *drive_access_action_=nullptr;
	QAction *midi_monitor_action_=nullptr;
	QAction *mouse_debug_action_=nullptr;
	QAction *cpu_debug_action_=nullptr;
	QTimer *fullscreen_chrome_hide_timer_=nullptr;
	DebugTextWindow *mouse_debug_window_=nullptr;
	DebugTextWindow *midi_monitor_window_=nullptr;
	DebugTextWindow *cpu_debug_window_=nullptr;

	bool fullscreen_=false;
	bool fullscreen_cursor_hidden_=false;
	bool cached_differential_integration_=false;
	bool cached_mouse_bios_active_=false;
	bool cached_mouse_capture_released_=false;
	QLabel *mouse_mode_label_=nullptr;
	QTimer *mouse_mode_alternate_timer_=nullptr;
	int mouse_mode_category_=-1;
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
	QRect windowed_geometry_;
	Outside_World::StatusBarInfo drive_access_status_{};

	bool emu_restarting_=false;
	bool emu_restart_pending_=false;
	bool emu_started_=false;
	bool emu_stop_in_progress_=false;
	std::function<void()> emu_stop_done_;

	static constexpr int kFullscreenChromeHideMs=5000;
	static constexpr int kFullscreenChromeToggleMs=250;
	static constexpr int kFullscreenMouseMoveThresholdPx=5;
};
