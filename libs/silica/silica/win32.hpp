// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  win32.hpp
//! @brief Windows
//!
//! @author (C) 2017, Niki.
//====================================================================
#ifndef SILICA_WIN32_HPP
#define SILICA_WIN32_HPP

#include "basis.h"

namespace si {

namespace win32 {

SILICA_INLINE void FillSolidRect(HDC hdc, COLORREF clr, RECT *pRect)
{
	::SetBkColor(hdc, clr);
	::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, pRect, NULL, 0, NULL);
}

//

} // namespace win32

} // namespace si

#endif /* SILICA_WIN32_HPP */
