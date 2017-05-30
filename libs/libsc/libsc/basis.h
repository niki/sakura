// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  basis.h
//! @brief 基本
//!
//! @author (C) 2017, sayacat.
//====================================================================
#ifndef SC_BASIS_H
#define SC_BASIS_H

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

#define SC_INLINE static inline

#endif /* SC_BASIS_H */
