#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーからほとんど使用されていない部分を除外する
// Windows ヘッダー ファイル
#include <windows.h>
// C ランタイム ヘッダー ファイル
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <assert.h>
#include <string>
#include <memory>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>	// for std::transform, std::max,std::min
#include <iomanip>		// for マニュピレーター

#ifdef max
	#undef max
	#undef min
#endif

#define _WINSOCKAPI_
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>

// アプリケーション内Windows メッセージ
#define WMA_COMMRECVED			(WM_APP + 1)	// 内部使用
#define WMA_REDRAW				(WM_APP + 2)	// 内部使用
#define WMA_GEMBUTTON			(WM_APP + 3)	// 内部使用


// new/delete
#ifdef CHECK_LEAK_MEMORY
#define NNEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define NNEW new
#endif
#define NDELETE(p) 		{if(p!=nullptr){delete (p);(p)=nullptr;}}
#define NDELETEARRAY(p) {if(p!=nullptr){delete[] (p);(p)=nullptr;}}

// string
#ifdef _UNICODE
typedef std::wstring tstring;
typedef std::wifstream tifstream;
typedef std::wostringstream tostringstream;
typedef std::wistringstream tistringstream;
#else
typedef std::string tstring;
typedef std::ifstream tifstream;
typedef std::ostringstream tostringstream;
typedef std::istringstream tistringstream;
#endif

#define FOREGROUND_WHITE		(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE)
