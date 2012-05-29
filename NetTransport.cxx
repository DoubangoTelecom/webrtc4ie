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
#include "NetTransport.h"
#include "Utils.h"

#include "tinynet.h"

#include <comutil.h>
#include <stdio.h>

#define THIS_VERSION	"1.0.0"

STDMETHODIMP CNetTransport::InterfaceSupportsErrorInfo(REFIID riid)
{
	static const IID* arr[] = 
	{
		&IID_INetTransport
	};

	for (int i=0; i < sizeof(arr) / sizeof(arr[0]); i++)
	{
		if (InlineIsEqualGUID(*arr[i],riid))
			return S_OK;
	}
	return S_FALSE;
}

CNetTransport::CNetTransport()
:mTransport(NULL),
mLooperHandle(NULL),
mLooperProc(NULL),
mDefaultDestAddr(NULL),
mDefaultDestPort(NULL)
{
	TSK_DEBUG_INFO("CNetTransport::CNetTransport");
	CUtils::Initialize();
}

CNetTransport::~CNetTransport()
{
	Stop();
	TSK_OBJECT_SAFE_FREE(mTransport);

	if(mLooperHandle && mLooperProc){
		SetWindowLongPtr(mLooperHandle, GWL_WNDPROC, (LONG)mLooperProc);
	}
	TSK_FREE(mDefaultDestAddr);
}

STDMETHODIMP CNetTransport::SetDomain(BSTR domain)
{
	if(!domain){
		TSK_DEBUG_ERROR("Invalid parameter");
		return E_INVALIDARG;
	}

	tnet_dns_ctx_t* dnsCtx = tnet_dns_ctx_create();
	if(!dnsCtx){
		TSK_DEBUG_ERROR("Failed to create SND context");
		return E_FAIL;
	}

	tnet_port_t port;
	char* domainStr = _com_util::ConvertBSTRToString(domain);
	TSK_FREE(mDefaultDestAddr);
	
	if(tnet_dns_query_naptr_srv(dnsCtx, domainStr, "SIP+D2U", &mDefaultDestAddr, &port) == 0){
		mDefaultDestPort = port;
	}

	TSK_FREE(domainStr);
	TSK_OBJECT_SAFE_FREE(dnsCtx);

	return S_OK;
}

STDMETHODIMP CNetTransport::Start(LONGLONG looper)
{
	TSK_DEBUG_INFO("CNetTransport::Start");

	if(mTransport && TSK_RUNNABLE(mTransport)->started){
		TSK_DEBUG_ERROR("Already started");
		return E_FAIL;
	}

	int iRet;
	tnet_ip_t bestsource;
	if((iRet = tnet_getbestsource("sipml5.org", 5060, tnet_socket_type_udp_ipv4, &bestsource))){
		TSK_DEBUG_ERROR("Failed to get best source [%d]", iRet);
		memcpy(bestsource, "0.0.0.0", 7);
	}

	if(!mTransport){
		mTransport = tnet_transport_create(bestsource, TNET_SOCKET_PORT_ANY, tnet_socket_type_udp_ipv4, "NetTransport");
		if(!mTransport){
			TSK_DEBUG_ERROR("Failed to create network transport");
			return E_FAIL;
		}
		tnet_transport_set_callback(mTransport, CNetTransport::DgramCb, this);
	}

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

	if((iRet = tnet_transport_start(mTransport)) != 0){
		TSK_DEBUG_ERROR("Failed to start transport [%d]", iRet);
		return E_FAIL;
	}

	return S_OK;
}

STDMETHODIMP CNetTransport::SendTo(BSTR msg, BSTR addr, USHORT port)
{
	if(!msg || !addr || !port){
		TSK_DEBUG_ERROR("Invalid parameter");
		return E_INVALIDARG;
	}

	if(!mTransport || !TSK_RUNNABLE(mTransport)->started){
		TSK_DEBUG_ERROR("Not started");
		return E_FAIL;
	}

	HRESULT hRet;

	char* addrStr = _com_util::ConvertBSTRToString(addr);
	struct sockaddr_storage addrO;
	if(tnet_sockaddr_init(addrStr, port, mTransport->master->type, &addrO)){
		TNET_PRINT_LAST_ERROR("tnet_sockaddr_init() failed");
		hRet = E_FAIL;
		goto bail;
	}

	char* msgStr = _com_util::ConvertBSTRToString(msg);
	hRet = tnet_transport_sendto(mTransport, mTransport->master->fd, (const struct sockaddr *)&addrO, msgStr, tsk_strlen(msgStr)) > 0 ? S_OK : E_FAIL;

bail:
	TSK_FREE(addrStr);
	TSK_FREE(msgStr);
	return hRet;
}

STDMETHODIMP CNetTransport::get_localIP(BSTR* pVal)
{
	if(mTransport && mTransport->master){
		_bstr_t bstr(mTransport->master->ip);
		*pVal = bstr.GetBSTR();
	}
	else *pVal = NULL;
	return S_OK;
}

STDMETHODIMP CNetTransport::get_localPort(USHORT* pVal)
{
	if(mTransport && mTransport->master) *pVal =  mTransport->master->port;
	else *pVal = 0;
	return S_OK;
}

STDMETHODIMP CNetTransport::get_defaultDestAddr(BSTR* pVal)
{
	_bstr_t bstr(mDefaultDestAddr);
	*pVal = bstr.GetBSTR();
	return S_OK;
}

STDMETHODIMP CNetTransport::get_defaultDestPort(USHORT* pVal)
{
	*pVal =  mDefaultDestPort;
	return S_OK;
}

STDMETHODIMP CNetTransport::get_version(BSTR* pVal)
{
	*pVal = _bstr_t(THIS_VERSION);
	return S_OK;
}

STDMETHODIMP CNetTransport::Stop(void)
{
	if(mTransport && TSK_RUNNABLE(mTransport)->started){
		int iRet;
		if((iRet = tnet_transport_shutdown(mTransport)) != 0){
			TSK_DEBUG_ERROR("Failed to stop transport [%d]", iRet);
			return E_FAIL;
		}
	}

	return S_OK;
}

STDMETHODIMP CNetTransport::StartDebug()
{
	CUtils::StartDebug();
	return S_OK;
}

STDMETHODIMP CNetTransport::StopDebug()
{
	CUtils::StopDebug();
	return S_OK;
}

int CNetTransport::DgramCb(const struct tnet_transport_event_s* e)
{
	CNetTransport* This = (CNetTransport*)e->callback_data;

	TSK_DEBUG_INFO("CNetTransport::DgramCb");

	switch(e->type){
		case event_data: {
				break;
			}
		case event_closed:
		case event_connected:
		default:{
				return 0;
			}
	}

	USHORT eType = e->type;
	_bstr_t oData = CUtils::ToBSTR((const char*)e->data, e->size);
	if(This->mLooperHandle && This->mLooperProc){
		CNetTransportEvent* oEvent = new CNetTransportEvent(eType, oData.copy());
		if(!PostMessageA(This->mLooperHandle, WM_NET_EVENT, reinterpret_cast<WPARAM>(This), reinterpret_cast<LPARAM>(oEvent))){
			TSK_DEBUG_ERROR("PostMessageA() failed");
			This->Fire_OnEvent(eType, oData);
			if(oEvent) delete oEvent, oEvent = NULL;
		}
	}
	else{
		This->Fire_OnEvent(eType, oData.GetBSTR());
	}

	return 0;
}