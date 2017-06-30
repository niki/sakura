// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  win32.hpp
//! @brief Windows
//!
//! @author (C) 2017, Uzuki.
//====================================================================
#ifndef SILICA_WIN32_HPP
#define SILICA_WIN32_HPP

#include "basis.h"

namespace si {

//==================================================================
// win32
//==================================================================
namespace win32 {

SILICA_INLINE void FillSolidRect(HDC hdc, COLORREF clr, RECT *pRect) {
	::SetBkColor(hdc, clr);
	::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, pRect, NULL, 0, NULL);
}

//

} /* namespace of win32 */

} /* namespace of si */

#endif /* SILICA_WIN32_HPP */
