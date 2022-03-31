// -*- mode:c++; coding:utf-8-ws -*-
/*
Copyright (c) 1998-2018 by Norio Nakatani & Collaborators
Copyright (c) 2017-2022 by niki

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.

   3. This notice may not be removed or altered from any source distribution.
*/

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

#include <silica/silica.h>

#endif /* MY_CONFIG_H */
