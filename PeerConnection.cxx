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

#include "stdafx.h"
#include "PeerConnection.h"
#include "Utils.h"

#include "tinydav/tdav.h"

#include "tinymedia.h"
#include "tsk.h"

#include <comutil.h>
#include <stdio.h>

#define THIS_VERSION	"1.0.0"
#define USE_IPV6		FALSE
#define USE_RTCP_MUX	TRUE
#define USE_ICE_RTCP	(!USE_RTCP_MUX)
#define USE_ICE_JINGLE	TRUE
#define ICE_TIMEOUT		6000

HWND FIXME_HWND = NULL;

typedef enum IceOptions_e
{
	IceOptions_All,
	IceOptions_NoRelay,
	IceOptions_OnlyRelay
}
IceOptions_t;

typedef enum ReadyState_e
{
	ReadyStateNew = 0, // initial state
	ReadyStateOpening = 1, // local or remote desc set
	ReadyStateActive = 2,  // local and remote desc set
	ReadyStateClosed = 3
}
ReadyState_t;
	
typedef enum IceState_e
{
	IceStateGathering = 0x100,
    IceStateWaiting = 0x200,
    IceStateChecking = 0x300,
    IceStateConnected = 0x400,
    IceStateCompleted = 0x500,
    IceStateFailed = 0x600,
    IceStateClosed = 0x700
}
IceState_t;

typedef enum SdpAction_e
{
	SdpActionOffer = 0x100,
	SdpActionProvisionalAnswer = 0x200,
	SdpActionAnswer = 0x300
}
SdpAction_t;

static BOOL g_bAlwaysCreateOnCurrentThread = TRUE; // because "ThreadingModel 'Apartment'"
static BOOL g_bAVPF = TRUE;

STDMETHODIMP CPeerConnection::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_IPeerConnection
	};

	for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}

CPeerConnection::CPeerConnection():
mRemoteVideo(0),
mLocalVideo(0),
mReadyState(ReadyStateNew),
mIceState(IceStateClosed),
mSessionMgr(NULL),
mMediaType(tmedia_none),
mSdpLocal(NULL),
mSdpRemote(NULL),
mIceCtxAudio(NULL),
mIceCtxVideo(NULL),
mLooperHandle(NULL),
mLooperProc(NULL),
mFullScreen(VARIANT_FALSE)
{
	CUtils::Initialize();
	InitializeCriticalSection(&mCSIceCallback);
}

CPeerConnection::~CPeerConnection()
{
	TSK_OBJECT_SAFE_FREE(mSessionMgr);
	TSK_OBJECT_SAFE_FREE(mIceCtxAudio);
	TSK_OBJECT_SAFE_FREE(mIceCtxVideo);

	TSK_OBJECT_SAFE_FREE(mSdpLocal);
	TSK_OBJECT_SAFE_FREE(mSdpRemote);

	DeleteCriticalSection(&mCSIceCallback);
}

STDMETHODIMP CPeerConnection::close(void)
{
	TSK_OBJECT_SAFE_FREE(mSessionMgr);
	TSK_OBJECT_SAFE_FREE(mIceCtxAudio);
	TSK_OBJECT_SAFE_FREE(mIceCtxVideo);

	mReadyState = ReadyStateClosed;
	mIceState = IceStateClosed;

	if(mLooperHandle && mLooperProc){
		SetWindowLongPtr(mLooperHandle, GWL_WNDPROC, (LONG)mLooperProc);
	}

	return S_OK;
}

STDMETHODIMP CPeerConnection::createOffer(VARIANT_BOOL has_audio, VARIANT_BOOL has_video, BSTR* offer)
{
	return CreateLo(has_audio, has_video, offer, TRUE);
}

STDMETHODIMP CPeerConnection::createAnswer(VARIANT_BOOL has_audio, VARIANT_BOOL has_video, BSTR* answer)
{
	return CreateLo(has_audio, has_video, answer, FALSE);
}


STDMETHODIMP CPeerConnection::startIce(SHORT IceOptions, LONGLONG looper)
{
	IceOptions_t options = (IceOptions_t)IceOptions;

	(options);

	if(!mSdpLocal){
		TSK_DEBUG_ERROR("No local offer yet");
		return E_FAIL;
	}

	int iRet;

	if(!looper){
		TSK_DEBUG_WARN("Starting without looper");
	}
	if(mLooperHandle && mLooperProc){
		SetWindowLongPtr(mLooperHandle, GWL_WNDPROC, (LONG)mLooperProc);
		mLooperProc = NULL;
	}
	if((mLooperHandle = (HWND)looper)){
		mLooperProc = (WNDPROC) SetWindowLongPtr(mLooperHandle, GWL_WNDPROC, (LONG)CUtils::WndProc);
		if(!mLooperProc){
			TSK_DEBUG_ERROR("SetWindowLongPtr() failed with errcode=%d", GetLastError());
		}
	}
#if 0
	if((iRet = tmedia_session_mgr_set_ice_ctx(mSessionMgr, mIceCtxAudio, mIceCtxVideo)) != 0){
		TSK_DEBUG_ERROR("Failed to set ICE contexts");
		return E_FAIL;
	}
#endif

	if((mMediaType & tmedia_audio)){
		if(mIceCtxAudio && (iRet = tnet_ice_ctx_start(mIceCtxAudio)) != 0){
			TSK_DEBUG_WARN("tnet_ice_ctx_start() failed with error code = %d", iRet);
			return E_FAIL;
		}
	}
	if((mMediaType & tmedia_video)){
		if(mIceCtxVideo && (iRet = tnet_ice_ctx_start(mIceCtxVideo)) != 0){
			TSK_DEBUG_WARN("tnet_ice_ctx_start() failed with error code = %d", iRet);
			return E_FAIL;
		}
	}

	return S_OK;
}

STDMETHODIMP CPeerConnection::setLocalDescription(USHORT action, BSTR desc)
{
	(action);

	if(mReadyState == ReadyStateActive){
#if 0 // ICE never timeout: never setup a call without ICE
		IceSetTimeout(ICE_TIMEOUT);
#endif
	 }

	return S_OK;
}

STDMETHODIMP CPeerConnection::setRemoteDescription(USHORT action, BSTR desc)
{
	HRESULT hRet = S_OK;
	char* sdpStr = NULL;
	tsdp_message_t* sdpObj = NULL;

	if(!desc){
		TSK_DEBUG_ERROR("Invalid argument");
		hRet = E_INVALIDARG;
		goto bail;
	}

	(action);

	sdpStr = _com_util::ConvertBSTRToString(desc);
	if(!(sdpObj = tsdp_message_parse(sdpStr, tsk_strlen(sdpStr)))){
		TSK_DEBUG_ERROR("Failed to parse remote description");
		hRet = E_FAIL;
		goto bail;
	}

	if(!mSessionMgr){
		HRESULT hRet = CreateSessionMgr(tmedia_type_from_sdp(sdpObj), FALSE);
		if(FAILED(hRet)){
			return hRet;
		}
	}

	int iRet;
	if((iRet = tmedia_session_mgr_set_ro(mSessionMgr, sdpObj)) != 0){
		TSK_DEBUG_ERROR("tmedia_session_mgr_set_ro() failed with error code = %d", iRet);
		hRet = E_FAIL;
		goto bail;
	}

	switch(action){
		case SdpActionOffer:
			{
				if(FAILED(IceProcessRo(sdpObj, TRUE))){
					TSK_DEBUG_ERROR("Failed to process remote offer");
					hRet = E_FAIL;
					goto bail;
				}
				break;
			}
		case SdpActionAnswer:
		case SdpActionProvisionalAnswer:
			{
				if(FAILED(IceProcessRo(sdpObj, FALSE))){
					TSK_DEBUG_ERROR("Failed to process remote offer");
					hRet = E_FAIL;
					goto bail;
				}
				break;
			}
		default:
			{
				TSK_DEBUG_ERROR("Unknown action: %hu", action);
				hRet = E_FAIL;
				goto bail;
			}
	}

	TSK_OBJECT_SAFE_FREE(mSdpRemote);
	mSdpRemote = (tsdp_message_t*)tsk_object_ref(sdpObj);
	mReadyState = (mSdpLocal && mSdpRemote) ? ReadyStateActive : ReadyStateOpening;

	if(mReadyState == ReadyStateActive){
		IceSetTimeout(ICE_TIMEOUT);
	 }
	
bail:
	TSK_FREE(sdpStr);
	TSK_OBJECT_SAFE_FREE(sdpObj);
	return hRet;
}

STDMETHODIMP CPeerConnection::get_localDescription(BSTR* pVal)
{
	return SerializeSdp(mSdpLocal, pVal);
}

STDMETHODIMP CPeerConnection::get_remoteDescription(BSTR* pVal)
{
	return SerializeSdp(mSdpRemote, pVal);
}

STDMETHODIMP CPeerConnection::get_readyState(USHORT* pVal)
{
	*pVal = (USHORT)mReadyState;
	return S_OK;
}

STDMETHODIMP CPeerConnection::get_iceState(USHORT* pVal)
{
	*pVal = (USHORT)mIceState;
	return S_OK;
}

STDMETHODIMP CPeerConnection::get_remoteVideo(LONGLONG* pVal)
{
	*pVal = mRemoteVideo;
	return S_OK;
}

STDMETHODIMP CPeerConnection::put_remoteVideo(LONGLONG newVal)
{
	mRemoteVideo = newVal;
	return S_OK;
}

STDMETHODIMP CPeerConnection::get_localVideo(LONGLONG* pVal)
{
	*pVal = mLocalVideo;
	return S_OK;
}

STDMETHODIMP CPeerConnection::put_localVideo(LONGLONG newVal)
{
	mLocalVideo = newVal;
	return S_OK;
}

STDMETHODIMP CPeerConnection::get_version(BSTR* pVal)
{
	*pVal = _bstr_t(THIS_VERSION);
	return S_OK;
}

STDMETHODIMP CPeerConnection::get_fullScreen(VARIANT_BOOL* pVal)
{
	*pVal = mFullScreen;
	return S_OK;
}

STDMETHODIMP CPeerConnection::put_fullScreen(VARIANT_BOOL newVal)
{
	mFullScreen = newVal;
	if(mSessionMgr){
		int32_t fs = (mFullScreen == VARIANT_TRUE) ? 1 : 0;
		tmedia_session_mgr_set(mSessionMgr,
			TMEDIA_SESSION_CONSUMER_SET_INT32(tmedia_video, "fullscreen", fs),
			TMEDIA_SESSION_SET_NULL());
	}
	
	return S_OK;
}

STDMETHODIMP CPeerConnection::Init(BSTR configuration)
{
	if(configuration){ // e.g. STUN 'stun.l.google.com:19302'
	}
	return S_OK;
}

STDMETHODIMP CPeerConnection::StartDebug()
{
	CUtils::StartDebug();
	return S_OK;
}

STDMETHODIMP CPeerConnection::StopDebug()
{
	CUtils::StopDebug();
	return S_OK;
}

void CPeerConnection::StartMedia()
{
	if(mSessionMgr){
		tmedia_session_mgr_start(mSessionMgr);
	}
}

HRESULT CPeerConnection::CreateSessionMgr(tmedia_type_t eMediaType, BOOL offerer)
{
	if(mSessionMgr){
		TSK_DEBUG_WARN("Session manager already defined");
		return S_OK;
	}

	int iRet;
	tnet_ip_t bestsource;
	if((iRet = tnet_getbestsource("sipml5.org", 5060, tnet_socket_type_udp_ipv4, &bestsource))){
		TSK_DEBUG_ERROR("Failed to get best source [%d]", iRet);
		memcpy(bestsource, "0.0.0.0", 7);
	}
	if(!(mSessionMgr = tmedia_session_mgr_create(eMediaType, bestsource, USE_IPV6, offerer))){
		TSK_DEBUG_ERROR("Failed to create media session manager");
		return E_FAIL;
	}

	mMediaType = eMediaType;
	HRESULT hRet = IceCreateCtx(eMediaType);
	if(FAILED(hRet)){
		TSK_DEBUG_ERROR("Failed to create ICE context");
		return hRet;
	}

	if((iRet = tmedia_session_mgr_set_ice_ctx(mSessionMgr, mIceCtxAudio, mIceCtxVideo)) != 0){
		TSK_DEBUG_ERROR("Failed to set ICE contexts");
		return E_FAIL;
	}
	 
	int32_t fs = (mFullScreen == VARIANT_TRUE) ? 1 : 0;
	tmedia_session_mgr_set(mSessionMgr,
			TMEDIA_SESSION_SET_INT32(mSessionMgr->type, "avpf-enabled", g_bAVPF), // Otherwise will be negociated using SDPCapNeg (RFC 5939)
			TMEDIA_SESSION_PRODUCER_SET_INT64(tmedia_video, "local-hwnd", mLocalVideo),
			TMEDIA_SESSION_PRODUCER_SET_INT32(tmedia_video, "create-on-current-thead", g_bAlwaysCreateOnCurrentThread),
			TMEDIA_SESSION_CONSUMER_SET_INT64(tmedia_video, "remote-hwnd", mRemoteVideo),
			TMEDIA_SESSION_CONSUMER_SET_INT32(tmedia_video, "create-on-current-thead", g_bAlwaysCreateOnCurrentThread),
			TMEDIA_SESSION_CONSUMER_SET_INT32(tmedia_video, "fullscreen", fs),
			TMEDIA_SESSION_SET_NULL());

	return S_OK;
}

HRESULT CPeerConnection::CreateLo(VARIANT_BOOL has_audio, VARIANT_BOOL has_video, BSTR* sdp, BOOL offerer)
{
	tmedia_type_t eMediaType = tmedia_none;
	if(has_audio) eMediaType = (tmedia_type_t)(eMediaType | tmedia_audio);
	if(has_video) eMediaType = (tmedia_type_t)(eMediaType | tmedia_video);

	if(!mSessionMgr){
		HRESULT hRet = CreateSessionMgr(eMediaType, offerer);
		if(FAILED(hRet)){
			return hRet;
		}
	}
	
	const tsdp_message_t* sdp_lo = tmedia_session_mgr_get_lo(mSessionMgr);
	if(!sdp_lo){
		TSK_DEBUG_ERROR("Cannot get local offer");
		return E_FAIL;
	}

	char* sdp_lo_str = tsdp_message_tostring(sdp_lo);
	if(!sdp_lo_str){
		TSK_DEBUG_ERROR("Cannot serialize local offer");
		return E_FAIL;
	}

	 _bstr_t bstr(sdp_lo_str);
	 *sdp = bstr.GetBSTR();

	 TSK_FREE(sdp_lo_str);

	 TSK_OBJECT_SAFE_FREE(mSdpLocal);
	 mSdpLocal = (tsdp_message_t*)tsk_object_ref((tsk_object_t*)sdp_lo);
	 mReadyState = (mSdpLocal && tmedia_session_mgr_get_ro(mSessionMgr)) ? ReadyStateActive : ReadyStateOpening;

	return S_OK;
}

HRESULT CPeerConnection::SerializeSdp(struct tsdp_message_s* sdp, BSTR* pVal)
{
	if(sdp){
		char* sdpStr = tsdp_message_tostring(sdp);
		if(!sdpStr){
			TSK_DEBUG_ERROR("Cannot serialize local offer");
			return E_FAIL;
		}

		 _bstr_t bstr(sdpStr);
		 *pVal = bstr.GetBSTR();
		 TSK_FREE(sdpStr);
	}
	 
	return S_OK;
}


HRESULT CPeerConnection::IceCreateCtx(tmedia_type_t _eMediaType)
{
	if(!mIceCtxAudio && (_eMediaType & tmedia_audio)){
		mIceCtxAudio = tnet_ice_ctx_create(USE_ICE_JINGLE, USE_IPV6, USE_ICE_RTCP, FALSE/*audio*/, &CPeerConnection::IceCallback, this);
		if(!mIceCtxAudio){
			TSK_DEBUG_ERROR("Failed to create ICE audio context");
			return E_FAIL;
		}
		tnet_ice_ctx_set_stun(mIceCtxAudio, "stun.l.google.com", 19302, "Doubango", "stun-username", "stun-password"); //FIXME: should depends on the
	}
	if(!mIceCtxVideo && (_eMediaType & tmedia_video)){
		mIceCtxVideo = tnet_ice_ctx_create(USE_ICE_JINGLE, USE_IPV6, USE_ICE_RTCP, TRUE/*video*/, &CPeerConnection::IceCallback, this);
		if(!mIceCtxVideo){
			TSK_DEBUG_ERROR("Failed to create ICE video context");
			return E_FAIL;
		}
		tnet_ice_ctx_set_stun(mIceCtxVideo, "stun.l.google.com", 19302, "Doubango", "stun-username", "stun-password"); // FIXME
	}

	// "none" comparison is used to exclude the "first call"
	if(mMediaType != tmedia_none && mMediaType != _eMediaType){
		// cancels contexts associated to old medias
		if(mIceCtxAudio && !(_eMediaType & tmedia_audio)){
			tnet_ice_ctx_cancel(mIceCtxAudio);
		}
		if(mIceCtxVideo && !(_eMediaType & tmedia_video)){
			tnet_ice_ctx_cancel(mIceCtxVideo);
		}
		// cancels contexts associated to new medias (e.g. session "remove" then "add")
		// cancel() on newly created contexts don't have any effect
		if(mIceCtxAudio && (!(_eMediaType & tmedia_audio) && (mMediaType & tmedia_audio))){
			//tnet_ice_ctx_cancel(mIceCtxAudio);
		}
		if(mIceCtxVideo && (!(_eMediaType & tmedia_video) && (mMediaType & tmedia_video))){
			//tnet_ice_ctx_cancel(mIceCtxVideo);
		}
	}

	mMediaType = _eMediaType;
	

	// For now disable timers until both parties get candidates
	// (RECV ACK) or RECV (200 OK)
	IceSetTimeout(-1);
	
	return S_OK;
}

HRESULT CPeerConnection::IceSetTimeout(INT timeout)
{
	if(mIceCtxAudio){
		tnet_ice_ctx_set_concheck_timeout(mIceCtxAudio, timeout);
	}
	if(mIceCtxVideo){
		tnet_ice_ctx_set_concheck_timeout(mIceCtxVideo, timeout);
	}
	return S_OK;
}

BOOL CPeerConnection::IceGotLocalCandidates()
{
	return (!tnet_ice_ctx_is_active(mIceCtxAudio) || tnet_ice_ctx_got_local_candidates(mIceCtxAudio))
			&& (!tnet_ice_ctx_is_active(mIceCtxVideo) || tnet_ice_ctx_got_local_candidates(mIceCtxVideo));
}

HRESULT CPeerConnection::IceProcessRo(const struct tsdp_message_s* sdp_ro, BOOL isOffer)
{
	if(!sdp_ro){
		TSK_DEBUG_ERROR("Invalid argument");
		return E_INVALIDARG;
	}

	char* ice_remote_candidates;
	const tsdp_header_M_t* M;
	tsk_size_t index;
	const tsdp_header_A_t *A;
	const char* sess_ufrag = tsk_null;
	const char* sess_pwd = tsk_null;
	int ret = 0, i;

	if(!sdp_ro){
		TSK_DEBUG_ERROR("Invalid argument");
		return E_INVALIDARG;
	}
	if(!mIceCtxAudio && !mIceCtxVideo){
		TSK_DEBUG_ERROR("Not ready yet");
		return E_FAIL;
	}

	// session level attributes
	
	if((A = tsdp_message_get_headerA(sdp_ro, "ice-ufrag"))){
		sess_ufrag = A->value;
	}
	if((A = tsdp_message_get_headerA(sdp_ro, "ice-pwd"))){
		sess_pwd = A->value;
	}
	
#if 0 // Use RTCWeb Profile (tmedia_profile_rtcweb)
	{
		const tsdp_header_S_t *S;
		if((S = (const tsdp_header_S_t *)tsdp_message_get_header(sdp_ro, tsdp_htype_S)) && S->value){
			self->ice.is_jingle = tsk_strcontains(S->value, tsk_strlen(S->value), "webrtc");
		}
	}
#endif
	
	for(i = 0; i < 2; ++i){
		if((M = tsdp_message_find_media(sdp_ro, i==0 ? "audio": "video"))){
			const char *ufrag = sess_ufrag, *pwd = sess_pwd;
			ice_remote_candidates = tsk_null;
			index = 0;
			if((A = tsdp_header_M_findA(M, "ice-ufrag"))){
				ufrag = A->value;
			}
			if((A = tsdp_header_M_findA(M, "ice-pwd"))){
				pwd = A->value;
			}

			while((A = tsdp_header_M_findA_at(M, "candidate", index++))){
				tsk_strcat_2(&ice_remote_candidates, "%s\r\n", A->value);
			}
			// ICE processing will be automatically stopped if the remote candidates are not valid
			// ICE-CONTROLLING role if we are the offerer
			ret = tnet_ice_ctx_set_remote_candidates(i==0 ? mIceCtxAudio : mIceCtxVideo, ice_remote_candidates, ufrag, pwd, !isOffer, USE_ICE_JINGLE);
			TSK_SAFE_FREE(ice_remote_candidates);
		}
	}

	return ret == 0 ? S_OK : E_FAIL;
}

BOOL CPeerConnection::IceIsConnected()
{
	return (!tnet_ice_ctx_is_active(mIceCtxAudio) || tnet_ice_ctx_is_connected(mIceCtxAudio))
		&& (!tnet_ice_ctx_is_active(mIceCtxVideo) || tnet_ice_ctx_is_connected(mIceCtxVideo));
}

int CPeerConnection::IceCallback(const tnet_ice_event_t *e)
{
	int ret = 0;
	CPeerConnection *This;

	This = (CPeerConnection *)e->userdata;

	EnterCriticalSection(&This->mCSIceCallback);

	TSK_DEBUG_INFO("ICE callback: %s", e->phrase);

	switch(e->type){
		case tnet_ice_event_type_started:
			{
				This->mIceState = IceStateGathering;
				break;
			}

		case tnet_ice_event_type_gathering_completed:
		case tnet_ice_event_type_conncheck_succeed:
		case tnet_ice_event_type_conncheck_failed:
		case tnet_ice_event_type_cancelled:
			{
				if(e->type == tnet_ice_event_type_gathering_completed){
					char* candidateStr = NULL;
					const char *mediaStr;
					const char* ufragStr = tnet_ice_ctx_get_ufrag(e->ctx);
					const char* pwdStr = tnet_ice_ctx_get_pwd(e->ctx);
					BOOL bGotAllCandidates = This->IceGotLocalCandidates();
					_bstr_t media((mediaStr = (e->ctx == This->mIceCtxAudio) ? "audio" : "video"));
					const tsdp_header_M_t* M = tsdp_message_find_media(This->mSdpLocal, mediaStr);
					tsk_size_t nIceCandidatesCount = tnet_ice_ctx_count_local_candidates(e->ctx);
					for(USHORT index = 0; index < nIceCandidatesCount; ++index){
						candidateStr = tsk_strdup(tnet_ice_candidate_tostring(((tnet_ice_candidate_t*)tnet_ice_ctx_get_local_candidate_at(e->ctx, index))));
						if(M){
							tsdp_header_M_add_headers((tsdp_header_M_t*)M, TSDP_HEADER_A_VA_ARGS("candidate", candidateStr), tsk_null);
						}
						tsk_strcat_2(&candidateStr, " webrtc4ie-ufrag %s webrtc4ie-pwd %s", ufragStr, pwdStr);
						_bstr_t candidate = candidateStr;
						TSK_FREE(candidateStr);
						BOOL bMoreToFollow = !(bGotAllCandidates && (index == (nIceCandidatesCount - 1)));
						if(This->mLooperHandle && This->mLooperProc){
							CPeerConnectionEvent* oEvent = new CPeerConnectionEvent(media.copy(), candidate.copy(), bMoreToFollow);
							if(!PostMessageA(This->mLooperHandle, WM_ICE_EVENT_CANDIDATE, reinterpret_cast<WPARAM>(This), reinterpret_cast<LPARAM>(oEvent))){
								if(oEvent) delete oEvent, oEvent = NULL;
								TSK_DEBUG_ERROR("PostMessageA() failed");
								This->Fire_IceCallback(media, candidate, bMoreToFollow);
							}
						}
						else{
							This->Fire_IceCallback(media, candidate, bMoreToFollow);
						}					

						TSK_FREE(candidateStr);
					}

					if(bGotAllCandidates){
						This->mIceState = IceStateCompleted;
					}
				}
				else if(e->type == tnet_ice_event_type_conncheck_succeed){
					if(This->IceIsConnected()){
						This->mIceState = IceStateConnected;
						if(!PostMessageA(This->mLooperHandle, WM_ICE_EVENT_CONNECTED, reinterpret_cast<WPARAM>(This), NULL)){
							This->StartMedia();
						}
					}
				}
				break;
			}

		// fatal errors which discard ICE process
		case tnet_ice_event_type_gathering_host_candidates_failed:
		case tnet_ice_event_type_gathering_reflexive_candidates_failed:
			{
				This->mIceState = IceStateFailed;
				break;
			}
	}

	LeaveCriticalSection(&This->mCSIceCallback);

	return ret;
}