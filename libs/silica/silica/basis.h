// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  basis.h
//! @brief 基本
//!
//! @author (C) 2017, Uzuki.
//====================================================================
#ifndef SILICA_BASIS_H
#define SILICA_BASIS_H

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

#define SILICA_INLINE static inline

#endif /* SILICA_BASIS_H */
