//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: CVehicle.cpp
// Project: Server.Core
// Author(s): jenksta
// License: See LICENSE in root directory
//
//==============================================================================

#include "CVehicle.h"
#include "CNetworkManager.h"
#include "CPlayerManager.h"

extern CNetworkManager * g_pNetworkManager;
extern CPlayerManager * g_pPlayerManager;

CVehicle::CVehicle(EntityId vehicleId, int iModelId, CVector3 vecSpawnPosition, CVector3 vecSpawnRotation, BYTE byteColor1, BYTE byteColor2, BYTE byteColor3, BYTE byteColor4)
{
	m_vehicleId = vehicleId;
	m_iModelId = iModelId;
	memcpy(&m_vecSpawnPosition, &vecSpawnPosition, sizeof(CVector3));
	memcpy(&m_vecSpawnRotation, &vecSpawnRotation, sizeof(CVector3));
	m_byteSpawnColors[0] = byteColor1;
	m_byteSpawnColors[1] = byteColor2;
	m_byteSpawnColors[2] = byteColor3;
	m_byteSpawnColors[3] = byteColor4;
	Reset();
	SpawnForWorld();
}

CVehicle::~CVehicle()
{
	DestroyForWorld();
}

void CVehicle::Reset()
{
	m_pDriver = NULL;
	memset(m_pPassengers, 0, sizeof(m_pPassengers));
	m_uiHealth = 1000;
	memcpy(&m_vecPosition, &m_vecSpawnPosition, sizeof(CVector3));
	memcpy(&m_vecRotation, &m_vecSpawnRotation, sizeof(CVector3));
	memset(&m_vecTurnSpeed, 0, sizeof(CVector3));
	memset(&m_vecMoveSpeed, 0, sizeof(CVector3));
	memcpy(m_byteColors, m_byteSpawnColors, sizeof(m_byteColors));
	memset(&m_bIndicatorState, 0, sizeof(m_bIndicatorState));
	ResetComponents(false);
	m_fDirtLevel = 0.0f;
	m_bHazzardState = false;
	m_iHornDuration = 0;
	m_bSirenState = false;
	m_ucVariation = 0;
	m_ucLocked = 0;
	m_bEngineStatus = false;
}

void CVehicle::SpawnForPlayer(EntityId playerId)
{
	CBitStream bsSend;
	bsSend.WriteCompressed(m_vehicleId);
	bsSend.Write(m_iModelId);
	bsSend.Write(m_uiHealth);
	bsSend.Write(m_vecPosition);

	if(!m_vecRotation.IsEmpty())
	{
		bsSend.Write1();
		bsSend.Write(m_vecRotation);
	}
	else
		bsSend.Write0();

	if(!m_vecTurnSpeed.IsEmpty())
	{
		bsSend.Write1();
		bsSend.Write(m_vecTurnSpeed);
	}
	else
		bsSend.Write0();

	if(!m_vecMoveSpeed.IsEmpty())
	{
		bsSend.Write1();
		bsSend.Write(m_vecMoveSpeed);
	}
	else
		bsSend.Write0();

	bsSend.Write((char *)m_byteColors, sizeof(m_byteColors));
	bsSend.Write(m_fDirtLevel);
	for(int i = 0; i < 4; ++ i)
	{
		if(m_bIndicatorState[i])
			bsSend.Write1();
		else
			bsSend.Write0();
	}
	for(int i = 0; i < 9; ++ i)
	{
		if(m_bComponents[i])
			bsSend.Write1();
		else
			bsSend.Write0();
	}
	bsSend.Write(m_iHornDuration);
	bsSend.Write(m_bSirenState);
	bsSend.Write(m_ucLocked);
	m_bEngineStatus ? bsSend.Write1() : bsSend.Write0();
	
	if(m_ucVariation != 0)
	{
		bsSend.Write1();
		bsSend.Write(m_ucVariation);
	}
	else
		bsSend.Write0();

	g_pNetworkManager->RPC(RPC_NewVehicle, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, playerId, false);
}

void CVehicle::DestroyForPlayer(EntityId playerId)
{
	CBitStream bsSend;
	bsSend.WriteCompressed(m_vehicleId);
	g_pNetworkManager->RPC(RPC_DeleteVehicle, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, playerId, false);
}

void CVehicle::SpawnForWorld()
{
	for(EntityId i = 0; i < MAX_PLAYERS; i++)
	{
		if(g_pPlayerManager->DoesExist(i))
			SpawnForPlayer(i);
	}
}

void CVehicle::DestroyForWorld()
{
	for(EntityId i = 0; i < MAX_PLAYERS; i++)
	{
		if(g_pPlayerManager->DoesExist(i))
			DestroyForPlayer(i);
	}
}

bool CVehicle::IsOccupied()
{
	// Do we have a driver?
	if(m_pDriver)
		return true;
	else
	{
		// Loop through all passenger seats
		for(BYTE i = 0; i < MAX_VEHICLE_PASSENGERS; i++)
		{
			// Do we have a passenger in this seat?
			if(m_pPassengers[i])
				return true;
		}
	}

	// Vehicle is not occupied
	return false;
}

void CVehicle::SetPassenger(BYTE byteSeatId, CPlayer * pPassenger)
{
	if(byteSeatId < MAX_VEHICLE_PASSENGERS)
		return;

	m_pPassengers[byteSeatId] = pPassenger;
}

CPlayer * CVehicle::GetPassenger(BYTE byteSeatId)
{
	if(byteSeatId >= MAX_VEHICLE_PASSENGERS)
		return NULL;

	return m_pPassengers[byteSeatId];
}

void CVehicle::SetOccupant(BYTE byteSeatId, CPlayer * pOccupant)
{
	if(byteSeatId == 0)
		SetDriver(pOccupant);
	else
		SetPassenger((byteSeatId - 1), pOccupant);
}

CPlayer * CVehicle::GetOccupant(BYTE byteSeatId)
{
	if(byteSeatId == 0)
		return GetDriver();

	return GetPassenger(byteSeatId - 1);
}

void CVehicle::Respawn()
{
	DestroyForWorld();
	Reset();
	SpawnForWorld();
}

void CVehicle::StoreInVehicleSync(InVehicleSyncData * syncPacket)
{
	m_vecPosition = syncPacket->vecPos;
	m_vecRotation = syncPacket->vecRotation;
	m_uiHealth = syncPacket->uiHealth;
	memcpy(m_byteColors, syncPacket->byteColors, sizeof(m_byteColors));
	memcpy(&m_vecTurnSpeed, &syncPacket->vecTurnSpeed, sizeof(CVector3));
	memcpy(&m_vecMoveSpeed, &syncPacket->vecMoveSpeed, sizeof(CVector3));
	m_fDirtLevel = syncPacket->fDirtLevel;
	m_bSirenState = syncPacket->bSirenState;
	m_bEngineStatus = syncPacket->bEngineStatus;
}

void CVehicle::StorePassengerSync(PassengerSyncData * syncPacket)
{
	
}


void CVehicle::SetModel(int iModelId)
{
	m_iModelId = iModelId;
	ResetComponents();
	// TODO: RPC
}

int CVehicle::GetModel()
{
	return m_iModelId;
}

void CVehicle::SetHealth(unsigned int uHealth)
{
	m_uiHealth = uHealth;
	CBitStream bsSend;
	bsSend.WriteCompressed(m_vehicleId);
	bsSend.Write(uHealth);
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleHealth, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

unsigned int CVehicle::GetHealth()
{
	return m_uiHealth;
}

void CVehicle::SetPosition(CVector3 vecPosition)
{
	memcpy(&m_vecPosition, &vecPosition, sizeof(CVector3));
	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write(vecPosition);
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleCoordinates, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

void CVehicle::GetPosition(CVector3 * vecPosition)
{
	memcpy(vecPosition, &m_vecPosition, sizeof(CVector3));
}

void CVehicle::SetRotation(CVector3 vecRotation)
{
	memcpy(&m_vecRotation, &vecRotation, sizeof(CVector3));
	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write(vecRotation);
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleRotation, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

void CVehicle::GetRotation(CVector3 * vecRotation)
{
	memcpy(vecRotation, &m_vecRotation, sizeof(CVector3));
}

void CVehicle::SetDirtLevel(float fDirtLevel)
{
	m_fDirtLevel = fDirtLevel;
	
	CBitStream bsSend;

	bsSend.Write(m_vehicleId);

	bsSend.Write(fDirtLevel);

	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleDirtLevel, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

float CVehicle::GetDirtLevel()
{
	return m_fDirtLevel;
}

void CVehicle::SetTurnSpeed(CVector3 vecTurnSpeed)
{
	memcpy(&m_vecTurnSpeed, &vecTurnSpeed, sizeof(CVector3));
	// TODO: RPC
}

void CVehicle::GetTurnSpeed(CVector3 * vecTurnSpeed)
{
	memcpy(vecTurnSpeed, &m_vecTurnSpeed, sizeof(CVector3));
}

void CVehicle::SetMoveSpeed(CVector3 vecMoveSpeed)
{
	memcpy(&m_vecMoveSpeed, &vecMoveSpeed, sizeof(CVector3));
	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write(vecMoveSpeed);
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleMoveSpeed, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

void CVehicle::GetMoveSpeed(CVector3 * vecMoveSpeed)
{
	memcpy(vecMoveSpeed, &m_vecMoveSpeed, sizeof(CVector3));
}

void CVehicle::SetColors(BYTE byteColor1, BYTE byteColor2, BYTE byteColor3, BYTE byteColor4)
{
	m_byteColors[0] = byteColor1;
	m_byteColors[1] = byteColor2;
	m_byteColors[2] = byteColor3;
	m_byteColors[3] = byteColor4;
	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write((char *)m_byteColors, sizeof(m_byteColors));
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleColor, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

void CVehicle::GetColors(BYTE &byteColor1, BYTE &byteColor2, BYTE &byteColor3, BYTE &byteColor4)
{
	byteColor1 = m_byteColors[0];
	byteColor2 = m_byteColors[1];
	byteColor3 = m_byteColors[2];
	byteColor4 = m_byteColors[3];
}

void CVehicle::SoundHorn(unsigned int iDuration)
{
	m_iHornDuration = iDuration;
	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write(iDuration);
	g_pNetworkManager->RPC(RPC_ScriptingSoundVehicleHorn, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

void CVehicle::SetSirenState(bool bSirenState)
{
	m_bSirenState = bSirenState;

	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write(bSirenState);
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleSirenState, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}


bool CVehicle::GetSirenState()
{
	return m_bSirenState;
}

bool CVehicle::SetLocked(unsigned char ucLocked)
{
	if(ucLocked <= 2)
	{
		m_ucLocked = ucLocked;

		CBitStream bsSend;
		bsSend.Write(m_vehicleId);
		bsSend.Write(m_ucLocked);

		g_pNetworkManager->RPC(RPC_ScriptingSetVehicleLocked, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
		return true;
	}

	return false;
}

unsigned char CVehicle::GetLocked()
{
	return m_ucLocked;
}

void CVehicle::SetIndicatorState(bool bFrontLeft, bool bFrontRight, bool bBackLeft, bool bBackRight)
{
	m_bIndicatorState[0] = bFrontLeft;
	m_bIndicatorState[1] = bFrontRight;
	m_bIndicatorState[2] = bBackLeft;
	m_bIndicatorState[3] = bBackRight;

	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write((unsigned char)(bFrontLeft + 2*bFrontRight + 4*bBackLeft + 8*bBackRight));

	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleIndicators, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}

bool CVehicle::GetIndicatorState(unsigned char ucSlot)
{
	if(ucSlot >= 0 && ucSlot <= 3)
	{
		return m_bIndicatorState[ucSlot];
	}
	
	return false;
}

void CVehicle::SetComponentState(unsigned char ucSlot, bool bOn)
{
	if(ucSlot >= 0 && ucSlot <= 8)
	{
		if(m_bComponents[ucSlot] != bOn)
		{
			m_bComponents[ucSlot] = bOn;

			CBitStream bsSend;
			bsSend.Write(m_vehicleId);
			for(int i = 0; i < 9; ++ i)
			{
				if(m_bComponents[i])
					bsSend.Write1();
				else
					bsSend.Write0();
			}

			g_pNetworkManager->RPC(RPC_ScriptingSetVehicleComponents, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
		}
	}
}

void CVehicle::ResetComponents(bool bNotify)
{
	memset(&m_bComponents, 0, sizeof(m_bComponents));

	// Make sure some default components are added (they can be removed)
	switch(m_iModelId)
	{
		case 4: // Benson
		case 51: // Mule
		case 85: // Steed
		case 104: // Yankee
			m_bComponents[0] = true; // missing half of it otherwise, could chose anything from 0 on
			break;
		case 6: // Blista
		case 26: // Faction
		case 76: // Ruiner
		case 87: // Stratum
			m_bComponents[1] = true; // Roof window
			break;
		case 13: // Cabby
			m_bComponents[5] = true; // Taxi sign
			break;
		case 35: // Futo
			m_bComponents[0] = true; // driver seat
			break;
		case 52: // Noose
		case 63: // Police
		case 64: // Police 2
			m_bComponents[0] = true; // Sirens
			break;
		case 89: // Sultan
			m_bComponents[1] = true; // Air vent
			break;
		case 92: // Taxi
		case 93: // Taxi 2
			m_bComponents[4] = true; // Taxi sign
			break;
		case 98: // Vigero (trashed)
			m_bComponents[2] = true;
			break;
		case 105: // Freeway
		case 109: // PCJ
			m_bComponents[0] = true; // Tank
			m_bComponents[6] = true; // Exhaust
			break;
		case 108: // NRG
			m_bComponents[3] = true; // Body
			m_bComponents[7] = true; // Exhaust
			break;
		case 112: // Annihilator
		case 113: // Maverick
		case 114: // Police Maverick
		case 115: // Heli-Tours Maverick
			m_bComponents[0] = true; // Rotor blades
			break;
		case 123: // Tropic
			m_bComponents[4] = true;
			break;
	}

	if(bNotify)
	{
		CBitStream bsSend;
		bsSend.Write(m_vehicleId);
		for(int i = 0; i < 9; ++ i)
		{
			if(m_bComponents[i])
				bsSend.Write1();
			else
				bsSend.Write0();
		}

		g_pNetworkManager->RPC(RPC_ScriptingSetVehicleComponents, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
	}
}

bool CVehicle::GetComponentState(unsigned char ucSlot)
{
	if(ucSlot >= 0 && ucSlot <= 8)
	{
		return m_bComponents[ucSlot];
	}

	return false;
}

void CVehicle::SetVariation(unsigned char ucVariation)
{
	if(m_ucVariation != ucVariation)
	{
		m_ucVariation = ucVariation;

		CBitStream bsSend;
		bsSend.Write(m_vehicleId);
		bsSend.Write(ucVariation);

		g_pNetworkManager->RPC(RPC_ScriptingSetVehicleVariation, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
	}
}

unsigned char CVehicle::GetVariation()
{
	return m_ucVariation;
}

void CVehicle::SetEngineStatus(bool bEngineStatus)
{
	m_bEngineStatus = bEngineStatus;

	CBitStream bsSend;
	bsSend.Write(m_vehicleId);
	bsSend.Write(bEngineStatus);
	g_pNetworkManager->RPC(RPC_ScriptingSetVehicleEngineState, &bsSend, PRIORITY_HIGH, RELIABILITY_RELIABLE_ORDERED, INVALID_ENTITY_ID, true);
}


bool CVehicle::GetEngineStatus()
{
	return m_bEngineStatus;
}