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
#include "InputController.h"

#include <fstream>
#include <iostream>

#include <rfb/rfb.h>

#include "keysymdef.h"

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
{
}

                 // A      B      Left   Right  Up     Down   LeftS  RightS Select Start
                 // A      B      Left   Right  Up     Down   L      R      1      2
bool keysDown[] = { false, false, false, false, false, false, false, false, false, false,
                 // C      D      5      6      7      8      N      S      3      4
                    false, false, false, false, false, false, false, false, false, false };
bool shouldSetFF = false;
void processRfbKeyEvent(rfbBool down, rfbKeySym keySym, struct _rfbClientRec *cl) {
    if (down) {
        std::cout << "we got a key! " << keySym << std::endl;
    } else {
        std::cout << "key released! " << keySym << std::endl;
    }

    switch (keySym) {
    case XK_A:
        keysDown[0] = down;
        break;
    case XK_B:
        keysDown[1] = down;
        break;
    case XK_KP_Left:
        keysDown[2] = down;
        break;
    case XK_KP_Right:
        keysDown[3] = down;
        break;
    case XK_KP_Up:
        keysDown[4] = down;
        break;
    case XK_KP_Down:
        keysDown[5] = down;
        break;
    case XK_L:
        keysDown[6] = down;
        break;
    case XK_R:
        keysDown[7] = down;
        break;
    case XK_1:
        keysDown[8] = down;
        break;
    case XK_2:
        keysDown[9] = down;
        break;
    case XK_C:
        keysDown[10] = down;
        break;
    case XK_D:
        keysDown[11] = down;
        break;
    case XK_5:
        keysDown[12] = down;
        break;
    case XK_6:
        keysDown[13] = down;
        break;
    case XK_7:
        keysDown[14] = down;
        break;
    case XK_8:
        keysDown[15] = down;
        break;
    case XK_N:
        keysDown[16] = down;
        break;
    case XK_S:
        keysDown[17] = down;
        break;
    case XK_3:
        keysDown[18] = down;
        break;
    case XK_4:
        keysDown[19] = down;
        break;
    case XK_X:
        shouldSetFF = down;
        break;
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
          m_context->multiplayerController()->g_vnc->deferUpdateTime = 0;

          m_context->multiplayerController()->g_vnc->kbdAddEvent = processRfbKeyEvent;

          rfbInitServer(m_context->multiplayerController()->g_vnc);
          rfbRunEventLoop(m_context->multiplayerController()->g_vnc, 1, TRUE); // still have no fucking clue what the `us` param does, but increasing it makes the code go faster lmao
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

void updateKey(std::shared_ptr<CoreController> m_context, int i, GBAKey key) {
    if (keysDown[i])
        m_context->addKey(key);
    else
        m_context->clearKey(key);
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

#ifndef COLOR_5_6_5
    m_backing = m_backing.rgbSwapped();
#endif

    if (!m_isSecondDisplay) {
        updateKey(m_context, 0, GBA_KEY_A);
        updateKey(m_context, 1, GBA_KEY_B);
        updateKey(m_context, 2, GBA_KEY_LEFT);
        updateKey(m_context, 3, GBA_KEY_RIGHT);
        updateKey(m_context, 4, GBA_KEY_UP);
        updateKey(m_context, 5, GBA_KEY_DOWN);
        updateKey(m_context, 6, GBA_KEY_L);
        updateKey(m_context, 7, GBA_KEY_R);
        updateKey(m_context, 8, GBA_KEY_SELECT);
        updateKey(m_context, 9, GBA_KEY_START);
    } else {
        updateKey(m_context, 10, GBA_KEY_A);
        updateKey(m_context, 11, GBA_KEY_B);
        updateKey(m_context, 12, GBA_KEY_LEFT);
        updateKey(m_context, 13, GBA_KEY_RIGHT);
        updateKey(m_context, 14, GBA_KEY_UP);
        updateKey(m_context, 15, GBA_KEY_DOWN);
        updateKey(m_context, 16, GBA_KEY_L);
        updateKey(m_context, 17, GBA_KEY_R);
        updateKey(m_context, 18, GBA_KEY_SELECT);
        updateKey(m_context, 19, GBA_KEY_START);
    }

    //if (shouldSetFF) {
    //m_context->setFastForward(shouldSetFF);
    //m_context->forceFastForward(shouldSetFF);
    //}

    frameCount++;
    std::cout << frameCount << std::endl;
    if (frameCount == 2) {
        if (!m_isSecondDisplay) {
            m_context->multiplayerController()->g_firstScreenOutput = m_backing.rgbSwapped().copy(0, 0, m_width, m_height);
            m_context->multiplayerController()->g_vncFrameReady = true;
            QPainter painter(&m_context->multiplayerController()->g_vncoutput);
            painter.drawImage(0, 0, m_context->multiplayerController()->g_firstScreenOutput);
            painter.drawImage(m_context->multiplayerController()->g_secondScreenOutput.width(), 0, m_context->multiplayerController()->g_secondScreenOutput);

            memcpy(m_context->multiplayerController()->g_vnc->frameBuffer, reinterpret_cast<char*>(m_context->multiplayerController()->g_vncoutput.bits()), m_width * 2 * m_height * 4);
        } else {
            m_context->multiplayerController()->g_secondScreenOutput = m_backing.rgbSwapped().copy(0, 0, m_width, m_height);
        }

        rfbMarkRectAsModified(m_context->multiplayerController()->g_vnc, 0, 0, m_width * 2, m_height);
        frameCount = 0;
    }
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
}
