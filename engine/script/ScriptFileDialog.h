

#ifndef _TA_SCRIPT_FILE_DIALOG_
#define _TA_SCRIPT_FILE_DIALOG_

#pragma once

#include <string>
#include "ScriptBase.h"

#define OP_FS_DEF_FILTER	L"�����ļ�(*.*)|*.*||"

class CScriptFileDialog : public CScriptBase {
public:
	CScriptFileDialog();
	virtual ~CScriptFileDialog();

	bool Select(bool bIsOpen, LPCWSTR lpszFilter = OP_FS_DEF_FILTER, LPCWSTR lpszTitle = NULL);

	inline LPCWSTR GetPath()
	{
		return m_path_.c_str();
	}

protected:
	std::wstring m_path_;
};

#endif //_TA_SCRIPT_FILE_DIALOG_