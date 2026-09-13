#pragma once

#include <QVariantMap>
#include <QWidget>

#include "mouse_coord_write_scan.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/*! Mouse integration fields for Settings → Mouse integration tab
    (game-cursor X/Y pair, range, offset, scale, operation type). */
class MouseCoordProfilePage : public QWidget
{
	Q_OBJECT

public:
	explicit MouseCoordProfilePage(QWidget *parent=nullptr);

	void setProfile(const QVariantMap &profile);
	/*! CD fingerprint from mouseCoordWriteScanState — enables preset load when a match exists. */
	void setDiscFingerprint(unsigned int fingerprintHash32);
	/*! Gray out Mouse integration unless Mouse BIOS soft cursor is resolvable. */
	void setMouseBiosActive(bool active);
	/*! Update live AH=4BH EXE shown next to Bind (from mouseCoordWriteScanState). */
	void setLiveAppExec(const QString &name,unsigned int hash32);
	/*! Fill game-cursor phys + range from scan-window X/Y selection.
	    When hasDsOff, also store DS.base-relative offsets (WC-style). */
	void setGameCursorFromSelection(unsigned int physX,unsigned int physY,
	                                unsigned int minX,unsigned int maxX,
	                                unsigned int minY,unsigned int maxY,
	                                bool hasRangeX,bool hasRangeY,
	                                unsigned int dsOffX=0,unsigned int dsOffY=0,
	                                unsigned int dsSelector=0,bool hasDsOff=false);
	/*! Fill optional Phys 2 from scan-window X2/Y2 (range shared with pair0). */
	void setGameCursor2FromSelection(unsigned int physX,unsigned int physY,
	                                 unsigned int dsOffX=0,unsigned int dsOffY=0,
	                                 unsigned int dsSelector=0,bool hasDsOff=false);
	QVariantMap profile(void);
	void setEditorEnabled(bool enabled);

Q_SIGNALS:
	void contentChanged();
	/*! Request immediate INI write of the current live EXE bind. */
	void bindCurrentAppExecRequested();
	/*! Open Memory scan (Settings closes until Cancel or Update). */
	void openMemoryScanRequested();

private:
	void updateModeNotes(void);
	void applyMosAvailability(void);
	void clearAppSpecificFields(void);
	void updateAppExecLabels(void);
	void bindLiveAppExec(void);
	void refreshPresetButtons(void);
	void applySystemMousePreset(void);
	void applyUserMousePreset(void);
	void saveMousePreset(void);
	bool hasAppSpecificSettings(void) const;
	int selectedMode(void) const;
	void setSelectedMode(int mode);
	void emitContentChanged(void);

	QLineEdit *game_phys_x_=nullptr;
	QLineEdit *game_phys_y_=nullptr;
	/*! Optional second Game Phys pair (row below Phys 1). */
	QLineEdit *game_phys2_x_=nullptr;
	QLineEdit *game_phys2_y_=nullptr;
	/*! DS.base-relative offsets (shown when captured; editable). */
	QLineEdit *game_ds_off_x_=nullptr;
	QLineEdit *game_ds_off_y_=nullptr;
	QLineEdit *game_ds_off2_x_=nullptr;
	QLineEdit *game_ds_off2_y_=nullptr;
	unsigned int pair0_ds_sel_=0;
	unsigned int pair1_ds_sel_=0;
	QSpinBox *game_min_x_=nullptr;
	QSpinBox *game_max_x_=nullptr;
	QSpinBox *game_min_y_=nullptr;
	QSpinBox *game_max_y_=nullptr;
	QComboBox *mode_combo_=nullptr;
	QPushButton *clear_btn_=nullptr;
	QLabel *mode_note_=nullptr;
	QSpinBox *offset_x_=nullptr;
	QSpinBox *offset_y_=nullptr;
	QSpinBox *scale_x_=nullptr;
	QSpinBox *scale_y_=nullptr;
	QSpinBox *scale2_x_=nullptr;
	QSpinBox *scale2_y_=nullptr;
	QCheckBox *invert_x_=nullptr;
	QCheckBox *invert_y_=nullptr;
	QCheckBox *wait_feedback_=nullptr;
	/*! When app-specific is selected: on → memory write (DW), off → game port (GF). */
	QCheckBox *memory_write_=nullptr;
	/*! App-specific: skip MOS soft writes while Mouse BIOS is alive. */
	QCheckBox *stop_soft_write_=nullptr;
	QLabel *app_exec_live_label_=nullptr;
	QLabel *app_exec_bound_label_=nullptr;
	QPushButton *bind_app_exec_btn_=nullptr;
	QPushButton *phys_search_btn_=nullptr;
	QPushButton *system_preset_btn_=nullptr;
	QPushButton *user_preset_load_btn_=nullptr;
	QPushButton *preset_save_btn_=nullptr;
	unsigned int disc_fingerprint_hash32_=0;
	bool has_system_mouse_preset_=false;
	bool has_user_mouse_preset_=false;
	QString live_app_exec_name_;
	unsigned int live_app_exec_hash_=0;
	QString bound_app_exec_name_;
	unsigned int bound_app_exec_hash_=0;
	bool verified_=true;
	bool mouse_bios_active_=false;
	bool suppress_change_=false;
	/*! Last chosen operation type — do not rely on QComboBox::currentData() alone
	    (disabled parent widgets can make currentData() look like 0). */
	int integration_mode_=MouseCoordWriteScan::INTEGRATION_AUTO;
};
