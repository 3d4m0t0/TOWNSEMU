#include "debug_text_window.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

DebugTextWindow::DebugTextWindow(const QString &title,QWidget *parent)
    :QDialog(parent)
{
	setWindowTitle(title);
	setAttribute(Qt::WA_DeleteOnClose,false);
	resize(720,360);

	auto *layout=new QVBoxLayout(this);
	layout->setContentsMargins(8,8,8,8);
	layout->setSpacing(6);

	text_=new QPlainTextEdit(this);
	text_->setReadOnly(true);
	text_->setLineWrapMode(QPlainTextEdit::NoWrap);
	text_->setUndoRedoEnabled(false);
	text_->setTextInteractionFlags(
	    Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
	QFont mono=text_->font();
	mono.setStyleHint(QFont::Monospace);
	mono.setFamily(QStringLiteral("monospace"));
	text_->setFont(mono);
	layout->addWidget(text_,1);

	auto *buttons=new QHBoxLayout();
	buttons->addStretch(1);
	clear_button_=new QPushButton(tr("Clear"),this);
	connect(clear_button_,&QPushButton::clicked,this,&DebugTextWindow::clearText);
	buttons->addWidget(clear_button_);
	close_button_=new QPushButton(tr("Close"),this);
	connect(close_button_,&QPushButton::clicked,this,&QDialog::close);
	buttons->addWidget(close_button_);
	layout->addLayout(buttons);
}

void DebugTextWindow::setCloseButtonVisible(bool visible)
{
	if(nullptr!=close_button_)
	{
		close_button_->setVisible(visible);
	}
}

void DebugTextWindow::setClearButtonVisible(bool visible)
{
	if(nullptr!=clear_button_)
	{
		clear_button_->setVisible(visible);
	}
}

void DebugTextWindow::setLiveText(const QString &text)
{
	if(nullptr==text_)
	{
		return;
	}
	if(text_->toPlainText()==text)
	{
		return;
	}
	// Don't clobber an active selection — user is copying.
	if(text_->textCursor().hasSelection())
	{
		return;
	}
	const int scroll=text_->verticalScrollBar() ? text_->verticalScrollBar()->value() : 0;
	text_->setPlainText(text);
	if(nullptr!=text_->verticalScrollBar())
	{
		text_->verticalScrollBar()->setValue(scroll);
	}
}

void DebugTextWindow::appendLine(const QString &line)
{
	if(nullptr==text_ || line.isEmpty())
	{
		return;
	}
	text_->appendPlainText(line);
	// Cap growth so long MIDI sessions stay responsive.
	while(text_->blockCount()>kMaxAppendBlocks)
	{
		QTextCursor c(text_->document());
		c.movePosition(QTextCursor::Start);
		c.select(QTextCursor::BlockUnderCursor);
		c.removeSelectedText();
		c.deleteChar(); // remove the leftover newline
	}
}

void DebugTextWindow::clearText()
{
	if(nullptr!=text_)
	{
		text_->clear();
	}
}

void DebugTextWindow::closeEvent(QCloseEvent *event)
{
	QDialog::closeEvent(event);
	Q_EMIT windowClosed();
}
