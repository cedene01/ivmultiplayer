//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: VehicleNatives.h
// Project: Server.Core
// Author(s): jenksta
//            MaVe
// License: See LICENSE in root directory
//
//==============================================================================

#pragma once

#include "../Natives.h"

class CVehicleNatives
{
private:
	static SQInteger GetCoordinates(SQVM * pVM);
	static SQInteger GetRotation(SQVM * pVM);
	static SQInteger IsValid(SQVM * pVM);
	static SQInteger GetColor(SQVM * pVM);
	static SQInteger GetModel(SQVM * pVM);
	static SQInteger GetHealth(SQVM * pVM);
	static SQInteger GetEngineHealth(SQVM * pVM);
	static SQInteger GetVelocity(SQVM * pVM);
	static SQInteger GetAngularVelocity(SQVM * pVM);
	static SQInteger GetLocked(SQVM * pVM);
public:
	static void      Register(CScriptingManager * pScriptingManager);
};