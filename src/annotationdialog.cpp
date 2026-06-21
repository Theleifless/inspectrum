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

#include "annotationdialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

AnnotationDialog::AnnotationDialog(const QString &label, const QString &comment,
                                   const QColor &color, QWidget *parent)
    : QDialog(parent), chosenColour(color)
{
    setWindowTitle(tr("Edit Annotation"));

    auto *form = new QFormLayout;

    labelEdit = new QLineEdit(label, this);
    form->addRow(tr("Label (core:label):"), labelEdit);

    commentEdit = new QPlainTextEdit(comment, this);
    commentEdit->setTabChangesFocus(true);
    form->addRow(tr("Comment (core:comment):"), commentEdit);

    colourEnabled = new QCheckBox(tr("Set box colour (presentation:color)"), this);
    colourEnabled->setChecked(color.isValid());
    form->addRow(colourEnabled);

    colourButton = new QPushButton(this);
    connect(colourButton, &QPushButton::clicked, this, &AnnotationDialog::pickColour);
    form->addRow(tr("Colour:"), colourButton);

    connect(colourEnabled, &QCheckBox::toggled, this, &AnnotationDialog::colourEnabledChanged);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(buttons);

    colourButton->setEnabled(colourEnabled->isChecked());
    updateColourButton();
}

QString AnnotationDialog::label() const
{
    return labelEdit->text();
}

QString AnnotationDialog::comment() const
{
    return commentEdit->toPlainText();
}

QColor AnnotationDialog::color() const
{
    return colourEnabled->isChecked() ? chosenColour : QColor();
}

void AnnotationDialog::pickColour()
{
    QColor initial = chosenColour.isValid() ? chosenColour : QColor(Qt::white);
    QColor picked = QColorDialog::getColor(initial, this, tr("Annotation Colour"),
                                           QColorDialog::ShowAlphaChannel);
    if (picked.isValid()) {
        chosenColour = picked;
        updateColourButton();
    }
}

void AnnotationDialog::colourEnabledChanged(bool enabled)
{
    colourButton->setEnabled(enabled);
    updateColourButton();
}

void AnnotationDialog::updateColourButton()
{
    if (colourEnabled->isChecked() && chosenColour.isValid()) {
        // Show the chosen colour as the button's background plus its hex value,
        // picking readable text by luminance.
        QString fg = (chosenColour.lightnessF() > 0.5) ? "black" : "white";
        colourButton->setText(chosenColour.name(QColor::HexArgb));
        colourButton->setStyleSheet(
            QString("background-color: %1; color: %2;")
                .arg(chosenColour.name(QColor::HexArgb), fg));
    } else {
        colourButton->setText(tr("(none)"));
        colourButton->setStyleSheet(QString());
    }
}
