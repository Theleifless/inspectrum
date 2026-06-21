/*
 *  Copyright (C) 2015-2016, Mike Walters <mike@flomp.net>
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

#pragma once

#include <QGraphicsView>
#include <QPaintEvent>

#include "cursors.h"
#include "inputsource.h"
#include "plot.h"
#include "samplesource.h"
#include "spectrogramplot.h"
#include "spectrumview.h"
#include "traceplot.h"

class PlotView : public QGraphicsView, Subscriber
{
    Q_OBJECT

public:
    PlotView(InputSource *input);
    void setSampleRate(double rate);

    // The spectrogram's current top edge (in global screen coordinates) and its
    // pixel height. A docked SpectrumView calls this at paint time to align its
    // frequency axis with the spectrogram. Returns false if unavailable.
    bool spectrogramScreenBand(int &topGlobal, int &heightOut);

signals:
    void timeSelectionChanged(float time);
    void pointerMoved(QString time, QString frequency);
    void pointerLeft();
    void zoomIn();
    void zoomOut();
    void freqZoomNudge(int steps);
    void spectrumPlotAdded(SpectrumView *plot);
    // Emitted when annotation edit mode changes (e.g. via the right-click SigMF
    // menu) so the dock checkbox can stay in sync.
    void annotationEditChanged(bool enabled);

public slots:
    void cursorsMoved();
    void enableCursors(bool enabled);
    void enableScales(bool enabled);
    void enableAnnotations(bool enabled);
    void enableAnnotationEdit(bool enabled);
    void enableAnnoLabels(bool enabled);
    void enableAnnotationCommentsTooltips(bool enabled);
    void enableAnnoColors(bool enabled);
    void enableCrosshairs(bool enabled);
    void invalidateEvent() override;
    void repaint();
    void setCursorSegments(int segments);
    void setFFTAndZoom(int fftSize, int zoomLevel);
    void setFrequencyZoom(double zoom);
    void setPowerMin(int power);
    void setPowerMax(int power);

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent * event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent * event) override;
    void scrollContentsBy(int dx, int dy) override;
    bool viewportEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    Cursors cursors;
    SampleSource<std::complex<float>> *mainSampleSource = nullptr;
    SpectrogramPlot *spectrogramPlot = nullptr;
    std::vector<std::unique_ptr<Plot>> plots;
    std::vector<SpectrumView*> spectrumPlots;  // owned by their QDockWidgets (MainWindow)
    range_t<size_t> viewRange;
    range_t<size_t> selectedSamples;
    int zoomPos;
    size_t zoomSample;

    int fftSize = 1024;
    int zoomLevel = 1;
    int nfftSkip = 1;
    double currentFreqZoom = 1.0; // last applied vertical zoom; seeds newly added plots
    int powerMin;
    int powerMax;
    bool cursorsEnabled;
    double sampleRate = 0.0;
    bool timeScaleEnabled;
    int scrollZoomStepsAccumulated = 0;
    bool annotationCommentsEnabled;
    bool annotationLabelsEnabled;
    bool annotationColorsEnabled;
    bool crosshairsEnabled = false;   // crosshair overlay (toggled by the checkbox)
    bool crosshairValid = false;      // overlay drawable: pointer seen inside the viewport
    QPoint pointerPos;                 // last pointer position; drives overlay + readout

    // Annotation drag-editing (enabled via the "Edit annotations" toggle). While
    // active, dragging an annotation's edge/corner resizes it and dragging its
    // body moves it; the edit is written to annotationList and persisted with
    // File -> Save Annotations to SigMF.
    bool annotationEditEnabled = false;
    bool annotationDragging = false;
    int annoDragIndex = -1;
    AnnotationHandle annoDragHandle = AnnotationHandle::None;
    range_t<size_t> annoDragStartSamples;   // annotation's range at drag start
    range_t<double> annoDragStartFreq;
    size_t annoDragStartPointerSample = 0;  // pointer position at drag start (for Body moves)
    double annoDragStartPointerFreq = 0;

    void addPlot(Plot *plot);
    void editAnnotation(int index);
    bool annotationEditMouse(QEvent::Type type, QMouseEvent *event);
    void updateAnnotationEditCursor(const QPoint &pos);
    size_t pointerSampleAt(int viewportX);
    double pointerFreqAt(int viewportY);
    void addSpectrumPlot();
    void updateSpectrumPlots();
    void emitTimeSelection();
    void emitPointerPosition();
    void extractSymbols(std::shared_ptr<AbstractSampleSource> src, bool toClipboard);
    void exportSamples(std::shared_ptr<AbstractSampleSource> src);
    template<typename SOURCETYPE> void exportSamples(std::shared_ptr<AbstractSampleSource> src);
    void writeSigMFMeta(const QString &dataFilename, const QString &datatype, double sampleRate, double centerFrequency);
    int plotsHeight();
    size_t samplesPerColumn();
    void updateViewRange(bool reCenter);
    void updateView(bool reCenter = false, bool expanding = false);
    void paintTimeScale(QPainter &painter, QRect &rect, range_t<size_t> sampleRange);
    void paintCrosshair(QPainter &painter, QRect &rect);
    void updateAnnotationTooltip(QMouseEvent *event);

    int sampleToColumn(size_t sample);
    size_t columnToSample(int col);
};
