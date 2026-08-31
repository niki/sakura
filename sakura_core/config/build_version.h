// -*- mode:c++; coding:utf-8-ws -*-
#ifndef SAKURA_BUILD_VERSION_H
#define SAKURA_BUILD_VERSION_H

// PR_LV_YY/PR_LV_MDD(日付、数値/文字列両形式)・PR_LV_SUFFIX は
// tools/gen_build_version.ps1 が preBuild.bat から毎ビルド時(JST基準)に
// 自動生成する。手動で編集不要。バージョン表記は "2.3.yy.mddx" 形式
// (yy=西暦下2桁, mdd=月[ゼロ埋めなし]+日[2桁ゼロ埋め]で3〜4桁,
// x=同日ビルドの枝番a,b,c...)。
// (例: 8/15の1回目 2.3.26.815 → 2回目 2.3.26.815a → 3回目 2.3.26.815b)。
//
// このヘッダは my_config.h (my.h経由で全.cppに強制インクルードされる) には
// 置かないこと。build_version_generated.h はビルドの度に内容が変わるため、
// my_config.h 経由で全ファイルに波及させると毎回フルビルドになってしまう。
// バージョン情報が実際に必要な下記3ファイルだけが直接includeする。
//   - sakura_core/sakura_rc.rc
//   - sakura_core/dlg/CDlgAbout.cpp
//   - sakura_core/env/CSakuraEnvironment.cpp
#include "config/build_version_generated.h"

// PR_VER(数値WORD4個、VERSIONINFO用)にはPR_LV_YY/PR_LV_MDDの数値形式を使う
// (yyは0-99、mddは月*100+日で最大1231。いずれもWORD(0-65535)に収まる)。
// 枝番xは英字のためWORDには入らず、PR_VER_STR(文字列)側にのみ現れる。
#define PR_VER      2,3,PR_LV_YY,PR_LV_MDD
#define PR_VER_STR "2.3." PR_LV_YY_STR "." PR_LV_MDD_STR PR_LV_SUFFIX
#define PR_VER_VAL	2320

#endif // SAKURA_BUILD_VERSION_H
