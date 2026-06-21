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

#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include "abstractsamplesource.h"
#include "util.h"

class Plot : public QObject, public Subscriber
{
    Q_OBJECT

public:
    Plot(std::shared_ptr<AbstractSampleSource> src);
    ~Plot();
    void invalidateEvent() override;
    virtual bool mouseEvent(QEvent::Type type, QMouseEvent *event);
    virtual void leaveEvent();
    virtual std::shared_ptr<AbstractSampleSource> output();
    virtual void paintBack(QPainter &painter, QRect &rect, range_t<size_t> sampleRange);
    virtual void paintMid(QPainter &painter, QRect &rect, range_t<size_t> sampleRange);
    virtual void paintFront(QPainter &painter, QRect &rect, range_t<size_t> sampleRange);
    int height() const { return _height; };

    // PlotView pushes its global samples-per-pixel before painting so subclasses
    // (TracePlot in particular) use the SAME x-scale as the spectrogram instead
    // of stretching to fit sampleRange across the viewport.
    void setViewSamplesPerColumn(size_t spc) { _viewSamplesPerColumn = spc; }
    size_t viewSamplesPerColumn() const { return _viewSamplesPerColumn; }

signals:
    void repaint();

protected:
    void setHeight(int height) { _height = height; };

    std::shared_ptr<AbstractSampleSource> sampleSource;

private:
    // TODO: don't hardcode this
    int _height = 200;
    size_t _viewSamplesPerColumn = 1;
};
