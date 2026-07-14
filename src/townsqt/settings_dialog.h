#pragma once

#include <QDialog>
#include <QString>

#include "townsdef.h"

#include "townsqt_rom_availability.h"

class QGroupBox;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTabWidget;

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	struct Values
	{
		int cpuFrequencyMhz=33;
		int memSizeInMB=4;
		bool cpuHighFidelity=false;
		bool pretend386DX=false;
		bool useFPU=false;
		bool fastScsi=false;
		bool midiBoard=false;
		int modelGroupIndex=0;
		int displayScale=1;
		bool autoScaling=false;
		bool maintainAspect=true;
		bool damperWireLine=false;
		bool scanLineEffectIn15KHz=false;
		bool fullscreenVsync=true;
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
		bool pcmLpfEnabled=false;
		int pcmLpfCutoffHz=8000;
		bool waylandIdleInhibit=false;
		unsigned int gamePort0=TOWNS_GAMEPORTEMU_PHYSICAL0;
		unsigned int gamePort1=TOWNS_GAMEPORTEMU_MOUSE;
		int maxButtonHoldTimeMs0=0;
		int maxButtonHoldTimeMs1=0;
		int mouseIntegrationSpeed=256;
		bool considerVRAMOffsetInMouseIntegration=true;
		bool differentialMouseIntegration=false;
		bool snapMouseIntegration=false;
		int snapMouseWarmupFrames=60;
		int mouseMinX=0;
		int mouseMinY=0;
		int mouseMaxX=1023;
		int mouseMaxY=767;
		unsigned int appSpecificSetting=TOWNS_APPSPECIFIC_NONE;
	};

	explicit SettingsDialog(const Values &initial,const QString &romDir=QString(),QWidget *parent=nullptr);

	Values values() const;

Q_SIGNALS:
	void settingsApplied(const Values &values);

private Q_SLOTS:
	void markDirty();
	void onApply();
	void resetCurrentTabToDefaults();
	void updateModelDescription();
	void updateMachineTabControls();
	void onAudioBackendChanged();
	void updateFunctionTab();
	void updateAppSpecificDescription();
	void updateAudioTabMidiSection();
	void browseMidiSoundFont();
	void populateMidiOutputCombo(const QString &select_id=QString());

private:
	void buildUi();
	void applyFixedDialogSize();
	void loadFromValues(const Values &values);
	void applyToValues(Values &out) const;
	void connectDirtyTracking();
	void setApplyEnabled(bool enabled);
	void populateSoundDeviceCombo(const QString &backend,const QString &select_device=QString());
	void updateRestrictedModelComboItems();
	void updateSysRomInfoLabel();
	bool isModelSelectionAllowed(int model_index) const;

	static Values defaultValues();

	Values values_;
	Values default_values_;
	QString rom_dir_;
	bool marty_ex_rom_present_=false;
	TownsQtSysRomProfile sys_rom_profile_=TownsQtSysRomProfile::Missing;
	int marty_model_index_=-1;
	QTabWidget *tabs_=nullptr;
	QPushButton *apply_button_=nullptr;

	bool loading_=false;

	QWidget *machine_page_=nullptr;
	QWidget *peripheral_page_=nullptr;
	QWidget *video_page_=nullptr;
	QWidget *audio_page_=nullptr;
	QWidget *function_page_=nullptr;

	QSpinBox *mem_size_mb_=nullptr;
	QButtonGroup *fidelity_group_=nullptr;
	QLabel *mem_label_=nullptr;
	QGroupBox *fidelity_box_=nullptr;
	QWidget *opt_grid_widget_=nullptr;
	QCheckBox *pretend_386_=nullptr;
	QCheckBox *use_fpu_=nullptr;
	QCheckBox *fast_scsi_=nullptr;
	QCheckBox *midi_board_=nullptr;
	QComboBox *model_group_=nullptr;
	QLabel *sys_rom_info_label_=nullptr;
	QLabel *model_description_=nullptr;

	QSpinBox *display_scale_=nullptr;
	QCheckBox *auto_scale_=nullptr;
	QCheckBox *maintain_aspect_=nullptr;
	QCheckBox *scanline_15k_=nullptr;
	QCheckBox *damper_wire_=nullptr;
	QCheckBox *fullscreen_vsync_=nullptr;
	QButtonGroup *sprite_group_=nullptr;

	QCheckBox *pcm_resample_sinc_=nullptr;
	QCheckBox *pcm_lpf_enabled_=nullptr;
	QSpinBox *pcm_lpf_cutoff_=nullptr;
	QComboBox *sound_device_=nullptr;
	QComboBox *sound_backend_=nullptr;

	QSlider *fm_volume_slider_=nullptr;
	QLabel *fm_volume_value_=nullptr;
	QSlider *pcm_volume_slider_=nullptr;
	QLabel *pcm_volume_value_=nullptr;
	QSlider *cdda_volume_slider_=nullptr;
	QLabel *cdda_volume_value_=nullptr;
	QSlider *midi_volume_slider_=nullptr;
	QLabel *midi_volume_value_=nullptr;

	QComboBox *midi_output_=nullptr;
	QLineEdit *midi_soundfont_edit_=nullptr;
	QString midi_soundfont_path_;
	QPushButton *midi_soundfont_browse_=nullptr;
	QWidget *midi_fluidsynth_panel_=nullptr;
	QLabel *midi_alsa_note_=nullptr;

	QCheckBox *idle_inhibit_=nullptr;
	QCheckBox *snap_mouse_integration_=nullptr;
	QSpinBox *snap_mouse_warmup_=nullptr;

	QComboBox *gameport0_=nullptr;
	QComboBox *gameport1_=nullptr;
	QSpinBox *max_button_hold0_=nullptr;
	QSpinBox *max_button_hold1_=nullptr;
	QSlider *mouse_speed_slider_=nullptr;
	QLabel *mouse_speed_value_=nullptr;
	QCheckBox *mouse_vram_offset_=nullptr;
	QCheckBox *diff_mouse_integration_=nullptr;
	QSpinBox *mouse_min_x_=nullptr;
	QSpinBox *mouse_min_y_=nullptr;
	QSpinBox *mouse_max_x_=nullptr;
	QSpinBox *mouse_max_y_=nullptr;

	QComboBox *app_specific_=nullptr;
	QLabel *app_specific_description_=nullptr;
};
