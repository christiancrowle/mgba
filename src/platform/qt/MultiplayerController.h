/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QMutex>
#include <QList>
#include <QObject>
#include <QImage>

#include <rfb/rfb.h>

#include <mgba/core/lockstep.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/sio/lockstep.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/sio/lockstep.h>
#endif

#include <memory>
#include <chrono>

struct GBSIOLockstepNode;
struct GBASIOLockstepNode;

namespace QGBA {

class CoreController;

class MultiplayerController : public QObject {
Q_OBJECT

public:
	MultiplayerController();
	~MultiplayerController();

	bool attachGame(CoreController*);
	void detachGame(CoreController*);

	int attached();
    int playerId(CoreController*);

    bool g_vncFrameReady = false;
    std::chrono::high_resolution_clock::time_point theTime;
    QImage g_firstScreenOutput{nullptr};
    QImage g_secondScreenOutput{nullptr};
    rfbScreenInfoPtr g_vnc;
    QImage g_vncoutput{nullptr};

signals:
	void gameAttached();
	void gameDetached();

private:
	struct Player {
#ifdef M_CORE_GB
		Player(CoreController* controller, GBSIOLockstepNode* node);
#endif
#ifdef M_CORE_GBA
		Player(CoreController* controller, GBASIOLockstepNode* node);
#endif

		CoreController* controller;
		GBSIOLockstepNode* gbNode = nullptr;
		GBASIOLockstepNode* gbaNode = nullptr;
		int awake = 1;
		int32_t cyclesPosted = 0;
		unsigned waitMask = 0;
	};
	union {
		mLockstep m_lockstep;
#ifdef M_CORE_GB
		GBSIOLockstep m_gbLockstep;
#endif
#ifdef M_CORE_GBA
		GBASIOLockstep m_gbaLockstep;
#endif
	};
    QList<Player> m_players;
    QMutex m_lock;
};

}
