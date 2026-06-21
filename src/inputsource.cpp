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

#include "inputsource.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <algorithm>

#include <QFileInfo>

#include <QElapsedTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmapCache>
#include <QRect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QColor>


class ComplexF32SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<float>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<float>*>(src);
        std::copy(&s[start], &s[start + length], dest);
    }
};

class ComplexF64SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<double>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<double>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<double>& v) -> std::complex<float> {
                return { static_cast<float>(v.real()) , static_cast<float>(v.imag()) };
            }
        );
    }
};

class ComplexS32SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<int32_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<int32_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<int32_t>& v) -> std::complex<float> {
                const float k = 1.0f / 2147483648.0f;
                return { v.real() * k, v.imag() * k };
            }
        );
    }
};

class ComplexS16SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<int16_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<int16_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<int16_t>& v) -> std::complex<float> {
                const float k = 1.0f / 32768.0f;
                return { v.real() * k, v.imag() * k };
            }
        );
    }
};

class ComplexS8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<int8_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<int8_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<int8_t>& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { v.real() * k, v.imag() * k };
            }
        );
    }
};

class ComplexU8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(std::complex<uint8_t>);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const std::complex<uint8_t>*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const std::complex<uint8_t>& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { (v.real() - 127.4f) * k, (v.imag() - 127.4f) * k };
            }
        );
    }
};

class RealF32SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(float);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const float*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const float& v) -> std::complex<float> {
                return {v, 0.0f};
            }
        );
    }
};

class RealF64SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(double);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const double*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const double& v) -> std::complex<float> {
                return {static_cast<float>(v), 0.0f};
            }
        );
    }
};

class RealS16SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(int16_t);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const int16_t*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const int16_t& v) -> std::complex<float> {
                const float k = 1.0f / 32768.0f;
                return { v * k, 0.0f };
            }
        );
    }
};

class RealS8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(int8_t);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const int8_t*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const int8_t& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { v * k, 0.0f };
            }
        );
    }
};

class RealU8SampleAdapter : public SampleAdapter {
public:
    size_t sampleSize() override {
        return sizeof(uint8_t);
    }

    void copyRange(const void* const src, size_t start, size_t length, std::complex<float>* const dest) override {
        auto s = reinterpret_cast<const uint8_t*>(src);
        std::transform(&s[start], &s[start + length], dest,
            [](const uint8_t& v) -> std::complex<float> {
                const float k = 1.0f / 128.0f;
                return { (v - 127.4f) * k, 0 };
            }
        );
    }
};

InputSource::InputSource()
{
}

InputSource::~InputSource()
{
    cleanup();
}

void InputSource::cleanup()
{
    if (mmapData != nullptr) {
        inputFile->unmap(mmapData);
        mmapData = nullptr;
    }

    if (inputFile != nullptr) {
        delete inputFile;
        inputFile = nullptr;
    }
}

QJsonObject InputSource::readMetaData(const QString &filename)
{
    QFile datafile(filename);
    if (!datafile.open(QFile::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Error while opening meta data file: " + datafile.errorString().toStdString());
    }

    QJsonDocument d = QJsonDocument::fromJson(datafile.readAll());
    datafile.close();
    auto root = d.object();

    if (!root.contains("global") || !root["global"].isObject()) {
        throw std::runtime_error("SigMF meta data is invalid (no global object found)");
    }

    auto global = root["global"].toObject();

    if (!global.contains("core:datatype") || !global["core:datatype"].isString()) {
        throw std::runtime_error("SigMF meta data does not specify a valid datatype");
    }


    auto datatype = global["core:datatype"].toString();
    if (datatype.compare("cf32_le") == 0) {
        sampleAdapter = std::make_unique<ComplexF32SampleAdapter>();
    } else if (datatype.compare("ci32_le") == 0) {
        sampleAdapter = std::make_unique<ComplexS32SampleAdapter>();
    } else if (datatype.compare("ci16_le") == 0) {
        sampleAdapter = std::make_unique<ComplexS16SampleAdapter>();
    } else if (datatype.compare("ci8") == 0) {
        sampleAdapter = std::make_unique<ComplexS8SampleAdapter>();
    } else if (datatype.compare("cu8") == 0) {
        sampleAdapter = std::make_unique<ComplexU8SampleAdapter>();
    } else if (datatype.compare("rf32_le") == 0) {
        sampleAdapter = std::make_unique<RealF32SampleAdapter>();
        _realSignal = true;
    } else if (datatype.compare("ri16_le") == 0) {
        sampleAdapter = std::make_unique<RealS16SampleAdapter>();
        _realSignal = true;
    } else if (datatype.compare("ri8") == 0) {
        sampleAdapter = std::make_unique<RealS8SampleAdapter>();
        _realSignal = true;
    } else if (datatype.compare("ru8") == 0) {
        sampleAdapter = std::make_unique<RealU8SampleAdapter>();
        _realSignal = true;
    } else {
        throw std::runtime_error("SigMF meta data specifies unsupported datatype");
    }

    if (global.contains("core:sample_rate") && global["core:sample_rate"].isDouble()) {
        setSampleRate(global["core:sample_rate"].toDouble());
    }


    if (root.contains("captures") && root["captures"].isArray()) {
        auto captures = root["captures"].toArray();

        for (auto capture_ref : captures) {
            if (capture_ref.isObject()) {
                auto capture = capture_ref.toObject();
                if (capture.contains("core:frequency") && capture["core:frequency"].isDouble()) {
                    frequency = capture["core:frequency"].toDouble();
                }
            } else {
                throw std::runtime_error("SigMF meta data is invalid (invalid capture object)");
            }
        }
    }

    if(root.contains("annotations") && root["annotations"].isArray()) {

        size_t offset = 0;

        if (global.contains("core:offset")) {
            offset = global["offset"].toDouble();
        }

        auto annotations = root["annotations"].toArray();

        for (auto annotation_ref : annotations) {
            if (annotation_ref.isObject()) {
                auto sigmf_annotation = annotation_ref.toObject();

                const size_t sample_start = sigmf_annotation["core:sample_start"].toDouble();

                if (sample_start < offset)
                    continue;

                const size_t rel_sample_start = sample_start - offset;

                const size_t sample_count = sigmf_annotation["core:sample_count"].toDouble();
                auto sampleRange = range_t<size_t>{rel_sample_start, rel_sample_start + sample_count - 1};

                const double freq_lower_edge = sigmf_annotation["core:freq_lower_edge"].toDouble();
                const double freq_upper_edge = sigmf_annotation["core:freq_upper_edge"].toDouble();
                auto frequencyRange = range_t<double>{freq_lower_edge, freq_upper_edge};

                auto label = sigmf_annotation["core:label"].toString();
                if (label.isEmpty()) {
                    label = sigmf_annotation["core:description"].toString();
                }

                auto comment = sigmf_annotation["core:comment"].toString();

                auto sigmf_color = sigmf_annotation["presentation:color"].toString();
                // SigMF uses the format "#RRGGBBAA" for alpha-channel colors, QT uses "#AARRGGBB"
                // Check length first so the empty/short-string case short-circuits before at(0):
                // in Qt6 an empty QString has a null data pointer and at(0) would crash.
                if ((sigmf_color.length() == 9) && (sigmf_color.at(0) == '#')) {
                    sigmf_color = "#" + sigmf_color.mid(7,2) + sigmf_color.mid(1,6);
                }
                auto boxColor = QString::fromStdString("white");
                if (QColor::isValidColor(sigmf_color)) {
                    boxColor = sigmf_color;
                }

                annotationList.emplace_back(sampleRange, frequencyRange, label, comment, boxColor);
            }
        }
    }

    return root;
}

void InputSource::openFile(const char *filename)
{
    QFileInfo fileInfo(filename);
    std::string suffix = std::string(fileInfo.suffix().toLower().toUtf8().constData());
    if (_fmt != "") { suffix = _fmt; } // allow fmt override
    if ((suffix == "cfile") || (suffix == "cf32")  || (suffix == "fc32")) {
        sampleAdapter = std::make_unique<ComplexF32SampleAdapter>();
    }
    else if ((suffix == "cf64")  || (suffix == "fc64")) {
        sampleAdapter = std::make_unique<ComplexF64SampleAdapter>();
    }
    else if ((suffix == "cs32") || (suffix == "sc32") || (suffix == "c32")) {
        sampleAdapter = std::make_unique<ComplexS32SampleAdapter>();
    }
    else if ((suffix == "cs16") || (suffix == "sc16") || (suffix == "c16")) {
        sampleAdapter = std::make_unique<ComplexS16SampleAdapter>();
    }
    else if ((suffix == "cs8") || (suffix == "sc8") || (suffix == "c8")) {
        sampleAdapter = std::make_unique<ComplexS8SampleAdapter>();
    }
    else if ((suffix == "cu8") || (suffix == "uc8")) {
        sampleAdapter = std::make_unique<ComplexU8SampleAdapter>();
    }
    else if (suffix == "f32") {
        sampleAdapter = std::make_unique<RealF32SampleAdapter>();
        _realSignal = true;
    }
    else if (suffix == "f64") {
        sampleAdapter = std::make_unique<RealF64SampleAdapter>();
        _realSignal = true;
    }
    else if (suffix == "s16") {
        sampleAdapter = std::make_unique<RealS16SampleAdapter>();
        _realSignal = true;
    }
    else if (suffix == "s8") {
        sampleAdapter = std::make_unique<RealS8SampleAdapter>();
        _realSignal = true;
    }
    else if (suffix == "u8") {
        sampleAdapter = std::make_unique<RealU8SampleAdapter>();
        _realSignal = true;
    }
    else {
        sampleAdapter = std::make_unique<ComplexF32SampleAdapter>();
    }

    QString dataFilename;

    annotationList.clear();
    sigmfMetaFilename.clear();
    QString metaFilename;

    if (suffix == "sigmf-meta" || suffix == "sigmf-data" || suffix == "sigmf-") {
        dataFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".sigmf-data";
        metaFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".sigmf-meta";
        sigmfMetaFilename = metaFilename;
        auto metaData = readMetaData(metaFilename);
        QFile datafile(dataFilename);
        if (!datafile.open(QFile::ReadOnly | QIODevice::Text)) {
            auto global = metaData["global"].toObject();
            if (global.contains("core:dataset")) {
                auto datasetfilename = global["core:dataset"].toString();
                if(QFileInfo(datasetfilename).isAbsolute()){
                    dataFilename = datasetfilename;
                }
                else{
                    dataFilename = fileInfo.path() + "/" + datasetfilename;
                }
            }
        }
    }
    else if (suffix == "sigmf") {
        throw std::runtime_error("SigMF archives are not supported. Consider extracting a recording.");
    }
    else {
        dataFilename = filename;
    }

    auto file = std::make_unique<QFile>(dataFilename);
    if (!file->open(QFile::ReadOnly)) {
        throw std::runtime_error(file->errorString().toStdString());
    }

    auto size = file->size();
    sampleCount = size / sampleAdapter->sampleSize();

    auto data = file->map(0, size);
    if (data == nullptr)
        throw std::runtime_error("Error mmapping file");

    cleanup();

    inputFile = file.release();
    mmapData = data;

    invalidate();
}

void InputSource::setSampleRate(double rate)
{
    sampleRate = rate;
    invalidate();
}

double InputSource::rate()
{
    return sampleRate;
}

std::unique_ptr<std::complex<float>[]> InputSource::getSamples(size_t start, size_t length)
{
    if (inputFile == nullptr)
        return nullptr;

    if (mmapData == nullptr)
        return nullptr;

    if(start < 0 || length < 0)
        return nullptr;

    if (start + length > sampleCount)
        return nullptr;

    auto dest = std::make_unique<std::complex<float>[]>(length);
    sampleAdapter->copyRange(mmapData, start, length, dest.get());

    return dest;
}

void InputSource::setFormat(std::string fmt){
    _fmt = fmt;
}

// Persist the current annotationList back into the .sigmf-meta we loaded from.
// Preserves every other top-level field (global, captures, vendor extensions,
// extra annotation keys) by reading the existing JSON, replacing only the
// "annotations" array, and rewriting atomically. On first save we leave a
// .sigmf-meta.bak so the user can recover the original.
bool InputSource::saveAnnotationsToSigMF(QString *errorOut)
{
    auto setErr = [&](const QString &m) { if (errorOut) *errorOut = m; };

    if (sigmfMetaFilename.isEmpty()) {
        setErr("No SigMF metadata file is associated with the current input "
               "(open a .sigmf-meta or .sigmf-data file to enable annotation save).");
        return false;
    }

    // Read the existing meta JSON so we don't clobber unrelated fields.
    QJsonObject root;
    QFile in(sigmfMetaFilename);
    if (in.open(QFile::ReadOnly)) {
        QJsonParseError parseErr;
        auto doc = QJsonDocument::fromJson(in.readAll(), &parseErr);
        in.close();
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            setErr(QString("Existing %1 is not valid JSON: %2")
                       .arg(sigmfMetaFilename, parseErr.errorString()));
            return false;
        }
        root = doc.object();
    } else {
        setErr(QString("Cannot read existing meta file %1: %2")
                   .arg(sigmfMetaFilename, in.errorString()));
        return false;
    }

    // Carry forward the same offset behaviour the loader applied: annotationList
    // sampleRanges are stored relative to (capture - core:offset). Add the
    // offset back when writing.
    size_t offset = 0;
    if (root.contains("global") && root["global"].isObject()) {
        auto global = root["global"].toObject();
        // Match the (currently-buggy) loader: it checks "core:offset" but reads
        // "offset". We preserve whatever behaviour the user actually saw on load
        // so round-tripping is stable.
        if (global.contains("core:offset")) {
            offset = static_cast<size_t>(global["offset"].toDouble());
        }
    }

    // Positionally pair current annotations with originals so vendor-namespaced
    // keys (dsp:*, application:*, etc.) on each annotation survive a save.
    // This is correct as long as the user only edited existing annotations.
    // Once create/delete UI lands we'll need explicit identity tracking.
    QJsonArray origAnnos;
    if (root.contains("annotations") && root["annotations"].isArray())
        origAnnos = root["annotations"].toArray();

    QJsonArray outAnnos;
    for (size_t i = 0; i < annotationList.size(); ++i) {
        const auto &a = annotationList[i];

        QJsonObject obj;
        if (i < static_cast<size_t>(origAnnos.size()) && origAnnos[int(i)].isObject())
            obj = origAnnos[int(i)].toObject();

        const size_t sampleStart = a.sampleRange.minimum + offset;
        const size_t sampleEnd = a.sampleRange.maximum + offset;
        const size_t sampleCount = (sampleEnd >= sampleStart)
            ? (sampleEnd - sampleStart + 1) : 0;

        obj["core:sample_start"] = static_cast<double>(sampleStart);
        obj["core:sample_count"] = static_cast<double>(sampleCount);
        obj["core:freq_lower_edge"] = a.frequencyRange.minimum;
        obj["core:freq_upper_edge"] = a.frequencyRange.maximum;

        if (a.label.isEmpty()) obj.remove("core:label");
        else obj["core:label"] = a.label;

        if (a.comment.isEmpty()) obj.remove("core:comment");
        else obj["core:comment"] = a.comment;

        if (a.boxColor.isValid()) {
            // Qt stores "#AARRGGBB"; SigMF expects "#RRGGBBAA". Mirror the
            // loader's conversion in reverse.
            QString qcol = a.boxColor.name(QColor::HexArgb); // "#AARRGGBB"
            if (qcol.length() == 9 && qcol.at(0) == '#') {
                QString sigmfCol = "#" + qcol.mid(3, 6) + qcol.mid(1, 2);
                obj["presentation:color"] = sigmfCol;
            }
        }
        outAnnos.append(obj);
    }
    root["annotations"] = outAnnos;

    // Back up the original on first save (don't keep overwriting the backup).
    QString bakFilename = sigmfMetaFilename + ".bak";
    if (!QFile::exists(bakFilename)) {
        QFile::copy(sigmfMetaFilename, bakFilename);
    }

    // Atomic-ish write: write to .tmp, then rename over the original.
    QString tmpFilename = sigmfMetaFilename + ".tmp";
    QFile::remove(tmpFilename);
    QFile out(tmpFilename);
    if (!out.open(QFile::WriteOnly | QIODevice::Text)) {
        setErr(QString("Cannot open %1 for writing: %2")
                   .arg(tmpFilename, out.errorString()));
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    out.close();

    QFile::remove(sigmfMetaFilename);
    if (!QFile::rename(tmpFilename, sigmfMetaFilename)) {
        setErr(QString("Wrote %1 but could not rename to %2")
                   .arg(tmpFilename, sigmfMetaFilename));
        return false;
    }

    return true;
}
