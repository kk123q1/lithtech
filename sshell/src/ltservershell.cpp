//------------------------------------------------------------------------------//
//
// MODULE   : ltservershell.cpp
//
// PURPOSE  : CLTServerShell - Implementation
//
// CREATED  : 03/02/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

// Definition of class
#include "ltservershell.h"

// Global pointers to server interfaces
#include "serverinterfaces.h"

// Msg Ids
#include "msgids.h"

//--Engine includes--

// Object Create Struct definition
#include <ltobjectcreate.h>

// ILTPhysics definition
#include <iltphysics.h>

// ILTCommon definition
#include <iltcommon.h>

#include "AutoMessage.h"
#include "FxDefs.h"
#include "FxFlags.h"
#include "UserFlags.h"

//-----------------------------------------------------------------------------
SETUP_SERVERSHELL()

// Define our instantiation of the IServerShell interface.
define_interface(CLTServerShell, IServerShell);

CLTServerShell *g_pSShell;

CLTServerShell::CLTServerShell()
:
m_hClient(NULL),
m_pClientObj(NULL),
m_hClientHandle(NULL)
{}


//-----------------------------------------------------------------------------
//  CLTServerShell::OnServerInitialized()
//
//-----------------------------------------------------------------------------
LTRESULT CLTServerShell::OnServerInitialized()
{
	//g_pLTServer->CPrint("OnServerInitialized()");
	LTRESULT result;

	// Set the pointer to our server shell
	g_pSShell = this;

	// Verify that all server interfaces initialized correctly.
	result = VerifyServerInterfaces();
	if (LT_OK != result)
	{
		return result;
	}

	return LT_OK;
}


//-----------------------------------------------------------------------------
//  CLTServerShell::OnServerTerm()
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnServerTerm()
{
	//g_pLTServer->CPrint("OnServerTerm()");
}

//-----------------------------------------------------------------------------
//  CLTServerShell::SRand(unsigned int uiRand)
//
//-----------------------------------------------------------------------------
void CLTServerShell::SRand(unsigned int uiRand)
{
	//g_pLTServer->CPrint("CLTServerShell::SRand");
	srand(uiRand);
}

//----------------------------------------------------------------------------
//  CLTServerShell::RegisterObject(CWorldProperties *pWorldProps)
//
//----------------------------------------------------------------------------
void CLTServerShell::RegisterObject(WorldProperties *pWorldProps)
{
	m_pWorldProperties = pWorldProps;
}


//----------------------------------------------------------------------------
//  CLTServerShell::UnRegisterObject(CWorldProperties *pWorldProps)
//
//----------------------------------------------------------------------------
void CLTServerShell::UnRegisterObject(WorldProperties *pWorldProps)
{
	m_pWorldProperties = NULL;
}


//----------------------------------------------------------------------------
// CLTServerShell::OnClientEnterWorld(HCLIENT hClient)
//
//----------------------------------------------------------------------------
LPBASECLASS CLTServerShell::OnClientEnterWorld(HCLIENT hClient)
{
	ObjectCreateStruct objCreateStruct;
	HCLASS hClass;
	LTVector vPos, vStartPos;
	LTRotation rStartRot;

	hClass = g_pLTServer->GetClass("CPlayerSrvr");

	if (!hClass)
	{
		assert(!"ERROR: Couldn't get BaseClass!!");
		return NULL;
	}

	objCreateStruct.Clear();
	objCreateStruct.m_ObjectType = OT_NORMAL;
	objCreateStruct.m_Flags = FLAG_SOLID | FLAG_FORCECLIENTUPDATE;

	// Set client's pos and rot according to the "StartPoint", if one exists.
	this->FindStartPoint(vStartPos, rStartRot);
	objCreateStruct.m_Pos = vStartPos;
	objCreateStruct.m_Rotation = rStartRot;

	// Create our client object
	m_pClientObj = g_pLTServer->CreateObject(hClass, &objCreateStruct);

	// Store our client
	m_hClient = hClient;

	if (m_pClientObj)
	{
		g_pLTServer->SetClientUserData(hClient, m_pClientObj);
		m_hClientHandle = g_pLTServer->ObjectToHandle(m_pClientObj);
		vPos.x = vPos.y = vPos.z = 10.0f;
		g_pLTSPhysics->SetObjectDims(m_hClientHandle, &vPos, 0);

		{
			// Create a Special FX message and attach it to the object
			ILTMessage_Write *pMsg;
			LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
			if (LT_OK == nResult)
			{
				pMsg->IncRef();
				pMsg->Writeint8(MSG_PLAYEROBJ);
				g_pLTServer->SetObjectSFXMessage(m_hClientHandle, pMsg->Read());
				pMsg->DecRef();
			}
		}
	}

	// Send the props to the client
	if( m_pWorldProperties )
		m_pWorldProperties->SendToClient();
	else
		g_pLTServer->CPrint("WARNING: Invalid WorldProps object, can't send world props");

	// Send over the position of the startpoint so the client can position the player
	ILTMessage_Write *pMsg;
	LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	pMsg->IncRef();
	pMsg->Writeint8(MSG_STARTPOINT_POS_ROT);
	pMsg->WriteLTVector(vStartPos);
	pMsg->WriteLTRotation(rStartRot);
	g_pLTServer->SendToClient(pMsg->Read(), LTNULL,  MESSAGE_GUARANTEED);
	pMsg->DecRef();

	return m_pClientObj;
}


//----------------------------------------------------------------------------
//  CLTServerShell::OnClientExitWorld(HCLIENT hClient)
//
//----------------------------------------------------------------------------
void CLTServerShell::OnClientExitWorld(HCLIENT hClient)
{
	//g_pLTServer->CPrint("OnClientExitWorld(%p)", hClient);
}


//----------------------------------------------------------------------------
//  CLTServerShell::OnMessage(HCLIENT hSender, ILTMessage_Read *pMessage)
//
//----------------------------------------------------------------------------
void CLTServerShell::OnMessage(HCLIENT hSender, ILTMessage_Read *pMessage)
{
	//g_pLTServer->CPrint("OnMessage(%p,%p)", hSender, pMessage);
	
	BaseClass *pBaseClass = (BaseClass *)g_pLTServer->GetClientUserData(hSender);
	HOBJECT hPlayer = g_pLTServer->ObjectToHandle(pBaseClass);
	
	pMessage->SeekTo(0);
	uint8 messageID = pMessage->Readuint8();
	
	switch(messageID)
	{
	case MSG_CS_POSITION:
		{
			//g_pLTServer->CPrint("MSG_CS_POSITION");
			LTVector vPos;
			vPos = pMessage->ReadLTVector();
			g_pLTSPhysics->MoveObject(hPlayer, &vPos, 0);

			g_pLTServer->GetObjectPos(hPlayer, &vPos);
			g_pLTServer->SetClientViewPos(hSender, &vPos);
		}
		break;
	case MSG_CS_ROTATION:
		{
			//g_pLTServer->CPrint("MSG_CS_ROTATION");
			LTRotation rRot;
			rRot = pMessage->ReadLTRotation();
			g_pLTServer->SetObjectRotation(hPlayer, &rRot);
		}
		break;
	case MSG_CS_POSANDROT:
		{
			//g_pLTServer->CPrint("MSG_CS_POSANDROT");
			LTVector vPos;
			LTRotation rRot;
			vPos = pMessage->ReadLTVector();
			rRot = pMessage->ReadLTRotation();
			g_pLTSPhysics->MoveObject(hPlayer, &vPos, 0);
			g_pLTServer->SetObjectRotation(hPlayer, &rRot);

			g_pLTServer->GetObjectPos(hPlayer, &vPos);
			g_pLTServer->SetClientViewPos(hSender, &vPos);
		}
		break;
    case MSG_CS_CHANGEFX:
        {
            char sFxName[64];
            pMessage->ReadString(sFxName, 63);
            g_pLTServer->CPrint("SERVER Fx: %s", sFxName);
            

            // Find our special effect object
            ObjArray<HOBJECT, 1> _FxObjects;
            uint32 nFound = 0;
            g_pLTServer->FindNamedObjects("SpecialFX", _FxObjects, &nFound);

            if(nFound > 0)
            {

                // Inform the client that we are removing this object. OT_NORMAL objects
                // don't invoke the clientshell's OnObjectRemove, which we need to use to
                // inform clientfx to remove the effect.
                ILTMessage_Write *pMsg;
                LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
                pMsg->IncRef();
                pMsg->Writeuint8(MSG_SC_REMOVEOBJECT);
                pMsg->WriteObject(_FxObjects.GetObject(0));
                g_pLTServer->SendToClient(pMsg->Read(), hSender, 0);
                pMsg->DecRef();

                // Remove this object from the server
                g_pLTServer->RemoveObject(_FxObjects.GetObject(0));
            }
                // Create new object
                ObjectCreateStruct ocs;
                ocs.Clear();
                ocs.m_ObjectType = OT_NORMAL;
                strncpy(ocs.m_Name, "SpecialFX", 127);
                strncpy(ocs.m_Filename, "SpecialFX", 127);
                ocs.m_Pos.Init(0.0f, -32.0f, 0.0f);
                ocs.m_Rotation.Init();

                HCLASS hClass = g_pLTServer->GetClass("BaseClass");
                BaseClass* pObj = (BaseClass*)g_pLTServer->CreateObject(hClass, &ocs);

                if(pObj)
                {

                    // Set up the FX message
                    CAutoMessage cMsg;
                    cMsg.Writeuint8( SFX_CLIENTFXGROUP );
                    cMsg.WriteString( sFxName );
                    cMsg.Writeuint32( FXFLAG_LOOP );
                    cMsg.Writeuint8( false );
                                       
                    g_pLTServer->SetObjectSFXMessage( pObj->m_hObject, cMsg.Read() );
                    
                    // Set flags so the client knows we are on...
                    g_pLTSCommon->SetObjectFlags( pObj->m_hObject, OFT_Flags, FLAG_FORCECLIENTUPDATE, FLAG_FORCECLIENTUPDATE );                
                    g_pLTSCommon->SetObjectFlags( pObj->m_hObject, OFT_User, USRFLG_SFX_ON, USRFLG_SFX_ON );
                }
                

            
        }
        break;
    case MSG_CS_STOPEFFECT:
        {
            // Find our special effect object
            ObjArray<HOBJECT, 1> _FxObjects;
            uint32 nFound = 0;
            g_pLTServer->FindNamedObjects("SpecialFX", _FxObjects, &nFound);

            if(nFound > 0)
            {

                // Inform the client that we are removing this object. OT_NORMAL objects
                // don't invoke the clientshell's OnObjectRemove, which we need to use to
                // inform clientfx to remove the effect.
                ILTMessage_Write *pMsg;
                LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
                pMsg->IncRef();
                pMsg->Writeuint8(MSG_SC_REMOVEOBJECT);
                pMsg->WriteObject(_FxObjects.GetObject(0));
                g_pLTServer->SendToClient(pMsg->Read(), hSender, 0);
                pMsg->DecRef();

                // Remove this object from the server
                g_pLTServer->RemoveObject(_FxObjects.GetObject(0));
            }
            
        }
        break;
		
		
	default:
		g_pLTServer->CPrint("Server: Received Unknown message");
		break;
	}
	
}


//----------------------------------------------------------------------------
// CLTServerShell::FindStartPoint(LTVector &vStartPtPos, 
//                                LTRotation &rStartPtRot)
//
//----------------------------------------------------------------------------
LTRESULT CLTServerShell::FindStartPoint(LTVector &vStartPtPos, 
										LTRotation &rStartPtRot)
{
	
	// Get a list of the startpoint objects with a particular name (should be 1)
	ObjArray <HOBJECT, 1> pStartPt;
	
	g_pLTServer->FindNamedObjects("GameStartPoint0", pStartPt);
	
	if (pStartPt.NumObjects() == 0)
	{
		// Fall back: Check for objects named "StartPoint".
		g_pLTServer->FindNamedObjects("StartPoint", pStartPt);
		if (pStartPt.NumObjects() == 0)
		{
			vStartPtPos.Init(0.0f, 0.0f, 0.0f);
			rStartPtRot.Init();
			g_pLTServer->CPrint("Can't find StartPoint. Using (%f, %f, %f) instead.", 
				vStartPtPos.x,
				vStartPtPos.y,
				vStartPtPos.z);
			return LT_NOTFOUND;
		}
		else
		{
			// Get startpoint's position and rotation
			g_pLTServer->GetObjectPos(pStartPt.GetObject(0), &vStartPtPos);
			g_pLTServer->GetObjectRotation(pStartPt.GetObject(0), &rStartPtRot);			
		}
	}
	else
	{
		// Get startpoint's position and rotation
		g_pLTServer->GetObjectPos(pStartPt.GetObject(0), &vStartPtPos);
		g_pLTServer->GetObjectRotation(pStartPt.GetObject(0), &rStartPtRot);					
	}
	
	return LT_OK;
	
}

//-----------------------------------------------------------------------------
//  CLTServerShell::OnAddClient(HCLIENT hClient)
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnAddClient(HCLIENT hClient)
{
	//g_pLTServer->CPrint("OnAddClient(%p)", hClient);
}

//-----------------------------------------------------------------------------
//  CLTServerShell::OnRemoveClient(HCLIENT hClient)
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnRemoveClient(HCLIENT hClient)
{
	//g_pLTServer->CPrint("OnRemoveClient(%p)", hClient);
}

//-----------------------------------------------------------------------------
//  CLTServerShell::PreStartWorld(bool bSwitchingWorlds)
//
//-----------------------------------------------------------------------------
void CLTServerShell::PreStartWorld(bool bSwitchingWorlds)
{
	//g_pLTServer->CPrint("PreStartWorld(%d)", bSwitchingWorlds);
}

//-----------------------------------------------------------------------------
//  CLTServerShell::PostStartWorld()
//
//-----------------------------------------------------------------------------
void CLTServerShell::PostStartWorld()
{
	//g_pLTServer->CPrint("PostStartWorld()");
}

//-----------------------------------------------------------------------------
//  CLTServerShell::Update(LTFLOAT timeElapsed)
//
//-----------------------------------------------------------------------------
void CLTServerShell::Update(LTFLOAT timeElapsed)
{
	//g_pLTServer->CPrint("Update(%g)", timeElapsed);
}

//-----------------------------------------------------------------------------
//  CLTServerShell::CacheFiles()
//
//-----------------------------------------------------------------------------
void CLTServerShell::CacheFiles()
{
	//g_pLTServer->CPrint("CacheFiles()");
}

//-----------------------------------------------------------------------------
//  CLTServerShell::FileLoadNotify(const char *pFilename, LTRESULT status)
//
//-----------------------------------------------------------------------------
LTRESULT CLTServerShell::FileLoadNotify(const char *pFilename, LTRESULT status)
{
	//g_pLTServer->CPrint("FileLoadNotify(%s, %d)", pFilename, status);

	return LT_OK;
}

//-----------------------------------------------------------------------------
//  CLTServerShell::OnPlaybackFinish()
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnPlaybackFinish()
{
	//g_pLTServer->CPrint("OnPlaybackFinish()");
}

//-----------------------------------------------------------------------------
//  CLTServerShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4], 
//		uint16 senderPort)
//
//-----------------------------------------------------------------------------
LTRESULT CLTServerShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4],
										uint16 senderPort)
{
	//g_pLTServer->CPrint("ProcessPacket(%p, %p, %d)",pMsg, senderAddr, senderPort);
	
	return LT_OK;
}

