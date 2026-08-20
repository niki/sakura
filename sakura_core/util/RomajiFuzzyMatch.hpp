/*!	@file
	@brief 正規化ベースのあいまい検索(キー文字列統一化 + 命令式/ヘボン式両対応)

	@author Yu-zuki.
	@date 2026.08.20 新規作成 // NKMM_COMMAND_PALETTE_ROMAJI

	sakura固有の依存(wchar_t/std::wstring、CKanjiReadingDict等)を一切持たない、
	汎用のヘッダオンリーあいまい検索エンジン。UTF-8のstd::stringだけを扱う。
	呼び出し側(util/CFuzzyMatchJp.h/.cpp)がwstring<->UTF-8変換と、
	漢字ヒューリスティック(読みの事前展開)を担当し、このヘッダは
	「ひらがな・カタカナ・ローマ字表記ゆれの正規化」と「正規化後の
	コードポイント列同士のスコアリング付き部分列マッチ」だけに専念する。
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <optional>
#include <cstdint>

namespace fuzzy {

// ============================================================
// UTF-8 <-> コードポイント変換
// ============================================================
// 日本語(かな・漢字)を1文字単位で扱うため、UTF-8のバイト列を
// Unicodeコードポイント(char32_t)の列にいったんデコードする。
// byteOffset/byteLength は、その文字が元のUTF-8文字列の何バイト目から
// 何バイト分を占めるかを表す(ハイライト表示等の位置計算に使う)。

struct DecodedChar {
	char32_t	codepoint;		// Unicodeコードポイント
	size_t		byteOffset;		// 元の文字列内での開始バイト位置
	size_t		byteLength;		// このコードポイントが占めるUTF-8バイト数(1〜4)
};

// UTF-8文字列を1文字ずつコードポイントへデコードする。
// 不正なバイト列に当たった場合はそのバイトを1文字として扱い処理を止めない
// (エディタ上のテキストは何が来ても取りこぼさず、ベストエフォートで扱う)。
inline std::vector<DecodedChar> Utf8Decode( const std::string& s )
{
	std::vector<DecodedChar>	out;
	size_t	i = 0;
	while( i < s.size() ){
		unsigned char	c0 = static_cast<unsigned char>( s[i] );
		char32_t	cp = 0;
		size_t	len = 1;

		if( ( c0 & 0x80 ) == 0x00 ){
			cp = c0; len = 1;
		}else if( ( c0 & 0xE0 ) == 0xC0 && i + 1 < s.size() ){
			cp  = ( c0 & 0x1Fu ) << 6;
			cp |= ( static_cast<unsigned char>( s[i + 1] ) & 0x3Fu );
			len = 2;
		}else if( ( c0 & 0xF0 ) == 0xE0 && i + 2 < s.size() ){
			cp  = ( c0 & 0x0Fu ) << 12;
			cp |= ( static_cast<unsigned char>( s[i + 1] ) & 0x3Fu ) << 6;
			cp |= ( static_cast<unsigned char>( s[i + 2] ) & 0x3Fu );
			len = 3;
		}else if( ( c0 & 0xF8 ) == 0xF0 && i + 3 < s.size() ){
			cp  = ( c0 & 0x07u ) << 18;
			cp |= ( static_cast<unsigned char>( s[i + 1] ) & 0x3Fu ) << 12;
			cp |= ( static_cast<unsigned char>( s[i + 2] ) & 0x3Fu ) << 6;
			cp |= ( static_cast<unsigned char>( s[i + 3] ) & 0x3Fu );
			len = 4;
		}else{
			cp = c0; len = 1;	// 不正バイト列のフォールバック(1バイトだけ読み飛ばす)
		}
		out.push_back( { cp, i, len } );
		i += len;
	}
	return out;
}

// 1つのコードポイントをUTF-8バイト列としてoutの末尾へ追加する(Utf8Decodeの逆変換)。
inline void Utf8AppendCodepoint( std::string& out, char32_t cp )
{
	if( cp <= 0x7F ){
		out.push_back( static_cast<char>( cp ) );
	}else if( cp <= 0x7FF ){
		out.push_back( static_cast<char>( 0xC0 | ( cp >> 6 ) ) );
		out.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
	}else if( cp <= 0xFFFF ){
		out.push_back( static_cast<char>( 0xE0 | ( cp >> 12 ) ) );
		out.push_back( static_cast<char>( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
		out.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
	}else{
		out.push_back( static_cast<char>( 0xF0 | ( cp >> 18 ) ) );
		out.push_back( static_cast<char>( 0x80 | ( ( cp >> 12 ) & 0x3F ) ) );
		out.push_back( static_cast<char>( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
		out.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
	}
}

// ============================================================
// かな正規化・ローマ字変換テーブル
// ============================================================
namespace kana {

constexpr char32_t SOKUON  = 0x30C3;	// っ(促音: 次の子音を重ねる)
constexpr char32_t CHOONPU = 0x30FC;	// ー(長音: 直前の母音を繰り返す)

// 半角カタカナの「基底文字(濁点・半濁点を除いた素の文字)」を、
// 対応する全角カタカナのコードポイントへ変換する。
// 半角カタカナはUnicodeのU+FF66〜U+FF9Dに1文字ずつ独立して割り当てられており、
// 濁点(ﾞ)・半濁点(ﾟ)は別の独立したコードポイント(結合文字的に使われる)。
// そのため濁音・半濁音は「基底文字+濁点」の2コードポイントで表現される点に注意
// (この関数は基底文字だけを変換し、濁点の適用はApplyDakuten側で行う)。
inline char32_t HalfwidthBaseToFullwidth( char32_t cp )
{
	switch( cp ){
		case 0xFF66: return 0x30F2; case 0xFF67: return 0x30A1;
		case 0xFF68: return 0x30A3; case 0xFF69: return 0x30A5;
		case 0xFF6A: return 0x30A7; case 0xFF6B: return 0x30A9;
		case 0xFF6C: return 0x30E3; case 0xFF6D: return 0x30E5;
		case 0xFF6E: return 0x30E7; case 0xFF6F: return 0x30C3;
		case 0xFF70: return 0x30FC; case 0xFF71: return 0x30A2;
		case 0xFF72: return 0x30A4; case 0xFF73: return 0x30A6;
		case 0xFF74: return 0x30A8; case 0xFF75: return 0x30AA;
		case 0xFF76: return 0x30AB; case 0xFF77: return 0x30AD;
		case 0xFF78: return 0x30AF; case 0xFF79: return 0x30B1;
		case 0xFF7A: return 0x30B3; case 0xFF7B: return 0x30B5;
		case 0xFF7C: return 0x30B7; case 0xFF7D: return 0x30B9;
		case 0xFF7E: return 0x30BB; case 0xFF7F: return 0x30BD;
		case 0xFF80: return 0x30BF; case 0xFF81: return 0x30C1;
		case 0xFF82: return 0x30C4; case 0xFF83: return 0x30C6;
		case 0xFF84: return 0x30C8; case 0xFF85: return 0x30CA;
		case 0xFF86: return 0x30CB; case 0xFF87: return 0x30CC;
		case 0xFF88: return 0x30CD; case 0xFF89: return 0x30CE;
		case 0xFF8A: return 0x30CF; case 0xFF8B: return 0x30D2;
		case 0xFF8C: return 0x30D5; case 0xFF8D: return 0x30D8;
		case 0xFF8E: return 0x30DB; case 0xFF8F: return 0x30DE;
		case 0xFF90: return 0x30DF; case 0xFF91: return 0x30E0;
		case 0xFF92: return 0x30E1; case 0xFF93: return 0x30E2;
		case 0xFF94: return 0x30E4; case 0xFF95: return 0x30E6;
		case 0xFF96: return 0x30E8; case 0xFF97: return 0x30E9;
		case 0xFF98: return 0x30EA; case 0xFF99: return 0x30EB;
		case 0xFF9A: return 0x30EC; case 0xFF9B: return 0x30ED;
		case 0xFF9C: return 0x30EF; case 0xFF9D: return 0x30F3;
		default: return 0;	// 半角カタカナの範囲外(記号等は呼び出し側で弾く)
	}
}

inline bool IsHalfwidthKatakanaBase( char32_t cp ) { return cp >= 0xFF66 && cp <= 0xFF9D; }
inline bool IsDakutenMark( char32_t cp ) { return cp == 0xFF9E; }		// ﾞ
inline bool IsHandakutenMark( char32_t cp ) { return cp == 0xFF9F; }	// ﾟ

// 全角カタカナの基底文字に濁点を適用し、濁音の文字へ変換する
// (対応しない文字はそのまま返す=異常入力への防御)。
inline char32_t ApplyDakuten( char32_t base )
{
	switch( base ){
		case 0x30AB: return 0x30AC; case 0x30AD: return 0x30AE;
		case 0x30AF: return 0x30B0; case 0x30B1: return 0x30B2;
		case 0x30B3: return 0x30B4; case 0x30B5: return 0x30B6;
		case 0x30B7: return 0x30B8; case 0x30B9: return 0x30BA;
		case 0x30BB: return 0x30BC; case 0x30BD: return 0x30BE;
		case 0x30BF: return 0x30C0; case 0x30C1: return 0x30C2;
		case 0x30C4: return 0x30C5; case 0x30C6: return 0x30C7;
		case 0x30C8: return 0x30C9; case 0x30CF: return 0x30D0;
		case 0x30D2: return 0x30D3; case 0x30D5: return 0x30D6;
		case 0x30D8: return 0x30D9; case 0x30DB: return 0x30DC;
		case 0x30A6: return 0x30F4;	// ウ -> ヴ(外来語のv音)
		default: return base;
	}
}
// 全角カタカナの基底文字に半濁点を適用する(ハ行のみ: パ行等)。
inline char32_t ApplyHandakuten( char32_t base )
{
	switch( base ){
		case 0x30CF: return 0x30D1; case 0x30D2: return 0x30D4;
		case 0x30D5: return 0x30D7; case 0x30D8: return 0x30DA;
		case 0x30DB: return 0x30DD;
		default: return base;
	}
}

// 拗音(きゃ・しょ等)、外来語の拡張音(ふぁ・ゔぁ等)は、
// カタカナ2文字の組み合わせで1つのモーラ(発音単位)を構成する
// (キ+ャ -> "kya" のように、単純に1文字ずつローマ字化すると "kiya" になって
//  しまう表記・発音上のズレを避けるため、専用のテーブルを引く)。
//
// c1: 1文字目(通常の五十音由来のカタカナ)
// c2: 2文字目(小書きのゃゅょ、または小書きの母音ぁぃぅぇぉ)
// 戻り値: 該当する組み合わせがあればそのローマ字、なければnullopt
//         (nulloptの場合は呼び出し側が1文字ずつ単独変換にフォールバックする)
inline std::optional<std::string> LookupDigraph( char32_t c1, char32_t c2 )
{
	// 2つのコードポイントを1つの64bitキーに詰めてテーブル検索する
	auto key = []( char32_t a, char32_t b ) { return ( static_cast<uint64_t>( a ) << 32 ) | b; };
	static const std::vector<std::pair<uint64_t, const char*>> table = {
		// --- 標準拗音(きゃ・しゃ・ちゃ・にゃ・ひゃ・みゃ・りゃ・ぎゃ・じゃ・びゃ・ぴゃ行) ---
		{key(0x30AD, 0x30E3), "kya"}, {key(0x30AD, 0x30E5), "kyu"}, {key(0x30AD, 0x30E7), "kyo"},
		{key(0x30AE, 0x30E3), "gya"}, {key(0x30AE, 0x30E5), "gyu"}, {key(0x30AE, 0x30E7), "gyo"},
		{key(0x30B7, 0x30E3), "sha"}, {key(0x30B7, 0x30E5), "shu"}, {key(0x30B7, 0x30E7), "sho"},
		{key(0x30B8, 0x30E3), "ja"},  {key(0x30B8, 0x30E5), "ju"},  {key(0x30B8, 0x30E7), "jo"},
		{key(0x30C1, 0x30E3), "cha"}, {key(0x30C1, 0x30E5), "chu"}, {key(0x30C1, 0x30E7), "cho"},
		{key(0x30CB, 0x30E3), "nya"}, {key(0x30CB, 0x30E5), "nyu"}, {key(0x30CB, 0x30E7), "nyo"},
		{key(0x30D2, 0x30E3), "hya"}, {key(0x30D2, 0x30E5), "hyu"}, {key(0x30D2, 0x30E7), "hyo"},
		{key(0x30D3, 0x30E3), "bya"}, {key(0x30D3, 0x30E5), "byu"}, {key(0x30D3, 0x30E7), "byo"},
		{key(0x30D4, 0x30E3), "pya"}, {key(0x30D4, 0x30E5), "pyu"}, {key(0x30D4, 0x30E7), "pyo"},
		{key(0x30DF, 0x30E3), "mya"}, {key(0x30DF, 0x30E5), "myu"}, {key(0x30DF, 0x30E7), "myo"},
		{key(0x30EA, 0x30E3), "rya"}, {key(0x30EA, 0x30E5), "ryu"}, {key(0x30EA, 0x30E7), "ryo"},
		// --- 外来語由来の拡張音(小書きの母音との組み合わせ) ---
		{key(0x30D5, 0x30A1), "fa"}, {key(0x30D5, 0x30A3), "fi"}, {key(0x30D5, 0x30A7), "fe"}, {key(0x30D5, 0x30A9), "fo"},  // ふぁふぃふぇふぉ
		{key(0x30A6, 0x30A3), "wi"}, {key(0x30A6, 0x30A7), "we"}, {key(0x30A6, 0x30A9), "wo"},  // うぃうぇうぉ
		{key(0x30F4, 0x30A1), "va"}, {key(0x30F4, 0x30A3), "vi"}, {key(0x30F4, 0x30A7), "ve"}, {key(0x30F4, 0x30A9), "vo"},  // ゔぁゔぃゔぇゔぉ
		{key(0x30C6, 0x30A3), "ti"}, {key(0x30C8, 0x30A5), "tu"},  // てぃ・とぅ
		{key(0x30C7, 0x30A3), "di"}, {key(0x30C9, 0x30A5), "du"},  // でぃ・どぅ
		{key(0x30C4, 0x30A1), "tsa"}, {key(0x30C4, 0x30A3), "tsi"}, {key(0x30C4, 0x30A7), "tse"}, {key(0x30C4, 0x30A9), "tso"},  // つぁつぃつぇつぉ
		{key(0x30B7, 0x30A7), "she"}, {key(0x30B8, 0x30A7), "je"}, {key(0x30C1, 0x30A7), "che"},  // しぇ・じぇ・ちぇ
	};
	uint64_t k = key( c1, c2 );
	for( auto& e : table ) if( e.first == k ) return std::string( e.second );
	return std::nullopt;
}

// カタカナ単独1文字 -> ローマ字(清音・濁音・半濁音+ん・ゔを網羅)。
// 小書きの母音・小書きのゃゅょが単独で出現した場合(拗音の相方が続かない場合)は、
// 対応する通常サイズの音と同じローマ字にフォールバックする
// (例: 単独の「ぇ」は "e" 扱い。変換不能よりは良い近似)。
inline std::optional<std::string> LookupMonograph( char32_t cp )
{
	switch( cp ){
		case 0x30A2: case 0x30A1: return std::string("a");   // ア・ァ
		case 0x30A4: case 0x30A3: return std::string("i");   // イ・ィ
		case 0x30A6: case 0x30A5: return std::string("u");   // ウ・ゥ
		case 0x30A8: case 0x30A7: return std::string("e");   // エ・ェ
		case 0x30AA: case 0x30A9: return std::string("o");   // オ・ォ
		case 0x30AB: return std::string("ka"); case 0x30AD: return std::string("ki");
		case 0x30AF: return std::string("ku"); case 0x30B1: return std::string("ke");
		case 0x30B3: return std::string("ko");
		case 0x30AC: return std::string("ga"); case 0x30AE: return std::string("gi");
		case 0x30B0: return std::string("gu"); case 0x30B2: return std::string("ge");
		case 0x30B4: return std::string("go");
		case 0x30B5: return std::string("sa"); case 0x30B7: return std::string("shi");
		case 0x30B9: return std::string("su"); case 0x30BB: return std::string("se");
		case 0x30BD: return std::string("so");
		case 0x30B6: return std::string("za"); case 0x30B8: return std::string("ji");
		case 0x30BA: return std::string("zu"); case 0x30BC: return std::string("ze");
		case 0x30BE: return std::string("zo");
		case 0x30BF: return std::string("ta"); case 0x30C1: return std::string("chi");
		case 0x30C4: return std::string("tsu"); case 0x30C6: return std::string("te");
		case 0x30C8: return std::string("to");
		case 0x30C0: return std::string("da"); case 0x30C2: return std::string("di");
		case 0x30C5: return std::string("du"); case 0x30C7: return std::string("de");
		case 0x30C9: return std::string("do");
		case 0x30CA: return std::string("na"); case 0x30CB: return std::string("ni");
		case 0x30CC: return std::string("nu"); case 0x30CD: return std::string("ne");
		case 0x30CE: return std::string("no");
		case 0x30CF: return std::string("ha"); case 0x30D2: return std::string("hi");
		case 0x30D5: return std::string("fu"); case 0x30D8: return std::string("he");
		case 0x30DB: return std::string("ho");
		case 0x30D0: return std::string("ba"); case 0x30D3: return std::string("bi");
		case 0x30D6: return std::string("bu"); case 0x30D9: return std::string("be");
		case 0x30DC: return std::string("bo");
		case 0x30D1: return std::string("pa"); case 0x30D4: return std::string("pi");
		case 0x30D7: return std::string("pu"); case 0x30DA: return std::string("pe");
		case 0x30DD: return std::string("po");
		case 0x30DE: return std::string("ma"); case 0x30DF: return std::string("mi");
		case 0x30E0: return std::string("mu"); case 0x30E1: return std::string("me");
		case 0x30E2: return std::string("mo");
		case 0x30E4: case 0x30E3: return std::string("ya");  // ヤ・ャ(単独時)
		case 0x30E6: case 0x30E5: return std::string("yu");  // ユ・ュ(単独時)
		case 0x30E8: case 0x30E7: return std::string("yo");  // ヨ・ョ(単独時)
		case 0x30E9: return std::string("ra"); case 0x30EA: return std::string("ri");
		case 0x30EB: return std::string("ru"); case 0x30EC: return std::string("re");
		case 0x30ED: return std::string("ro");
		case 0x30EF: return std::string("wa"); case 0x30F2: return std::string("wo");
		case 0x30F3: return std::string("n");   // ン(撥音)
		case 0x30F4: return std::string("vu");  // ヴ(単独時。子音+母音の組はLookupDigraph側)
		default: return std::nullopt;  // カタカナではない(漢字・記号等)
	}
}

// 命令式ローマ字(si/ti/tu/zi/hu/sya...)からヘボン式(正規形: shi/chi/tsu/ji/fu/sha...)への
// エイリアス表。LookupMonograph/LookupDigraphが返すのは常にヘボン式のため、
// パターン文字列側・検索対象側の双方に同じNormalizeForSearchを通すことで、
// 命令式で入力してもヘボン式由来の文字列と同じ正規化結果に畳み込まれ一致する。
//
// エントリは3文字系(拗音由来の"sya"等)を先に、2文字系を後に書いてあるが、
// 実際のマッチ(ApplyRomajiAliases)はこの並び順に関係なく最長一致を選ぶ。
struct AliasEntry { const char* variant; const char* canonical; };
inline const std::vector<AliasEntry>& RomajiAliases()
{
	static const std::vector<AliasEntry> table = {
		// 拗音系(3文字): 命令式の子音+y+母音 -> ヘボン式
		{"sya", "sha"}, {"syu", "shu"}, {"syo", "sho"},
		{"tya", "cha"}, {"tyu", "chu"}, {"tyo", "cho"},
		{"zya", "ja"},  {"zyu", "ju"},  {"zyo", "jo"},
		// 単独音系(2文字)
		{"si", "shi"}, {"ti", "chi"}, {"tu", "tsu"}, {"hu", "fu"}, {"zi", "ji"},
		// 外来語拡張音(てぃ/でぃ)の別表記。"h"を挟んで小書きの母音であることを
		// 明示するIME等の入力習慣(例: Google日本語入力の既定ローマ字テーブル)
		{"thi", "ti"}, {"dhi", "di"},
		// 撥音「ん」の二重n表記(IME等で「んば」のように次の文字との連結誤認識を
		// 避けるための入力習慣)を単なる「ん」1つとして畳み込む(例: shinnbunn -> shinbun)。
		{"nn", "n"},
	};
	return table;
}

}  // namespace kana

// ============================================================
// 正規化本体
// ============================================================
namespace detail {

// 単語境界(word boundary)判定用の文字種分類。
// あいまい検索のスコアリングで、単語の先頭にヒットした場合に加点するために使う
// (例: 入力 "gc" が "GetChanges" にヒットする際、離れたG と C の直後で加点する)。
enum class CharClass { Lower, Upper, Digit, Separator, Other };

// 正規化される前の元の文字ごとに文字種を分類する。
// ここでの分類は正規化後の比較(小文字化したローマ字同士の一致判定)とは
// 独立して行っており、camelCaseの大文字境界等の情報を保持するために
// あえて正規化前の情報を見ている(単に小文字化してしまうと境界が分からなくなるため)。
//
// かな・漢字などASCII以外の文字はすべて Other 扱いになるようにしており、
// 「ASCII単語 <-> かな/漢字」の切り替わり地点を自動的に単語境界として
// 扱える(例: "設定Dialog" の D の前、"Fileを開く" の を の前など)。
inline CharClass ClassifyOriginal( char32_t cp )
{
	if( cp >= 'a' && cp <= 'z' ) return CharClass::Lower;
	if( cp >= 'A' && cp <= 'Z' ) return CharClass::Upper;
	if( cp >= '0' && cp <= '9' ) return CharClass::Digit;
	if( cp == '_' || cp == '-' || cp == ' ' || cp == '/' || cp == '.' ||
		cp == '\\' || cp == 0x3000 /* 全角スペース */ )
		return CharClass::Separator;
	return CharClass::Other;
}

// Stage2: かなの表記ゆれをもう1段階だけ吸収するトークン化。
// ここではまだローマ字化はせず、次のことだけを行う:
//   - 半角カナ(基底+濁点/半濁点)を全角カタカナ相当のコードポイントに統合
//   - ひらがなを対応する全角カタカナ相当のコードポイントに変換(offset)
// これにより以降のモーラ分解・ローマ字変換ロジックは
// 「全角カタカナのコードポイント」だけを見ればよくなり、実装が単純化される。
struct Stage2Token {
	char32_t	cp;			// 統合後のコードポイント(カタカナ相当 or 関係ない文字はそのまま)
	size_t		byteOffset;	// 元のUTF-8文字列での開始バイト位置
	size_t		byteLength;	// 元の文字列で何バイト分に対応するか
							// (半角カナ+濁点の2文字分をまとめて1トークンにした場合は
							//  その合計バイト数になる)
};

// 元の文字列をデコードした結果(decoded)を受け取り、Stage2Tokenの列に変換する。
inline std::vector<Stage2Token> BuildStage2( const std::vector<DecodedChar>& decoded )
{
	std::vector<Stage2Token> out;
	out.reserve( decoded.size() );
	size_t i = 0;
	while( i < decoded.size() ){
		char32_t cp = decoded[i].codepoint;

		// --- ケース1: 半角カナの基底文字 ---
		// 直後が濁点/半濁点なら1トークンにまとめて統合する(2文字 -> 1コードポイント)。
		// 濁点等が続かない場合は基底文字だけを全角カタカナに変換する。
		if( kana::IsHalfwidthKatakanaBase( cp ) ){
			char32_t full = kana::HalfwidthBaseToFullwidth( cp );
			size_t off = decoded[i].byteOffset;
			size_t len = decoded[i].byteLength;
			if( i + 1 < decoded.size() && kana::IsDakutenMark( decoded[i + 1].codepoint ) ){
				full = kana::ApplyDakuten( full );
				len = ( decoded[i + 1].byteOffset + decoded[i + 1].byteLength ) - off;
				i += 2;
			}else if( i + 1 < decoded.size() && kana::IsHandakutenMark( decoded[i + 1].codepoint ) ){
				full = kana::ApplyHandakuten( full );
				len = ( decoded[i + 1].byteOffset + decoded[i + 1].byteLength ) - off;
				i += 2;
			}else{
				i += 1;
			}
			out.push_back( { full, off, len } );
			continue;
		}

		// --- ケース2: 全角ひらがな ---
		// Unicodeではひらがなブロック(U+3041〜)とカタカナブロック(U+30A1〜)が
		// 同じ並び順で配置されているため、+0x60するだけでカタカナ相当になる。
		if( cp >= 0x3041 && cp <= 0x3096 ){
			out.push_back( { cp + 0x60, decoded[i].byteOffset, decoded[i].byteLength } );
			i += 1;
			continue;
		}

		// --- ケース3: それ以外(全角カタカナはすでにこの形なので素通し。漢字等も同様) ---
		out.push_back( { cp, decoded[i].byteOffset, decoded[i].byteLength } );
		i += 1;
	}
	return out;
}

// モーラ(発音の最小単位)1つ分の変換結果。
// 例: 拗音「きゃ」は2つのStage2Tokenから1つのMora("kya")になる。
// プレースホルダー自体はローマ字を持たず、直後・直前のモーラを見て初めて
// 具体的な文字が決まる特殊マーカーとして扱う(isSokuon/isChoonpu)。
struct Mora {
	std::string	text;			// このモーラのローマ字(または関係ない文字そのまま)
	size_t		byteOffset;		// 元の文字列での対応開始位置
	size_t		byteLength;		// 元の文字列でのバイト長
	bool		isSokuon = false;	// 促音プレースホルダー(っ)。textは空文字列
	bool		isChoonpu = false;	// 長音プレースホルダー(ー)。textは空文字列
	bool		wordBoundary = false;	// このモーラの先頭が単語境界かどうか
};

}  // namespace detail

// 正規化結果一式。
//   normalized       : 人間が読める正規化後のUTF-8文字列(デバッグ表示用)
//   codepoints        : 正規化後の1文字ずつのコードポイント列(実際の比較に使う)
//   originByteOffset  : codepoints[i] が元の文字列の何バイト目に対応するか
//                        (ハイライト表示・複数出力文字が同じ元位置を指す場合もある)
//   isWordBoundary    : codepoints[i] が単語境界(スコアボーナス対象)かどうか
struct NormalizedText {
	std::string				normalized;
	std::vector<char32_t>	codepoints;
	std::vector<size_t>	originByteOffset;
	std::vector<bool>		isWordBoundary;
};

// Pass 3: 命令式ローマ字等の別スペルをヘボン式(正規形)へ畳み込む(ASCII部分のみ対象。非ASCIIは素通し)。
// すでにローマ字化・小文字化された NormalizedText を受け取る。
// 先頭から貪欲に、その位置で一致する最長のエイリアスを探して置き換えていく
// (例: "tu" という2文字が見つかっても、"tsu" の3文字に置き換える)。
//
// 置き換えでコードポイント数が変化する(2文字->3文字など)ため、
// 対応する originByteOffset / isWordBoundary は、置き換え前の範囲の先頭文字が持つ
// 値をすべての置換後文字にコピーするという単純化を行っている
// (ハイライト精度はモーラ単位程度で十分なため、この粒度で問題ない)。
inline NormalizedText ApplyRomajiAliases( const NormalizedText& in )
{
	NormalizedText out;
	size_t n = in.codepoints.size();
	size_t i = 0;
	const auto& aliases = kana::RomajiAliases();

	while( i < n ){
		std::string window;
		size_t maxCheck = std::min<size_t>( 3, n - i );
		for( size_t k = 0; k < maxCheck; ++k ){
			char32_t cp = in.codepoints[i + k];
			if( cp > 0x7F ) break;
			window.push_back( static_cast<char>( cp ) );
		}

		const kana::AliasEntry* matched = nullptr;
		size_t matchedLen = 0;
		for( auto& a : aliases ){
			size_t vlen = std::string( a.variant ).size();
			if( vlen <= window.size() && window.compare( 0, vlen, a.variant ) == 0 && vlen > matchedLen ){
				matched = &a;
				matchedLen = vlen;
			}
		}

		if( matched ){
			size_t origin = in.originByteOffset[i];
			bool boundary = in.isWordBoundary[i];
			std::string canon = matched->canonical;
			for( size_t k = 0; k < canon.size(); ++k ){
				out.codepoints.push_back( static_cast<char32_t>( static_cast<unsigned char>( canon[k] ) ) );
				out.originByteOffset.push_back( origin );
				out.isWordBoundary.push_back( k == 0 && boundary );
			}
			i += matchedLen;
		}else{
			out.codepoints.push_back( in.codepoints[i] );
			out.originByteOffset.push_back( in.originByteOffset[i] );
			out.isWordBoundary.push_back( in.isWordBoundary[i] );
			++i;
		}
	}

	for( char32_t c : out.codepoints ) Utf8AppendCodepoint( out.normalized, c );
	return out;
}

// 正規化のエントリポイント。検索対象文字列・ユーザー入力パターンの双方に
// 必ずこの関数を通してから比較する(それによって表記ゆれが吸収される)。
//
// 処理の流れ:
//   Stage2 (BuildStage2)  : 半角カナ統合 + ひらがな->カタカナのoffset変換
//   Pass 1 (このループ)    : Stage2トークン列をモーラ単位に分解する。
//                            拗音(2文字combo)を先読みでチェックする。
//                            促音・長音はプレースホルダーとして記録するだけに留める。
//   Pass 2 (そのあとのループ): モーラ列を最終的なコードポイント列に変換する。
//                            促音プレースホルダーは、次のモーラの先頭子音を重ねる処理。
//                            長音プレースホルダーは、直前の母音を繰り返す処理を行う。
//                            ここで初めて確定する(前後の情報が必要なため)。
//   Pass 3 (ApplyRomajiAliases): 命令式ローマ字等のスペルをヘボン式に畳み込む。
inline NormalizedText NormalizeForSearch( const std::string& original )
{
	auto decoded = Utf8Decode( original );
	auto tokens = detail::BuildStage2( decoded );

	std::vector<detail::CharClass> classes;
	classes.reserve( tokens.size() );
	for( auto& t : tokens ) classes.push_back( detail::ClassifyOriginal( t.cp ) );

	// ---------- Pass 1: トークン化 -> モーラ化 ----------
	std::vector<detail::Mora> morae;
	morae.reserve( tokens.size() );
	for( size_t i = 0; i < tokens.size(); ){
		bool boundary;
		if( i == 0 ){
			boundary = true;
		}else{
			auto prev = classes[i - 1];
			auto curr = classes[i];
			boundary = ( prev == detail::CharClass::Separator ) ||
					   ( prev == detail::CharClass::Lower && curr == detail::CharClass::Upper ) ||
					   ( prev == detail::CharClass::Digit && curr != detail::CharClass::Digit ) ||
					   ( prev == detail::CharClass::Other && curr != detail::CharClass::Other ) ||
					   ( prev != detail::CharClass::Other && curr == detail::CharClass::Other );
		}

		char32_t cp = tokens[i].cp;

		if( cp == kana::SOKUON ){
			morae.push_back( { "", tokens[i].byteOffset, tokens[i].byteLength, true, false, boundary } );
			++i; continue;
		}
		if( cp == kana::CHOONPU ){
			morae.push_back( { "", tokens[i].byteOffset, tokens[i].byteLength, false, true, boundary } );
			++i; continue;
		}

		if( i + 1 < tokens.size() ){
			auto di = kana::LookupDigraph( cp, tokens[i + 1].cp );
			if( di ){
				size_t len = ( tokens[i + 1].byteOffset + tokens[i + 1].byteLength ) - tokens[i].byteOffset;
				morae.push_back( { *di, tokens[i].byteOffset, len, false, false, boundary } );
				i += 2; continue;
			}
		}

		auto mono = kana::LookupMonograph( cp );
		if( mono ){
			morae.push_back( { *mono, tokens[i].byteOffset, tokens[i].byteLength, false, false, boundary } );
			++i; continue;
		}

		// 全角英数記号(！Ａ０等) -> 半角化・小文字化
		if( cp >= 0xFF01 && cp <= 0xFF5E ){
			char32_t half = cp - 0xFEE0;
			if( half >= 'A' && half <= 'Z' ) half += 0x20;
			morae.push_back( { std::string( 1, static_cast<char>( half ) ), tokens[i].byteOffset, tokens[i].byteLength, false, false, boundary } );
			++i; continue;
		}
		// 全角スペース -> 半角スペース
		if( cp == 0x3000 ){
			morae.push_back( { " ", tokens[i].byteOffset, tokens[i].byteLength, false, false, boundary } );
			++i; continue;
		}
		// ASCII大文字 -> 小文字
		if( cp >= 'A' && cp <= 'Z' ){
			morae.push_back( { std::string( 1, static_cast<char>( cp + 0x20 ) ), tokens[i].byteOffset, tokens[i].byteLength, false, false, boundary } );
			++i; continue;
		}
		// 上記いずれにも該当しない(漢字・ハングル・記号等) -> 無変換でそのまま
		{
			std::string s;
			Utf8AppendCodepoint( s, cp );
			morae.push_back( { s, tokens[i].byteOffset, tokens[i].byteLength, false, false, boundary } );
			++i;
		}
	}

	// ---------- Pass 2: モーラ列 -> 最終コードポイント列 ----------
	NormalizedText result;
	auto isVowel = []( char c ) { return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; };

	for( size_t m = 0; m < morae.size(); ++m ){
		const auto& mo = morae[m];

		if( mo.isSokuon ){
			// 促音(っ): 直後の実体を持つモーラを探し、その先頭が子音であれば
			// その子音だけを1文字、促音自身の位置情報を付けて出力する
			// (例: 「っか」の次が "ka" なら 'k' を出力 -> 結果として "kka" になる)。
			// 直後が母音始まり(通常は起こらない)の場合は何も出力しない(安全側)。
			for( size_t n2 = m + 1; n2 < morae.size(); ++n2 ){
				if( morae[n2].text.empty() ) continue;
				char c0 = morae[n2].text[0];
				if( !isVowel( c0 ) ){
					result.codepoints.push_back( static_cast<char32_t>( static_cast<unsigned char>( c0 ) ) );
					result.originByteOffset.push_back( mo.byteOffset );
					result.isWordBoundary.push_back( mo.wordBoundary );
				}
				break;
			}
			continue;
		}

		if( mo.isChoonpu ){
			// 長音(ー): それまでに出力済みの最後の文字が母音であれば、
			// 同じ母音をもう一度出力する。文字列の先頭が長音の場合など
			// 繰り返し対象がなければ何も出力しない(要件があいまいなので無視する)。
			if( !result.codepoints.empty() ){
				char32_t last = result.codepoints.back();
				if( last <= 0x7F && isVowel( static_cast<char>( last ) ) ){
					result.codepoints.push_back( last );
					result.originByteOffset.push_back( mo.byteOffset );
					result.isWordBoundary.push_back( false );	// 長音自体は新しい単語の先頭にはならない
				}
			}
			continue;
		}

		// 通常のモーラ: text の中身(1〜3文字のローマ字、または関係ない文字1つ)を
		// 1コードポイントずつ展開して追加する。最初の1文字だけ単語境界フラグを引き継ぐ。
		auto cps = Utf8Decode( mo.text );
		for( size_t c = 0; c < cps.size(); ++c ){
			result.codepoints.push_back( cps[c].codepoint );
			result.originByteOffset.push_back( mo.byteOffset );
			result.isWordBoundary.push_back( c == 0 && mo.wordBoundary );
		}
	}

	for( char32_t c : result.codepoints ) Utf8AppendCodepoint( result.normalized, c );

	// ---------- Pass 3: 命令式ローマ字エイリアスの畳み込み ----------
	return ApplyRomajiAliases( result );
}

// ============================================================
// あいまい検索本体(正規化後のコードポイント列に対して行う)
// ============================================================

// スコアリングの重み。用途に応じて調整可能。
// 値を大きくするほどその要素を順位に強く効かせる。
struct FuzzyMatchConfig {
	int consecutiveBonus = 15;		// 直前の一致から連続してヒットした場合の加点
	int wordBoundaryBonus = 30;	// 単語境界(先頭・camelCase境界等)にヒットした場合の加点
	int firstCharBonus = 20;		// パターンの最初の文字が対象の先頭にヒットした場合の加点
	int gapPenalty = -2;			// ヒット間の未使用文字1つあたりの減点(間が空くほど減点)
	int leadingGapPenalty = -3;	// 最初のヒットまでの未使用文字1つあたりの減点
};

// あいまい検索の中核処理。
// patternCp(検索クエリを正規化したコードポイント列)が、
// target(検索対象を正規化した結果)の中に、順序を保った部分列として
// 出現するかどうかを判定する。出現すればスコアを計算する。
//
// アルゴリズムは貪欲法(グリーディ): パターンの各文字について、
// targetの現在位置以降で最初に見つかった一致を採用し、そのまま前に進む。
// 動的計画法(DP)による厳密な最適解ではないが、コマンドパレット規模
// (候補数百〜数千件)であれば十分高速かつ実用上ほぼ最適な結果を得られる。
//
// outScore: マッチした場合のスコア(高いほど「良い」ヒットとする)
// outMatchedCpIndices: 指定されれば、targetのどのインデックスにヒットしたかを格納
//                       (ハイライト表示に使う)
inline bool FuzzyMatchNormalized( const std::vector<char32_t>& patternCp,
								   const NormalizedText& target,
								   int& outScore,
								   std::vector<size_t>* outMatchedCpIndices = nullptr,
								   const FuzzyMatchConfig& cfg = FuzzyMatchConfig{} )
{
	if( patternCp.empty() ){ outScore = 0; return true; }
	const auto& tCp = target.codepoints;
	if( tCp.empty() || patternCp.size() > tCp.size() ) return false;

	std::vector<size_t> matched;
	matched.reserve( patternCp.size() );
	int score = 0;
	size_t tIdx = 0;
	long lastMatch = -1;

	for( size_t p = 0; p < patternCp.size(); ++p ){
		bool found = false;
		for( ; tIdx < tCp.size(); ++tIdx ){
			if( tCp[tIdx] != patternCp[p] ) continue;

			long gap = static_cast<long>( tIdx ) - lastMatch - 1;
			if( lastMatch == -1 ){
				score += static_cast<int>( gap ) * cfg.leadingGapPenalty;
				if( tIdx == 0 ) score += cfg.firstCharBonus;
			}else if( gap == 0 ){
				score += cfg.consecutiveBonus;
			}else{
				score += static_cast<int>( gap ) * cfg.gapPenalty;
			}

			if( target.isWordBoundary[tIdx] ) score += cfg.wordBoundaryBonus;

			matched.push_back( tIdx );
			lastMatch = static_cast<long>( tIdx );
			++tIdx;
			found = true;
			break;
		}
		if( !found ) return false;
	}

	// 対象文字列が短いほど無駄な情報が少なく精確とみなし、わずかに加点する
	// (同じ部分一致でも、長い文字列より短い文字列を上位に出す)。
	score += static_cast<int>( tCp.size() - patternCp.size() ) * -1;

	if( outMatchedCpIndices ) *outMatchedCpIndices = std::move( matched );
	outScore = score;
	return true;
}

}  // namespace fuzzy
