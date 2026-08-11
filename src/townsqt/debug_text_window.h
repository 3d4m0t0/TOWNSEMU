#pragma once

#include <QDialog>

class QPlainTextEdit;
class QPushButton;

/*! Floating debug text window: selectable / copyable plain text.
    Use setLiveText() for continuously replaced status, or appendLine() for logs. */
class DebugTextWindow : public QDialog
{
	Q_OBJECT

public:
	explicit DebugTextWindow(const QString &title,QWidget *parent=nullptr);

	void setLiveText(const QString &text);
	void appendLine(const QString &line);
	void clearText();
	void setCloseButtonVisible(bool visible);
	void setClearButtonVisible(bool visible);

Q_SIGNALS:
	void windowClosed();

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	QPlainTextEdit *text_=nullptr;
	QPushButton *clear_button_=nullptr;
	QPushButton *close_button_=nullptr;
	static constexpr int kMaxAppendBlocks=4000;
};
