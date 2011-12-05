//============== IV: Multiplayer - http://code.iv-multiplayer.com ==============
//
// File: Main.cpp
// Project: Client.Core
// Author(s): jenksta
//            Einstein
//            MaVe
// License: See LICENSE in root directory
//
//==============================================================================

#include <winsock2.h>
#include <windows.h>
#include <d3d9.h>
#include <CLogFile.h>
#include "CGame.h"
#include "CXLiveHook.h"
#include "CChatWindow.h"
#include "CInputWindow.h"
#include "CDirect3DHook.h"
#include "CDirectInputHook.h"
#include "CCursorHook.h"
#include "CNetworkManager.h"
#include "CPlayerManager.h"
#include "CLocalPlayer.h"
#include "CVehicleManager.h"
#include "CObjectManager.h"
#include "CBlipManager.h"
#include "CActorManager.h"
#include "CCheckpointManager.h"
#include "CPickupManager.h"
#include "CModelManager.h"
#include "Scripting.h"
#include "Commands.h"
#include "CCamera.h"
#include "CGUI.h"
#include <CSettings.h>
#include "CClientScriptManager.h"
#include "CMainMenu.h"
#include "CFPSCounter.h"
#include "SharedUtility.h"
#include "CFileTransfer.h"
#include "CGraphics.h"
#include "KeySync.h"
#include "CStreamer.h"
#include <Game/CTime.h>
#include "CEvents.h"
#include <Game/CTrafficLights.h>
#include <Network/CNetworkModule.h>
#include "CCredits.h"
#include "CNameTags.h"
#include "CClientTaskManager.h"
#include "CPools.h"
#include "CIVWeather.h"
#include <CExceptionHandler.h>
#include "CScreenShot.h"

IDirect3DDevice9     * g_pDevice = NULL;
CChatWindow          * g_pChatWindow = NULL;
CInputWindow         * g_pInputWindow = NULL;
CNetworkManager      * g_pNetworkManager = NULL;
CPlayerManager       * g_pPlayerManager = NULL;
CLocalPlayer         * g_pLocalPlayer = NULL;
CVehicleManager      * g_pVehicleManager = NULL;
CObjectManager       * g_pObjectManager = NULL;
CBlipManager         * g_pBlipManager = NULL;
CActorManager        * g_pActorManager = NULL;
CCheckpointManager   * g_pCheckpointManager = NULL;
CPickupManager       * g_pPickupManager = NULL;
CModelManager        * g_pModelManager = NULL;
CCamera              * g_pCamera = NULL;
CGUI                 * g_pGUI = NULL;
CGraphics            * g_pGraphics = NULL;
CScriptingManager    * g_pScriptingManager = NULL;
CClientScriptManager * g_pClientScriptManager = NULL;
CMainMenu            * g_pMainMenu = NULL;
CFPSCounter          * g_pFPSCounter = NULL;
CGUIStaticText       * g_pVersionIdentifier = NULL;
CFileTransfer        * g_pFileTransfer = NULL;
CStreamer            * g_pStreamer = NULL;
CTime                * g_pTime = NULL;
CEvents              * g_pEvents = NULL;
CTrafficLights       * g_pTrafficLights = NULL;
CCredits             * g_pCredits = NULL;
CNameTags            * g_pNameTags = NULL;
CClientTaskManager   * g_pClientTaskManager = NULL;

bool           g_bWindowedMode = false;
bool           g_bFPSToggle = false;
unsigned short g_usPort;
String         g_strHost;
String         g_strNick;
String         g_strPassword;

// TODO: Move this to another class?
extern float fTextPos[2];
extern String strTextText;
extern int iTextTime;
extern DWORD dwTextStartTick;

void ResetGame();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch(fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			// Disable thread library notifications
			DisableThreadLibraryCalls(hModule);

			// Install the exception handler
			CExceptionHandler::Install();

			// Open the log file
			CLogFile::Open("Client.log");

			// Log the version
			CLogFile::Printf(VERSION_IDENTIFIER);

			// Open the settings file
			CSettings::Open(SharedUtility::GetAbsolutePath("clientsettings.xml"));

			// Parse the command line
			CSettings::ParseCommandLine(GetCommandLine());

			// Load the global vars from the settings
			g_strHost = CVAR_GET_STRING("ip");
			g_usPort = CVAR_GET_INTEGER("port");
			g_strNick = CVAR_GET_STRING("nick");
			g_strPassword = CVAR_GET_STRING("pass");
			g_bWindowedMode = CVAR_GET_BOOL("windowed");
			g_bFPSToggle = CVAR_GET_BOOL("fps");

			// IE9 fix - disabled if disableie9fix is set or shift is pressed
			if(!CVAR_GET_BOOL("disableie9fix") || GetAsyncKeyState(VK_SHIFT) > 0)
			{
				// Get the version info
				DWORD dwHandle;
				DWORD dwSize = GetFileVersionInfoSize("wininet.dll", &dwHandle);
				BYTE* byteFileInfo = new BYTE[dwSize];
				GetFileVersionInfo("wininet.dll", dwHandle, dwSize, byteFileInfo);

				unsigned int uiLen;
				VS_FIXEDFILEINFO* fileInfo;
				VerQueryValue(byteFileInfo, "\\", (LPVOID *)&fileInfo, &uiLen);
				delete byteFileInfo;

				// using IE9?
				if(fileInfo->dwFileVersionMS == 0x90000)
				{
					// Try and load a wininet.dll from the iv:mp directory
					if(!LoadLibrary(SharedUtility::GetAbsolutePath("wininet.dll")))
					{
						// Get path to it
						char szFindPath[MAX_PATH] = {0};
						char szWinSxsPath[MAX_PATH] = {0};
						char szBuildVersion[] = "00000";
						GetEnvironmentVariable("windir", szWinSxsPath, sizeof(szWinSxsPath));
						strcat_s(szWinSxsPath, sizeof(szWinSxsPath), "\\WinSxS\\");
						strcpy_s(szFindPath, sizeof(szFindPath), szWinSxsPath);
						strcat_s(szFindPath, sizeof(szFindPath), "x86_microsoft-windows-i..tocolimplementation_31bf3856ad364e35*");

						// try to find a usable wininet.dll in WinSXS (basically any non-9.x version)
						bool bLoaded = false;
						WIN32_FIND_DATA lpFindFileData;
						HANDLE hFindFile = FindFirstFile(szFindPath, &lpFindFileData);
						do
						{
							if(hFindFile == INVALID_HANDLE_VALUE)
								break;

							if(strlen(lpFindFileData.cFileName) > 63)
							{
								if(lpFindFileData.cFileName[62] < '9')
								{
									char szFullPath[MAX_PATH];
									sprintf_s(szFullPath, MAX_PATH, "%s%s\\wininet.dll", szWinSxsPath, lpFindFileData.cFileName);
									if(LoadLibrary(szFullPath))
									{
										CLogFile::Printf("Using %s to address IE9 issue", szFullPath);
										bLoaded = true;
										break;
									}
								}
							}
						}
						while(FindNextFile(hFindFile, &lpFindFileData));

						// Still failed, tell the user
						if(!bLoaded)
						{
							if(MessageBox(0, "Unfortunately, you have Internet Explorer 9 installed which is not compatible with GTA:IV. Do you want proceed anyway (and possibly crash?)", "IV:MP", MB_YESNO | MB_ICONERROR) == IDNO)
							{
								// Doesn't want to continue
								ExitProcess(0);
							}

							// Save the user's choice
							CVAR_SET_BOOL("disableie9fix", true);
						}
					}
				}
			}

			// Initialize the streamer
			g_pStreamer = new CStreamer();

			// Initialize the time
			g_pTime = new CTime();

			// Initialize the traffic lights
			g_pTrafficLights = new CTrafficLights();

			// Initialize the client task manager
			g_pClientTaskManager = new CClientTaskManager();

			// Initialize the game
			CGame::Initialize();

			// Install the XLive hook
			CXLiveHook::Install();

			// Install the Direct3D hook
			CDirect3DHook::Install();

			// Install the DirectInput hook
			CDirectInputHook::Install();

			// Install the Cursor hook
#ifdef IVMP_DEBUG
			CCursorHook::Install();
#endif
			// Initialize the file transfer
			g_pFileTransfer = new CFileTransfer();

			// Initialize the events manager
			g_pEvents = new CEvents();

			// Initialize the network module, if it fails, exit
			if(!CNetworkModule::Init())
			{
				CLogFile::Printf("Failed to initialize the network module!\n");
				ExitProcess(0);
			}
		}
		break;
	case DLL_PROCESS_DETACH:
		{
			CLogFile::Printf("Shutdown 1");

			// Delete our file transfer
			SAFE_DELETE(g_pFileTransfer);
			CLogFile::Printf("Shutdown 2");

			// Delete our camera
			SAFE_DELETE(g_pCamera);
			CLogFile::Printf("Shutdown 3");

			// Delete our model manager
			SAFE_DELETE(g_pModelManager);
			CLogFile::Printf("Shutdown 4");

			// Delete our pickup manager
			SAFE_DELETE(g_pPickupManager);
			CLogFile::Printf("Shutdown 5");

			// Delete our checkpoint manager
			SAFE_DELETE(g_pCheckpointManager);
			CLogFile::Printf("Shutdown 6");

			// Delete our object manager
			SAFE_DELETE(g_pObjectManager);
			CLogFile::Printf("Shutdown 7");

			// Delete our blip manager
			SAFE_DELETE(g_pBlipManager);
			CLogFile::Printf("Shutdown 8");

			// Delete our actor manager
			SAFE_DELETE(g_pActorManager);
			CLogFile::Printf("Shutdown 9");

			// Delete our vehicle manager
			SAFE_DELETE(g_pVehicleManager);
			CLogFile::Printf("Shutdown 10");

			// Delete our local player
			SAFE_DELETE(g_pLocalPlayer);
			CLogFile::Printf("Shutdown 11");

			// Delete our player manager
			SAFE_DELETE(g_pPlayerManager);
			CLogFile::Printf("Shutdown 12");

			// Delete our network manager
			SAFE_DELETE(g_pNetworkManager);
			CLogFile::Printf("Shutdown 13");

			// Delete our name tags
			SAFE_DELETE(g_pNameTags);
			CLogFile::Printf("Shutdown 14");

			// Delete our input window
			SAFE_DELETE(g_pInputWindow);
			CLogFile::Printf("Shutdown 15");

			// Delete our chat window
			SAFE_DELETE(g_pChatWindow);
			CLogFile::Printf("Shutdown 16");

			// Delete our fps counter
			SAFE_DELETE(g_pFPSCounter);
			CLogFile::Printf("Shutdown 17");

			// Delete our credits
			SAFE_DELETE(g_pCredits);
			CLogFile::Printf("Shutdown 18");

			// Delete our main menu
			SAFE_DELETE(g_pMainMenu);
			CLogFile::Printf("Shutdown 19");

			// Delete our gui
			SAFE_DELETE(g_pGUI);
			CLogFile::Printf("Shutdown 20");

			// Delete our streamer class
			SAFE_DELETE(g_pStreamer);
			CLogFile::Printf("Shutdown 21");

			// Delete our time class
			SAFE_DELETE(g_pTime);
			CLogFile::Printf("Shutdown 22");

			// Delete our traffic lights
			SAFE_DELETE(g_pTrafficLights);
			CLogFile::Printf("Shutdown 23");

			// Delete our client task manager
			SAFE_DELETE(g_pClientTaskManager);
			CLogFile::Printf("Shutdown 24");

			// Delete our events manager
			SAFE_DELETE(g_pEvents);
			CLogFile::Printf("Shutdown 25");

			// Uninstall the Cursor hook
#ifdef IVMP_DEBUG
			CCursorHook::Uninstall();
#endif
			CLogFile::Printf("Shutdown 26");

			// Uninstall the DirectInput hook
			CDirectInputHook::Uninstall();
			CLogFile::Printf("Shutdown 27");

			// Uninstall the Direct3D hook
			CDirect3DHook::Uninstall();
			CLogFile::Printf("Shutdown 28");

			// Uninstall the XLive hook
			// TODO
			CLogFile::Printf("Shutdown 29");

			// Shutdown our game
			CLogFile::Printf("Shutdown CGame");
			CGame::Shutdown();
			CLogFile::Printf("Shutdown 30");

			// Close the settings file
			CSettings::Close();
			CLogFile::Printf("Shutdown 31");

			// Close the log file
			CLogFile::Close();
		}
		break;
	}

	return TRUE;
}

// debug view
#define DEBUG_TEXT_TOP (40.0f + (MAX_DISPLAYED_MESSAGES * 20))
float g_fDebugTextTop = 0;

void DrawDebugText(String strText)
{
	// Get the font
	CEGUI::Font * pFont = NULL/*g_pGUI->GetFont("tahoma-bold", 10)*/;

	// Draw the text
	g_pGUI->DrawText(strText, CEGUI::Vector2(26.0f, g_fDebugTextTop), CEGUI::colour(0xFFFFFFFF), pFont);

	// Increment the text top
	g_fDebugTextTop += 14.0f;
}

void DebugDumpTask(String strName, CIVTask * pTask)
{
	if(!pTask)
	{
		//DrawDebugText(String("%s: None (9999)", strName.Get()));
		return;
	}

	DrawDebugText(String("%s: %s (%d)", strName.Get(), pTask->GetName(), pTask->GetType()));
	CIVTask * pSubTask = NULL;

	while((pSubTask = pTask->GetSubTask()))
	{
		DrawDebugText(String("%s: %s (%d)", strName.Get(), pSubTask->GetName(), pSubTask->GetType()));
		pTask = pSubTask;
	}
}

bool bFireGunDisabled = false;

void DebugDumpTasks(int iType)
{
	CIVPedTaskManager * pPedTaskManager = g_pLocalPlayer->GetGamePlayerPed()->GetPedTaskManager();

	if(iType == 0)
	{
		DrawDebugText("Priority Tasks: ");
		DrawDebugText("");
		DebugDumpTask("PhysicalResponse", pPedTaskManager->GetTask(TASK_PRIORITY_PHYSICAL_RESPONSE));
		DebugDumpTask("EventResponseTemp", pPedTaskManager->GetTask(TASK_PRIORITY_EVENT_RESPONSE_TEMP));
		DebugDumpTask("EventResponseNonTemp", pPedTaskManager->GetTask(TASK_PRIORITY_EVENT_RESPONSE_NONTEMP));
		DebugDumpTask("Primary", pPedTaskManager->GetTask(TASK_PRIORITY_PRIMARY));
		DebugDumpTask("Default", pPedTaskManager->GetTask(TASK_PRIORITY_DEFAULT));
		DrawDebugText("");
	}
	else if(iType == 1)
	{
		DrawDebugText("Secondary Tasks: ");
		DrawDebugText("");
		DebugDumpTask("Attack", pPedTaskManager->GetTaskSecondary(TASK_SECONDARY_ATTACK));
		DebugDumpTask("Duck", pPedTaskManager->GetTaskSecondary(TASK_SECONDARY_DUCK));
		DebugDumpTask("Say", pPedTaskManager->GetTaskSecondary(TASK_SECONDARY_SAY));
		DebugDumpTask("FacialComplex", pPedTaskManager->GetTaskSecondary(TASK_SECONDARY_FACIAL_COMPLEX));
		DebugDumpTask("PartialAnim", pPedTaskManager->GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM));
		DebugDumpTask("IK", pPedTaskManager->GetTaskSecondary(TASK_SECONDARY_IK));
		DrawDebugText("");
		
	}
	else if(iType == 2)
	{
		DrawDebugText("Unknown Tasks: ");
		DrawDebugText("");

		for(int i = 0; i < 3; i++)
		{
			CIVTask * pTask = g_pClientTaskManager->GetClientTaskFromGameTask(pPedTaskManager->GetPedTaskManager()->m_unknownTasks[i]);
			DebugDumpTask(String("UnknownTask%d", i), pTask);
		}

		DrawDebugText("");
	}
}

#include "CRemotePlayer.h"
CRemotePlayer * pClonePlayer = NULL;

void DrawDebugView()
{
	if(g_pGUI && g_pLocalPlayer && g_pCamera)
	{
		if(GetAsyncKeyState(VK_F8) && !pClonePlayer)
		{
			pClonePlayer = new CRemotePlayer();
			pClonePlayer->Spawn(CVector3(), 0);
		}

		/*if(GetAsyncKeyState(VK_F7))
		{
			*(DWORD *)(CGame::GetBase() + 0xCCA0E0) = 0x900004C2;
			g_pChatWindow->AddInfoMessage("Disabled CTaskSimpleFireGun::SetPedPosition");
		}
		else if(GetAsyncKeyState(VK_F8))
		{
		// this is where the target data is processed
			*(DWORD *)(CGame::GetBase() + 0xCCAA30) = 0x900004C2;
			g_pChatWindow->AddInfoMessage("Disabled CTaskSimpleFireGun::ProcessPed");
		}*/

		g_fDebugTextTop = DEBUG_TEXT_TOP;
		DrawDebugText("Local Player Debug: ");
		DrawDebugText("");
		CVector3 vecPosition;
		g_pLocalPlayer->GetPosition(vecPosition);
		DrawDebugText(String("Position: %f, %f, %f Heading (C/D): %f, %f", vecPosition.fX, vecPosition.fY, vecPosition.fZ, g_pLocalPlayer->GetCurrentHeading(), g_pLocalPlayer->GetDesiredHeading()));
		CVector3 vecMoveSpeed;
		g_pLocalPlayer->GetMoveSpeed(vecMoveSpeed);
		DrawDebugText(String("Move Speed: %f, %f, %f", vecMoveSpeed.fX, vecMoveSpeed.fY, vecMoveSpeed.fZ));
		CIVCam * pGameCam = g_pCamera->GetGameCam();
		CVector3 vecCamPosition;
		pGameCam->GetPosition(vecCamPosition);
		CVector3 vecCamForward;
		vecCamForward = pGameCam->GetCam()->m_data1.m_matMatrix.vecForward;
		CVector3 vecLookAt;
		vecLookAt.fX = vecCamPosition.fX + /*floatmul(*/vecCamForward.fX/*, fScale)*/;
		vecLookAt.fY = vecCamPosition.fY + /*floatmul(*/vecCamForward.fY/*, fScale)*/;
		vecLookAt.fZ = vecCamPosition.fZ + /*floatmul(*/vecCamForward.fZ/*, fScale)*/;
		DrawDebugText(String("Camera Position: %f, %f, %f Camera Look At: %f, %f, %f", vecCamPosition.fX, vecCamPosition.fY, vecCamPosition.fZ, vecLookAt.fX, vecLookAt.fY, vecLookAt.fZ));
		DrawDebugText(String("Health: %d Armour: %d Ducking: %d", g_pLocalPlayer->GetHealth(), g_pLocalPlayer->GetArmour(), g_pLocalPlayer->IsDucking()));
		unsigned int uiWeaponId = g_pLocalPlayer->GetCurrentWeapon();
		DrawDebugText(String("Weapon: %d Ammo: %d", uiWeaponId, g_pLocalPlayer->GetAmmo(uiWeaponId)));	
		IVEntity * pDamageEntity = g_pLocalPlayer->GetGamePlayerPed()->GetPhysical()->m_pLastDamageEntity;
		eWeaponType damageWeapon = g_pLocalPlayer->GetGamePlayerPed()->GetPhysical()->m_lastDamageWeapon;
		DrawDebugText(String("Last Damage Entity: 0x%x Last Damage Weapon: %d", pDamageEntity, (unsigned int)damageWeapon));
		DrawDebugText("");

		DebugDumpTasks(0);
		DebugDumpTasks(1);
		DebugDumpTasks(2);

		// get targetting pool (this is always 0)
		/*struct IVTargetting { };
		CIVPool<IVTargetting *> * pTargettingPool = new CIVPool<IVTargetting *>(*(IVPool **)COffsets::VAR_TargettingPool);
		DrawDebugText(String("Targetting Pool Count: %d", pTargettingPool->GetUsed()));*/

		/*if(bGotSimpleFireGun)
		{
			DrawDebugText("64 bytes from SimpleFireGun Task: ");
			BYTE * pMemory = (BYTE *)pTask;
			for(int i = 0; i < 4; i++)
			{
				DrawDebugText(String("%.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x, %.2x", 
					pMemory[(16*i)],pMemory[(16*i)+1],pMemory[(16*i)+2],pMemory[(16*i)+3],
					pMemory[(16*i)+4],pMemory[(16*i)+5],pMemory[(16*i)+6],pMemory[(16*i)+7],
					pMemory[(16*i)+8],pMemory[(16*i)+9],pMemory[(16*i)+10],pMemory[(16*i)+11],
					pMemory[(16*i)+12],pMemory[(16*i)+13],pMemory[(16*i)+14],pMemory[(16*i)+15]));
			}
		}*/

		// delete targetting pool
		//delete pTargettingPool;

		// Debug aim test
		CControlState controlState;
		g_pLocalPlayer->GetControlState(&controlState);

		if(controlState.IsAiming() || controlState.IsFiring())
		{
			float fScale = 10.0f;
			vecLookAt.fX = vecCamPosition.fX + (vecCamForward.fX * fScale);
			vecLookAt.fY = vecCamPosition.fY + (vecCamForward.fY * fScale);
			vecLookAt.fZ = vecCamPosition.fZ + (vecCamForward.fZ * fScale);
			CNetworkVehicle * pVehicle = g_pVehicleManager->Get(0);

			if(pVehicle)
				pVehicle->SetPosition(vecLookAt);
		}

		if(pClonePlayer)
		{
			vecPosition.fX += 5.0f;
			pClonePlayer->SetPosition(vecPosition);
			pClonePlayer->SetCurrentHeading(g_pLocalPlayer->GetCurrentHeading());

			if(pClonePlayer->GetCurrentWeapon() != uiWeaponId)
				pClonePlayer->GiveWeapon(uiWeaponId, g_pLocalPlayer->GetAmmo(uiWeaponId));

			pClonePlayer->SetDucking(g_pLocalPlayer->IsDucking());
			pClonePlayer->SetControlState(&controlState);

			{
				float fScale = 10.0f;
				CVector3 vecAim = (vecCamPosition + (vecCamForward * fScale));
				vecAim.fX += 5.0f;

				if(controlState.IsFiring())
					Scripting::TaskShootAtCoord(pClonePlayer->GetScriptingHandle(), vecAim.fX, vecAim.fY, vecAim.fZ, 45000, 5);
				else if(controlState.IsAiming())
					Scripting::TaskAimGunAtCoord(pClonePlayer->GetScriptingHandle(), vecAim.fX, vecAim.fY, vecAim.fZ, 45000);
				else
				{
					CIVTask * pTask = pClonePlayer->GetGamePlayerPed()->GetPedTaskManager()->GetTask(TASK_PRIORITY_PRIMARY);

					if(pTask && (pTask->GetType() == TASK_COMPLEX_GUN || pTask->GetType() == TASK_COMPLEX_AIM_AND_THROW_PROJECTILE))
						pClonePlayer->GetGamePlayerPed()->GetPedTaskManager()->RemoveTask(TASK_PRIORITY_PRIMARY);
				}
			}
		}
	}
}
// debug view end

// Direct3DDevice9::EndScene
void Direct3DRender()
{
	//CLogFile::Printf("Direct3DRender: Current thread id is 0x%x", GetCurrentThreadId());
	// Check for pause menu
	if(CGame::IsMenuActive() && CGame::GetState() == GAME_STATE_INGAME)
		CGame::SetState(GAME_STATE_PAUSE_MENU);
	else if(!CGame::IsMenuActive() && CGame::GetState() == GAME_STATE_PAUSE_MENU)
		CGame::SetState(GAME_STATE_INGAME);

	// Are we in the main menu?
	if(CGame::GetState() == GAME_STATE_MAIN_MENU || CGame::GetState() == GAME_STATE_LOADING)
	{
		// Is the main menu shown?
		if(CGame::IsMenuActive())
		{
			// Clear the screen
			g_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0, 0);
		}
	}

	if(g_pMainMenu && CGame::GetState() == GAME_STATE_MAIN_MENU || CGame::GetState() == GAME_STATE_IVMP_PAUSE_MENU)
	{
		if(!g_pMainMenu->IsVisible())
			g_pMainMenu->SetVisible(true);
	}
	else
	{
		if(g_pMainMenu->IsVisible())
			g_pMainMenu->SetVisible(false);
	}

	if(g_pClientScriptManager && g_pClientScriptManager->GetGUIManager())
	{
		if(CGame::GetState() != GAME_STATE_INGAME)
		{
			if(!g_pClientScriptManager->GetGUIManager()->IsHidden())
				g_pClientScriptManager->GetGUIManager()->Hide();
		}
		else
		{
			if(g_pClientScriptManager->GetGUIManager()->IsHidden())
				g_pClientScriptManager->GetGUIManager()->Show();
		}
	}

	// If our GUI class exists render it
	if(g_pGUI)
		g_pGUI->Render();

	// If our main menu exists process it
	if(g_pMainMenu)
		g_pMainMenu->Process();

	// If our credits class exists process it
	if(g_pCredits)
		g_pCredits->Process();

	// If our fps class exists update it
	if(g_pFPSCounter)
		g_pFPSCounter->Pulse();

	// If our scripting manager exists, call the frame event
	if(g_pEvents && !g_pMainMenu->IsVisible())
		g_pEvents->Call("frameRender");

	// Check if our screen shot write failed
	if(CScreenShot::IsDone())
	{
		if(CScreenShot::HasSucceeded())
			g_pChatWindow->AddInfoMessage("Screen shot written (%s).", CScreenShot::GetWriteName().Get());
		else
			g_pChatWindow->AddInfoMessage("Screen shot write failed (%s).", CScreenShot::GetError().Get());

		CScreenShot::Reset();
	}

	// Are we in game?
	if(CGame::GetState() == GAME_STATE_INGAME)
	{
		// Is F4 held down and do we have a network manager?
		if(GetAsyncKeyState(VK_F4) && g_pNetworkManager)
		{
			// Get the network statistics
			CNetStats * pNetStats = g_pNetworkManager->GetNetClient()->GetNetStats();

			// Convert the network statistics to a string
			char szNetworkStats[10000];
			pNetStats->ToString(szNetworkStats, 2);

			// Create the statistics string
			String strStats(szNetworkStats);

			// Append loaded and unloaded model counts to the stats
			// jenksta: too performance heavy to be done every frame
			//strStats.AppendF("Models (Loaded/Unload): %d/%d\n", CGame::GetLoadedModelCount(), CGame::GetUnloadedModelCount());

			// Append streamed in/out entity counts and streamed in limits to the stats
			strStats.AppendF("Vehicles (StreamedIn/StreamedInLimit): %d/%d\n", g_pStreamer->GetStreamedInEntityCountOfType(STREAM_ENTITY_VEHICLE), g_pStreamer->GetStreamedInLimitOfType(STREAM_ENTITY_VEHICLE));
			strStats.AppendF("Pickups (StreamedIn/StreamedInLimit): %d/%d\n", g_pStreamer->GetStreamedInEntityCountOfType(STREAM_ENTITY_PICKUP), g_pStreamer->GetStreamedInLimitOfType(STREAM_ENTITY_PICKUP));
			strStats.AppendF("Objects (StreamedIn/StreamedInLimit): %d/%d\n", g_pStreamer->GetStreamedInEntityCountOfType(STREAM_ENTITY_OBJECT), g_pStreamer->GetStreamedInLimitOfType(STREAM_ENTITY_OBJECT));
			strStats.AppendF("Checkpoints (StreamedIn/StreamedInLimit): %d/%d\n", g_pStreamer->GetStreamedInEntityCountOfType(STREAM_ENTITY_CHECKPOINT), g_pStreamer->GetStreamedInLimitOfType(STREAM_ENTITY_CHECKPOINT));

			// Draw the string
			g_pGUI->DrawText(strStats, CEGUI::Vector2(26, 30), (CEGUI::colour)D3DCOLOR_RGBA(255, 255, 255, 255), g_pGUI->GetFont("tahoma-bold", 10));
		}
		else
		{
			// If our chat window exists draw it
			if(g_pChatWindow)
				g_pChatWindow->Draw();

			// If our input window exists draw it
			if(g_pInputWindow)
				g_pInputWindow->Draw();
		}

		// If our name tags exist draw them
		if(g_pNameTags)
			g_pNameTags->Draw();

		// Update the time
		if(g_pTime)
		{
			// jenksta: Setting this every frame causes the weather to change
			// rapidly
			unsigned char ucHour = 0, ucMinute = 0;
			g_pTime->GetTime(&ucHour, &ucMinute);
			CGame::SetTime(ucHour, ucMinute);
			CGame::SetDayOfWeek(g_pTime->GetDayOfWeek());
		}

		if(GetAsyncKeyState(VK_F3) & 1)
		{
			g_pChatWindow->AddInfoMessage("Creating explosion near your position");
			CVector3 vecPosition;
			g_pLocalPlayer->GetPosition(vecPosition);
			vecPosition.fX += 10.0f;
			CGame::CreateExplosion(vecPosition, 0, 1.0f, true, false);
			g_pChatWindow->AddInfoMessage("Created explosion near your position");
		}

		if(GetAsyncKeyState(VK_F5) & 1)
		{
			CNetworkVehicle * pVehicle = g_pLocalPlayer->GetVehicle();

			if(pVehicle)
			{
				g_pChatWindow->AddInfoMessage("Turning on current vehicle headlights");
				IVVehicle * pGameVehicle = pVehicle->GetGameVehicle()->GetVehicle();
				//*(BYTE *)(pVehicle->GetGameVehicle()->GetVehicle() + 0xF71) |= 1;
				*((BYTE *)pGameVehicle + 3953) = *((BYTE *)pGameVehicle + 3953) & 0xFE | 2;
				g_pChatWindow->AddInfoMessage("Turned on current vehicle headlights");
			}
		}

		// CViewportManager + 0x00 = sysArray (CViewport *)
		// CViewport + 0x53C = Viewport ID
		// GET_VIEWPORT_POS_AND_SIZE(unsigned int uiViewportId, float * fPosX, float * fPosY, float * fSizeX, float * fSizeY)
		// (pViewport + 0x10) is always used
		// ((pViewport + 0x10) + 0x298) = float fPosX;
		// (((pViewport + 0x10) + 0x298) + 0x4) = float fPosY;
		// (((pViewport + 0x10) + 0x298) + 0x8) = float fSizeX;
		// (((pViewport + 0x10) + 0x298) + 0xC) = float fSizeY;
		// GET_VIEWPORT_POSITION_OF_COORD(float fCoordX, float fCoordY, float fCoordZ, unsigned int uiViewportId, float * fPosX, float * fPosY)
		// Viewport 1 = CViewportPrimaryOrtho
		// Viewport 2 = CViewportGame
		// Viewport 3 = CViewportRadar
		// Draw text for each vehicle
		if(g_pVehicleManager)
		{
			CVector3 vecWorldPosition;
			Vector2 vecScreenPosition;

			for(EntityId i = 0; i < MAX_VEHICLES; i++)
			{
				if(g_pVehicleManager->Exists(i))
				{
					CNetworkVehicle * pVehicle = g_pVehicleManager->Get(i);

					if(!pVehicle->IsStreamedIn())
						continue;

					if(!pVehicle->IsOnScreen())
						continue;
	
					pVehicle->GetPosition(vecWorldPosition);
					CGame::GetScreenPositionFromWorldPosition(vecWorldPosition, vecScreenPosition);
					g_pGUI->DrawText(String("Vehicle %d", i), CEGUI::Vector2(vecScreenPosition.X, vecScreenPosition.Y));
				}
			}
		}
	}

#ifdef _DEBUG
	DrawDebugView();
#endif
}

// Direct3DDevice9::Reset
void Direct3DInvalidate()
{
	// If our gui instance exists inform it of the device loss
	if(g_pGUI)
		g_pGUI->OnLostDevice();

	// If our graphics instance exists inform it of the device loss
	if(g_pGraphics)
		g_pGraphics->OnLostDevice();
}

// Direct3DDevice9::Reset
void Direct3DReset()
{
	// If our GUI instance does not exist create it
	if(!g_pGUI)
	{
		g_pGUI = new CGUI(g_pDevice);

		if(g_pGUI->Initialize())
		{
			// Version identifier text
			g_pVersionIdentifier = g_pGUI->CreateGUIStaticText();
			g_pVersionIdentifier->setText(VERSION_IDENTIFIER);
			CEGUI::Font * pFont = g_pGUI->GetFont("tahoma-bold");
			float fTextWidth = pFont->getTextExtent(VERSION_IDENTIFIER);
			float fTextHeight = pFont->getFontHeight();
			g_pVersionIdentifier->setSize(CEGUI::UVector2(CEGUI::UDim(0, fTextWidth), CEGUI::UDim(0, fTextHeight)));
			float fTextX = pFont->getTextExtent("_");
			float fTextY = -(fTextX + fTextHeight);
			g_pVersionIdentifier->setPosition(CEGUI::UVector2(CEGUI::UDim(0, fTextX), CEGUI::UDim(1, fTextY)));
			g_pVersionIdentifier->setProperty("FrameEnabled", "false");
			g_pVersionIdentifier->setProperty("BackgroundEnabled", "false");
			g_pVersionIdentifier->setFont(pFont);
			g_pVersionIdentifier->setProperty("TextColours", "tl:FFFFFFFF tr:FFFFFFFF bl:FFFFFFFF br:FFFFFFFF");
			g_pVersionIdentifier->setAlpha(0.6f);
			g_pVersionIdentifier->setVisible(true);

			// TODO: Make the default stuff (Chat window, main menu, e.t.c) a xml layout so it
			// can be edited by users
			// TODO: Also load the font from an xml layout so it can be edited by users
			// TODO: Ability to output all server messages to the client console?
			// TODO: A script console when client side scripts are implemented

			CLogFile::Printf("GUI initialized");
		}
		else
			CLogFile::Printf("GUI initialization failed");
	}
	else
	{
		// If our GUI class does exist inform it of the device reset
		g_pGUI->OnResetDevice();
	}

	// If our graphics instance does not exist create it
	if(!g_pGraphics)
		g_pGraphics = new CGraphics(g_pDevice);
	else
		g_pGraphics->OnResetDevice();

	// If our main menu class does not exist create it
	if(!g_pMainMenu)
		g_pMainMenu = new CMainMenu();
	else
		g_pMainMenu->OnResetDevice();

	// If our credits class does not exist create it
	if(!g_pCredits)
		g_pCredits = new CCredits(g_pGUI);

	// If our fps counter class does not exist create it
	if(!g_pFPSCounter)
		g_pFPSCounter = new CFPSCounter();

	// If our fps counter class does not exist create it
	if(!g_pChatWindow)
		g_pChatWindow = new CChatWindow();

	// If our input window class does not exist create it
	if(!g_pInputWindow)
	{
		g_pInputWindow = new CInputWindow();
		RegisterCommands();
	}

	// If our name tags class does not exist create it
	if(!g_pNameTags)
		g_pNameTags = new CNameTags();
}

bool g_bResetGame = false;
void InternalResetGame();

void GameLoad()
{
	// Initialize the pools
	CGame::GetPools()->Initialize();
	CLogFile::Printf("Initialized pools");

	// Reset the game
	InternalResetGame();
}

bool bDoPlayerShit = false;
CNetworkPlayer * pPlayer = NULL;

#if 0
CNetworkPlayer * pPlayers[32];
bool bFirst = true;
#endif

void GameScriptProcess()
{
#if 0
	if(bFirst)
	{
		for(int i = 0; i < 32; i++)
			pPlayers[i] = NULL;

		bFirst = false;
	}

	if(GetAsyncKeyState(VK_F5))
	{
		CLogFile::Printf("Player creation test");
		for(int i = 0; i < 100; i++)
		{
			CLogFile::Printf("Create players %d time", i);
			for(int x = 1; x < 32; x++)
			{
				CLogFile::Printf("New player %d", x);
				pPlayers[x] = new CNetworkPlayer(false);
				CLogFile::Printf("New player done %d", x);
				CLogFile::Printf("Create player %d", x);
				pPlayers[x]->Create();
				CLogFile::Printf("Create player done %d", x);
			}
			CLogFile::Printf("Create players done %d time", i);
			CLogFile::Printf("Destroy players %d time", i);
			for(int x = 1; x < 32; x++)
			{
				CLogFile::Printf("Destroy player %d", x);
				pPlayers[x]->Destroy();
				CLogFile::Printf("Destroy player done %d", x);
				CLogFile::Printf("Delete player %d", x);
				delete pPlayers[x];
				pPlayers[x] = NULL;
				CLogFile::Printf("Delete player done %d", x);
			}
			CLogFile::Printf("Destroy players done %d time", i);
		}
		CLogFile::Printf("Player creation done test");
	}
	if(GetAsyncKeyState(VK_F7))
		bDoPlayerShit = true;

	if(bDoPlayerShit && CGame::GetState() == GAME_STATE_INGAME && g_pNetworkManager && g_pNetworkManager->HasJoinedGame())
	{
		if(pPlayer == NULL)
		{
			g_pPlayerManager->Add(1, "j3nk5t4");
			pPlayer = g_pPlayerManager->GetAt(1);
			g_pPlayerManager->Spawn(1, 90, CVector3(), 0.0f);
		}

		CVector3 vecPosition;
		g_pLocalPlayer->GetPosition(&vecPosition);
		vecPosition.fX += 5;
		pPlayer->SetPosition(&vecPosition);
		pPlayer->SetCurrentHeading(g_pLocalPlayer->GetCurrentHeading());
		CControlState controlState;
		g_pLocalPlayer->GetControlState(&controlState);
		pPlayer->SetControlState(&controlState);
		AimSyncData aimSyncData;
		g_pLocalPlayer->GetAimSyncData(&aimSyncData);
		//pPlayer->SetAimSyncData(&aimSyncData);
		pPlayer->LockHealth(g_pLocalPlayer->GetHealth());
		pPlayer->LockArmour(g_pLocalPlayer->GetArmour());
		unsigned int uiWeaponId = g_pLocalPlayer->GetCurrentWeapon();
		unsigned int uiAmmo = g_pLocalPlayer->GetAmmo(uiWeaponId);

		if(pPlayer->GetCurrentWeapon() != uiWeaponId)
		{
			g_pChatWindow->AddInfoMessage("Changing weapon to %d (%d ammo)\n", uiWeaponId, uiAmmo);
			pPlayer->GiveWeapon(uiWeaponId, g_pLocalPlayer->GetAmmo(uiWeaponId));
			pPlayer->SetCurrentWeapon(uiWeaponId);
		}

		if(pPlayer->GetAmmo(uiWeaponId) != uiAmmo)
		{
			g_pChatWindow->AddInfoMessage("Changing ammo to %d (%d weapon)", uiAmmo, uiWeaponId);
			pPlayer->SetAmmo(uiWeaponId, uiAmmo);
		}
	}
#endif

	// Do we need to reset the game?
	if(g_bResetGame)
	{
		// Reset the game
		InternalResetGame();

		// Flag the game as no longer needed to reset
		g_bResetGame = false;
	}

	// If our network manager exists process it
	if(g_pNetworkManager)
		g_pNetworkManager->Process();

	// If we have text to draw draw it
	// TODO: Move this to another class?
	if(iTextTime > 0)
	{
		if((int)(SharedUtility::GetTime() - dwTextStartTick) < iTextTime)
			Scripting::DisplayTextWithLiteralString(fTextPos[0], fTextPos[1], "STRING", strTextText);
		else
			iTextTime = 0;
	}
}

void ResetGame()
{
	g_bResetGame = true;
}

void InternalResetGame()
{
	CLogFile::Printf("Initializing game for multiplayer activities");

	// TODO: Reset functions for all of these classes or something so i don't have to delete and recreate them?
	SAFE_DELETE(g_pModelManager);
	g_pModelManager = new CModelManager();
	CLogFile::Printf("Created model manager instance");

	SAFE_DELETE(g_pPickupManager);
	g_pPickupManager = new CPickupManager();
	CLogFile::Printf("Created pickup manager instance");

	SAFE_DELETE(g_pCheckpointManager);
	g_pCheckpointManager = new CCheckpointManager();
	CLogFile::Printf("Created checkpoint manager instance");

	SAFE_DELETE(g_pActorManager);
	g_pActorManager = new CActorManager();
	CLogFile::Printf("Created actor manager instance");

	SAFE_DELETE(g_pBlipManager);
	g_pBlipManager = new CBlipManager();
	CLogFile::Printf("Created blip manager instance");

	SAFE_DELETE(g_pObjectManager);
	g_pObjectManager = new CObjectManager();
	CLogFile::Printf("Created object manager instance");

	SAFE_DELETE(g_pVehicleManager);
	g_pVehicleManager = new CVehicleManager();
	CLogFile::Printf("Created vehicle manager instance");

	// Reset the streamer
	g_pStreamer->Reset();
	CLogFile::Printf("Reset streamer instance");

	if(!g_pLocalPlayer)
	{
		g_pLocalPlayer = new CLocalPlayer();
		CLogFile::Printf("Created local player instance");
	}

	g_pLocalPlayer->SetPlayerId(INVALID_ENTITY_ID);
	g_pLocalPlayer->SetModel(Scripting::MODEL_PLAYER);
	g_pLocalPlayer->Teleport(CVector3());
	g_pLocalPlayer->SetPlayerControlAdvanced(false, false);
	g_pLocalPlayer->RemoveAllWeapons();
	g_pLocalPlayer->ResetMoney();
	g_pLocalPlayer->SetHealth(200);
	CLogFile::Printf("Reset local player instance");

	SAFE_DELETE(g_pPlayerManager);
	g_pPlayerManager = new CPlayerManager();
	CLogFile::Printf("Created player manager instance");

	// Do we have a network manager instance?
	if(g_pNetworkManager)
	{
		// If we are connected disconnect
		if(g_pNetworkManager->IsConnected())
			g_pNetworkManager->Disconnect();

		// Delete our network manager instance
		SAFE_DELETE(g_pNetworkManager);
	}

	g_pNetworkManager = new CNetworkManager();
	CLogFile::Printf("Created network manager instance");

	g_pNetworkManager->Startup(g_strHost, g_usPort, g_strPassword);
	CLogFile::Printf("Started network manager instance");

	SAFE_DELETE(g_pClientScriptManager);
	g_pClientScriptManager = new CClientScriptManager();
	CLogFile::Printf("Created client script manager instance");

	// Reset file transfer
	g_pFileTransfer->Reset();

	// Reset all events
	g_pEvents->clear();
	CLogFile::Printf("Reset events instance");

	g_pTime->SetTime(12, 0);
	g_pTime->SetDayOfWeek(2);
	g_pTime->SetMinuteDuration(0);
	g_pTrafficLights->Reset();
	g_pNameTags->SetEnabled(true);
	CGame::SetWantedLevel(0);
	CGame::SetHudVisible(false);
	CGame::SetRadarVisible(false);
	CGame::SetAreaNamesEnabled(false);
	CGame::GetWeather()->SetWeather(WEATHER_EXTRA_SUNNY);
	CGame::ResetScrollBars();
	CGame::SetScrollBarColor();
	CGame::ToggleLazlowStation(true);
	Scripting::SetScenarioPedDensityMultiplier(0, 0);
	// SetCanBurstCarTyres(bool canburst);
	Scripting::SetMaxWantedLevel(0);
	Scripting::SetCreateRandomCops(false);
	// Test if this is needed (Script is unloaded, so it shouldn't be)
	Scripting::AllowStuntJumpsToTrigger(false);
	CLogFile::Printf("Reset world");

	if(!g_pCamera)
	{
		g_pCamera = new CCamera();
		CLogFile::Printf("Created camera instance");
	}

	g_pCamera->SetPosition(CVector3(HAPPINESS_CAMERA_POS));
	g_pCamera->SetLookAt(CVector3(HAPPINESS_CAMERA_LOOK_AT));
	CLogFile::Printf("Reset camera instance");

	g_pNetworkManager->Connect();
	CLogFile::Print("Sent network connection request");

	CLogFile::Printf("Sucessfully (re)initialized game for multiplayer activities");
}
