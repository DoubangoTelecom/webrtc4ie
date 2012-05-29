/*
* Copyright (C) 2012 Doubango Telecom <http://www.doubango.org>
*
* Contact: Mamadou Diop <diopmamadou(at)doubango[dot]org>
*	
* This file is part of Open Source webrtc4ie project <http://code.google.com/p/webrtc4ie/>
*
* webrtc4ie is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as publishd by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*	
* webrtc4ie is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*	
* You should have received a copy of the GNU General Public License
* along with webrtc4ie.
*/
#include "StdAfx.h"
#include "Utils.h"
#include "NetTransport.h"
#include "PeerConnection.h"

#include "tinydav/tdav.h"

#include "tinymedia.h"
#include "tinynet.h"

static BOOL g_bInitialized = FALSE;
static BOOL g_bHasDebugConsole = FALSE;
static char* g_sNullTerminated = NULL;
static UINT g_iNullTerminated = 0;
static CRITICAL_SECTION g_CS;

void CUtils::Initialize(void)
{
	if(!g_bInitialized){
		InitializeCriticalSection(&g_CS);

		int iRet;		
		if((iRet = tnet_startup()) != 0){
			TSK_DEBUG_ERROR("tnet_startup failed with error code=%d", iRet);
			return;
		}
		if(tdav_init() == 0){
			g_bInitialized = TRUE;
			TSK_DEBUG_INFO("Library succeesfully initilized");
		}
		else{
			TSK_DEBUG_ERROR("Failed to initialize");
		}

		// Disable AMR, G.729, H.261 codecs
		tdav_set_codecs((tdav_codec_id_t)(
				tdav_codec_id_gsm |
				tdav_codec_id_pcma |
				tdav_codec_id_pcmu |
				tdav_codec_id_ilbc |
				tdav_codec_id_speex_nb |
				tdav_codec_id_speex_wb |
				tdav_codec_id_speex_uwb |
				tdav_codec_id_g722 |

				tdav_codec_id_h263 |
				tdav_codec_id_h263p |
				tdav_codec_id_h263pp |
				tdav_codec_id_h264_bp |
				tdav_codec_id_h264_mp |
				tdav_codec_id_h264_hp |
				tdav_codec_id_theora |
				tdav_codec_id_mp4ves_es |
				tdav_codec_id_vp8)
			);

		tmedia_defaults_set_profile(tmedia_profile_rtcweb);
		tmedia_defaults_set_pref_video_size(tmedia_pref_video_size_vga);
	}
}


BOOL CUtils::StartDebug(void)
{
	if (AllocConsole()){
		freopen("CONIN$", "r", stdin); 
		freopen("CONOUT$", "w", stdout); 
		freopen("CONOUT$", "w", stderr);
		SetConsoleTitleA("WebRTC for IE Debug Console");
		return TRUE;
	}
	return FALSE;
}

BOOL CUtils::StopDebug(void)
{
	return FreeConsole();
}

_bstr_t CUtils::ToBSTR(const char* notNullTerminated, UINT size)
{
	EnterCriticalSection(&g_CS);
	if(g_iNullTerminated < (size + 1)){
		if(!(g_sNullTerminated = (char*)realloc(g_sNullTerminated, (size + 1)))) g_iNullTerminated = 0;
		else g_iNullTerminated = (size + 1);
	}

	if(g_iNullTerminated){
		memcpy(g_sNullTerminated, notNullTerminated, size);
		g_sNullTerminated[size] = '\0';
	}

	_bstr_t oData(g_sNullTerminated);

	LeaveCriticalSection(&g_CS);

	return oData;
}

LRESULT CALLBACK CUtils::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg){
		case WM_NET_EVENT:
			{
				TSK_DEBUG_INFO("Utils::WndProc::WM_NET_EVENT");			
				CNetTransport* This = reinterpret_cast<CNetTransport*>(wParam);
				CNetTransportEvent* oEvent = reinterpret_cast<CNetTransportEvent*>(lParam);
				if(oEvent){
					This->Fire_OnEvent(oEvent->GetType(), oEvent->GetData());
					delete oEvent;
				}
				break;
			}

		case WM_ICE_EVENT_CANDIDATE:
			{
				TSK_DEBUG_INFO("Utils::WndProc::WM_ICE_EVENT");
				CPeerConnection* This = reinterpret_cast<CPeerConnection*>(wParam);
				CPeerConnectionEvent* oEvent = reinterpret_cast<CPeerConnectionEvent*>(lParam);
				if(oEvent){
					This->Fire_IceCallback(oEvent->GetMedia(), oEvent->GetCandidate(), oEvent->GetMoreToFollow());
					delete oEvent;
				}
				break;
			}
		case WM_ICE_EVENT_CONNECTED:
			{
				CPeerConnection* This = reinterpret_cast<CPeerConnection*>(wParam);
				This->StartMedia();
				break;
			}
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}