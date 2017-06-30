// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  registry.hpp
//! @brief レジストリ
//!
//! @author (C) 2017, Uzuki.
//====================================================================
#ifndef SILICA_REGISTRY_HPP
#define SILICA_REGISTRY_HPP

#include "basis.h"
//#include <locale>
//#include <codecvt>
//#include <boost/container/vector.hpp>
#include <errno.h>
#include <vector>

//*****
// https://msdn.microsoft.com/ja-jp/library/cc429950.aspx
//  LONG RegOpenKeyEx(
//    HKEY hKey,         // 開いている親キーのハンドル
//    LPCTSTR lpSubKey,  // 開くべきサブキーの名前
//    DWORD ulOptions,   // 予約済み
//    REGSAM samDesired, // セキュリティアクセスマスク
//    PHKEY phkResult    // 開くことに成功したサブキーのハンドル
//  );
//*****
// https://msdn.microsoft.com/ja-jp/library/cc429931.aspx
//  LONG RegQueryValueEx(
//    HKEY hKey,            // キーのハンドル
//    LPCTSTR lpValueName,  // レジストリエントリ名
//    LPDWORD lpReserved,   // 予約済み(NULL)
//    LPDWORD lpType,       // データ型が格納されるバッファ
//    LPBYTE lpData,        // データが格納されるバッファ(NULLでサイズを知る)
//    LPDWORD lpcbData      // データバッファのサイズ
//  );
//*****
// https://msdn.microsoft.com/ja-jp/library/cc429904.aspx
//  LONG RegCreateKeyEx(
//    HKEY hKey,                                  // 開くべきキーのハンドル
//    LPCTSTR lpSubKey,                           // サブキーの名前
//    DWORD Reserved,                             // 予約済み
//    LPTSTR lpClass,                             // クラスの文字列
//    DWORD dwOptions,                            // 特別なオプション
//    REGSAM samDesired,                          // 希望のセキュリティアクセス権
//    LPSECURITY_ATTRIBUTES lpSecurityAttributes, // 継承の指定
//    PHKEY phkResult,                            // キーのハンドル
//    LPDWORD lpdwDisposition    // 既存かどうかを示す値が格納される変数
//  );
//*****
// https://msdn.microsoft.com/ja-jp/library/cc429936.aspx
//  LONG RegSetValueEx(
//    HKEY hKey,           // 親キーのハンドル
//    LPCTSTR lpValueName, // レジストリエントリ名
//    DWORD Reserved,      // 予約済み(NULL)
//    DWORD dwType,        // レジストリエントリのデータ型
//    CONST BYTE *lpData,  // レジストリエントリのデータ
//    DWORD cbData         // レジストリエントリのデータのサイズ
//  );

namespace si {

//==================================================================
// reg
//==================================================================
namespace reg {

//------------------------------------------------------------------
//! ini形式のキー情報からレジストリキー名作成
//------------------------------------------------------------------
SILICA_INLINE std::tstring genkey(const std::tstring &prof, const std::tstring &section = _T("")) {
	if (section.empty()) {
		return _T("Software\\") + si::file::fname(prof);
	} else {
		return _T("Software\\") + si::file::fname(prof) + _T("\\") + section;
	}
}

} // namespace of reg

} // namespace of si

//------------------------------------------------------------------
//! レジストリクラス
//------------------------------------------------------------------
class RegKey {
public:
	//RegKey() : hKey(0) {}
	explicit RegKey(const std::tstring &key_name, bool write_ok = false)
	    : hKey(0), dwDisposition_((DWORD)-1), key_name_(key_name) {
		if (write_ok) {
			DWORD dwDisposition;
			::RegCreateKeyEx(HKEY_CURRENT_USER, key_name.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
			                 KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
			dwDisposition_ = dwDisposition;
		} else {
			::RegOpenKeyEx(HKEY_CURRENT_USER, key_name.c_str(), 0, KEY_ALL_ACCESS, &hKey);
		}
	}
	virtual ~RegKey() {
		if (hKey != 0) {
			::RegCloseKey(hKey);
		}
	}

	operator HKEY &() { return hKey; }
	HKEY *as_ptr() { return &hKey; }
	bool valid() const { return hKey != 0; }

	//! エントリーの種類を取得
	bool getType(const std::tstring &entry, DWORD *pdwType, DWORD *pdwByte) const {
		if (!valid()) return false;

		DWORD &dwType = *pdwType;
		DWORD &dwByte = *pdwByte;
		return ::RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, NULL, &dwByte) == ERROR_SUCCESS;
	}

	//! DWORDの読み込み
	bool read(const std::tstring &entry, DWORD *data) const {
		if (!valid()) return false;

		DWORD dwType = REG_DWORD;
		DWORD dwByte = sizeof(DWORD);
		return ::RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, (BYTE *)data, &dwByte) ==
		       ERROR_SUCCESS;
	}

	//! DWORDの読み込み
	DWORD get(const std::tstring &entry, DWORD dwDefault) const {
		DWORD dwData;
		if (read(entry, &dwData)) {
			return dwData;
		} else {
			return dwDefault;
		}
	}

	//! 文字列の読み込み
	bool read(const std::tstring &entry, LPCTSTR data) const {
		if (!valid()) return false;

		DWORD dwType;
		DWORD dwByte;

		LONG rc = ::RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, NULL, &dwByte);
		if (rc != ERROR_SUCCESS) return false;
		if (!data) return (rc == ERROR_SUCCESS);

		rc = ::RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, (LPBYTE)data, &dwByte);
		if (rc != ERROR_SUCCESS) return false;
		((LPBYTE)data)[dwByte] = '\0';

		return (rc == ERROR_SUCCESS);
	}

	//! 文字列の読み込み
	LPCTSTR get_s(const std::tstring &entry, LPCTSTR pszDefault) const {
		static TCHAR tempBuff[512] = {};
		if (read(entry, tempBuff)) {
			return tempBuff;
		} else {
			return pszDefault;
		}
	}

	//! DWORDの書き込み
	bool write(const std::tstring &entry, DWORD data) {
		if (!valid()) return false;

		return ::RegSetValueEx(hKey, entry.c_str(), NULL, REG_DWORD, (CONST BYTE *)&data,
		                       sizeof(DWORD)) == ERROR_SUCCESS;
	}

	//! 文字列の書き込み
	bool write(const std::tstring &entry, LPCTSTR data, size_t size) {
		if (!valid()) return false;

		return ::RegSetValueEx(hKey, entry.c_str(), NULL, REG_SZ, (CONST BYTE *)data, (int)size) ==
		       ERROR_SUCCESS;
	}

	bool write(const std::tstring &entry, const std::tstring &data) {
		return write(entry, data.c_str(), (data.length() + 1) * sizeof(TCHAR));
	}

	//! キーの削除
	bool deleteKey(const std::tstring &key) {
		return ::RegDeleteKey(hKey, key.c_str()) == ERROR_SUCCESS;
	}

	//! エントリの削除
	bool deleteEntry(const std::tstring &entry) {
		return ::RegDeleteValue(hKey, entry.c_str()) == ERROR_SUCCESS;
	}

protected:
	HKEY hKey;
	DWORD dwDisposition_; // 新規作成: REG_CREATED_NEW_KEY, 既存: REG_OPENED_EXISTING_KEY
	const std::tstring &key_name_;
};

//! 書き込み可能なレジストリ
class RegKeyRW : public RegKey {
public:
	explicit RegKeyRW(const std::tstring &key_name) : RegKey(key_name, true) {}
};

//! スコープ内のみで有効なレジストリ (スコープから外れるとキーは削除されます)
class ScopedRegKey : public RegKey {
public:
	explicit ScopedRegKey(const std::tstring &key_name) : RegKey(key_name, true) {}
	~ScopedRegKey() {
		if (hKey != 0 && dwDisposition_ == REG_CREATED_NEW_KEY) { // 作成元のみ削除
			deleteKey(key_name_);
		}
	}
};

//------------------------------------------------------------------
//! レジストリキーのエントリ列挙
//------------------------------------------------------------------
SILICA_INLINE bool EnumRegKeyEntry(const std::tstring &key, std::vector<std::tstring> &e,
                                   std::vector<std::tstring> *v = nullptr) {
	RegKey hKey(key);

	TCHAR szValueName[256];
	DWORD dwValueNameSize;
	DWORD dwType;
	BYTE lpData[256];
	DWORD dwDataSize;
	DWORD i = 0;

	while (true) {
		dwValueNameSize = sizeof(szValueName) / sizeof(szValueName[0]);
		dwDataSize = sizeof(lpData) / sizeof(lpData[0]);

		// インデックス i に対するレジストリエントリを取得する
		LONG lRet = ::RegEnumValue(hKey, i++, szValueName, &dwValueNameSize, NULL, &dwType, lpData,
		                           &dwDataSize);

		if (lRet != ERROR_SUCCESS) {
			break;
		}

		if (v) {
			switch (dwType) {
			case REG_DWORD: {
				DWORD n;
				memcpy(&n, lpData, sizeof(n));
#if defined(_UNICODE)
				(*v).push_back(std::to_wstring(n));
#else
				(*v).push_back(std::to_string(n));
#endif
			} break;
			case REG_SZ:
				(*v).push_back((LPCTSTR)lpData);
				break;
			default:
				(*v).push_back(_T(""));
				break;
			}
		}

		e.push_back(szValueName);
	}

	return true;
}

//------------------------------------------------------------------
//! レジストリキーの読み取り(SZ)
//! @param prof プロファイル名
//! @param section セクション名
//! @param entry エントリー名
//! @param data データ
//------------------------------------------------------------------
SILICA_INLINE bool RegGetProfileString(const std::tstring &prof, const std::tstring &section,
                                       const std::tstring &entry, std::tstring &data) {
	DWORD dwType;
	DWORD dwByte;

	data = _T("");

	RegKey hKey(si::reg::genkey(prof, section));
	if (!hKey.getType(entry, &dwType, &dwByte)) return false;

	if (dwType == REG_DWORD) {
		int i;
		bool ret = hKey.read(entry, (DWORD *)&i);
		if (ret) {
#if defined(_UNICODE)
			data.assign(std::to_wstring(i));
#else
			data.assign(std::to_string(i));
#endif
		}

		return ret;
	} else if (dwByte > 0) {
		TCHAR *buffer = new TCHAR[dwByte + 1];
		buffer[0] = L'\0';

		bool ret = hKey.read(entry, (LPCTSTR)buffer);
		if (ret) {
			buffer[dwByte] = L'\0';
			data.assign(buffer);
		}

		delete[] buffer;
		return ret;
	} else {
		return true;
	}
}

//------------------------------------------------------------------
//! レジストリキーの書き込み(SZ)
//! @param prof プロファイル名
//! @param section セクション名
//! @param entry エントリー名
//! @param data データ
//------------------------------------------------------------------
SILICA_INLINE bool RegSetProfileString(const std::tstring &prof, const std::tstring &section,
                                       const std::tstring &entry, const std::tstring &data) {
	if (data.empty()) {
		//return RegKeyRW(si::reg::genkey(prof, section)).deleteEntry(entry);  // 空のときは削除
		return RegKeyRW(si::reg::genkey(prof, section)).write(entry, _T(""));
	} else {
		int i = 0;
		bool is_num = false;
		TCHAR *endptr;
		errno = 0;

		// 数値にできるか？
		i = _tcstol(data.c_str(), &endptr, 10);
		is_num = !(*endptr != L'\0' || (i == INT_MAX && errno == ERANGE));

		if (is_num) {
			return RegKeyRW(si::reg::genkey(prof, section)).write(entry, (DWORD)i);
		} else {
			return RegKeyRW(si::reg::genkey(prof, section)).write(entry, data);
		}
	}
}

#endif /* SILICA_REGISTRY_HPP */
