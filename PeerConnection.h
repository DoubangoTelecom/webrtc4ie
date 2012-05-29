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

#pragma once
#include "resource.h"       // main symbols

#include "webrtc4ie_i.h"
#include "_IPeerConnectionEvents_CP.h"


#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif

// CPeerConnectionEvent

class CPeerConnectionEvent
{
public:
	CPeerConnectionEvent(BSTR media, BSTR candidate, BOOL moreToFollow):
		mMedia(media),
		mCandidate(candidate),
		mMoreToFollow(moreToFollow)
		{ }

	~CPeerConnectionEvent()
	{
		if(mMedia) ::SysFreeString(mMedia);
		if(mCandidate) ::SysFreeString(mCandidate);
	}

	BSTR GetMedia(){ return mMedia; }
	BSTR GetCandidate(){ return mCandidate; }
	BOOL GetMoreToFollow(){ return mMoreToFollow; }

private:
	BSTR mMedia;
	BSTR mCandidate;
	BOOL mMoreToFollow;
};


// CPeerConnection

class ATL_NO_VTABLE CPeerConnection :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CPeerConnection, &CLSID_PeerConnection>,
	public ISupportErrorInfo,
	public IConnectionPointContainerImpl<CPeerConnection>,
	public CProxy_IPeerConnectionEvents<CPeerConnection>,
	public IObjectWithSiteImpl<CPeerConnection>,
	public IProvideClassInfo2Impl<&CLSID_PeerConnection, &__uuidof(_IPeerConnectionEvents), &LIBID_webrtc4ieLib>,
	public IDispatchImpl<IPeerConnection, &IID_IPeerConnection, &LIBID_webrtc4ieLib, /*wMajor =*/ 1, /*wMinor =*/ 0>,
	public IObjectSafetyImpl<CPeerConnection, INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA>
{
public:
	CPeerConnection();
	virtual ~CPeerConnection();

DECLARE_REGISTRY_RESOURCEID(IDR_PEERCONNECTION)


BEGIN_COM_MAP(CPeerConnection)
	COM_INTERFACE_ENTRY(IPeerConnection)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(ISupportErrorInfo)
	COM_INTERFACE_ENTRY(IConnectionPointContainer)
	COM_INTERFACE_ENTRY(IObjectWithSite)
	COM_INTERFACE_ENTRY(IProvideClassInfo)
    COM_INTERFACE_ENTRY(IProvideClassInfo2)
	COM_INTERFACE_ENTRY(IObjectSafety)
END_COM_MAP()

BEGIN_CONNECTION_POINT_MAP(CPeerConnection)
	CONNECTION_POINT_ENTRY(__uuidof(_IPeerConnectionEvents))
END_CONNECTION_POINT_MAP()
// ISupportsErrorInfo
	STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid);


	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

public:
	STDMETHOD(close)(void);
	STDMETHOD(createOffer)(VARIANT_BOOL has_audio, VARIANT_BOOL has_video, BSTR* sdp);
	STDMETHOD(createAnswer)(VARIANT_BOOL has_audio, VARIANT_BOOL has_video, BSTR* sdp);
	STDMETHOD(startIce)(SHORT IceOptions, LONGLONG looper);
	STDMETHOD(setLocalDescription)(USHORT action, BSTR desc);
	STDMETHOD(setRemoteDescription)(USHORT action, BSTR desc);
	STDMETHOD(get_localDescription)(BSTR* pVal);
	STDMETHOD(get_remoteDescription)(BSTR* pVal);
	STDMETHOD(get_readyState)(USHORT* pVal);
	STDMETHOD(get_iceState)(USHORT* pVal);
	STDMETHOD(get_remoteVideo)(LONGLONG* pVal);
	STDMETHOD(put_remoteVideo)(LONGLONG newVal);
	STDMETHOD(get_localVideo)(LONGLONG* pVal);
	STDMETHOD(put_localVideo)(LONGLONG newVal);
	STDMETHOD(get_version)(BSTR* pVal);
	STDMETHOD(get_fullScreen)(VARIANT_BOOL* pVal);
	STDMETHOD(put_fullScreen)(VARIANT_BOOL newVal);
	
	STDMETHOD(Init)(BSTR configuration);
	STDMETHOD(StartDebug)(void);
	STDMETHOD(StopDebug)(void);
	
	void StartMedia();

private:
	HRESULT CreateSessionMgr(enum tmedia_type_e eMediaType, BOOL offerer);
	HRESULT CreateLo(VARIANT_BOOL has_audio, VARIANT_BOOL has_video, BSTR* sdp, BOOL offerer);
	HRESULT SerializeSdp(struct tsdp_message_s* sdp, BSTR* pVal);
	HRESULT IceCreateCtx(enum tmedia_type_e eMediaType);
	HRESULT IceSetTimeout(INT timeout);
	BOOL IceGotLocalCandidates();
	HRESULT IceProcessRo(const struct tsdp_message_s* sdp_ro, BOOL isOffer);
	BOOL IceIsConnected();
	static int IceCallback(const struct tnet_ice_event_s *e);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	enum tmedia_type_e mMediaType;
	struct tmedia_session_mgr_s* mSessionMgr;

	struct tsdp_message_s* mSdpLocal;
	struct tsdp_message_s* mSdpRemote;
	
	struct tnet_ice_ctx_s *mIceCtxAudio;
	struct tnet_ice_ctx_s *mIceCtxVideo;

	enum ReadyState_e mReadyState;
	enum IceState_e mIceState;

	LONGLONG mRemoteVideo;
	LONGLONG mLocalVideo;

	HWND mLooperHandle;
	WNDPROC mLooperProc;

	VARIANT_BOOL mFullScreen;

	CRITICAL_SECTION mCSIceCallback;
};

OBJECT_ENTRY_AUTO(__uuidof(PeerConnection), CPeerConnection)
