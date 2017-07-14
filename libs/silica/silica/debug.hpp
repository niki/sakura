// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  debug.hpp
//! @brief デバッグ用
//!
//! @author (C) 2017, Uzuki.
//====================================================================
#ifndef SILICA_DEBUG_HPP
#define SILICA_DEBUG_HPP

#include "log.hpp"

#define DebugOutputCaller(prefix) si::logln(L"%s: %s (%d)", _T(prefix), _T(__FUNCTION__), __LINE__)

#endif /* SILICA_DEBUG_HPP */
