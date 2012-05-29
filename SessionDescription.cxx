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
#include "SessionDescription.h"

#include "tinynet.h"
#include "tinysdp.h"
#include "tsk.h"

#include <comutil.h>
#include <stdio.h>

// CSessionDescription

STDMETHODIMP CSessionDescription::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_ISessionDescription
	};

	for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}

CSessionDescription::CSessionDescription()
:mSdp(NULL)
{
}

CSessionDescription::~CSessionDescription()
{
	TSK_OBJECT_SAFE_FREE(mSdp);
}

STDMETHODIMP CSessionDescription::toSdp(BSTR* sdp)
{
	if(!mSdp){
		TSK_DEBUG_ERROR("Not initialized");
		return E_FAIL;
	}

	char* sdp_str = tsdp_message_tostring(mSdp);
	if(!sdp_str){
		TSK_DEBUG_ERROR("Cannot serialize local offer");
		return E_FAIL;
	}

	 _bstr_t bstr(sdp_str);
	 *sdp = bstr.GetBSTR();

	 TSK_FREE(sdp_str);

	return S_OK;
}


STDMETHODIMP CSessionDescription::Init(BSTR sdp)
{
	if(!sdp){
		TSK_DEBUG_ERROR("Invalid argument");
		return E_INVALIDARG;
	}

	char* sdpStr = _com_util::ConvertBSTRToString(sdp);
	TSK_OBJECT_SAFE_FREE(mSdp);
	if(!(mSdp = tsdp_message_parse(sdpStr, tsk_strlen(sdpStr)))){
		TSK_DEBUG_ERROR("Initialization failed");
		TSK_FREE(sdpStr);
		return E_FAIL;
	}
	TSK_FREE(sdpStr);

	return S_OK;
}

STDMETHODIMP CSessionDescription::addCandidate(BSTR media, BSTR candidate)
{
	HRESULT hRet = S_OK;
	char *mediaStr = tsk_null;

	if(!candidate || !media){
		TSK_DEBUG_ERROR("Invalid argument");
		hRet = E_INVALIDARG;
		goto bail;
	}
	if(!mSdp){
		TSK_DEBUG_ERROR("Not initialized");
		hRet = E_FAIL;
		goto bail;
	}

	char* candidateStr = _com_util::ConvertBSTRToString(candidate);
	mediaStr = _com_util::ConvertBSTRToString(media);

	const tsdp_header_M_t* M = tsdp_message_find_media(mSdp, mediaStr);
	if(!M){
		TSK_DEBUG_ERROR("Failed to find mediaType=%s", mediaStr);
		hRet = E_FAIL;
		goto bail;
	}
	
	tnet_ice_candidate_t* oCnadidate = tnet_ice_candidate_parse(candidateStr);
	if(oCnadidate){
		if(!((tsdp_header_M_t*)M)->port){
			((tsdp_header_M_t*)M)->port = oCnadidate->port;
		}
		if(!((tsdp_header_M_t*)M)->C){
			tsdp_header_M_add_headers((tsdp_header_M_t*)M,
				TSDP_HEADER_C_VA_ARGS("IN", (FALSE ? "IP6" : "IP4"), oCnadidate->connection_addr),
				TSDP_HEADER_A_VA_ARGS("ice-ufrag", tsk_params_get_param_value(oCnadidate->extension_att_list, "webrtc4ie-ufrag")),
				TSDP_HEADER_A_VA_ARGS("ice-pwd", tsk_params_get_param_value(oCnadidate->extension_att_list, "webrtc4ie-pwd")),
					tsk_null);
		}
		tsk_params_remove_param(oCnadidate->extension_att_list, "webrtc4ie-ufrag");
		tsk_params_remove_param(oCnadidate->extension_att_list, "webrtc4ie-pwd");

		tsdp_header_M_add_headers((tsdp_header_M_t*)M, TSDP_HEADER_A_VA_ARGS("candidate", tnet_ice_candidate_tostring(oCnadidate)), tsk_null);
	}
	

bail:
	TSK_OBJECT_SAFE_FREE(oCnadidate);
	TSK_FREE(candidateStr);
	TSK_FREE(mediaStr);

	return hRet;
}
