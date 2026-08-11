#pragma once

#include <QDialog>

class QLabel;
class QSlider;

/*! Non-modal FM/PCM/CDDA/MIDI volume mixer. Changes apply immediately. */
class AudioMixerDialog : public QDialog
{
	Q_OBJECT

public:
	explicit AudioMixerDialog(QWidget *parent=nullptr);

Q_SIGNALS:
	void volumesChanged(int fm_percent,int pcm_percent,int cdda_percent,int midi_percent);

private:
	void emitVolumes();
	void updateMidiVolumeEnabled();

	QSlider *fm_slider_=nullptr;
	QLabel *fm_value_=nullptr;
	QSlider *pcm_slider_=nullptr;
	QLabel *pcm_value_=nullptr;
	QSlider *cdda_slider_=nullptr;
	QLabel *cdda_value_=nullptr;
	QSlider *midi_slider_=nullptr;
	QLabel *midi_value_=nullptr;
};
