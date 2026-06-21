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

#include <QPixmapCache>
#include <QTextStream>
#include <QtConcurrent>
#include <QPainterPath>
#include "samplesource.h"
#include "traceplot.h"

TracePlot::TracePlot(std::shared_ptr<AbstractSampleSource> source) : Plot(source) {
    connect(this, &TracePlot::imageReady, this, &TracePlot::handleImage);
}

void TracePlot::paintMid(QPainter &painter, QRect &rect, range_t<size_t> sampleRange)
{
    if (sampleRange.length() == 0) return;

    // Prefer the explicit samples-per-column set by PlotView so we share the
    // same x-axis scale as the spectrogram (governed by fftSize / zoomLevel).
    // Computing it locally as sampleRange.length() / rect.width() goes wrong
    // when viewRange is clamped to a small file -- the trace stretches across
    // the full viewport while the spectrogram fills only the leading pixels,
    // and the two panels visibly disagree on the time axis.
    int spc = samplesPerColumn > 0
        ? samplesPerColumn
        : std::max(1, int(sampleRange.length() / rect.width()));
    int samplesPerTile = tileWidth * spc;
    size_t tileID = sampleRange.minimum / samplesPerTile;
    size_t tileOffset = sampleRange.minimum % samplesPerTile; // Number of samples to skip from first image tile
    int xOffset = tileOffset / spc; // Number of columns to skip from first image tile

    // Paint first (possibly partial) tile
    painter.drawPixmap(
        QRect(rect.x(), rect.y(), tileWidth - xOffset, height()),
        getTile(tileID++, samplesPerTile),
        QRect(xOffset, 0, tileWidth - xOffset, height())
    );

    // Paint remaining tiles
    for (int x = tileWidth - xOffset; x < rect.right(); x += tileWidth) {
        painter.drawPixmap(
            QRect(x, rect.y(), tileWidth, height()),
            getTile(tileID++, samplesPerTile)
        );
    }
}

QPixmap TracePlot::getTile(size_t tileID, size_t sampleCount)
{
    QPixmap pixmap(tileWidth, height());
    QString key;
    QTextStream(&key) << "traceplot_" << this << "_" << tileID << "_" << sampleCount;
    if (QPixmapCache::find(key, &pixmap))
        return pixmap;

    if (!tasks.contains(key)) {
        range_t<size_t> sampleRange{tileID * sampleCount, (tileID + 1) * sampleCount};
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QtConcurrent::run(&TracePlot::drawTile, this, key, QRect(0, 0, tileWidth, height()), sampleRange);
#else
        QtConcurrent::run(this, &TracePlot::drawTile, key, QRect(0, 0, tileWidth, height()), sampleRange);
#endif
        tasks.insert(key);
    }
    pixmap.fill(Qt::transparent);
    return pixmap;
}

void TracePlot::drawTile(QString key, const QRect &rect, range_t<size_t> sampleRange)
{
    QImage image(rect.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    auto firstSample = sampleRange.minimum;
    auto length = sampleRange.length();

    // Clamp the request to what the source actually has, shrinking the draw
    // rect proportionally. Without this, at high zoom samplesPerTile can
    // exceed the file size; getSamples then returns nullptr, drawTile bails
    // before emitting an image, and the tile stays fully transparent -- the
    // "trace plot disappears at high zoom" symptom. The spectrogram already
    // handles this case (zero-pads to -inf); TracePlot was the outlier.
    QRect drawRect = rect;
    auto clampToAvailable = [&](size_t available) -> bool {
        if (firstSample >= available)
            return false;
        if (firstSample + length > available) {
            size_t valid = available - firstSample;
            drawRect.setWidth(int(double(rect.width()) * valid / length));
            length = valid;
        }
        return true;
    };

    // Is it a 2-channel (complex) trace?
    if (auto src = dynamic_cast<SampleSource<std::complex<float>>*>(sampleSource.get())) {
        if (!clampToAvailable(src->count())) {
            emit imageReady(key, image);
            return;
        }
        auto samples = src->getSamples(firstSample, length);
        if (samples == nullptr) {
            emit imageReady(key, image);
            return;
        }

        painter.setPen(Qt::red);
        plotTrace(painter, drawRect, reinterpret_cast<float*>(samples.get()), length, 2);
        painter.setPen(Qt::blue);
        plotTrace(painter, drawRect, reinterpret_cast<float*>(samples.get())+1, length, 2);

    // Otherwise is it single channel?
    } else if (auto src = dynamic_cast<SampleSource<float>*>(sampleSource.get())) {
        if (!clampToAvailable(src->count())) {
            emit imageReady(key, image);
            return;
        }
        auto samples = src->getSamples(firstSample, length);
        if (samples == nullptr) {
            emit imageReady(key, image);
            return;
        }

        painter.setPen(Qt::green);
        plotTrace(painter, drawRect, samples.get(), length, 1);
    } else {
        throw std::runtime_error("TracePlot::paintMid: Unsupported source type");
    }

    emit imageReady(key, image);
}

void TracePlot::handleImage(QString key, QImage image)
{
    auto pixmap = QPixmap::fromImage(image);
    QPixmapCache::insert(key, pixmap);
    tasks.remove(key);
    emit repaint();
}

void TracePlot::plotTrace(QPainter &painter, const QRect &rect, float *samples, size_t count, int step = 1)
{
    QPainterPath path;
    range_t<float> xRange{0, rect.width() - 2.f};
    range_t<float> yRange{0, rect.height() - 2.f};
    const float xStep = 1.0 / count * rect.width();
    for (size_t i = 0; i < count; i++) {
        float sample = samples[i*step];
        float x = i * xStep;
        float y = (1 - sample) * (rect.height() / 2);

        x = xRange.clip(x) + rect.x();
        y = yRange.clip(y) + rect.y();

        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    painter.drawPath(path);
}
