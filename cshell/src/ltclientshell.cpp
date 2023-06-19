//------------------------------------------------------------------------------//
// 
// MODULE   : ltclientshell.cpp
// 
// PURPOSE  : CLTClientShell - Implementation
// 
// CREATED  : 2002/03/02
// 
// (c) 2002 LithTech, Inc.  All Rights Reserved
// 
//------------------------------------------------------------------------------//

#include "ltclientshell.h"

#include "camera.h"
#include "clientinterfaces.h"
#include "msgids.h"
#include "commandids.h"

#include "worldpropsclient.h"
#include "playerclnt.h"

// Client-side helper functions
#include "clienthelper.h"

#include "ClientFXDB.h"

// Engine Includes
#include <iltclient.h>
#include <iltphysics.h>
#include <iltsoundmgr.h>
#include <iltcommon.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
//	This macro defines certain DLL exports and variables
SETUP_CLIENTSHELL();

CLTClientShell* g_pCShell = NULL;

// Define the instantiation of the ClientShell interface.
define_interface(CLTClientShell, IClientShell);


//------------------------------------------------------------------------------
//	CLTClientShell::CLTClientShell()
//
//------------------------------------------------------------------------------
CLTClientShell::CLTClientShell()
:
m_hCamera(NULL),
m_bRender(true),
m_bFirstUpdate(true),
m_vPlayerStartPos(0.0f, 160.0f, 0.0f),
m_pWorldPropsClnt(NULL),
m_pPlayer(NULL),
m_nFxPageOffset(0),
m_bRenderGui(true),
m_nCurrentFxKeyPressed(-1)
{
	g_pCShell= this;
	m_rPlayerStartRot.Init();
}


//------------------------------------------------------------------------------
//	CLTClientShell::~CLTClientShell()
//
//------------------------------------------------------------------------------
CLTClientShell::~CLTClientShell()
{
}


//------------------------------------------------------------------------------
//  CLTClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)
//
//------------------------------------------------------------------------------
LTRESULT CLTClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)
{
	
	LTRESULT result = LT_OK;
	
	result = VerifyClientInterfaces();
	if (ShutdownIfFailed(result, "Error in VerifyClientInterfaces(). Shutting down.\n"))
	{
		return result;
	}
	
	result = InitRenderer(pMode);
	if (ShutdownIfFailed(result, "Error in InitRenderer(). Shutting down.\n"))
	{
		return result;
	}
	
	result = InitSound();
	DebugPrintIfFailed(result, "Error: Couldn't intialize sound system.\n");
	
	result = InitGame();
	if (ShutdownIfFailed(result, "Error in InitGame(). Shutting down.\n"))
	{
		return result;
	}
	
	
	// Instantiate the camera class
	m_pCamera = new CCamera;
	
	if( !m_pCamera )
		return LT_ERROR;
	
	// Create the CAMERA
	result = m_pCamera->CreateCamera();
	if (ShutdownIfFailed(result, "Error in CreateCamera(). Shutting down.\n"))
	{
		return result;
	}

	m_hCamera = m_pCamera->GetCamera();

	this->CreateWorldPropObject();
	

    // Initialize Our ClientFX Database
    if(!CClientFXDB::GetSingleton().Init(g_pLTClient))
    {
        g_pLTClient->ShutdownWithMessage( "Could not init ClientFXDB!" );
        return LT_ERROR;
    }

	// Init the ClientFX mgr... (this must become before most other classes)
	if( !m_ClientFXMgr.Init( g_pLTClient ) )
	{
		// Make sure ClientFX.fxd is built and in the game dir
		g_pLTClient->ShutdownWithMessage( "Could not init ClientFXMgr!" );
		return LT_ERROR;
	}

    // We need to make sure to setup the camera for the FX Mgr
    m_ClientFXMgr.SetCamera(m_hCamera);

    // List the currently available FX.
    uint32 nFX = CClientFXDB::GetSingleton().GetNumFX();
    g_pLTClient->CPrint("Number of ClientFX: %d", nFX);
    for(uint32 i = 0; i < nFX; i++)
    {
        g_pLTClient->CPrint("%d. FX Name: %s", i,CClientFXDB::GetSingleton().GetFXNameByIndex(i));
        //m_stlvFXNameList.push_back(CClientFXDB::GetSingleton().GetFXNameByIndex(i));
    }
    
    m_Gui.Init("fonts/SQR721B.TTF", "Square721 BT");
    RecalcSideText();
	return result;
}

//---------------------------------------------------------------------------
//  CLTClientShell::InitRenderer(RMode *pMode)
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::InitRenderer(RMode *pMode)
{
	//	g_pLTClient->CPrint("CLTClientShell::InitRenderer");
	
	//	Attempt to set the render mode based on pMode.
	//		pMode contains command line parameters or
	//		console vars included in autoexec.cfg. Such as:
	//
	//		screenwidth 800
	//		screenheight 600
	//		bitdepth 32
	//
	//		Or on the command line:
	//
	//		lithtech.exe +screenwidth 800 +screenheight 600 +bitdepth 32
	
	if (g_pLTClient->SetRenderMode(pMode) != LT_OK)
	{
		g_pLTClient->DebugOut("Error: Couldn't set requested render mode!\n");
		
		// Try 640x480x16 as a fallback.
		
		RMode newRMode;
		memset(&newRMode, 0, sizeof(newRMode));
		
		newRMode.m_Width		= 640;
		newRMode.m_Height		= 480;
		newRMode.m_BitDepth		= 16;
		
		newRMode.m_bHWTnL   = pMode->m_bHWTnL;
		sprintf(newRMode.m_InternalName, "%s", pMode->m_InternalName);
		sprintf(newRMode.m_Description, "%s", pMode->m_Description);
		
		// Give it another try.
		g_pLTClient->DebugOut("Attempting to set render mode to 640x480x16...\n");
		
		if (g_pLTClient->SetRenderMode(&newRMode) != LT_OK)
		{
			// Still didn't work.
			g_pLTClient->DebugOut(
				"Error: Couldn't set to 640x480x16 mode. Shutting down.\n");
			g_pLTClient->ShutdownWithMessage(
				"Error: Couldn't set to 640x480x16 mode. Shutting down.\n");
			
			return LT_ERROR;
		}
	}
	
	return LT_OK;
}

//---------------------------------------------------------------------------
//  CLTClientShell::InitSound()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::InitSound()
{
	//	g_pLTClient->CPrint("CLTClientShell::InitSound");
	
	InitSoundInfo sndInfo;
	
	sndInfo.Init();
	
    // Initialize sound with defaults.
	return g_pLTCSoundMgr->InitSound(&sndInfo);
}

//---------------------------------------------------------------------------
//  CLTClientShell::InitGame()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::InitGame()
{
	LTRESULT result;
	const char* pLevelName = NULL;
	StartGameRequest request;
	memset(&request, 0, sizeof(StartGameRequest));
	
	request.m_Type = STARTGAME_NORMAL;
	
	HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("runworld");
	pLevelName = g_pLTClient->GetVarValueString(hVar);
	
	if (pLevelName)
	{
		// (length includes the NULL character)
		LTStrCpy(request.m_WorldName, pLevelName, strlen(pLevelName)+1);
	}
	else
	{
		strcpy(request.m_WorldName, "worlds/demoroom");
	}
	
	//request.m_HostInfo.m_dwMaxPlayers = 1;
	
	result = g_pLTClient->StartGame(&request);
	if (LT_OK != result)
	{
		DebugPrintIfFailed(result, "Start Game Request failed.\n");
		return result;
	}
	
	return result;
}


//---------------------------------------------------------------------------
//  CLTClientShell::OnEnterWorld()
//
//---------------------------------------------------------------------------
void CLTClientShell::OnEnterWorld()
{
	// Clear the input buffer for a clean start
	g_pLTClient->ClearInput();
}


//---------------------------------------------------------------------------
//  CLTClientShell::OnExitWorld()
//
//---------------------------------------------------------------------------
void CLTClientShell::OnExitWorld()
{
	if (m_pPlayer)
	{
		delete m_pPlayer;
		m_pPlayer = NULL;
	}

	
    // Stop all the client FX
	m_ClientFXMgr.ShutdownAllFX();
    
}


//---------------------------------------------------------------------------
//  CLTClientShell::OnMessage(ILTMessage_Read* pMessage)
//
//---------------------------------------------------------------------------
void CLTClientShell::OnMessage(ILTMessage_Read* pMessage)
{
	pMessage->SeekTo(0);
	uint8 messageID = pMessage->Readuint8();
	
	switch (messageID)
	{
	case MSG_STARTPOINT_POS_ROT:
		{
			m_vPlayerStartPos = pMessage->ReadLTVector();
			m_rPlayerStartRot = pMessage->ReadLTRotation();
		}
		break;
		
	case MSG_WORLD_PROPS:
		{
			m_pWorldPropsClnt->UnpackWorldProps(pMessage);
		}
		break;
    case MSG_SC_REMOVEOBJECT:
        {
            HOBJECT hObj = pMessage->ReadObject();

            OnObjectRemove(hObj);

        }
        break;
		
	default:
		break;
	}
	
}


//---------------------------------------------------------------------------
//  CLTClientShell::Update()
//
//---------------------------------------------------------------------------
void CLTClientShell::Update()
{
	
	LTVector vDims;
	LTVector vPos;
	LTRotation rRot;
	
	if( m_bFirstUpdate )
	{
		m_hPlayer = g_pLTClient->GetClientObject();
		vDims.x = vDims.y = vDims.z = 10.0f;
		g_pLTCPhysics->SetObjectDims(m_hPlayer, &vDims, 0);
		g_pLTCPhysics->SetGlobalForce(LTVector(0.0f, -1000.0f, 0.0f));
		g_pLTCPhysics->SetMass(m_hPlayer, 30.0f);
		g_pLTCCommon->SetObjectFlags(m_hPlayer, OFT_Flags, 0, FLAG_GRAVITY);
		m_bFirstUpdate = false;
	}
	
	g_pLTClient->GetObjectPos(m_hPlayer, &vPos);
	g_pLTClient->GetObjectRotation(m_hPlayer, &rRot);
	g_pLTClient->SetObjectPosAndRotation(m_hCamera, &vPos, &rRot);
	
	PollInput();	// Poll for user input
	
	if( m_bRender )
	{
		if( Render() != LT_OK )
		{
			g_pLTClient->DebugOut("Error in Render()\n");
			g_pLTClient->Shutdown();
		}
	}
	
	// Update the world properties class
	if( m_pWorldPropsClnt )
		m_pWorldPropsClnt->Update();
}


//---------------------------------------------------------------------------
//  CLTClientShell::Render()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::Render()
{
	LTRESULT result;

    float fFrameTime = g_pLTClient->GetFrameTime();

	//	Clear the screen to prepare for the next draw
	result = g_pLTClient->ClearScreen(NULL, CLEARSCREEN_SCREEN | CLEARSCREEN_RENDER, 0 );
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in ClearScreen()\n");
		g_pLTClient->Shutdown();
	}	

	//update all the effects status as well as any that might effect the camera
	m_ClientFXMgr.UpdateAllActiveFX( m_bRender );

	// Render the effects
	m_ClientFXMgr.RenderAllActiveFX( m_bRender );


	//	Tell LithTech we are going to be starting our 3d render
	result = g_pLTClient->Start3D();
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in Start3D()\n");
		g_pLTClient->Shutdown();
	}
	
	//	Tell LithTech to render the scene using this camera
	result = g_pLTClient->RenderCamera(m_hCamera, fFrameTime);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in RenderCamera()\n");
		g_pLTClient->Shutdown();
	}
	
    if(m_bRenderGui)
    {
        m_Gui.Render();
    }


	//	Tell LithTech to finish rendering for this scene
	result = g_pLTClient->End3D(END3D_CANDRAWCONSOLE);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in End3D()\n");
		g_pLTClient->Shutdown();
	}
	
	//	Tell LithTech to flip the screen surface
	result = g_pLTClient->FlipScreen(NULL);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in FlipScreen()\n");
		g_pLTClient->Shutdown();
	}
	
	
	return LT_OK;
}


//---------------------------------------------------------------------------
//	CLTClientShell::OnEvent(uint32 dwEventID, uint32 dwParam)
//
//---------------------------------------------------------------------------
void CLTClientShell::OnEvent(uint32 dwEventID, uint32 dwParam)
{

    switch(dwEventID)
    {
    case LTEVENT_LOSTFOCUS:
        m_bRender = false;
        break;
        
    case LTEVENT_RENDERTERM:
        {            
            m_bRender = false;
            
            // Let the ClientFx mgr know the renderer is shutting down
            m_ClientFXMgr.OnRendererShutdown();
        }
        break;
        
    case LTEVENT_GAINEDFOCUS:
        m_bRender = true;
        break;
        
    case LTEVENT_RENDERINIT:
        m_bRender = true;
        break;
        
    case LTEVENT_RENDERALMOSTINITTED:
        m_bRender = true;
        break;
        
        
    default:
        break;
    }

}


//---------------------------------------------------------------------------
//  CLTClientShell::OnCommandOn(int command)
//
//---------------------------------------------------------------------------
void CLTClientShell::OnCommandOn(int command)
{
	switch (command)
	{
        g_pLTClient->CPrint("%d", command);
   
    case COMMAND_FX_NEXT:
        {
            if( m_nFxPageOffset < (CClientFXDB::GetSingleton().GetNumFX() / 10) )            
            {
                m_nFxPageOffset += 10;
                RecalcSideText();
            }
        }
        break;
    case COMMAND_FX_PREV:
        {
            if( m_nFxPageOffset >= 10)
            {
                m_nFxPageOffset -= 10;
                RecalcSideText();
            }
        }
        break;
    case COMMAND_FX_TOGGLEGUI:
        {
            m_bRenderGui = !m_bRenderGui;
        }
        break;
    case COMMAND_FX_STOP:
        {
            SendStopFxToServer();
        }
        break;
	case COMMAND_QUIT:
		g_pLTClient->Shutdown();
		break;
	default:
		break;
	}
}


//---------------------------------------------------------------------------
//  CLTClientShell::OnCommandOff(int command)
//
//---------------------------------------------------------------------------
void CLTClientShell::OnCommandOff(int command)
{
}


//---------------------------------------------------------------------------
//  CLTClientShell::PollInput()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::PollInput()
{
	
	float offsets[3];
	LTVector vPos, vUp, vRight, vForward, vTemp;
	LTRotation rRot, rTemp;
	float fFrameTime;
	bool bMoved = false;
	bool bRotated = false;
	
	
	fFrameTime = g_pLTClient->GetFrameTime();
	
	// Get player info
	g_pLTClient->GetObjectPos(m_hPlayer, &vPos);
	g_pLTClient->GetObjectRotation(m_hPlayer, &rRot);
	g_pLTCCommon->GetRotationVectors(rRot, vUp, vRight, vForward);
	
	g_pLTClient->GetAxisOffsets(offsets);
	
	if ((offsets[0] != 0.0f) || 
		(offsets[1] != 0.0f) || 
		(offsets[2] != 0.0f))
	{
		bRotated = true;
		m_fYaw += (offsets[0]);
		m_fPitch += (offsets[1]);
		m_fRoll += (offsets[2]);
		
	}

	if (g_pLTClient->IsCommandOn(COMMAND_MOVE_LEFT))
	{
		vTemp = vRight * fFrameTime * -500.0f;
		vPos += vTemp;
		bMoved = true;
	}
	
	if (g_pLTClient->IsCommandOn(COMMAND_MOVE_RIGHT))	
	{
		vTemp = vRight * fFrameTime * 500.0f;
		vPos += vTemp;
		bMoved = true;
	}
	
	if ( g_pLTClient->IsCommandOn(COMMAND_MOVE_FORWARD))
	{
		vTemp = vForward * fFrameTime * 500.0f;
		vPos += vTemp;
		bMoved = true;
	}
	
	if (g_pLTClient->IsCommandOn(COMMAND_MOVE_BACKWARD))
	{
		vTemp = vForward * fFrameTime * -500.0f;
		vPos += vTemp;
		bMoved = true;
	}
	
	if (bMoved)
	{
		g_pLTCPhysics->MoveObject(m_hPlayer, &vPos, 0);
	}
	
	if (bRotated)
	{
		g_pLTCCommon->SetupEuler(rRot, m_fPitch, m_fYaw, m_fRoll);
		g_pLTClient->SetObjectRotation(m_hPlayer, &rRot);
		g_pLTClient->SetObjectRotation(m_hCamera, &rRot);
	}
	
	
	if (bMoved && bRotated)
		SendPosAndRot();
	else if (bMoved)
		SendPosition();
	else if (bRotated)
		SendRotation();
	
	return LT_OK;
}


//-----------------------------------------------------------------------------
//  CLTClientShell::SendRotation()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendRotation()
{
	LTRotation rRot;
	g_pLTClient->GetObjectRotation(m_hPlayer, &rRot);
	
	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);
	
	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_ROTATION);
		pMessage->WriteLTRotation(rRot);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}
}


//-----------------------------------------------------------------------------
//  CLTClientShell::SendPosition()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendPosition()
{
	
	LTVector vPos;
	g_pLTClient->GetObjectPos(m_hPlayer, &vPos);
	
	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);
	
	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_POSITION);
		pMessage->WriteLTVector(vPos);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}
	
}


//-----------------------------------------------------------------------------
//  CLTClientShell::SendPosAndRot()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendPosAndRot()
{
	
	LTVector vPos;
	g_pLTClient->GetObjectPos(m_hPlayer, &vPos);
	
	LTRotation rRot;
	g_pLTClient->GetObjectRotation(m_hPlayer, &rRot);
	
	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);
	
	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_POSANDROT);
		pMessage->WriteLTVector(vPos);
		pMessage->WriteLTRotation(rRot);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}
	
}


//-----------------------------------------------------------------------------
//  CLTClientShell::GetPlayerStartInfo(LTVector &vStartPos, 
//                                     LTRotation &rStartRot)
//
//-----------------------------------------------------------------------------
void CLTClientShell::GetPlayerStartInfo(LTVector &vStartPos, LTRotation &rStartRot)
{
	vStartPos = m_vPlayerStartPos;
	rStartRot = m_rPlayerStartRot;
}


//-----------------------------------------------------------------------------
//  CLTClientShell::CreateWorldPropObject()
//
//-----------------------------------------------------------------------------
void CLTClientShell::CreateWorldPropObject()
{
	m_pWorldPropsClnt = new CWorldPropsClnt;
}


//-----------------------------------------------------------------------------
//	CLTClientShell::DestroyWorldPropObject()
//
//-----------------------------------------------------------------------------
void CLTClientShell::DestroyWorldPropObject()
{
	if( m_pWorldPropsClnt )
	{
		delete m_pWorldPropsClnt;
		m_pWorldPropsClnt = NULL;
	}
}

//-----------------------------------------------------------------------------
//	CLTClientShell::OnLockRenderer()
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnLockRenderer()
{
	//g_pLTClient->CPrint("OnLockRenderer()");
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnUnLockRenderer()
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnUnLockRenderer()
{
	//g_pLTClient->CPrint("OnUnLockRenderer()");
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnKeyDown(int key, int rep)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnKeyDown(int key, int rep)
{
    // We need to remove the "repeating" ability
    if(key != m_nCurrentFxKeyPressed)
    {
        m_nCurrentFxKeyPressed = key;

        char cKey = (char)key;
        if((cKey >= '0')&&(cKey <= '9'))
        {
#ifdef _DEBUG
            g_pLTClient->CPrint("FX %c %d %d", cKey, cKey, cKey - 48);
#endif
            //Send request
            uint32 nSize = CClientFXDB::GetSingleton().GetNumFX();
                                 
            int nIndex = ((cKey - 48) - 1);            
            if( nIndex == -1) // if 0
            {
                nIndex = 9;
            }
            
            if( (m_nFxPageOffset + nIndex) < nSize)
            {                               
                char *name = CClientFXDB::GetSingleton().GetFXNameByIndex(m_nFxPageOffset + nIndex);
                SendFxChangeToServer(name);
                char buf[256];
                sprintf(buf,"Current FX: %s", name);
                m_Gui.SetTitleText(buf);
            }
        }
    }
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnKeyUp(int key)
//
//-----------------------------------------------------------------------------

void CLTClientShell::OnKeyUp(int key)
{
	//g_pLTClient->CPrint("OnKeyUp(%d)", key);
}


//-----------------------------------------------------------------------------
//  CLTClientShell::SpecialEffectNotify(HLOCALOBJ hObj, ILTMessage_Read *pMessage)
//
//-----------------------------------------------------------------------------
void CLTClientShell::SpecialEffectNotify(HLOCALOBJ hObj, ILTMessage_Read *pMessage)
{
	g_pLTClient->CPrint("SpecialEffectNotify(%p,%p)", hObj, pMessage);

    /*
	pMessage->SeekTo(0);
	uint8 messageID = pMessage->Readuint8();

	if (MSG_PLAYEROBJ == messageID)
	{
		m_pPlayer = new CPlayerClnt;
		m_pPlayer->SetHObject(hObj);

        // If the player reads this, you must rewind to pass it along to the ClientFXMgr
		m_pPlayer->SpecialEffectMessage(pMessage);
	}
    */

	pMessage->SeekTo(0);
	m_ClientFXMgr.OnSpecialEffectNotify( hObj, pMessage );
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnTouchNotify(HOBJECT hMain, CollisionInfo *pInfo, 
//					float forceMag)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::OnTouchNotify(HOBJECT hMain, CollisionInfo *pInfo, 
							float forceMag)
{
	//g_pLTClient->CPrint("OnTouchNotify(%p,%p,%g)", hMain, pInfo, forceMag);

	return LT_OK;
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnObjectRemove(HLOCALOBJ hObj)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnObjectRemove(HLOCALOBJ hObj)
{
	g_pLTClient->CPrint("OnObjectRemove(%p)", hObj);
	if(!hObj) return;
	m_ClientFXMgr.OnObjectRemove( hObj );
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnObjectMove(HLOCALOBJ hObj, bool bTeleport, LTVector *pNewPos)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::OnObjectMove(HLOCALOBJ hObj, bool bTeleport, LTVector *pNewPos)
{
	//g_pLTClient->CPrint("OnObjectMove(%p,%b,%p)", hObj, bTeleport, pNewPos);

	return LT_OK;
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnObjectRotate(HLOCALOBJ hObj, bool bTeleport, LTRotation *pNewRot)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::OnObjectRotate(HLOCALOBJ hObj, bool bTeleport, LTRotation *pNewRot)
{
	//g_pLTClient->CPrint("OnObjectRotate(%p,%d,%p)", hObj, bTeleport, pNewRot);

	return LT_OK;
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnPlaySound(PlaySoundInfo *pPlaySoundInfo)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnPlaySound(PlaySoundInfo *pPlaySoundInfo)
{
	//g_pLTClient->CPrint("OnPlaySound(%p)", pPlaySoundInfo);
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnConsolePrint(CConsolePrintData *pData)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnConsolePrint(CConsolePrintData *pData)
{
	//g_pLTClient->CPrint("OnConsolePrint(%p)",pData);
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnModelKey(HLOCALOBJ hObj, ArgList *pArgs)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnModelKey(HLOCALOBJ hObj, ArgList *pArgs)
{
	//g_pLTClient->CPrint("OnModelKey(%p, %p)", hObj, pArgs);
}

//-----------------------------------------------------------------------------
//  CLTClientShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4], 
//								uint16 senderPort)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4], 
									uint16 senderPort)
{
	//g_pLTClient->CPrint("ProcessPacket(%p,%p,%d)", pMsg, senderAddr, senderPort);

	return LT_OK;
}

//-----------------------------------------------------------------------------
//  CLTClientShell::OnEngineTerm()
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnEngineTerm()
{
	//g_pLTClient->CPrint("OnEngineTerm()");

	// Free memory
	if( m_hCamera )
	{	
		g_pLTClient->RemoveObject(m_hCamera);
		delete m_pCamera;
		m_pCamera = NULL;
	}
	if( m_hPlayer )
	{
		g_pLTClient->RemoveObject(m_hPlayer);
	}
	
	this->DestroyWorldPropObject();

    // Remove the camera handle from the ClientFXMgr
    m_ClientFXMgr.SetCamera(NULL);

    m_Gui.Dispose();

    // Clear our list
    //m_stlvFXNameList.clear();
}

//-----------------------------------------------------------------------------
//  CLTClientShell::PreUpdate()
//
//-----------------------------------------------------------------------------
void CLTClientShell::PreUpdate()
{
	//g_pLTClient->CPrint("PreUpdate()");
}

//-----------------------------------------------------------------------------
//  CLTClientShell::PostUpdate()
//
//-----------------------------------------------------------------------------
void CLTClientShell::PostUpdate()
{
	//g_pLTClient->CPrint("PostUpdate()");
}

//-----------------------------------------------------------------------------
//  CLTClientShell::PreLoadWorld(const char *pWorldName)
//
//-----------------------------------------------------------------------------
void CLTClientShell::PreLoadWorld(const char *pWorldName)
{
	//g_pLTClient->CPrint("PreLoadWorld(%p)", pWorldName);
}


//-----------------------------------------------------------------------------
//  CLTClientShell::SendFxChangeToServer(const char* sFxName)
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendFxChangeToServer(const char* sFxName)
{

	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);
	
	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_CHANGEFX);
		pMessage->WriteString(sFxName);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}

}


//-----------------------------------------------------------------------------
//  CLTClientShell::SendStopFxToServer()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendStopFxToServer()
{

	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);
	
	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_STOPEFFECT);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();

        m_Gui.SetTitleText("Current FX: <none>");
	}

}


//-----------------------------------------------------------------------------
//  CLTClientShell::RecalcSideText()
//
//-----------------------------------------------------------------------------
void CLTClientShell::RecalcSideText()
{
    
    char buf[1024];
    buf[0] = '\0';
    uint32 nSize = CClientFXDB::GetSingleton().GetNumFX();
    if(m_nFxPageOffset != 0)
    {
        sprintf(buf, "Page %d of %d\n", (m_nFxPageOffset/10) + 1, (nSize/10) + 1);
    }
    else
    {
        sprintf(buf, "Page 1 of %d\n", (nSize/10) + 1);
    }
    
    for(int i = 1; i < 11; i++)
    {
        char tmp[64];
        memset(tmp, 0, 63);
        char *name;
        if( (m_nFxPageOffset + i - 1) < nSize)
        {
            name = CClientFXDB::GetSingleton().GetFXNameByIndex(m_nFxPageOffset + i - 1);
        }
        else
        {
            name = "\0";
        }
        
        int num = i;
        if(num == 10)
        {
            num = 0;
        }

        sprintf(tmp, "%d. %s\n", num, name);
        strcat(buf, tmp);
    }
    strcat(buf, "] Next\n[ Prev\nT Toggle Gui\nQ Stop Effect");

    m_Gui.SetSideText(buf);

}
