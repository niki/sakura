/*!	@file
	@brief ローマ字入力のあいまい検索用、ひらがな→候補漢字テーブル

	@author Yu-zuki.
	@date 2026.08.19 新規作成 // NKMM_COMMAND_PALETTE_ROMAJI_KANJI
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CKANJIREADINGDICT_20260819_H_
#define SAKURA_CKANJIREADINGDICT_20260819_H_

#ifdef NKMM_COMMAND_PALETTE_ROMAJI_KANJI

//! ひらがな1文字を先頭の読み(音訓)に持ちそうな候補漢字の並びを返す(見つからなければ空文字列)
const wchar_t* GetKanjiCandidatesByHira( wchar_t cHira );

/*!	@brief 漢字1文字から、それが載っている行のひらがな(読みの先頭モーラ)を逆引きする

	GetKanjiCandidatesByHira()の逆方向。あいまい検索の正規化(NormalizeForSearchJp)で、
	検索対象文字列中に現れた漢字をローマ字表記へ事前展開するために使う。
	同じ漢字が複数行にまたがって登録されていた場合は最初に見つかった行を返す
	(テーブルは基本的に1漢字1行を想定しているため、通常は曖昧さは生じない)。
	見つからなければL'\0'を返す 20260820
*/
wchar_t GetHiraByKanji( wchar_t cKanji );

#endif // NKMM_COMMAND_PALETTE_ROMAJI_KANJI

#endif /* SAKURA_CKANJIREADINGDICT_20260819_H_ */
