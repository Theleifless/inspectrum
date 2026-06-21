/*
 *  Copyright (C) 2016, Mike Walters <mike@flomp.net>
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
#include <memory>
#include "abstractsamplesource.h"
#include "plot.h"
#include "util.h"

class TracePlot : public Plot
{
    Q_OBJECT

public:
    TracePlot(std::shared_ptr<AbstractSampleSource> source);

    void paintMid(QPainter &painter, QRect &rect, range_t<size_t> sampleRange);
    std::shared_ptr<AbstractSampleSource> source() { return sampleSource; };
    void setSamplesPerColumn(int spc) { samplesPerColumn = std::max(1, spc); }
    // Stretch the plot vertically by a fractional factor. Paired with the
    // spectrogram's freq zoom so derived plots track its scale instead of
    // staying frozen at their 200-pixel base height.
    void setVerticalZoom(double zoom) {
        setHeight(int(baseHeight * std::max(1.0, zoom) + 0.5));
    }

signals:
    void imageReady(QString key, QImage image);

public slots:
    void handleImage(QString key, QImage image);

private:
    QSet<QString> tasks;
    const int tileWidth = 1000;
    // Samples per pixel, fed in by PlotView so we stay aligned with the
    // spectrogram (which uses fftSize / zoomLevel). 0 falls back to the legacy
    // "stretch sampleRange to fill rect" behaviour for back-compat with any
    // future TracePlot not driven by PlotView.
    int samplesPerColumn = 0;
    // Cached natural height -- the value setHeight was last given before any
    // vertical-zoom multiplication. setVerticalZoom multiplies from this base
    // so successive zooms don't compound.
    const int baseHeight = 200;

    QPixmap getTile(size_t tileID, size_t sampleCount);
    void drawTile(QString key, const QRect &rect, range_t<size_t> sampleRange);
    void plotTrace(QPainter &painter, const QRect &rect, float *samples, size_t count, int step);
};
