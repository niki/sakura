// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  file.hpp
//! @brief ファイル
//!
//! @author (C) 2017, takamo.
//====================================================================
#ifndef MENOU_FILE_HPP
#define MENOU_FILE_HPP

#include "basis.h"

namespace mn {

//==================================================================
// file
//==================================================================
namespace file {

//------------------------------------------------------------------
//! 指定のファイルパスが存在するか
//! @param path パス名
//------------------------------------------------------------------
MENOU_INLINE BOOL exist(const std::tstring &path) {
  return ::PathFileExists(path.c_str());
}

//------------------------------------------------------------------
//! ファイル名の取得 (e.g. C:\Windows\System32\calc.exe => calc.exe)
//! @param path パス名
//------------------------------------------------------------------
MENOU_INLINE std::tstring fname(const std::tstring &path) {
  size_t pos = path.rfind(_T('\\'));
  if (pos != std::tstring::npos) {
    return path.substr(pos + 1, path.size() - pos - 1);
  } else {
    pos = path.rfind(_T('/'));
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
MENOU_INLINE std::tstring dirname(const std::tstring &path, bool lastDelimiter = true) {
  size_t pos = path.rfind(_T('\\'));
  if (pos != std::tstring::npos) {
    if (lastDelimiter) pos++;
    return path.substr(0, pos);
  } else {
    pos = path.rfind(_T('/'));
    if (pos != std::tstring::npos) {
      if (lastDelimiter) pos++;
      return path.substr(0, pos);
    }
  }
  return _T("");
}

//------------------------------------------------------------------
//! ベース名の取得 (e.g. C:\Windows\System32\calc.exe => C:\Windows\System32\calc)
//! @param path パス名
//------------------------------------------------------------------
MENOU_INLINE std::tstring basename(const std::tstring &path) {
  std::tstring s = fname(path);
  size_t pos = s.rfind(_T('.'));
  if (pos != std::tstring::npos) {
    return s.substr(0, pos);
  }
  return _T("");
}

//------------------------------------------------------------------
//! 拡張子名の取得 (e.g. C:\Windows\System32\calc.exe => .exe)
//! @param path パス名
//------------------------------------------------------------------
MENOU_INLINE std::tstring extname(const std::tstring &path) {
  std::tstring s = fname(path);
  size_t pos = s.rfind(_T('.'));
  if (pos != std::tstring::npos) {
    return s.substr(pos);
  }
  return _T("");
}

}  // namespace of file

} /* namespace of mn */

#endif /* MENOU_FILE_HPP */
