/***************************************************************************
 *                                                                         *
 *   This file is part of the FotoWall project,                            *
 *       http://code.google.com/p/fotowall                                 *
 *                                                                         *
 *   Copyright (C) 2009 by Enrico Ros <enrico.ros@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "PicturePropertiesItem.h"
#include "RenderOpts.h"
#include "ui_PicturePropertiesItem.h"
#include "frames/FrameFactory.h"
#include <QGraphicsSceneMouseEvent>
#include <QListWidgetItem>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QWidget>
#include <QSettings>

PicturePropertiesItem::PicturePropertiesItem(PictureContent * pictureContent, QGraphicsItem * parent)
    : QGraphicsProxyWidget(parent)
    , m_ui(new Ui::PicturePropertiesItem())
    , m_pictureContent(pictureContent)
    , m_frame(FrameFactory::defaultPanelFrame())
    , m_aniStep(0)
    , m_aniDirection(true)
{
    // WIDGET setup
    QWidget * widget = new QWidget();
#if QT_VERSION < 0x040500
    widget->setAttribute(Qt::WA_NoSystemBackground, true);
#else
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
#endif
    m_ui->setupUi(widget);
    m_ui->buttonBox->clear();
    QPushButton * applyButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogApplyButton), tr("Apply to All"));
    applyButton->setProperty("applyall", true);
    m_ui->buttonBox->addButton(applyButton, QDialogButtonBox::ApplyRole);
    QPushButton * closeButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogCloseButton), tr("Close"));
    m_ui->buttonBox->addButton(closeButton, QDialogButtonBox::RejectRole);

    // add frame items to the listview
    foreach (quint32 frameClass, FrameFactory::classes()) {
        // make icon from frame preview
        Frame * frame = FrameFactory::createFrame(frameClass);
        QIcon icon(frame->preview(48, 48));
        delete frame;

        // add the item to the list (and attach it the class)
        QListWidgetItem * item = new QListWidgetItem(icon, QString(), m_ui->listWidget);
        item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, frameClass);
    }

    // add effects items to the listview
    loadEffectsList();

    connect(m_ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(slotClose(QAbstractButton*)));
    connect(m_ui->listWidget, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(slotFrameSelected(QListWidgetItem*)));
    connect(m_ui->reflection, SIGNAL(toggled(bool)), this, SLOT(slotToggleMirror(bool)));
    connect(m_ui->front, SIGNAL(clicked()), m_pictureContent, SLOT(slotStackFront()));
    connect(m_ui->raise, SIGNAL(clicked()), m_pictureContent, SLOT(slotStackRaise()));
    connect(m_ui->lower, SIGNAL(clicked()), m_pictureContent, SLOT(slotStackLower()));
    connect(m_ui->back, SIGNAL(clicked()), m_pictureContent, SLOT(slotStackBack()));
    connect(m_ui->background, SIGNAL(clicked()), m_pictureContent, SIGNAL(backgroundMe()));
    connect(m_ui->invertButton, SIGNAL(clicked()), m_pictureContent, SLOT(slotFlipVertically()));
    connect(m_ui->flipButton, SIGNAL(clicked()), m_pictureContent, SLOT(slotFlipHorizontally()));
    connect(m_ui->save, SIGNAL(clicked()), m_pictureContent, SLOT(slotSave()));
    connect(m_ui->del, SIGNAL(clicked()), m_pictureContent, SIGNAL(deleteMe()), Qt::QueuedConnection);
    connect(m_ui->effectsListWidget, SIGNAL(itemActivated(QListWidgetItem *)), this, SLOT(slotEffectSelected(QListWidgetItem*)));

    // load values
    loadProperties();

    // ITEM setup
    setWidget(widget);
    setZValue(99999);

    // Transition setup
    m_aniTimer.start(20, this);
}

PicturePropertiesItem::~PicturePropertiesItem()
{
    delete m_frame;
    delete m_ui;
}

void PicturePropertiesItem::loadEffectsList()
{
    QListWidgetItem *item_invert = new QListWidgetItem(QIcon(":/data/effects-icons/invert-effect.png"), tr("Invert colors"), m_ui->effectsListWidget);
    item_invert->setToolTip(tr("Invert the colors of the picture"));
    item_invert->setData(Qt::UserRole, 0);
    QListWidgetItem *item_nvg = new QListWidgetItem(QIcon(":/data/effects-icons/nvg-effect.png"), tr("NVG"), m_ui->effectsListWidget);
    item_nvg->setToolTip(tr("Set the colors to levels of gray"));
    item_nvg->setData(Qt::UserRole, 1);
    QListWidgetItem *item_black = new QListWidgetItem(QIcon(":/data/effects-icons/black-and-white-effect.png"), tr("Black and White"), m_ui->effectsListWidget);
    item_black->setData(Qt::UserRole, 2);
    QListWidgetItem *no_effect = new QListWidgetItem(QIcon(":/data/effects-icons/no-effect.png"), tr("No effects"), m_ui->effectsListWidget);
    no_effect->setData(Qt::UserRole, 3);
}

PictureContent * PicturePropertiesItem::pictureContent() const
{
    return m_pictureContent;
}

void PicturePropertiesItem::loadProperties()
{
    // select the frame
    quint32 frameClass = m_pictureContent->frameClass();
    for (int i = 0; i < m_ui->listWidget->count(); ++i) {
        QListWidgetItem * item = m_ui->listWidget->item(i);
        if (item->data(Qt::UserRole).toUInt() == frameClass) {
            item->setSelected(true);
            break;
        }
    }

    // read other properties
    m_ui->reflection->setChecked(m_pictureContent->mirrorEnabled());
}

void PicturePropertiesItem::keepInBoundaries(const QRect & rect)
{
    QRect r = mapToScene(boundingRect()).boundingRect().toRect();
    r.setLeft(qBound(rect.left(), r.left(), rect.right() - r.width()));
    r.setTop(qBound(rect.top(), r.top(), rect.bottom() - r.height()));
    setPos(r.topLeft());
}

void PicturePropertiesItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    if (event->button() == Qt::RightButton)
        slotClose(0);
    QGraphicsProxyWidget::mousePressEvent(event);
}

void PicturePropertiesItem::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
    if (m_aniStep < 10)
        return;

    // draw custom background
#if QT_VERSION < 0x040500
    painter->fillRect(option->rect, QColor(250, 250, 250, 190));
#else
    m_frame->paint(painter, boundingRect().toRect(), false);
#endif

    // unbreak parent
#if 0
    if (m_aniStep >= 10 && m_aniStep <= 90)
        painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
#endif
    QGraphicsProxyWidget::paint(painter, option, widget);
}

void PicturePropertiesItem::timerEvent(QTimerEvent * event)
{
    // only act on our events
    if (event->timerId() == m_aniTimer.timerId()) {
        if (m_aniDirection) {
            m_aniStep += 10;
            // end of FadeIn
            if (m_aniStep >= 100) {
                m_aniStep = 100;
                resetTransform();
                m_aniTimer.stop();
                return;
            }
            qreal xCenter = boundingRect().center().x();
            setTransform(QTransform().translate(xCenter, 0).rotate((90*(100-m_aniStep)*(100-m_aniStep)) / 10000, Qt::XAxis).translate(-xCenter, 0));
        } else {
            m_aniStep -= 10;
            // end of FadeOut
            if (m_aniStep <= 0) {
                m_aniStep = 0;
                resetTransform();
                m_aniTimer.stop();
                emit closed();
                return;
            }
            qreal xCenter = boundingRect().center().x();
            qreal yCenter = boundingRect().center().y();
            setTransform(QTransform().translate(xCenter, yCenter).rotate(-90 + (90*m_aniStep*m_aniStep) / 10000, Qt::XAxis).translate(-xCenter, -yCenter));
        }
    }
    QObject::timerEvent(event);
}

void PicturePropertiesItem::slotEffectSelected(QListWidgetItem * item)
{
    // get the effect class
    if (!item)
        return;
    quint32 effectClass = item->data(Qt::UserRole).toUInt();

    // apply the effect
    m_pictureContent->setEffect(effectClass);
}

void PicturePropertiesItem::slotFrameSelected(QListWidgetItem * item)
{
    // get the frame class
    if (!item)
        return;
    quint32 frameClass = item->data(Qt::UserRole).toUInt();
    if (!frameClass)
        return;

    // create and set the frame
    Frame * frame = FrameFactory::createFrame(frameClass);
    if (frame)
        m_pictureContent->setFrame(frame);
}

void PicturePropertiesItem::slotToggleMirror(bool enabled)
{
    RenderOpts::LastMirrorEnabled = enabled;
    m_pictureContent->setMirrorEnabled(enabled);
}

void PicturePropertiesItem::slotClose(QAbstractButton * button)
{
    // apply to all if pressed
    if (button && button->property("applyall").toBool() == true) {
        emit applyAll(m_pictureContent->frameClass(), m_pictureContent->mirrorEnabled());

        QList<QListWidgetItem *> selectedEffects = m_ui->effectsListWidget->selectedItems();
        QList<QListWidgetItem *>::iterator it = selectedEffects.begin();
        for (; it != selectedEffects.end(); it++) {
            int effectClass = (*it)->data(Qt::UserRole).toUInt();
            emit applyEffectToAll(effectClass);
        }
    }

    // closure animation
    m_aniDirection = false;
    m_aniTimer.start(20, this);
}