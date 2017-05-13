// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  basis.h
//! @brief 基本
//!
//! @author (C) koma.
//====================================================================
#ifndef MIX_BASIS_H
#define MIX_BASIS_H

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

#define MIX_INLINE static inline

#endif /* MIX_BASIS_H */
