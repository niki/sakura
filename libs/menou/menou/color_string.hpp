// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  color_string.hpp
//! @brief 色文字列
//!
//! @author (C) 2017, sayacat.
//====================================================================
#ifndef MENOU_COLOR_STRING_HPP
#define MENOU_COLOR_STRING_HPP

#include "basis.h"

#define _RGB(r, g, b) (uint32_t)((((r)&0xff) << 16) | (((g & 0xff)) << 8) | (b & 0xff))
#define _BGR(r, g, b) (uint32_t)((((b)&0xff) << 16) | (((g & 0xff)) << 8) | (r & 0xff))

namespace mn {

class ColorString {
 public:
  ColorString() : color_code_(_T("")) {}
  explicit ColorString(const std::tstring &color_code) : color_code_(color_code) {}
  ColorString(int r, int g, int b, bool bName = false) : ColorString(ToColorCode(r, g, b, bName)) {}

  static std::tstring ToColorCode(int r, int g, int b, bool bName = false) {
    if (bName) {
      uint32_t rgb = _RGB(r, g, b);
      if (rgb == _RGB(255, 0, 0))     return _T("red");
      if (rgb == _RGB(255, 255, 0))   return _T("yellow");
      if (rgb == _RGB(0, 255, 0))     return _T("lime");
      if (rgb == _RGB(0, 255, 255))   return _T("aqua");
      if (rgb == _RGB(0, 0, 255))     return _T("blue");
      if (rgb == _RGB(255, 0, 255))   return _T("fuchsia");
      if (rgb == _RGB(128, 0, 0))     return _T("maroon");
      if (rgb == _RGB(128, 128, 0))   return _T("olive");
      if (rgb == _RGB(0, 128, 0))     return _T("green");
      if (rgb == _RGB(0, 128, 128))   return _T("teal");
      if (rgb == _RGB(0, 0, 128))     return _T("navy");
      if (rgb == _RGB(128, 0, 128))   return _T("purple");
      if (rgb == _RGB(0, 0, 0))       return _T("black");
      if (rgb == _RGB(128, 128, 128)) return _T("gray");
      if (rgb == _RGB(192, 192, 192)) return _T("silver");
      if (rgb == _RGB(255, 255, 255)) return _T("white");
      if (rgb == _RGB(255, 165, 0))   return _T("orange");
    }

    TCHAR buf[32];
    ::_stprintf_s(buf, sizeof(buf), _T("#%02x%02x%02x"), r, g, b);
    return buf;
  }

  enum eENDIAN {
    eENDIAN_RGB,
    eENDIAN_BGR
  };

  static uint32_t ToColor(const std::tstring &color_code, eENDIAN endian = eENDIAN_RGB) {
    if (color_code.empty()) return 0;

    TCHAR *pEnd;
    uint32_t rgb;

    // 0x00RRGGBB で管理

    if (*color_code.c_str() == _T('#')) {
      rgb = ::_tcstol(color_code.c_str() + 1, &pEnd, 16);  // カラーコード
    } else if (color_code == _T("red"))     { rgb = _RGB(255, 0, 0);
    } else if (color_code == _T("yellow"))  { rgb = _RGB(255, 255, 0);
    } else if (color_code == _T("lime"))    { rgb = _RGB(0, 255, 0);
    } else if (color_code == _T("aqua"))    { rgb = _RGB(0, 255, 255);
    } else if (color_code == _T("blue"))    { rgb = _RGB(0, 0, 255);
    } else if (color_code == _T("fuchsia")) { rgb = _RGB(255, 0, 255);
    } else if (color_code == _T("maroon"))  { rgb = _RGB(128, 0, 0);
    } else if (color_code == _T("olive"))   { rgb = _RGB(128, 128, 0);
    } else if (color_code == _T("green"))   { rgb = _RGB(0, 128, 0);
    } else if (color_code == _T("teal"))    { rgb = _RGB(0, 128, 128);
    } else if (color_code == _T("navy"))    { rgb = _RGB(0, 0, 128);
    } else if (color_code == _T("purple"))  { rgb = _RGB(128, 0, 128);
    } else if (color_code == _T("black"))   { rgb = _RGB(0, 0, 0);
    } else if (color_code == _T("gray"))    { rgb = _RGB(128, 128, 128);
    } else if (color_code == _T("silver"))  { rgb = _RGB(192, 192, 192);
    } else if (color_code == _T("white"))   { rgb = _RGB(255, 255, 255);
    } else if (color_code == _T("orange"))  { rgb = _RGB(255, 165, 0);
    } else {
      rgb = ::_tcstol(color_code.c_str(), &pEnd, 16);  // 16進数
      if (endian == eENDIAN_BGR ) {
        int r = rgb & 0xff;
        int g = (rgb >> 8) & 0xff;
        int b = (rgb >> 16) & 0xff;
        rgb = _RGB(r, g, b);
      }
    }
    
    return rgb;
  }

  static uint32_t ToCOLORREF(const std::tstring &color_code, eENDIAN endian = eENDIAN_RGB) {
    uint32_t rgb = ToColor(color_code, endian);
    
    // COLORREF形式にする
    int r = (rgb >> 16) & 0xff;
    int g = (rgb >> 8) & 0xff;
    int b = rgb & 0xff;
    return _BGR(r, g, b);
  }

  static std::tstring FromCOLORREF(uint32_t colorref, bool bName = false) {
    return ToColorCode(colorref & 0xff, (colorref >> 8) & 0xff, (colorref >> 16) & 0xff, bName);
  }

 private:
  std::tstring color_code_;
};

} /* namespace of mn */

#endif /* MENOU_COLOR_STRING_HPP */
