/*!	@file
	@brief ローマ字入力のまま日本語をあいまい検索するためのマッチング

	@author Yu-zuki.
	@date 2026.08.19 新規作成 // NKMM_COMMAND_PALETTE_ROMAJI
	@date 2026.08.20 util/RomajiFuzzyMatch.hpp(正規化+スコアリング付き部分列マッチの汎用エンジン)を
	                  用いた実装へ全面書き換え。sQueryとsTextの双方を同じ規則で正規化してから
	                  比較する方式になり、一致の良さをスコアとして返せるようになった // NKMM_COMMAND_PALETTE_ROMAJI
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CFUZZYMATCHJP_20260819_H_
#define SAKURA_CFUZZYMATCHJP_20260819_H_

#ifdef NKMM_COMMAND_PALETTE_ROMAJI

#include <string>

/*!	@brief sQueryがsText中に(表記ゆれを吸収しつつ)順序通り含まれるかどうかを判定する

	IME未使用でも、ローマ字のまま日本語のコマンド名を絞り込めるようにするための
	あいまいマッチング。ひらがな⇔カタカナ⇔ローマ字の表記ゆれを吸収し、
	NKMM_COMMAND_PALETTE_ROMAJI_KANJI有効時はさらに「読みの先頭モーラが一致しそうな
	漢字」も候補に含める(厳密な読み検証ではなくヒューリスティック)。
	一致した場合、pOutScoreが非NULLならマッチの良さ(高いほど上位表示にふさわしい)を
	書き込む。連続一致や単語先頭一致ほど高得点になる 20260820
*/
bool FuzzyMatchJapanese( const std::wstring& sQuery, const std::wstring& sText, int* pOutScore = NULL );

/*!	@brief 入力途中のローマ字文字列を、確定できたモーラ分だけかなへ変換した表示用文字列にする

	Windows検索ボックス等と同じ体験(入力欄自体にリアルタイムでかなを表示する簡易IME)を
	提供するための変換。まだ1モーラに満たない末尾の断片("kop"の"p"等)や、促音判定できない
	記号等はローマ字のまま残す。促音(っ)は子音の連続("kk"等)から判定して変換する。
	「-」は長音記号「ー」に変換する 20260819
*/
std::wstring ConvertRomajiToKana( const std::wstring& sText );

#endif // NKMM_COMMAND_PALETTE_ROMAJI

#endif /* SAKURA_CFUZZYMATCHJP_20260819_H_ */
