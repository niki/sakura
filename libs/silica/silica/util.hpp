// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  util.hpp
//! @brief ユーティリティ
//!
//! @author (C) 2017, Niki.
//====================================================================
#ifndef SILICA_UTIL_HPP
#define SILICA_UTIL_HPP

#include "basis.h"

#include <cstdint>

namespace si {

namespace util {

/*!
 * 文字列を真偽値に変換
 * @param s
 * @return true/false
 */
SILICA_INLINE bool to_b(const std::string &s)
{
	return !(s == "false" || s == "False" || s == "0");
}
SILICA_INLINE bool to_b(const std::wstring &s)
{
	return !(s == L"false" || s == L"False" || s == L"0");
}

/*!
 * wstring(UTF-16) を string(UTF-8) に変換
 * std::wstring_convert/<codecvt> はC++17で非推奨(かつサロゲートペアを誤変換する)ため、自前実装で代替
 */
SILICA_INLINE std::string to_bytes(const std::wstring &s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		uint32_t cp = s[i];
		if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size()) {
			uint32_t lo = s[i + 1];
			if (lo >= 0xDC00 && lo <= 0xDFFF) {
				cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
				++i;
			}
		}
		if (cp <= 0x7F) {
			out += static_cast<char>(cp);
		}
		else if (cp <= 0x7FF) {
			out += static_cast<char>(0xC0 | (cp >> 6));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else if (cp <= 0xFFFF) {
			out += static_cast<char>(0xE0 | (cp >> 12));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else {
			out += static_cast<char>(0xF0 | (cp >> 18));
			out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
	}
	return out;
}

/*!
 * string(UTF-8) を wstring(UTF-16) に変換
 */
SILICA_INLINE std::wstring from_bytes(const std::string &s)
{
	std::wstring out;
	out.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		uint8_t c0 = static_cast<uint8_t>(s[i]);
		uint32_t cp;
		size_t len;
		if (c0 < 0x80) { cp = c0; len = 1; }
		else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; len = 2; }
		else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; len = 3; }
		else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; len = 4; }
		else { ++i; continue; } // 不正なバイト列はスキップ

		if (i + len > s.size()) break;
		for (size_t k = 1; k < len; ++k) {
			cp = (cp << 6) | (static_cast<uint8_t>(s[i + k]) & 0x3F);
		}
		i += len;

		if (cp <= 0xFFFF) {
			out += static_cast<wchar_t>(cp);
		}
		else {
			cp -= 0x10000;
			out += static_cast<wchar_t>(0xD800 + (cp >> 10));
			out += static_cast<wchar_t>(0xDC00 + (cp & 0x3FF));
		}
	}
	return out;
}

/*!
 * 文字列の左端から指定の文字を取り除く
 * @param s
 * @param c
 */
SILICA_INLINE std::tstring ltrim(const std::tstring &s, const TCHAR c)
{
	const TCHAR *p = s.c_str();
	while (*p == c) {
		p++;
	}
	return p;
}

/*!
 * 文字列の右端から指定の文字を取り除く
 * @param s
 * @param c
 */
SILICA_INLINE std::tstring rtrim(const std::tstring &s, const TCHAR c)
{
	const TCHAR *p = s.c_str() + s.length() - 1;
	while (*p == c) {
		p--;
	}
	return s.substr(0, ((size_t)p - (size_t)s.c_str()) / sizeof(TCHAR) + 1);
}

/*!
 * 文字列の置換
 */
SILICA_INLINE int replace(std::tstring &s, const std::tstring &from, const std::tstring &to)
{
	int repcnt = 0;
	std::tstring::size_type pos = s.find(from);
	while (pos != std::tstring::npos) {
		s.replace(pos, from.size(), to);
		repcnt++;
		pos = s.find(from, pos + to.size());
	}
	return repcnt;
}

} // namespace util

} // namespace si

#endif /* SILICA_UTIL_HPP */
