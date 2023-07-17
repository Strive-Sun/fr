
#ifndef _SNOW_CUTE_COMMON_H_
#define _SNOW_CUTE_COMMON_H_ 1

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             //  �� Windows ͷ�ļ����ų�����ʹ�õ���Ϣ
// Windows ͷ�ļ�:
#include <windows.h>

//���Ԥ���� CreateDialogW
#ifdef CreateDialog
#undef CreateDialog
#endif

#ifdef RegisterClass
#undef RegisterClass
#endif

// C ����ʱͷ�ļ�
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <Commctrl.h>
#pragma comment(lib, "Comctl32.lib") 

//tcmalloc
#include <base/tcmalloc.h>

//std
#include <string>
#include <iostream>

//gdiplus
#ifndef _NO_GDIPLUS_
#include <atlimage.h>
using namespace Gdiplus;
#pragma comment(lib, "Gdiplus.lib") 
#endif

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif


#endif
