
#ifndef _SNOW_CUTE_FRAME_MENU_H_
#define _SNOW_CUTE_FRAME_MENU_H_ 1

#include "../common.h"

namespace view{
	namespace frame{

		class CMenu{
		public:
			CMenu();
			virtual ~CMenu();

			bool Create(bool autodelete_ = true);
			bool CreatePopup(bool autodelete_ = true);

			bool Append(unsigned short id, LPCWSTR text, UINT flags = MF_STRING, CMenu *pMenu = NULL);
			bool Insert(UINT position, unsigned short id, LPCWSTR text, UINT flags = MF_STRING | MF_BYPOSITION, CMenu *pMenu = NULL);
			bool Modify(UINT position, unsigned short id, LPCWSTR text, UINT flags = MF_STRING | MF_BYPOSITION, CMenu *pMenu = NULL);
			bool Delete(UINT position, UINT flags = MF_BYPOSITION);

			bool Remove();

			bool EnableItem(UINT id, bool enable_ = true);
			bool SetDefaultItem(UINT item, bool byposition = true);

			void Show(HWND hWnd, UINT flags = TPM_LEFTALIGN | TPM_RIGHTBUTTON, POINT *pt = NULL);

		protected:
			HMENU m_hMenu;
			bool m_autodelete;
		};

	};
};

#endif