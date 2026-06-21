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

#include <QColor>
#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// Modal editor for a single SigMF annotation's user-facing fields: core:label,
// core:comment and presentation:color. Seeded from the current values; the
// caller reads label()/comment()/color() back after exec() == Accepted.
//
// color() returns an invalid QColor when "Set box color" is unchecked, which
// the writer maps to "omit presentation:color" so we never emit a bogus field.
class AnnotationDialog : public QDialog
{
    Q_OBJECT

public:
    AnnotationDialog(const QString &label, const QString &comment,
                     const QColor &color, QWidget *parent = nullptr);

    QString label() const;
    QString comment() const;
    QColor color() const;   // invalid if the colour checkbox is unchecked

private slots:
    void pickColour();
    void colourEnabledChanged(bool enabled);

private:
    void updateColourButton();

    QLineEdit *labelEdit;
    QPlainTextEdit *commentEdit;
    QCheckBox *colourEnabled;
    QPushButton *colourButton;
    QColor chosenColour;
};
