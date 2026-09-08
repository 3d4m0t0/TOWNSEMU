#include "cpu_debug_window.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CpuDebugWindow::CpuDebugWindow(QWidget *parent)
    :DebugTextWindow(QStringLiteral("CPU / CS:EIP history"),parent)
{
	setWindowTitle(tr("CPU / CS:EIP history"));
	resize(900,640);
	setClearButtonVisible(false);

	auto *toolbar=new QHBoxLayout();
	toolbar->setContentsMargins(0,0,0,0);
	toolbar->setSpacing(8);

	pause_button_=new QPushButton(this);
	connect(pause_button_,&QPushButton::clicked,this,&CpuDebugWindow::onPauseClicked);
	toolbar->addWidget(pause_button_);

	toolbar->addWidget(new QLabel(tr("Address"),this));
	addr_edit_=new QLineEdit(this);
	addr_edit_->setPlaceholderText(tr("CS:EIP / 4000:187B / L:0004187B"));
	addr_edit_->setClearButtonEnabled(true);
	{
		const QFontMetrics fm(addr_edit_->font());
		const int w=fm.horizontalAdvance(QStringLiteral("CS:EIP / 4000:187B / L:0004187B"))+24;
		addr_edit_->setMinimumWidth(w);
	}
	toolbar->addWidget(addr_edit_,1);

	toolbar->addWidget(new QLabel(tr("Bytes"),this));
	length_spin_=new QSpinBox(this);
	length_spin_->setRange(16,4096);
	length_spin_->setSingleStep(16);
	length_spin_->setValue(256);
	{
		const QFontMetrics fm(length_spin_->font());
		length_spin_->setMinimumWidth(fm.horizontalAdvance(QStringLiteral("4096"))+36);
	}
	toolbar->addWidget(length_spin_);

	dump_button_=new QPushButton(tr("Dump"),this);
	connect(dump_button_,&QPushButton::clicked,this,&CpuDebugWindow::onDumpClicked);
	connect(addr_edit_,&QLineEdit::returnPressed,this,&CpuDebugWindow::onDumpClicked);
	toolbar->addWidget(dump_button_);

	dump_text_=new QPlainTextEdit(this);
	dump_text_->setReadOnly(true);
	dump_text_->setLineWrapMode(QPlainTextEdit::NoWrap);
	dump_text_->setUndoRedoEnabled(false);
	dump_text_->setTextInteractionFlags(
	    Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
	dump_text_->setPlaceholderText(
	    tr("Memory dump appears here. Prefer VM Stop first for a stable view."));
	QFont mono=dump_text_->font();
	mono.setStyleHint(QFont::Monospace);
	mono.setFamily(QStringLiteral("monospace"));
	dump_text_->setFont(mono);
	dump_text_->setMinimumHeight(140);

	auto *vbox=qobject_cast<QVBoxLayout*>(layout());
	if(nullptr!=vbox)
	{
		vbox->insertLayout(0,toolbar);
		// After insert: [toolbar, live text, buttons]. Put dump above Close.
		vbox->insertWidget(2,dump_text_,1);
	}

	updatePauseButton();
}

void CpuDebugWindow::setVmPaused(bool paused)
{
	if(vm_paused_==paused)
	{
		return;
	}
	vm_paused_=paused;
	updatePauseButton();
}

bool CpuDebugWindow::vmPaused() const
{
	return vm_paused_;
}

void CpuDebugWindow::setDumpText(const QString &text)
{
	if(nullptr!=dump_text_)
	{
		dump_text_->setPlainText(text);
	}
}

void CpuDebugWindow::onPauseClicked()
{
	Q_EMIT pauseToggled(!vm_paused_);
}

void CpuDebugWindow::onDumpClicked()
{
	if(nullptr==addr_edit_ || nullptr==length_spin_)
	{
		return;
	}
	const QString addr=addr_edit_->text().trimmed();
	if(addr.isEmpty())
	{
		setDumpText(tr("Enter an address (e.g. 4000:187B or L:0004187B)."));
		return;
	}
	Q_EMIT dumpRequested(addr,static_cast<unsigned int>(length_spin_->value()));
}

void CpuDebugWindow::updatePauseButton()
{
	if(nullptr==pause_button_)
	{
		return;
	}
	if(vm_paused_)
	{
		pause_button_->setText(tr("Resume VM"));
		pause_button_->setToolTip(tr("Resume the emulator (click again to stop)."));
	}
	else
	{
		pause_button_->setText(tr("Stop VM"));
		pause_button_->setToolTip(tr("Pause the emulator so registers and dumps stay stable."));
	}
}
