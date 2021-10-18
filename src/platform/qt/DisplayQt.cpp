/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayQt.h"

#include "CoreController.h"
#include "utils.h"

#include <QPainter>

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba-util/math.h>
#include "MultiplayerController.h"

#include <fstream>
#include <iostream>

#include <rfb/rfb.h>

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
{
}

void processRfbKeyEvent(rfbBool down, rfbKeySym keySym, struct _rfbClientRec *cl) {
    if (down) {
        std::cout << "we got a key! " << keySym << std::endl;
    }
}

int id = 1;
void DisplayQt::startDrawing(std::shared_ptr<CoreController> controller) {
	QSize size = controller->screenDimensions();
	m_width = size.width();
	m_height = size.height();
	m_backing = QImage();
	m_oldBacking = QImage();
	m_isDrawing = true;
	m_context = controller;

    m_context->multiplayerController()->g_secondScreenOutput = QImage(m_width, m_height, QImage::Format_ARGB32);
    m_context->multiplayerController()->g_vncoutput = QImage(m_width * 2, m_height, QImage::Format_ARGB32);

    if (controller->multiplayerController()) { // are we multiplayer rn
       if (id == 1) { // we only need one server
          m_isSecondDisplay = false;
          m_context->multiplayerController()->g_vnc = rfbGetScreen(0, 0, size.width() * 2, size.height(), 8, 3, 4);
          m_context->multiplayerController()->g_vnc->frameBuffer = reinterpret_cast<char*>(malloc(size.width() * 2 * size.height() * 4));

          static const char* passwords[2] = {"weed",0};
          m_context->multiplayerController()->g_vnc->authPasswdData = (void*)passwords;
          m_context->multiplayerController()->g_vnc->passwordCheck = rfbCheckPasswordByList;
          m_context->multiplayerController()->g_vnc->port = 18008;
          m_context->multiplayerController()->g_vnc->httpDir = "web/";
          //m_context->multiplayerController()->g_vnc->httpPort = 28008;
          m_context->multiplayerController()->g_vnc->desktopName = "GBAIP";

          m_context->multiplayerController()->g_vnc->kbdAddEvent = processRfbKeyEvent;

          rfbInitServer(m_context->multiplayerController()->g_vnc);
          rfbRunEventLoop(m_context->multiplayerController()->g_vnc, 100, TRUE); // still have no fucking clue what the `us` param does, but increasing it makes the code go faster lmao
       }
       id++;
    }

	emit drawingStarted();
}

void DisplayQt::stopDrawing() {
	m_isDrawing = false;
	m_context.reset();
}

void DisplayQt::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	update();
}

void DisplayQt::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
	update();
}

void DisplayQt::interframeBlending(bool lock) {
	Display::interframeBlending(lock);
	update();
}

void DisplayQt::filter(bool filter) {
	Display::filter(filter);
	update();
}

int frameCounter = 0;
void DisplayQt::framePosted() {
    frameCounter++;
	update();
	const color_t* buffer = m_context->drawContext();
	if (const_cast<const QImage&>(m_backing).bits() == reinterpret_cast<const uchar*>(buffer)) {
		return;
	}
	m_oldBacking = m_backing;

    //m_backing.save("fuck.jpg");

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB16);
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB555);
#endif
#else
    m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_ARGB32);
    m_backing = m_backing.convertToFormat(QImage::Format_RGB32);
#endif
    if (!m_isSecondDisplay) {
        m_context->multiplayerController()->g_firstScreenOutput = m_backing.copy(0, 0, m_width, m_height);
    } else {
        m_context->multiplayerController()->g_secondScreenOutput = m_backing.copy(0, 0, m_width, m_height);
    }
#ifndef COLOR_5_6_5
    m_backing = m_backing.rgbSwapped();
#endif
}

void DisplayQt::resizeContext() {
	if (!m_context) {
		return;
	}
	QSize size = m_context->screenDimensions();
	if (m_width != size.width() || m_height != size.height()) {
		m_width = size.width();
		m_height = size.height();
		m_oldBacking = std::move(QImage());
		m_backing = std::move(QImage());
	}
}

void DisplayQt::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (isFiltered()) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
	}
	QRect full(clampSize(QSize(m_width, m_height), size(), isAspectRatioLocked(), isIntegerScalingLocked()));

	if (hasInterframeBlending()) {
		painter.drawImage(full, m_oldBacking, QRect(0, 0, m_width, m_height));
		painter.setOpacity(0.5);
	}
	painter.drawImage(full, m_backing, QRect(0, 0, m_width, m_height));
	painter.setOpacity(1);
	if (isShowOSD()) {
		messagePainter()->paint(&painter);
    }

    if (!m_isSecondDisplay) {
        memcpy(m_context->multiplayerController()->g_vnc->frameBuffer, reinterpret_cast<char*>(m_context->multiplayerController()->g_vncoutput.bits()), m_width * 2 * m_height * 4);
        rfbMarkRectAsModified(m_context->multiplayerController()->g_vnc, 0, 0, m_width * 2, m_height);
    }
}
