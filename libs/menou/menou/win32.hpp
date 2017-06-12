// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  win32.hpp
//! @brief Windows
//!
//! @author (C) 2017, Reiris.
//====================================================================
#ifndef MENOU_WIN32_HPP
#define MENOU_WIN32_HPP

#include "basis.h"

namespace mn {

//==================================================================
// win32
//==================================================================
namespace win32 {

MENOU_INLINE void FillSolidRect(HDC hdc, COLORREF clr, RECT *pRect) {
  ::SetBkColor(hdc, clr);
  ::ExtTextOut(hdc, 0, 0, ETO_OPAQUE, pRect, NULL, 0, NULL);
}

//

} /* namespace of win32 */

} /* namespace of mn */

#endif /* MENOU_WIN32_HPP */
