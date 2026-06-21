# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

inspectrum is a Qt5 desktop tool for analysing captured SDR signals. It opens large (100GB+) IQ recordings, renders a spectrogram with zoom/pan, derives amplitude/frequency/phase/IQ plots from a tuned-and-filtered slice of the signal, and supports cursors for symbol/period measurement plus export of samples and demodulated data.

## Build & run

The project is CMake + C++14, depends on Qt5 (Widgets, Core, Concurrent), FFTW 3.x, and liquid-dsp (>= 1.3.0), discovered via pkg-config and the finders in `cmake/Modules/`.

```bash
# Configure + build (out-of-tree)
mkdir -p build && cd build
cmake ..
make -j$(nproc)        # macOS: -j$(sysctl -n hw.ncpu)

# Run
./src/inspectrum [filename]
./src/inspectrum -r 8000000 -f cs16 capture.bin   # override sample rate / format
```

`enable_testing()` is declared but there is currently no test suite — there are no test sources to run.

## Architecture

### DSP signal chain — pull-based, lazy, type-templated

The core abstraction is `SampleSource<T>` (templated on sample type, almost always `std::complex<float>` or `float`). Sources are chained: each one holds a `shared_ptr` to its upstream source and computes samples **on demand** via `getSamples(start, length)` — nothing is precomputed or stored for the whole file.

The chain for a typical derived plot:

```
InputSource (mmap'd file, SampleAdapter converts on-the-fly to complex<float>)
  └─> TunerTransform (frequency-shift + low-pass + decimate, via liquid-dsp)
        └─> AmplitudeDemod / FrequencyDemod / PhaseDemod  (complex<float> -> float)
              └─> Threshold  (float -> float)
```

- `InputSource` ([src/inputsource.cpp](src/inputsource.cpp)) memory-maps the file and uses a `SampleAdapter` subclass per on-disk format to convert raw bytes to `complex<float>` lazily. Unknown extensions default to `cf32`. Format strings and the SigMF parser live here.
- `SampleBuffer<Tin,Tout>` ([src/samplebuffer.h](src/samplebuffer.h)) is the base for transform stages: subclasses implement `work(input, output, count, sampleid)`. The demods and `TunerTransform` derive from it.
- `Tuner`/`TunerTransform` ([src/tuner.cpp](src/tuner.cpp), [src/tunertransform.cpp](src/tunertransform.cpp)) implement the draggable tuner overlay on the spectrogram — selecting a center frequency + bandwidth that feeds the derived plots.

### Invalidation (the subscriber graph)

Sources are also a publish/subscribe graph, independent of the Qt parent/child tree. `AbstractSampleSource` keeps a set of `Subscriber*`; calling `invalidate()` cascades `invalidateEvent()` downstream. `Plot`, `PlotView`, `MainWindow`, and `SampleBuffer` all implement `Subscriber`. When upstream state changes (new file, tuner moved, sample rate set), invalidation propagates down the chain, caches are dropped, and plots repaint. When editing the chain, remember a change to one stage must invalidate everything downstream or stale cached tiles will render.

### Plots & rendering

`Plot` ([src/plot.h](src/plot.h)) is the base for everything drawn in the stacked view. It is both a `QObject` (emits `repaint()`) and a `Subscriber`. Three paint layers: `paintBack` / `paintMid` / `paintFront`. Each `Plot` is constructed from an `AbstractSampleSource` and may expose a derived `output()` source that downstream plots subscribe to.

- `PlotView` ([src/plotview.cpp](src/plotview.cpp)) is a `QGraphicsView` that owns the vertical stack of plots, the shared `Cursors`, viewport/zoom state, and the screen<->sample coordinate mapping (`sampleToColumn`/`columnToSample`, `samplesPerColumn`). It also drives export, symbol extraction, and the crosshair overlay. This is the central coordinator — most "where does this number come from" questions resolve here.
- `SpectrogramPlot` ([src/spectrogramplot.cpp](src/spectrogramplot.cpp)) is the anchor plot: it runs the windowed `FFT` ([src/fft.cpp](src/fft.cpp)), tile-caches both raw FFT power (`fftCache`) and rendered pixmaps (`pixmapCache`) keyed by `TileCacheKey{fftSize, zoomLevel, nfftSkip, sample}`, draws the frequency scale, SigMF annotations, and the tuner. `samplesPerColumn = fftSize / zoomLevel` — TracePlots are fed this value so their x-axis stays pixel-aligned with the spectrogram.
- `TracePlot` ([src/traceplot.cpp](src/traceplot.cpp)) renders the line plots (amplitude/frequency/phase/IQ). It draws in `tileWidth`-wide tiles on background threads (`QtConcurrent`, signalled back via `imageReady`).
- `Plots` ([src/plots.h](src/plots.h)) is a registry: a `multimap<type_index, PlotInfo>` mapping a source's sample type to the derived plots it can spawn. This is how the right-click "add plot" menu knows that a `complex<float>` source offers amplitude/frequency/phase/sample plots while a `float` source offers a threshold plot. Add new plot types here.

### UI wiring

`MainWindow` ([src/mainwindow.cpp](src/mainwindow.cpp)) owns the `InputSource`, the `PlotView`, and the `SpectrogramControls` dock ([src/spectrogramcontrols.cpp](src/spectrogramcontrols.cpp), FFT size / power range / sample rate / format widgets). `SpectrumView` ([src/spectrumview.cpp](src/spectrumview.cpp)) is the standalone PSD window that reads per-column FFT lines back out of the `SpectrogramPlot` and aligns its frequency axis to the spectrogram band.

## Conventions

- Qt5 `AUTOMOC` is on — any class with `Q_OBJECT` is moc'd automatically; no manual moc invocation. New `.cpp` files must be added to the `inspectrum_sources` list in [src/CMakeLists.txt](src/CMakeLists.txt).
- All internal processing is 32-bit; 64-bit input samples are truncated to 32-bit on load.
- Sample positions are `size_t` throughout and ranges use `range_t<>` from [src/util.h](src/util.h) — files can exceed 2^32 samples, so don't narrow these to `int`.
- GPLv3. Source files carry the standard license header; keep it on new files.
