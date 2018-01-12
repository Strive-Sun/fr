
#include "global.h"
#include "common.h"
#include "resource.h"
#include "view.h"

#include "frame/frame.h"
#include "frame/frame_window.h"
#include "frame/messageloop.h"

namespace view{

	int MessageLoop()
	{
		return frame::messageloop::Loop();
	}

//
//  ����: RegisterClass()
//
//  Ŀ��: ע�ᴰ���ࡣ
//
//  ע��:
//
//    ����ϣ��
//    �˴�������ӵ� Windows 95 �еġ�RegisterClassEx��
//    ����֮ǰ�� Win32 ϵͳ����ʱ������Ҫ�˺��������÷������ô˺���ʮ����Ҫ��
//    ����Ӧ�ó���Ϳ��Ի�ù�����
//    ����ʽ��ȷ�ġ�Сͼ�ꡣ
//
ATOM RegisterClass(LPCWSTR lpszClassName, HICON hBigIcon, HICON hSmallIcon)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= global::hInstance;
	wcex.hIcon			= hBigIcon;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)GetStockObject(NULL_BRUSH);	//(HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= lpszClassName;
	wcex.hIconSm		= hSmallIcon;

	return RegisterClassEx(&wcex);
}

//
//  ����: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  Ŀ��: ���������ڵ���Ϣ��
//
//  WM_COMMAND	- ����Ӧ�ó���˵�
//  WM_PAINT	- ����������
//  WM_DESTROY	- �����˳���Ϣ������
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	frame::CWindow *pWindow = frame::GetWindow(hWnd);
	
	if (pWindow)
	{
		//����Ƿ����ڴ���
		bool handled = false;
		LRESULT result = pWindow->OnWndProc(message, wParam, lParam, handled);
		if (handled)
			return result;
	}

	switch (message)
	{
	case WM_PAINT:
		{
			PAINTSTRUCT ps = {0};
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	/*case WM_NCDESTROY:
		//�ǿͻ�������
		DefWindowProc(hWnd, message, wParam, lParam);

		break;*/
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

};