// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  basis.h
//! @brief 基本
//!
//! @author (C) 2017, Calette.
//====================================================================
#ifndef MENOU_BASIS_H
#define MENOU_BASIS_H

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

#define MENOU_INLINE static inline

#endif /* MENOU_BASIS_H */
