#pragma once

#include "debug_text_window.h"

class QLineEdit;
class QPushButton;
class QSpinBox;
class QPlainTextEdit;

/*! CPU / CS:EIP history with VM pause toggle and address dump. */
class CpuDebugWindow : public DebugTextWindow
{
	Q_OBJECT

public:
	explicit CpuDebugWindow(QWidget *parent=nullptr);

	void setVmPaused(bool paused);
	bool vmPaused() const;

	void setDumpText(const QString &text);

Q_SIGNALS:
	void pauseToggled(bool pause);
	void dumpRequested(const QString &addrSpec,unsigned int length);

private:
	void onPauseClicked();
	void onDumpClicked();
	void updatePauseButton();

	QPushButton *pause_button_=nullptr;
	QLineEdit *addr_edit_=nullptr;
	QSpinBox *length_spin_=nullptr;
	QPushButton *dump_button_=nullptr;
	QPlainTextEdit *dump_text_=nullptr;
	bool vm_paused_=false;
};
