// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  my_reg.h
//! @brief レジストリ
//!
//! @author (C) Rabbiteariris.
//====================================================================
#ifndef MY_REG_H
#define MY_REG_H

#include <memory>
#include <string>
//#include <locale>
//#include <codecvt>
//#include <vector>
#include <tchar.h>
#include <errno.h>

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

#if !defined(tstring)
#if defined(_UNICODE)
#define tstring wstring
#else
#define tstring string
#endif
#endif

namespace ut {

//------------------------------------------------------------------
//! ファイル名の取得
//! @param path パス名
//------------------------------------------------------------------
inline std::tstring fname(const std::tstring &path) {
  std::tstring fname;
  size_t pos = path.rfind('\\');
  if (pos != std::tstring::npos) {
    return path.substr(pos + 1, path.size() - pos - 1);
  } else {
    pos = path.rfind('/');
    if (pos != std::tstring::npos) {
      return path.substr(pos + 1, path.size() - pos - 1);
    }
  }
  return path;
}

//------------------------------------------------------------------
//! レジストリキー名作成
//------------------------------------------------------------------
inline std::tstring regkey(const std::tstring &prof, const std::tstring &section = _T("")) {
  if (section.empty()) {
    return _T("Software\\") + ut::fname(prof);
  } else {
    return _T("Software\\") + ut::fname(prof) + _T("\\") + section;
  }
}

}  // namespace of ut

//------------------------------------------------------------------
//! レジストリクラス
//------------------------------------------------------------------
class RegKey {
 public:
  RegKey() : hKey(0) {}
  explicit RegKey(const std::tstring &key_name, bool write_mode = false) : hKey(0) {
    if (write_mode) {
      DWORD dwDisposition;  // 新規作成:REG_CREATED_NEW_KEY
                            // 既存:REG_OPENED_EXISTING_KEY
      RegCreateKeyEx(HKEY_CURRENT_USER, key_name.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE,
                     KEY_ALL_ACCESS, NULL, &hKey, &dwDisposition);
    } else {
      RegOpenKeyEx(HKEY_CURRENT_USER, key_name.c_str(), 0, KEY_ALL_ACCESS, &hKey);
    }
  }
  ~RegKey() {
    if (hKey != 0) RegCloseKey(hKey);
  }

  operator HKEY &() { return hKey; }
  HKEY *as_ptr() { return &hKey; }
  bool valid() const { return hKey != 0; }

  //! エントリーの種類を取得
  bool getType(const std::tstring &entry, DWORD *pdwType, DWORD *pdwByte) const {
    if (!valid()) return false;

    DWORD &dwType = *pdwType;
    DWORD &dwByte = *pdwByte;
    return RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, NULL, &dwByte) == ERROR_SUCCESS;
  }

  //! DWORDの読み込み
  bool read(const std::tstring &entry, DWORD *data) const {
    if (!valid()) return false;

    DWORD dwType = REG_DWORD;
    DWORD dwByte = sizeof(DWORD);
    return RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, (BYTE *)data, &dwByte) ==
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

    LONG rc = RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, NULL, &dwByte);
    if (rc != ERROR_SUCCESS) return false;
    if (!data) return (rc == ERROR_SUCCESS);

    rc = RegQueryValueEx(hKey, entry.c_str(), NULL, &dwType, (LPBYTE)data, &dwByte);
    ((LPBYTE)data)[dwByte] = '\0';
    return (rc == ERROR_SUCCESS);
  }

  //! DWORDの書き込み
  bool write(const std::tstring &entry, DWORD data) {
    if (!valid()) return false;

    return RegSetValueEx(hKey, entry.c_str(), NULL, REG_DWORD, (CONST BYTE *)&data,
                         sizeof(DWORD)) == ERROR_SUCCESS;
  }

  //! 文字列の書き込み
  bool write(const std::tstring &entry, LPCTSTR data, size_t size) {
    if (!valid()) return false;

    return RegSetValueEx(hKey, entry.c_str(), NULL, REG_SZ, (CONST BYTE *)data, (int)size) ==
           ERROR_SUCCESS;
  }

  bool write(const std::tstring &entry, const std::tstring &data) {
    return write(entry, data.c_str(), (data.length() + 1) * sizeof(TCHAR));
  }

 private:
  HKEY hKey;
};

//------------------------------------------------------------------
//! レジストリキーの読み取り(SZ)
//! @param prof プロファイル名
//! @param section セクション名
//! @param entry エントリー名
//! @param data データ
//------------------------------------------------------------------
inline bool RegGetProfileString(const std::tstring &prof, const std::tstring &section,
                                const std::tstring &entry, std::tstring &data) {
  DWORD dwType;
  DWORD dwByte;

  data = _T("");

  RegKey hKey(ut::regkey(prof, section));
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
inline bool RegSetProfileString(const std::tstring &prof, const std::tstring &section,
                                const std::tstring &entry, const std::tstring &data) {
  if (data.empty()) {
    return RegKey(ut::regkey(prof, section), true).write(entry, _T(""));
  } else {
    int i = 0;
    bool is_num = false;
    TCHAR *endptr;
    errno = 0;

    // 数値にできるか？
    i = _tcstol(data.c_str(), &endptr, 10);
    is_num = !(*endptr != L'\0' || (i == INT_MAX && errno == ERANGE));

    if (is_num) {
      return RegKey(ut::regkey(prof, section), true).write(entry, (DWORD)i);
    } else {
      return RegKey(ut::regkey(prof, section), true).write(entry, data);
    }
  }
}

#endif /* MY_REG_H */
