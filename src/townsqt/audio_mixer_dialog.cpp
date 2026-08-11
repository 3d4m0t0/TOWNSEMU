#include "audio_mixer_dialog.h"

#include "townsqt_settings.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#if defined(__linux__)
#include "linux/midi_backend_probe.h"
#endif

namespace
{
void BindPercentLabel(QLabel *value_label,QSlider *slider)
{
	const auto sync=[value_label](int value){
		if(nullptr!=value_label)
		{
			value_label->setText(QStringLiteral("%1%").arg(value));
		}
	};
	sync(slider->value());
	QObject::connect(slider,&QSlider::valueChanged,value_label,sync);
}
}

AudioMixerDialog::AudioMixerDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Audio mixer"));
	setWindowFlag(Qt::Window,true);
	setModal(false);
	setAttribute(Qt::WA_DeleteOnClose,true);

	auto *layout=new QVBoxLayout(this);
	layout->setContentsMargins(12,12,12,12);
	layout->setSpacing(8);

	auto make_row=[&](const QString &label,QSlider *&slider,QLabel *&value_label,int initial){
		auto *row=new QHBoxLayout();
		row->setSpacing(8);
		auto *name=new QLabel(label,this);
		name->setMinimumWidth(48);
		row->addWidget(name);
		slider=new QSlider(Qt::Horizontal,this);
		slider->setRange(0,100);
		slider->setValue(std::clamp(initial,0,100));
		value_label=new QLabel(this);
		value_label->setMinimumWidth(36);
		value_label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
		row->addWidget(slider,1);
		row->addWidget(value_label);
		layout->addLayout(row);
		BindPercentLabel(value_label,slider);
		connect(slider,&QSlider::valueChanged,this,&AudioMixerDialog::emitVolumes);
	};

	make_row(tr("FM"),fm_slider_,fm_value_,TownsQtSettings::fmVolumePercent());
	make_row(tr("PCM"),pcm_slider_,pcm_value_,TownsQtSettings::pcmVolumePercent());
	make_row(tr("CDDA"),cdda_slider_,cdda_value_,TownsQtSettings::cddaVolumePercent());
	make_row(tr("MIDI"),midi_slider_,midi_value_,TownsQtSettings::midiVolumePercent());
	updateMidiVolumeEnabled();

	auto *close_row=new QHBoxLayout();
	close_row->addStretch();
	auto *close_button=new QPushButton(tr("Close"),this);
	connect(close_button,&QPushButton::clicked,this,&QDialog::close);
	close_row->addWidget(close_button);
	layout->addLayout(close_row);

	resize(std::max(360,sizeHint().width()),sizeHint().height());
}

void AudioMixerDialog::emitVolumes()
{
	Q_EMIT volumesChanged(
	    fm_slider_->value(),
	    pcm_slider_->value(),
	    cdda_slider_->value(),
	    midi_slider_->value());
}

void AudioMixerDialog::updateMidiVolumeEnabled()
{
	bool enabled=false;
#if defined(__linux__)
	const bool midi_board=TownsQtSettings::midiBoard();
	const QString output=TownsQtSettings::midiOutput();
	enabled=midi_board &&
	    (output.isEmpty() || output==QStringLiteral("fluidsynth")) &&
	    MidiBackendProbe::IsFluidSynthLibraryAvailable();
#endif
	if(nullptr!=midi_slider_)
	{
		midi_slider_->setEnabled(enabled);
	}
	if(nullptr!=midi_value_)
	{
		midi_value_->setEnabled(enabled);
	}
}
