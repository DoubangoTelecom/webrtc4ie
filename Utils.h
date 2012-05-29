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

#include <comutil.h>

#define WM_NET_EVENT	(WM_USER + 101)
#define WM_ICE_EVENT_CANDIDATE	(WM_NET_EVENT + 1)
#define WM_ICE_EVENT_CONNECTED	(WM_NET_EVENT + 2)

class CUtils
{
public:
	static void Initialize(void);
	static BOOL StartDebug(void);
	static BOOL StopDebug(void);
	static _bstr_t ToBSTR(const char* notNullTerminated, UINT size);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
