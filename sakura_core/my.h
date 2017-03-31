// -*- mode:c++; coding:utf-8-ws -*-
#ifndef MY_H
#define MY_H

#include <SDKDDKVer.h>

//#include <windows.h>
// winnt.hで必要(普段はwindows.hが定義している)
#ifdef _M_IX86          // コンパイラが定義しているはず
#define _X86_
#else
#define _AMD64_
#endif
#include <windef.h>     // 基本的な定義類
#include <stdarg.h>     // winbase.hに必要
#include <winbase.h>    // 他の各種API向けヘッダに必要そう
#include <winnt.h>      // windef, winbaseで足りない分(実はwindefが内部でincludeしてる)

#include <shlwapi.h>

#include "my_config.h"
#include "my_reg.h"

#endif /* MY_CONFIG_H */
