#include "SaveConfirmDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QStyle>
#include <QPalette>

SaveConfirmDialog::SaveConfirmDialog(const QString &fileName, const QString &fullPath, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Save Confirmation"));
    setMinimumWidth(500);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(25, 25, 25, 25);
    mainLayout->setSpacing(15);

    // Text Content
    QLabel *headerLabel = new QLabel(tr("Save changes before closing?"), this);
    headerLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    mainLayout->addWidget(headerLabel);

    QLabel *msgLabel = new QLabel(this);
    msgLabel->setTextFormat(Qt::RichText);
    msgLabel->setWordWrap(true);
    msgLabel->setText(tr("Do you want to save the changes made to <span style='color: %1;'><b>%2</b></span>?")
                      .arg(palette().color(QPalette::Highlight).name(), fileName));
    msgLabel->setStyleSheet("font-size: 13px; line-height: 1.4;");
    mainLayout->addWidget(msgLabel);

    // Subtle path footer
    QString displayPath = (fullPath.isEmpty() || fullPath == tr("New untitled document")) 
                          ? tr("New untitled document") 
                          : fullPath;
    
    QLabel *pathLabel = new QLabel(displayPath, this);
    pathLabel->setStyleSheet("font-size: 11px; color: gray; font-style: italic;");
    pathLabel->setWordWrap(true);
    mainLayout->addWidget(pathLabel);

    // Separator
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet(QString("background-color: %1;").arg(palette().color(QPalette::Mid).name()));
    mainLayout->addWidget(line);

    // Warning footer (below separator)
    QLabel *infoLabel = new QLabel(tr("Any unsaved changes will be lost."), this);
    infoLabel->setStyleSheet("font-size: 12px; color: #888;");
    mainLayout->addWidget(infoLabel);

    // Buttons Layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    // Unify button styles
    QPushButton *btnSave = new QPushButton(tr("Save"), this);
    btnSave->setDefault(true);
    btnSave->setCursor(Qt::PointingHandCursor);

    QPushButton *btnDiscard = new QPushButton(tr("Don't Save"), this);
    btnDiscard->setCursor(Qt::PointingHandCursor);

    QPushButton *btnCancel = new QPushButton(tr("Cancel"), this);
    btnCancel->setCursor(Qt::PointingHandCursor);

    buttonLayout->addStretch();
    buttonLayout->addWidget(btnSave);
    buttonLayout->addWidget(btnDiscard);
    buttonLayout->addWidget(btnCancel);


    mainLayout->addLayout(buttonLayout);

    // Event handlers
    connect(btnSave, &QPushButton::clicked, this, [this](){ done(Save); });
    connect(btnDiscard, &QPushButton::clicked, this, [this](){ done(Discard); });
    connect(btnCancel, &QPushButton::clicked, this, [this](){ done(Cancel); });
}

