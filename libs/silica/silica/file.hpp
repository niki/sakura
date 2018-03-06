// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  file.hpp
//! @brief ファイル
//!
//! @author (C) 2017, Niki.
//====================================================================
#ifndef SILICA_FILE_HPP
#define SILICA_FILE_HPP

#include "basis.h"

namespace si {

namespace file {

/*!
 * 指定のファイルパスが存在するか
 * @param path パス名
 */
SILICA_INLINE BOOL exist(const std::tstring &path)
{
	return ::PathFileExists(path.c_str());
}

/*!
 * ファイル名の取得 (e.g. C:\Windows\System32\calc.exe => calc.exe)
 * @param path パス名
 */
SILICA_INLINE std::tstring fname(const std::tstring &path)
{
	size_t pos = path.rfind(_T('\\'));
	if (pos != std::tstring::npos) {
		return path.substr(pos + 1, path.size() - pos - 1);
	}
	else {
		pos = path.rfind(_T('/'));
		if (pos != std::tstring::npos) {
			return path.substr(pos + 1, path.size() - pos - 1);
		}
	}
	return path;
}

/*!
 * ディレクトリ名の取得 (e.g. C:\Windows\System32\calc.exe => C:\Windows\System32\)
 * @param path パス名
 */
SILICA_INLINE std::tstring dirname(const std::tstring &path, bool lastDelimiter = true)
{
	size_t pos = path.rfind(_T('\\'));
	if (pos != std::tstring::npos) {
		if (lastDelimiter) pos++;
		return path.substr(0, pos);
	}
	else {
		pos = path.rfind(_T('/'));
		if (pos != std::tstring::npos) {
			if (lastDelimiter) pos++;
			return path.substr(0, pos);
		}
	}
	return _T("");
}

/*!
 * ベース名の取得 (e.g. C:\Windows\System32\calc.exe => C:\Windows\System32\calc)
 * @param path パス名
 */
SILICA_INLINE std::tstring basename(const std::tstring &path)
{
	std::tstring s = fname(path);
	size_t pos = s.rfind(_T('.'));
	if (pos != std::tstring::npos) {
		return s.substr(0, pos);
	}
	return _T("");
}

/*!
 * 拡張子名の取得 (e.g. C:\Windows\System32\calc.exe => .exe)
 * @param path パス名
 */
SILICA_INLINE std::tstring extname(const std::tstring &path)
{
	std::tstring s = fname(path);
	size_t pos = s.rfind(_T('.'));
	if (pos != std::tstring::npos) {
		return s.substr(pos);
	}
	return _T("");
}

/*!
 * ファイルパスの縮小
 * @param dest 出力バッファ
 * @param path パス
 * @param cc_len 縮小する最大長
 * @param sepa セパレータ
 */
SILICA_INLINE bool PathCompactPath(TCHAR *dest, const std::tstring &path, int cc_len, TCHAR sepa)
{
	std::tstring dir = dirname(path, false);
	std::tstring file = fname(path);
	
	si::text::SplitString split(dir, sepa);
	
	if (split.Size() < 2) {
		_tcscpy_s(dest, path.size() * sizeof(TCHAR), path.c_str());
		return false;
	}
	
	bool compact = false;
	
	while (1) {
		int l = file.size();
		
		// 長さを計算
		for (int i = 0; i < split.Size(); i++) {
			l += split.At(i).size() + 1; /*セパレータ分*/;
		}
		
		// 縮小サイズをオーバーしている
		if (l > cc_len) {
			int remove = split.Size() / 2;
			split.IgnoreToken(remove);
			compact = true;
		}
		else {
			break;
		}
	}
	
	int center = -1;
	
	if (compact) {
		center = split.Size() / 2;
	}
	
	std::tstring result;
	
	for (int i = 0; i < split.Size(); i++) {
		if (i == center) {
			result += _T("...");
			result += sepa;
		}
		else {
			result += split.At(i);
			result += sepa;
		}
	}
	
	result += file;
	
	_tcscpy_s(dest, _MAX_PATH, result.c_str());
	
	return true;
}

/*!
 * ファイルパスの縮小
 * @param dest 出力バッファ
 * @param path パス
 * @param cc_len 縮小する最大長
 * @param sepa セパレータ
 */
SILICA_INLINE bool PathCompactPath2(TCHAR *dest, const std::tstring &path, int cc_len, TCHAR sepa)
{
	std::tstring dir = dirname(path, false);
	std::tstring file = fname(path);
	
	si::text::SplitString split(dir, sepa);
	
	if (split.Size() < 2) {
		_tcscpy_s(dest, path.size() * sizeof(TCHAR), path.c_str());
		return false;
	}
	
	int compact_type = 0;
	
	while (1) {
		int l = file.size();
		
		// 長さを計算
		for (int i = 0; i < split.Size(); i++) {
			l += split.At(i).size() + 1; /*セパレータ分*/;
		}
		
		// 縮小サイズをオーバーしている
		if (l > cc_len) {
			int remove = split.Size() / 2;
			
			// 差分を求める
			int diff = l - cc_len;
			if (split.At(remove).size() > diff) {
				split.At(remove).resize(split.At(remove).size() - diff);
				si::logln(L"size=%d, diff=%d, %s", split.At(remove).size(), diff, split.At(remove).c_str());
				compact_type = 2;
				break;
			}
			else {
				split.IgnoreToken(remove);
				compact_type = 1;
			}
		}
		else {
			break;
		}
	}
	
	int center = -1;
	
	if (compact_type != 0) {
		center = split.Size() / 2;
	}
	
	std::tstring result;
	
	for (int i = 0; i < split.Size(); i++) {
		if (i == center) {
			if (compact_type == 2) {
				result += split.At(i);
			}
			result += _T("...");
			result += sepa;
		}
		else {
			result += split.At(i);
			result += sepa;
		}
	}
	
	result += file;
	
	_tcscpy_s(dest, _MAX_PATH, result.c_str());
	
	return true;
}

} // namespace file

} // namespace si

#endif /* SILICA_FILE_HPP */
