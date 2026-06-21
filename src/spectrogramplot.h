/*
 *  Copyright (C) 2015, Mike Walters <mike@flomp.net>
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

#include <QCache>
#include <QString>
#include <QWidget>
#include "fft.h"
#include "inputsource.h"
#include "plot.h"
#include "tuner.h"
#include "tunertransform.h"

#include <memory>
#include <array>
#include <cstdlib>
#include <math.h>
#include <vector>

class AnnotationLocation;

// Which part of an annotation box the pointer is over, for drag-editing.
// Corners resize both axes, edges resize one axis, Body moves the whole box.
enum class AnnotationHandle {
    None, Body, Left, Right, Top, Bottom,
    TopLeft, TopRight, BottomLeft, BottomRight
};

struct AnnotationHit {
    int index = -1;                              // into inputSource->annotationList
    AnnotationHandle handle = AnnotationHandle::None;
};


class TileCacheKey
{

public:
    TileCacheKey(int fftSize, int zoomLevel, int nfftSkip, size_t sample) {
        this->fftSize = fftSize;
        this->zoomLevel = zoomLevel;
        this->nfftSkip = nfftSkip;
        this->sample = sample;
    }

    bool operator==(const TileCacheKey &k2) const {
        return (this->fftSize == k2.fftSize) &&
               (this->zoomLevel == k2.zoomLevel) &&
               (this->nfftSkip == k2.nfftSkip) &&
               (this->sample == k2.sample);
    }

    int fftSize;
    int zoomLevel;
    int nfftSkip;
    size_t sample;
};

class SpectrogramPlot : public Plot
{
    Q_OBJECT

public:
    SpectrogramPlot(std::shared_ptr<SampleSource<std::complex<float>>> src);
    void invalidateEvent() override;
    std::shared_ptr<AbstractSampleSource> output() override;
    void paintFront(QPainter &painter, QRect &rect, range_t<size_t> sampleRange) override;
    void paintMid(QPainter &painter, QRect &rect, range_t<size_t> sampleRange) override;
    bool mouseEvent(QEvent::Type type, QMouseEvent *event) override;
    void leaveEvent();
    std::shared_ptr<SampleSource<std::complex<float>>> input() { return inputSource; };
    double getCenterFrequency();
    double frequencyAt(int y);

    // Accessors used by the standalone spectrum (PSD) window. getSpectrumLine()
    // returns the same per-column FFT power (dB) the spectrogram renders, one
    // value per bin from -Fs/2 (index 0) to +Fs/2 (index fftSize-1).
    std::vector<float> getSpectrumLine(size_t sample);
    int getColumnStride() { return getStride(); }  // samples between adjacent columns
    int getFFTSize() { return fftSize; }
    float getPowerMin() { return powerMin; }
    float getPowerMax() { return powerMax; }
    double getSampleRate() { return sampleRate; }
    bool isRealSignal() { return inputSource->realSignal(); }
    double getTunerOffsetFrequency();
    double getTunerRelativeBandwidth();
    void setDecimationPreview(int decimation, double frequencyOffset = 0);
    void setSampleRate(double sampleRate);
    bool tunerEnabled();
    void enableScales(bool enabled);
    void enableAnnotations(bool enabled);
    void enableAnnoLabels(bool enabled);
    bool isAnnotationsEnabled();
    void enableAnnoColors(bool enabled);
    QString *mouseAnnotationComment(const QMouseEvent *event);
    // Index into inputSource->annotationList of the annotation under the given
    // viewport position, or -1 if none. Uses the same hit-test geometry as
    // mouseAnnotationComment (populated during the last paint).
    int annotationIndexAt(const QPoint &pos);
    // Which annotation + handle (edge/corner/body) is under the given viewport
    // position, for drag-editing. handle == None if nothing is hit.
    AnnotationHit annotationHandleAt(const QPoint &pos, int margin = 6);
    // Absolute frequency (annotation frame, RF centre included) for a y offset
    // from the top of this plot. Inverse of the mapping paintAnnotations() uses,
    // so dragged edges line up with the drawn boxes.
    double annotationFreqAtY(int relY);

public slots:
    void setFFTSize(int size);
    void setPowerMax(int power);
    void setPowerMin(int power);
    void setZoomLevel(int zoom);
    void setSkip(int skip);
    void setFrequencyZoom(double zoom);
    void tunerMoved();

private:
    const int linesPerGraduation = 50;
    static const int tileSize = 65536; // This must be a multiple of the maximum FFT size

    std::shared_ptr<SampleSource<std::complex<float>>> inputSource;
    std::vector<AnnotationLocation> visibleAnnotationLocations;
    std::unique_ptr<FFT> fft;
    std::unique_ptr<float[]> window;
    QCache<TileCacheKey, QPixmap> pixmapCache;
    QCache<TileCacheKey, std::array<float, tileSize>> fftCache;
    uint colormap[256];

    int fftSize;
    int zoomLevel;
    int nfftSkip;
    double freqZoom = 1.0; // Vertical zoom: spectrogram height = fftSize * freqZoom
    int decimationPreview = 0;
    double decimationOffset = 0;
    float powerMax;
    float powerMin;
    double sampleRate;
    bool frequencyScaleEnabled;
    bool sigmfAnnotationsEnabled;
    bool sigmfAnnotationLabels;
    bool sigmfAnnotationColors;

    Tuner tuner;
    std::shared_ptr<TunerTransform> tunerTransform;

    QPixmap* getPixmapTile(size_t tile);
    float* getFFTTile(size_t tile);
    void getLine(float *dest, size_t sample);
    int getStride();
    int spectrumHeight();
    float getTunerPhaseInc();
    std::vector<float> getTunerTaps();
    int linesPerTile();
    void paintFrequencyScale(QPainter &painter, QRect &rect);
    void paintAnnotations(QPainter &painter, QRect &rect, range_t<size_t> sampleRange);
    void paintDecimationOverlay(QPainter &painter, QRect &rect);
};

class AnnotationLocation
{
public:
    Annotation annotation;
    int index;   // position in inputSource->annotationList

    AnnotationLocation(Annotation annotation, int index, int x, int y, int width, int height)
        : annotation(annotation), index(index), x(x), y(y), width(width), height(height) {}

    bool isInside(int pos_x, int pos_y) {
        return (x <= pos_x) && (pos_x <= x + width)
            && (y <= pos_y) && (pos_y <= y + height);
    }

    // Classify a point against this box's edges/corners/body. `margin` is the
    // grab tolerance (px) around each edge. Returns None if outside the box
    // (plus margin). Edges with width/height of only a few px still resolve to
    // a corner when near an end, which is the intuitive behaviour.
    AnnotationHandle handleAt(int px, int py, int margin) const {
        if (px < x - margin || px > x + width + margin ||
            py < y - margin || py > y + height + margin)
            return AnnotationHandle::None;

        bool nearLeft   = std::abs(px - x) <= margin;
        bool nearRight  = std::abs(px - (x + width)) <= margin;
        bool nearTop    = std::abs(py - y) <= margin;
        bool nearBottom = std::abs(py - (y + height)) <= margin;

        if (nearTop && nearLeft)     return AnnotationHandle::TopLeft;
        if (nearTop && nearRight)    return AnnotationHandle::TopRight;
        if (nearBottom && nearLeft)  return AnnotationHandle::BottomLeft;
        if (nearBottom && nearRight) return AnnotationHandle::BottomRight;
        if (nearLeft)                return AnnotationHandle::Left;
        if (nearRight)               return AnnotationHandle::Right;
        if (nearTop)                 return AnnotationHandle::Top;
        if (nearBottom)              return AnnotationHandle::Bottom;
        return AnnotationHandle::Body;
    }

private:
    int x;
    int y;
    int width;
    int height;
};
