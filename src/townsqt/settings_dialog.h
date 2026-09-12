#pragma once

#include <QDialog>
#include <QString>

#include "townsdef.h"

#include "townsqt_rom_availability.h"
#include "townsqt_cpu_profile.h"
#include "townsqt_settings.h"
#include "drive_config_page.h"

class QGroupBox;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class MouseCoordProfilePage;

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	struct Values
	{
		struct HddSlot
		{
			bool enabled=false;
			QString path;
		};

		int cpuFrequencyMhz=33;
		int cpuCustomFrequencyMhz=33;
		bool cpuFastMode=true;
		/*! Towns boot-key combination (BOOT_KEYCOMB_CD / F0 / F1 / H0). */
		unsigned int bootKeyComb=BOOT_KEYCOMB_CD;
		int memSizeInMB=4;
		bool cpuHighFidelity=false;
		bool pretend386DX=false;
		bool useFPU=false;
		bool fastScsi=false;
		bool fastFd=false;
		bool midiBoard=false;
		bool singleDrive=false;
		bool highResCrtc=true;
		bool highResPcm=true;
		TownsQtCpuKind cpuKind=TownsQtCpuKind::I486DX;
		int modelGroupIndex=0;
		int displayScale=1;
		bool autoScaling=false;
		bool maintainAspect=true;
		bool damperWireLine=false;
		bool scanLineEffectIn15KHz=false;
		bool fullscreenVsync=true;
		bool windowedVsync=false;
		int spriteTransferMode=0;
		bool pcmResampleHighQuality=false;
		QString audioBackend;
		QString audioDevice;
		int fmVolumePercent=50;
		int pcmVolumePercent=50;
		int cddaVolumePercent=100;
		int midiVolumePercent=100;
		QString midiSoundFont;
		QString midiOutput;
		QString midiAlsaPort;
		bool pcmLpfEnabled=false;
		int pcmLpfCutoffHz=8000;
		bool waylandIdleInhibit=false;
		unsigned int gamePort0=TOWNS_GAMEPORTEMU_PHYSICAL0;
		unsigned int gamePort1=TOWNS_GAMEPORTEMU_MOUSE;
		int maxButtonHoldTimeMs0=0;
		int maxButtonHoldTimeMs1=0;
		int mouseIntegrationSpeed=256;
		bool considerVRAMOffsetInMouseIntegration=true;
		bool autoDifferentialOnMosUnused=false;
		bool snapMouseIntegration=true;
		int snapMouseWarmupFrames=10;
		bool cddaCacheDuringDataRead=true;
		int cddaCachePostReadGraceSec=1;
		int mouseMinX=0;
		int mouseMinY=0;
		int mouseMaxX=1023;
		int mouseMaxY=767;
		unsigned int appSpecificSetting=TOWNS_APPSPECIFIC_NONE;
		HddSlot hdd[TownsQtSettings::kHddSlotCount];
		/*! Always true (disc profiles are always enabled). */
		bool useDiscProfiles=true;
		bool autoResumeEnabled=true;
		/*! Live disc profile state (not stored in townsqt.conf). */
		bool discMounted=false;
		bool discProfileAvailable=false;
		bool discProfileCreateRequested=false;
		QString discProfileFileName;
		/*! Profile overrides (written to fp_*.ini). Edited via Basics widgets when a profile exists. */
		int profileCpuFrequencyMhz=33;
		int profileCpuCustomFrequencyMhz=33;
		bool profileCpuFastMode=true;
		unsigned int profileBootKeyComb=BOOT_KEYCOMB_CD;
		int profileMemSizeInMB=4;
		unsigned int profileGamePort0=TOWNS_GAMEPORTEMU_PHYSICAL0;
		unsigned int profileGamePort1=TOWNS_GAMEPORTEMU_MOUSE;
		int profileMaxButtonHoldTimeMs0=0;
		int profileMaxButtonHoldTimeMs1=0;
		bool profileCpuHighFidelity=false;
		bool profilePretend386DX=false;
		bool profileUseFPU=false;
		bool profileFastScsi=false;
		bool profileFastFd=false;
		bool profileMidiBoard=false;
		bool profileSingleDrive=false;
		bool profileHasMouseIntegration=false;
	};

	explicit SettingsDialog(const Values &initial,const QString &romDir=QString(),QWidget *parent=nullptr);

	/*! Dialog::exec() result: leave Settings to open Memory scan (exclusive). */
	static constexpr int ResultOpenMemoryScan=2;

	Values values() const;
	bool applyPending(void) const;
	void setApplyPending(bool pending);
	/*! Refresh Profile-tab disc state while the dialog stays open (after Create). */
	void setDiscProfileState(const Values &values);
	/*! Load mouse-integration editor from live emu state (mouseCoordWriteScanState). */
	void setMouseCoordProfile(const QVariantMap &profile,bool mosActive);
	QVariantMap mouseCoordProfile(void) const;
	void setGameCursorFromSelection(unsigned int physX,unsigned int physY,
	                                unsigned int minX,unsigned int maxX,
	                                unsigned int minY,unsigned int maxY,
	                                bool hasRangeX,bool hasRangeY,
	                                unsigned int dsOffX=0,unsigned int dsOffY=0,
	                                unsigned int dsSelector=0,bool hasDsOff=false);
	void setGameCursor2FromSelection(unsigned int physX,unsigned int physY,
	                                 unsigned int dsOffX=0,unsigned int dsOffY=0,
	                                 unsigned int dsSelector=0,bool hasDsOff=false);
	void setMouseBiosActive(bool active);
	/*! Refresh live AH=4BH EXE shown on the Mouse integration tab. */
	void setLiveAppExec(const QString &name,unsigned int hash32);
	void focusMouseIntegrationTab(void);
	void focusBasicsTab(void);
	int mouseIntegrationTabIndex(void) const;
	void setDriveConfig(const DriveConfigPage::Values &values);
	DriveConfigPage::Values driveConfig(void) const;

Q_SIGNALS:
	void settingsApplied(const Values &values);
	void createDiscProfileRequested();
	void deleteDiscProfileRequested();
	/*! Memory scan button on the Mouse integration tab (Settings will close). */
	void openMemoryScanRequested();
	/*! Bind current guest EXE into the disc profile immediately (INI write). */
	void bindCurrentAppExecRequested();

private Q_SLOTS:
	void markDirty();
	void onApply();
	void resetCurrentTabToDefaults();
	void updateMachineTabControls();
	void updateProfileTabControls();
	void onAudioBackendChanged();
	void updateFunctionTab();
	void updateAudioTabMidiSection();
	void browseMidiSoundFont();
	void populateMidiOutputCombo(const QString &select_id=QString());
	void populateMidiAlsaPortCombo(const QString &select_id=QString());
	void onMidiOutputChanged();

private:
	void buildUi();
	void applyFixedDialogSize();
	void loadFromValues(const Values &values);
	void applyToValues(Values &out) const;
	void connectDirtyTracking();
	void setApplyEnabled(bool enabled);
	void populateSoundDeviceCombo(const QString &backend,const QString &select_device=QString());
	void updateCpuComboItems();
	void updateModelComboItems();
	void updateSysRomInfoLabel();
	void updateCapabilityStatusLabels();
	void updateDisplayScaleRange();
	void updateFastModeControls();
	bool selectedCpuFastMode() const;
	int selectedCpuFrequencyMhz() const;
	int selectedCpuCustomFrequencyMhz() const;
	void setCpuFrequencyWidgets(bool fast_mode,int active_mhz,int custom_mhz);
	unsigned int selectedBootKeyComb() const;
	void setBootDriveWidget(unsigned int keyComb);
	/*! True while a disc profile exists — Basics widgets edit that profile (not townsqt.conf). */
	bool editingDiscProfile(void) const;
	void applyProfileEditAppearance(void);
	void loadSharedMachineWidgets(const Values &values,bool fromProfile);
	void readSharedMachineWidgets(Values &out,bool toProfile) const;
	TownsQtCpuKind currentCpuKind() const;
	int currentModelGroupIndex() const;

	static Values defaultValues();

	Values values_;
	Values default_values_;
	DriveConfigPage::Values default_drive_config_;
	QString rom_dir_;
	bool marty_ex_rom_present_=false;
	TownsQtSysRomProfile sys_rom_profile_=TownsQtSysRomProfile::Missing;
	int sys_rom_level_=-1;
	QTabWidget *tabs_=nullptr;
	QPushButton *apply_button_=nullptr;

	bool loading_=false;

	QWidget *machine_page_=nullptr;
	QWidget *drive_config_page_host_=nullptr;
	DriveConfigPage *drive_config_page_=nullptr;
	QWidget *mouse_integration_page_=nullptr;
	MouseCoordProfilePage *mouse_coord_profile_page_=nullptr;
	QWidget *display_audio_page_=nullptr;
	QWidget *function_page_=nullptr;

	QSpinBox *mem_size_mb_=nullptr;
	QLabel *mem_label_=nullptr;
	QComboBox *cpu_fidelity_=nullptr;
	QButtonGroup *cpu_freq_group_=nullptr;
	QRadioButton *cpu_freq_preset_radio_=nullptr;
	QComboBox *cpu_freq_preset_=nullptr;
	QRadioButton *cpu_freq_custom_=nullptr;
	QSpinBox *cpu_freq_custom_mhz_=nullptr;
	QLabel *boot_drive_label_=nullptr;
	QComboBox *boot_drive_=nullptr;
	QWidget *opt_grid_widget_=nullptr;
	QCheckBox *pretend_386_=nullptr;
	QCheckBox *use_fpu_=nullptr;
	QCheckBox *fast_scsi_=nullptr;
	QCheckBox *fast_fd_=nullptr;
	QCheckBox *midi_board_=nullptr;
	QLabel *high_res_status_label_=nullptr;
	QLabel *ug_io_status_label_=nullptr;
	QComboBox *cpu_kind_=nullptr;
	QComboBox *model_group_=nullptr;
	QLabel *sys_rom_info_label_=nullptr;

	QWidget *disc_profile_bar_=nullptr;
	QLabel *disc_profile_status_label_=nullptr;
	QLabel *disc_profile_file_label_=nullptr;
	QPushButton *disc_profile_action_btn_=nullptr;
	/*! BIOS / CPU / Model — always global (townsqt.conf). */
	QWidget *machine_global_bar_=nullptr;
	/*! Clock / options / ports — disc-profile fields when a profile exists. */
	QWidget *profile_fields_box_=nullptr;
	QLabel *basics_footer_label_=nullptr;

	QCheckBox *auto_resume_enabled_=nullptr;

	QSpinBox *display_scale_=nullptr;
	QCheckBox *scanline_15k_=nullptr;
	QCheckBox *damper_wire_=nullptr;
	QCheckBox *fullscreen_vsync_=nullptr;
	QCheckBox *windowed_vsync_=nullptr;
	QButtonGroup *sprite_group_=nullptr;

	QCheckBox *pcm_resample_sinc_=nullptr;
	QCheckBox *pcm_lpf_enabled_=nullptr;
	QSpinBox *pcm_lpf_cutoff_=nullptr;
	QComboBox *sound_device_=nullptr;
	QComboBox *sound_backend_=nullptr;

	QComboBox *midi_output_=nullptr;
	QLineEdit *midi_soundfont_edit_=nullptr;
	QString midi_soundfont_path_;
	QPushButton *midi_soundfont_browse_=nullptr;
	QWidget *midi_fluidsynth_panel_=nullptr;
	QWidget *midi_alsa_port_panel_=nullptr;
	QStackedWidget *midi_detail_stack_=nullptr;
	QComboBox *midi_alsa_port_=nullptr;

	QCheckBox *idle_inhibit_=nullptr;
	QCheckBox *snap_mouse_integration_=nullptr;
	QCheckBox *cdda_cache_during_data_read_=nullptr;
	QSpinBox *cdda_cache_post_read_grace_sec_=nullptr;

	QComboBox *gameport0_=nullptr;
	QComboBox *gameport1_=nullptr;
	QSpinBox *max_button_hold0_=nullptr;
	QSpinBox *max_button_hold1_=nullptr;
	QCheckBox *auto_diff_on_mos_unused_=nullptr;
};
