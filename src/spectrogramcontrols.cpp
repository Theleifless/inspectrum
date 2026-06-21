/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
 *  Copyright (C) 2015, Jared Boone <jared@sharebrained.com>
 *
 *  This file is part of inspectrum.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "spectrogramcontrols.h"
#include <QIntValidator>
#include <QFileDialog>
#include <QSettings>
#include <QLabel>
#include <cmath>
#include "util.h"

SpectrogramControls::SpectrogramControls(const QString & title, QWidget * parent)
    : QDockWidget::QDockWidget(title, parent)
{
    widget = new QWidget(this);
    layout = new QFormLayout(widget);

    fileOpenButton = new QPushButton("Open file...", widget);
    layout->addRow(fileOpenButton);

    sampleRate = new QLineEdit();
    auto double_validator = new QDoubleValidator(this);
    double_validator->setBottom(0.0);
    sampleRate->setValidator(double_validator);
    layout->addRow(new QLabel(tr("Sample rate:")), sampleRate);

    // Read-only display, formatted with SI prefixes (e.g. "2.400 GHz"). Filled
    // from SigMF metadata by MainWindow when a file is opened.
    centerFrequency = new QLabel();
    layout->addRow(new QLabel(tr("Center frequency:")), centerFrequency);

    // Spectrogram settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>Spectrogram</b>")));

    // FFT size as an explicit dropdown of the actual values. Slider→2^value is
    // hostile to scan: you can't tell what size you're on without doing math
    // in your head. The combobox stores the integer fftSize directly as the
    // item's user data so we don't keep re-doing pow().
    fftSizeCombo = new QComboBox(widget);
    for (int e = 4; e <= 16; ++e) {
        int size = 1 << e;
        fftSizeCombo->addItem(QString::number(size), size);
    }
    layout->addRow(new QLabel(tr("FFT size:")), fftSizeCombo);

    zoomLevelSlider = new QSlider(Qt::Horizontal, widget);
    zoomLevelSlider->setRange(-6, 10);
    zoomLevelSlider->setPageStep(1);

    layout->addRow(new QLabel(tr("Zoom:")), zoomLevelSlider);

    // Frequency-only zoom: stretches the spectrogram vertically without
    // touching the time axis. Vertical scrollbar pans through the spectrum.
    // QDoubleSpinBox so the user can dial in fractional values (1.25x, 2.5x,
    // ...). Alt+wheel on the spectrogram also nudges this control.
    freqZoomSpin = new QDoubleSpinBox(widget);
    freqZoomSpin->setRange(1.0, 64.0);
    freqZoomSpin->setSingleStep(0.25);
    freqZoomSpin->setDecimals(2);
    freqZoomSpin->setSuffix("x");
    freqZoomSpin->setValue(1.0);
    layout->addRow(new QLabel(tr("Freq zoom:")), freqZoomSpin);

    powerMaxSlider = new QSlider(Qt::Horizontal, widget);
    powerMaxSlider->setRange(-140, 10);
    layout->addRow(new QLabel(tr("Power max:")), powerMaxSlider);

    powerMinSlider = new QSlider(Qt::Horizontal, widget);
    powerMinSlider->setRange(-140, 10);
    layout->addRow(new QLabel(tr("Power min:")), powerMinSlider);

    scalesCheckBox = new QCheckBox(widget);
    scalesCheckBox->setCheckState(Qt::Checked);
    layout->addRow(new QLabel(tr("Scales:")), scalesCheckBox);

    crosshairCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Crosshairs:")), crosshairCheckBox);

    // Pointer readout under the checkbox: time on the left, frequency on the
    // right. Updates with the mouse regardless of whether the overlay is on.
    pointerTimeLabel = new QLabel();
    pointerTimeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    pointerFreqLabel = new QLabel();
    pointerFreqLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addRow(pointerTimeLabel, pointerFreqLabel);

    // Time selection settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>Time selection</b>")));

    cursorsCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Enable cursors:")), cursorsCheckBox);

    cursorSymbolsSpinBox = new QSpinBox();
    cursorSymbolsSpinBox->setMinimum(1);
    cursorSymbolsSpinBox->setMaximum(99999);
    layout->addRow(new QLabel(tr("Symbols:")), cursorSymbolsSpinBox);

    rateLabel = new QLabel();
    layout->addRow(new QLabel(tr("Rate:")), rateLabel);

    periodLabel = new QLabel();
    layout->addRow(new QLabel(tr("Period:")), periodLabel);

    symbolRateLabel = new QLabel();
    layout->addRow(new QLabel(tr("Symbol rate:")), symbolRateLabel);

    symbolPeriodLabel = new QLabel();
    layout->addRow(new QLabel(tr("Symbol period:")), symbolPeriodLabel);

    // SigMF annotation settings
    layout->addRow(new QLabel()); // TODO: find a better way to add an empty row?
    layout->addRow(new QLabel(tr("<b>SigMF Annotation Controls</b>")));

    // "Display" toggles annotations on the spectrogram. The per-box options
    // (Labels / Comments / Colors) live in the spectrogram's right-click
    // "SigMF" menu, shown whenever Display is enabled.
    annosCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Display:")), annosCheckBox);

    // "Edit" arms drag-to-resize/move on the annotation boxes; off by default so
    // normal pan/cursor behaviour is unchanged until the user opts in.
    annoEditCheckBox = new QCheckBox(widget);
    layout->addRow(new QLabel(tr("Edit:")), annoEditCheckBox);

    widget->setLayout(layout);
    setWidget(widget);

    connect(fftSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpectrogramControls::fftSizeChanged);
    connect(zoomLevelSlider, &QSlider::valueChanged, this, &SpectrogramControls::zoomLevelChanged);
    connect(freqZoomSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpectrogramControls::onFreqZoomSpinChanged);
    connect(fileOpenButton, &QPushButton::clicked, this, &SpectrogramControls::fileOpenButtonClicked);
    connect(cursorsCheckBox, &QCheckBox::stateChanged, this, &SpectrogramControls::cursorsStateChanged);
    connect(powerMinSlider, &QSlider::valueChanged, this, &SpectrogramControls::powerMinChanged);
    connect(powerMaxSlider, &QSlider::valueChanged, this, &SpectrogramControls::powerMaxChanged);
}

void SpectrogramControls::clearCursorLabels()
{
    periodLabel->setText("");
    rateLabel->setText("");
    symbolPeriodLabel->setText("");
    symbolRateLabel->setText("");
}

void SpectrogramControls::cursorsStateChanged(int state)
{
    if (state == Qt::Unchecked) {
        clearCursorLabels();
    }
}

void SpectrogramControls::pointerMoved(QString time, QString frequency)
{
    // Frequency (relative to centre) is blank while the pointer is off the
    // spectrogram. The readout is "live" (full strength) while the pointer is
    // over the plot.
    pointerTimeLabel->setText(time);
    pointerFreqLabel->setText(frequency);
    pointerTimeLabel->setEnabled(true);
    pointerFreqLabel->setEnabled(true);
}

void SpectrogramControls::pointerLeft()
{
    // Keep the last reading but de-emphasise it to show it's no longer live.
    pointerTimeLabel->setEnabled(false);
    pointerFreqLabel->setEnabled(false);
}

void SpectrogramControls::setDefaults()
{
    fftOrZoomChanged();

    cursorsCheckBox->setCheckState(Qt::Unchecked);
    cursorSymbolsSpinBox->setValue(1);

    crosshairCheckBox->setCheckState(Qt::Unchecked);

    annosCheckBox->setCheckState(Qt::Checked);

    // Center frequency defaults to zero; it is auto-populated from SigMF metadata
    // when a file with a capture frequency is opened.
    centerFrequency->setText(tr("0 Hz"));

    // Try to set the sample rate from the last-used value
    QSettings settings;
    int savedSampleRate = settings.value("SampleRate", 8000000).toInt();
    sampleRate->setText(QString::number(savedSampleRate));

    // Restore FFT size by value (the legacy setting holds 2^v as an exponent,
    // so accept either). Default 512.
    int savedFftSize = settings.value("FFTSize", 512).toInt();
    if (savedFftSize > 0 && savedFftSize <= 16)
        savedFftSize = 1 << savedFftSize; // legacy exponent form
    int comboIdx = fftSizeCombo->findData(savedFftSize);
    if (comboIdx < 0)
        comboIdx = fftSizeCombo->findData(512);
    fftSizeCombo->setCurrentIndex(std::max(0, comboIdx));

    powerMaxSlider->setValue(settings.value("PowerMax", 0).toInt());
    powerMinSlider->setValue(settings.value("PowerMin", -100).toInt());
    zoomLevelSlider->setValue(settings.value("ZoomLevel", 0).toInt());

    double savedFreqZoom = settings.value("FreqZoom", 1.0).toDouble();
    if (savedFreqZoom < 1.0) savedFreqZoom = 1.0;
    freqZoomSpin->setValue(savedFreqZoom);
}

void SpectrogramControls::fftOrZoomChanged(void)
{
    int fftSize = fftSizeCombo->currentData().toInt();
    if (fftSize <= 0) fftSize = 512;
    int zoomLevel = zoomLevelSlider->value();
    if (zoomLevel >= 0)
        // zooming in by power-of-two steps
        zoomLevel = std::min(fftSize, (int)pow(2, zoomLevel));
    else
        // zooming out (skipping FFTs) by power-of-two steps
        zoomLevel = -1*std::min(fftSize, (int)pow(2, -1*zoomLevel));
    emit fftOrZoomChanged(fftSize, zoomLevel);
}

void SpectrogramControls::fftSizeChanged(int /*index*/)
{
    QSettings settings;
    settings.setValue("FFTSize", fftSizeCombo->currentData().toInt());
    fftOrZoomChanged();
}

void SpectrogramControls::zoomLevelChanged(int value)
{
    QSettings settings;
    settings.setValue("ZoomLevel", value);
    fftOrZoomChanged();
}

void SpectrogramControls::onFreqZoomSpinChanged(double value)
{
    if (value < 1.0) value = 1.0;
    QSettings settings;
    settings.setValue("FreqZoom", value);
    emit freqZoomChanged(value);
}

void SpectrogramControls::nudgeFreqZoom(int steps)
{
    // One Alt+wheel "click" multiplies by 1.1 -- gives smooth, geometric
    // adjustment so the same wheel motion feels consistent at 1x and 16x.
    // setValue snaps to the spinbox's step grid for tidy display.
    if (steps == 0) return;
    double factor = std::pow(1.1, steps);
    double newVal = freqZoomSpin->value() * factor;
    if (newVal < freqZoomSpin->minimum()) newVal = freqZoomSpin->minimum();
    if (newVal > freqZoomSpin->maximum()) newVal = freqZoomSpin->maximum();
    freqZoomSpin->setValue(newVal);
}

void SpectrogramControls::powerMinChanged(int value)
{
    QSettings settings;
    settings.setValue("PowerMin", value);
}

void SpectrogramControls::powerMaxChanged(int value)
{
    QSettings settings;
    settings.setValue("PowerMax", value);
}

void SpectrogramControls::fileOpenButtonClicked()
{
    QSettings settings;
    QString fileName;
    QFileDialog fileSelect(this);
    fileSelect.setNameFilter(tr("All files (*);;"
                "complex<float> file (*.cfile *.cf32 *.fc32);;"
                "complex<int8> HackRF file (*.cs8 *.sc8 *.c8);;"
                "complex<int16> Fancy file (*.cs16 *.sc16 *.c16);;"
                "complex<uint8> RTL-SDR file (*.cu8 *.uc8)"));

    // Try and load a saved state
    {
        QByteArray savedState = settings.value("OpenFileState").toByteArray();
        fileSelect.restoreState(savedState);

        // Filter doesn't seem to be considered part of the saved state
        QString lastUsedFilter = settings.value("OpenFileFilter").toString();
        if(lastUsedFilter.size())
            fileSelect.selectNameFilter(lastUsedFilter);
    }

    if(fileSelect.exec())
    {
        fileName = fileSelect.selectedFiles()[0];

        // Remember the state of the dialog for the next time
        QByteArray dialogState = fileSelect.saveState();
        settings.setValue("OpenFileState", dialogState);
        settings.setValue("OpenFileFilter", fileSelect.selectedNameFilter());
    }

    if (!fileName.isEmpty())
        emit openFile(fileName);
}

void SpectrogramControls::timeSelectionChanged(float time)
{
    if (cursorsCheckBox->checkState() == Qt::Checked) {
        periodLabel->setText(QString::fromStdString(formatSIValue(time)) + "s");
        rateLabel->setText(QString::fromStdString(formatSIValue(1 / time)) + "Hz");

        int symbols = cursorSymbolsSpinBox->value();
        symbolPeriodLabel->setText(QString::fromStdString(formatSIValue(time / symbols)) + "s");
        symbolRateLabel->setText(QString::fromStdString(formatSIValue(symbols / time)) + "Bd");
    }
}

void SpectrogramControls::zoomIn()
{
    zoomLevelSlider->setValue(zoomLevelSlider->value() + 1);
}

void SpectrogramControls::zoomOut()
{
    zoomLevelSlider->setValue(zoomLevelSlider->value() - 1);
}

