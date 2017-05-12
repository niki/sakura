// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  util.hpp
//! @brief ユーティリティ
//!
//! @author (C) koma.
//====================================================================
#ifndef MIX_UTIL_HPP
#define MIX_UTIL_HPP

#include <memory>
#include <string>
#include <tchar.h>

#if !defined(tstring)
#if defined(_UNICODE)
#define tstring wstring
#else
#define tstring string
#endif
#endif

namespace mix {

//==================================================================
// util
//==================================================================
namespace util {

//------------------------------------------------------------------
//! 文字列の左端から指定の文字を取り除く
//! @param s
//! @param c
//------------------------------------------------------------------
inline std::tstring ltrim(const std::tstring &s, const TCHAR c) {
  const TCHAR *p = s.c_str();
  while (*p == c) {
    p++;
  }
  return p;
}

//------------------------------------------------------------------
//! 文字列の右端から指定の文字を取り除く
//! @param s
//! @param c
//------------------------------------------------------------------
inline std::tstring rtrim(const std::tstring &s, const TCHAR c) {
  const TCHAR *p = s.c_str() + s.length() - 1;
  while (*p == c) {
    p--;
  }
  return s.substr(0, ((size_t)p - (size_t)s.c_str()) / sizeof(TCHAR) + 1);
}

} /* namespace of util */

//==================================================================
// file
//==================================================================
namespace file {

//------------------------------------------------------------------
//! 指定のファイルパスが存在するか
//! @param path パス名
//------------------------------------------------------------------
inline BOOL exist(const std::tstring &path) {
  return ::PathFileExists(path.c_str());
}

//------------------------------------------------------------------
//! ファイル名の取得 (e.g. C:\Windows\System32\calc.exe => calc.exe)
//! @param path パス名
//------------------------------------------------------------------
inline std::tstring fname(const std::tstring &path) {
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
//! ディレクトリ名の取得 (e.g. C:\Windows\System32\calc.exe => C:\Windows\System32\)
//! @param path パス名
//------------------------------------------------------------------
inline std::tstring dirname(const std::tstring &path) {
  size_t pos = path.rfind('\\');
  if (pos != std::tstring::npos) {
    return path.substr(0, pos + 1);
  } else {
    pos = path.rfind('/');
    if (pos != std::tstring::npos) {
      return path.substr(0, pos + 1);
    }
  }
  return _T("");
}

//------------------------------------------------------------------
//! ベース名の取得 (e.g. C:\Windows\System32\calc.exe => C:\Windows\System32\calc)
//! @param path パス名
//------------------------------------------------------------------
inline std::tstring basename(const std::tstring &path) {
  std::tstring s = fname(path);
  size_t pos = s.rfind('.');
  if (pos != std::tstring::npos) {
    return s.substr(0, pos);
  }
  return _T("");
}

//------------------------------------------------------------------
//! 拡張子名の取得 (e.g. C:\Windows\System32\calc.exe => .exe)
//! @param path パス名
//------------------------------------------------------------------
inline std::tstring extname(const std::tstring &path) {
  std::tstring s = fname(path);
  size_t pos = s.rfind('.');
  if (pos != std::tstring::npos) {
    return s.substr(pos);
  }
  return _T("");
}

}  // namespace of file

} /* namespace of mix */

#endif /* MIX_UTIL_HPP */
