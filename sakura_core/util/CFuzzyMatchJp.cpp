/*!	@file
	@brief ローマ字入力のまま日本語をあいまい検索するためのマッチング

	@author Yu-zuki.
	@date 2026.08.19 新規作成 // NKMM_COMMAND_PALETTE_ROMAJI
	@date 2026.08.19 拗音・外来語拡張かな(しゃ/ふぁ等の2文字表記)を追加、
	                  単体母音トークン(a/i/u/e/o/n)がローマ字→かな変換されず
	                  リテラルASCII文字としてしか一致しなかった不具合を修正
	@date 2026.08.20 あいまい検索エンジン部分をutil/RomajiFuzzyMatch.hpp
	                  (sakura非依存の汎用ヘッダオンリー実装。正規化+スコアリング付き
	                  部分列マッチ)を使う方式へ全面書き換え。このファイルは
	                  以下のsakura固有の結線だけを担当する:
	                    - std::wstring(UTF-16) <-> UTF-8 std::string の変換
	                    - 漢字ヒューリスティック(CKanjiReadingDict/複数モーラ読み
	                      テーブル)による、検索対象文字列中の漢字のひらがな読みへの
	                      事前展開(あいまい検索エンジン自体は漢字を素通しするだけで
	                      読みを知らないため、ここで展開してから渡す)
	                    - ConvertRomajiToKana(フィルタ欄のライブかな変換。あいまい
	                      検索とは別の関心事のため、以前からの実装をそのまま維持)

	Gretel Bar (ブラウザ拡張) の src/fuzzy_match.js を参考にC++へ移植した
	正規化ロジックは util/RomajiFuzzyMatch.hpp 側に集約されている。
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#include "StdAfx.h"

#ifdef NKMM_COMMAND_PALETTE_ROMAJI

#include "util/CFuzzyMatchJp.h"
#include "util/RomajiFuzzyMatch.hpp"

#ifdef NKMM_COMMAND_PALETTE_ROMAJI_KANJI
#include "util/CKanjiReadingDict.h"
#endif

#include <vector>
#include <map>
#include <algorithm>

namespace {

	// ============================================================
	// wstring(UTF-16) <-> UTF-8 変換
	// ============================================================
	// util/RomajiFuzzyMatch.hppはsakura非依存にするためUTF-8のstd::stringだけを
	// 扱う設計になっている。sakura内部はwchar_t(UTF-16)なので、あいまい検索
	// エンジンに渡す直前でこの変換をかける 20260820
	std::string WStrToUtf8( const std::wstring& s )
	{
		if( s.empty() ){ return std::string(); }
		int	nLen = ::WideCharToMultiByte( CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0, NULL, NULL );
		if( nLen <= 0 ){ return std::string(); }
		std::string	sRet( (size_t)nLen, '\0' );
		::WideCharToMultiByte( CP_UTF8, 0, s.c_str(), (int)s.size(), &sRet[0], nLen, NULL, NULL );
		return sRet;
	}

	/*! 半角の「-」を全角の長音記号「ー」と同一視する(ConvertRomajiToKana()が
		入力中に「-」を「ー」へ変換して表示するのと同じ扱い方)。

		util/RomajiFuzzyMatch.hppのNormalizeForSearchは、長音「ー」を「直前の母音を
		繰り返す」という規則で正規化するが、半角の「-」自体はただの区切り文字
		(CharClass::Separator)として素通しするだけで同じ変換はかからない。
		そのため、IME OFF等でライブかな変換を経ずに生のローマ字のまま渡ってきた
		"kopi-"のような入力(本来「コピー」を意図した表記)が、正規化後の文字列
		"kopii"と噛み合わず不一致になってしまう。あいまい検索へ渡す直前に半角「-」を
		「ー」へ寄せておくことで、この経路でも長音として扱われるようにする 20260820
	*/
	std::wstring NormalizeHyphenToChoonpu( const std::wstring& s )
	{
		std::wstring	sRet( s );
		for( size_t i = 0; i < sRet.size(); ++i ){
			if( L'-' == sRet[i] ){ sRet[i] = L'ー'; }
		}
		return sRet;
	}

	//! cがCJK統合漢字(基本範囲)かどうか
	bool IsKanjiChar( wchar_t c )
	{
		return 0x4E00 <= c && c <= 0x9FFF;
	}

	/*! sQuery中の漢字1文字ごとに、sText中にその漢字自体が(1文字でも)含まれているかを
		調べる。漢字が1つも含まれていなければ何もチェックせずtrueを返す(ローマ字のみの
		クエリには影響しない)。

		漢字ヒューリスティックは「読みの先頭モーラ」だけを見る粗い変換のため、これを
		通さずに素の文字同士で比較すると、単独の漢字がその読みの短いローマ字表記
		(例:「右」がう行の読みなら"u"の1文字)にまで縮んでしまい、その漢字を
		含まない無関係な文字列("UTF-8"等、たまたま"u"を含むだけのもの)にまで
		ヒットしてしまう。IME等で漢字そのものを直接入力した場合は、その漢字自体を
		含む項目だけに絞り込めた方が期待通りになるため、読みによるあいまい一致とは
		別に必須条件として課す 20260821
	*/
	bool ContainsAllQueryKanji( const std::wstring& sQuery, const std::wstring& sText )
	{
		for( size_t i = 0; i < sQuery.size(); ++i ){
			wchar_t	c = sQuery[i];
			if( IsKanjiChar( c ) && std::wstring::npos == sText.find( c ) ){
				return false;
			}
		}
		return true;
	}

#ifdef NKMM_COMMAND_PALETTE_ROMAJI_KANJI
	// ============================================================
	// 漢字ヒューリスティックの事前展開
	// ============================================================
	/*! 送り仮名付きの言葉で、1漢字が複数モーラ分の読みを持つもの(例: 「開く」の
		「開」はひ+らの2モーラ)を丸ごと展開するためのテーブル。

		候補漢字テーブル(CKanjiReadingDict)は「1漢字=読みの先頭1モーラ」という
		単純な行分けのため、"hira"のように2モーラ分をまとめて入力された場合、
		2モーラ目("ra")に対応する文字が本文中に別途無いと一致できない
		(「開く」は開+くの2文字しかなく、らに相当する文字が無い)。このテーブルは
		そうした語だけの例外的な補完で、漢字1文字から複数モーラ分のひらがな読みを
		引けるようにする。網羅的ではなく、メニュー文言で気づいたものを都度追加していく
		方針(気づいたら足す) 20260820
	*/
	struct MultiMoraKanjiEntry {
		wchar_t			cKanji;
		const wchar_t*	pszReading;	// ひらがな、2モーラ以上
	};

	const MultiMoraKanjiEntry	g_aMultiMoraKanjiTable[] = {
		{ L'開', L"ひら" },	// 開く
		{ L'戻', L"もど" },	// 戻す・戻る
		{ L'探', L"さが" },	// 探す
		{ L'隠', L"かく" },	// 隠す
		{ L'異', L"こと" },	// 異なる
		{ L'返', L"かえ" },	// 返す・返し
		{ L'選', L"えら" },	// 選ぶ・選んで
		{ L'合', L"あわ" },	// 合わせる
		{ L'違', L"ちが" },	// 違う・違います
		{ L'使', L"つか" },	// 使う・使えません
		{ L'上', L"うわ" },	// 上書き
		{ L'直', L"なお" },	// 直す
		{ L'検', L"けん" },	// 検索(単独モーラ辞書はけ行止まりで「けん」まで届かないため)
		{ L'索', L"さく" },	// 検索(同上。「さく」まで)
		{ L'右', L"みぎ" },	// 右移動・右クリックメニュー等(送り仮名の無い単独語)
		{ L'左', L"ひだり" },	// 左移動・左クリックメニュー等(同上)
		{ L'下', L"した" },	// 下移動・下検索等(同上)
		{ L'前', L"まえ" },	// 前の検索・前へ等(同上。「前方」等の音読み「ぜん」は元々
							// 単独モーラ辞書でも拾えていなかったため、この追加による
							// 悪化はない 20260821)
	};

	const wchar_t* FindMultiMoraReadingByKanji( wchar_t cKanji )
	{
		for( size_t i = 0; i < _countof( g_aMultiMoraKanjiTable ); ++i ){
			if( g_aMultiMoraKanjiTable[i].cKanji == cKanji ){ return g_aMultiMoraKanjiTable[i].pszReading; }
		}
		return NULL;
	}

	/*! 文字列中の漢字を、分かる範囲でひらがな読みへ事前展開する。

		util/RomajiFuzzyMatch.hppのNormalizeForSearchはひらがな・カタカナの
		表記ゆれは吸収するが、漢字は「読みを知らない文字」としてそのまま素通しする
		(1漢字が複数の読みを持ちうる以上、正規化エンジン単体では読みを決められない
		ため、これは意図された設計)。この関数で事前に「分かる漢字だけ」ひらがなへ
		展開してから正規化エンジンに渡すことで、検索対象・検索クエリ双方にローマ字入力を
		効かせられるようにする。展開できなかった漢字(ヒューリスティック辞書に無いもの)は
		そのまま残り、直接その漢字を含む文字列同士のリテラル一致にのみ寄与する 20260820
	*/
	std::wstring ExpandKanjiReadings( const std::wstring& sText )
	{
		std::wstring	sOut;
		sOut.reserve( sText.size() * 2 );
		for( size_t i = 0; i < sText.size(); ++i ){
			wchar_t	c = sText[i];

			const wchar_t*	pszMultiMora = FindMultiMoraReadingByKanji( c );
			if( NULL != pszMultiMora ){
				sOut += pszMultiMora;
				continue;
			}

			wchar_t	cHira = GetHiraByKanji( c );
			if( L'\0' != cHira ){
				sOut += cHira;
				continue;
			}

			sOut += c;
		}
		return sOut;
	}
#endif // NKMM_COMMAND_PALETTE_ROMAJI_KANJI


	// ============================================================
	// フィルタ欄のライブかな変換(ConvertRomajiToKana)専用のテーブル・処理
	// ============================================================
	// あいまい検索(FuzzyMatchJapanese)とは別の関心事(入力中のローマ字を
	// その場でかな表示に変換するIME風の機能)のため、専用の変換テーブルを持つ。
	// 五十音1〜2文字と、それに対応するローマ字表記(最大3種類)。
	// 2文字表記は拗音(きゃ/しゃ等)・外来語表記の拡張かな(ふぁ/うぇ等)用 20260819
	struct KanaRomaEntry {
		const wchar_t*	pszHira;
		const wchar_t*	aRoma[3];	// 未使用分はNULL
	};

	const KanaRomaEntry	g_aKanaRomaTable[] = {
		{ L"あ", { L"a" } },   { L"い", { L"i" } },   { L"う", { L"u" } },
		{ L"え", { L"e" } },   { L"お", { L"o" } },
		{ L"か", { L"ka" } },  { L"き", { L"ki" } },  { L"く", { L"ku" } },
		{ L"け", { L"ke" } },  { L"こ", { L"ko" } },
		{ L"さ", { L"sa" } },  { L"し", { L"shi", L"si" } }, { L"す", { L"su" } },
		{ L"せ", { L"se" } },  { L"そ", { L"so" } },
		{ L"た", { L"ta" } },  { L"ち", { L"chi", L"ti" } }, { L"つ", { L"tsu", L"tu" } },
		{ L"て", { L"te" } },  { L"と", { L"to" } },
		{ L"な", { L"na" } },  { L"に", { L"ni" } },  { L"ぬ", { L"nu" } },
		{ L"ね", { L"ne" } },  { L"の", { L"no" } },
		{ L"は", { L"ha" } },  { L"ひ", { L"hi" } },  { L"ふ", { L"fu", L"hu" } },
		{ L"へ", { L"he" } },  { L"ほ", { L"ho" } },
		{ L"ま", { L"ma" } },  { L"み", { L"mi" } },  { L"む", { L"mu" } },
		{ L"め", { L"me" } },  { L"も", { L"mo" } },
		{ L"や", { L"ya" } },  { L"ゆ", { L"yu" } },  { L"よ", { L"yo" } },
		{ L"ら", { L"ra" } },  { L"り", { L"ri" } },  { L"る", { L"ru" } },
		{ L"れ", { L"re" } },  { L"ろ", { L"ro" } },
		{ L"わ", { L"wa" } },  { L"を", { L"wo" } },  { L"ん", { L"n" } },
		{ L"が", { L"ga" } },  { L"ぎ", { L"gi" } },  { L"ぐ", { L"gu" } },
		{ L"げ", { L"ge" } },  { L"ご", { L"go" } },
		{ L"ざ", { L"za" } },  { L"じ", { L"ji", L"zi" } },  { L"ず", { L"zu" } },
		{ L"ぜ", { L"ze" } },  { L"ぞ", { L"zo" } },
		{ L"だ", { L"da" } },  { L"ぢ", { L"ji", L"di" } },  { L"づ", { L"zu", L"du" } },
		{ L"で", { L"de" } },  { L"ど", { L"do" } },
		{ L"ば", { L"ba" } },  { L"び", { L"bi" } },  { L"ぶ", { L"bu" } },
		{ L"べ", { L"be" } },  { L"ぼ", { L"bo" } },
		{ L"ぱ", { L"pa" } },  { L"ぴ", { L"pi" } },  { L"ぷ", { L"pu" } },
		{ L"ぺ", { L"pe" } },  { L"ぽ", { L"po" } },
		{ L"ゃ", { L"ya" } },  { L"ゅ", { L"yu" } },  { L"ょ", { L"yo" } },

		// 拗音(清音)
		{ L"きゃ", { L"kya" } }, { L"きゅ", { L"kyu" } }, { L"きょ", { L"kyo" } },
		{ L"しゃ", { L"sha", L"sya" } }, { L"しゅ", { L"shu", L"syu" } }, { L"しょ", { L"sho", L"syo" } },
		{ L"ちゃ", { L"cha", L"tya" } }, { L"ちゅ", { L"chu", L"tyu" } }, { L"ちょ", { L"cho", L"tyo" } },
		{ L"にゃ", { L"nya" } }, { L"にゅ", { L"nyu" } }, { L"にょ", { L"nyo" } },
		{ L"ひゃ", { L"hya" } }, { L"ひゅ", { L"hyu" } }, { L"ひょ", { L"hyo" } },
		{ L"みゃ", { L"mya" } }, { L"みゅ", { L"myu" } }, { L"みょ", { L"myo" } },
		{ L"りゃ", { L"rya" } }, { L"りゅ", { L"ryu" } }, { L"りょ", { L"ryo" } },

		// 拗音(濁音・半濁音)
		{ L"ぎゃ", { L"gya" } }, { L"ぎゅ", { L"gyu" } }, { L"ぎょ", { L"gyo" } },
		{ L"じゃ", { L"ja", L"zya", L"jya" } }, { L"じゅ", { L"ju", L"zyu", L"jyu" } }, { L"じょ", { L"jo", L"zyo", L"jyo" } },
		{ L"ぢゃ", { L"dya" } }, { L"ぢゅ", { L"dyu" } }, { L"ぢょ", { L"dyo" } },
		{ L"びゃ", { L"bya" } }, { L"びゅ", { L"byu" } }, { L"びょ", { L"byo" } },
		{ L"ぴゃ", { L"pya" } }, { L"ぴゅ", { L"pyu" } }, { L"ぴょ", { L"pyo" } },

		// 外来語表記の拡張かな(JIS X 4063相当)
		{ L"いぇ", { L"ye" } },
		{ L"うぃ", { L"wi" } }, { L"うぇ", { L"we" } },
		{ L"ゔぁ", { L"va" } }, { L"ゔぃ", { L"vi" } }, { L"ゔ",  { L"vu" } },
		{ L"ゔぇ", { L"ve" } }, { L"ゔぉ", { L"vo" } },
		{ L"しぇ", { L"she" } }, { L"じぇ", { L"je" } }, { L"ちぇ", { L"che" } },
		{ L"つぁ", { L"tsa" } }, { L"つぃ", { L"tsi" } }, { L"つぇ", { L"tse" } }, { L"つぉ", { L"tso" } },
		{ L"てぃ", { L"ti" } }, { L"でぃ", { L"di" } },
		{ L"とぅ", { L"tu" } }, { L"どぅ", { L"du" } },
		{ L"ふぁ", { L"fa" } }, { L"ふぃ", { L"fi" } }, { L"ふぇ", { L"fe" } },
		{ L"ふぉ", { L"fo" } }, { L"ふゅ", { L"fyu" } },
	};

	bool IsAsciiUpper( wchar_t c ){ return c >= L'A' && c <= L'Z'; }
	bool IsAsciiLower( wchar_t c ){ return c >= L'a' && c <= L'z'; }

	wchar_t ToLowerAscii( wchar_t c ){ return IsAsciiUpper( c ) ? (wchar_t)( c - L'A' + L'a' ) : c; }

	std::wstring ToLowerAsciiStr( const std::wstring& s )
	{
		std::wstring	sRet( s );
		for( size_t i = 0; i < sRet.size(); ++i ){ sRet[i] = ToLowerAscii( sRet[i] ); }
		return sRet;
	}

	//! ローマ字文字列(小文字)→対応しうるエントリの逆引きテーブル。KANA_ROMA_MAPを反転したもの
	struct RomaToHiraMap {
		std::map<std::wstring, std::vector<const KanaRomaEntry*> >	mapRomaToEntries;
		std::vector<std::wstring>										vKeysDesc;	// トークン化で最長一致優先するため長い順

		RomaToHiraMap()
		{
			for( size_t i = 0; i < _countof( g_aKanaRomaTable ); ++i ){
				const KanaRomaEntry&	entry = g_aKanaRomaTable[i];
				for( int j = 0; j < 3 && NULL != entry.aRoma[j]; ++j ){
					std::wstring	sRoma( entry.aRoma[j] );
					bool	bAllAsciiLower = !sRoma.empty();
					for( size_t k = 0; k < sRoma.size(); ++k ){
						if( !IsAsciiLower( sRoma[k] ) ){ bAllAsciiLower = false; break; }
					}
					if( !bAllAsciiLower ){ continue; }
					mapRomaToEntries[sRoma].push_back( &entry );
				}
			}
			for( std::map<std::wstring, std::vector<const KanaRomaEntry*> >::const_iterator it = mapRomaToEntries.begin();
				it != mapRomaToEntries.end(); ++it ){
				vKeysDesc.push_back( it->first );
			}
			std::sort( vKeysDesc.begin(), vKeysDesc.end(),
				[]( const std::wstring& a, const std::wstring& b ){ return a.size() > b.size(); } );
		}
	};

	const RomaToHiraMap& GetRomaToHiraMap()
	{
		static RomaToHiraMap	s_map;
		return s_map;
	}

	//! 促音(っ)の判定対象になりうる子音字かどうか(母音・nを除くASCII子音字)
	bool IsGeminationConsonant( wchar_t c )
	{
		if( !IsAsciiLower( c ) ){ return false; }
		switch( c ){
		case L'a': case L'i': case L'u': case L'e': case L'o': case L'n':
			return false;
		default:
			return true;
		}
	}

	/*! クエリ文字列を、ローマ字の「もじ単位」(例: "sa"→さ、"sha"→しゃ)とそれ以外の1文字
		(かな・カナ・漢字・記号等)に分割する。最長一致優先で、"sha"を's'+'h'+'a'等に
		バラさず1トークンとして扱う。子音字が連続している箇所("kekka"の"kk"等)は
		促音(っ)として1トークンにし、残りの子音から次のもじ単位を続ける 20260819
	*/
	std::vector<std::wstring> TokenizeQuery( const std::wstring& sQuery )
	{
		const RomaToHiraMap&	map = GetRomaToHiraMap();
		std::wstring	sLower = ToLowerAsciiStr( sQuery );
		std::vector<std::wstring>	vTokens;

		size_t	i = 0;
		while( i < sQuery.size() ){
			if( i + 1 < sQuery.size() && sLower[i] == sLower[i + 1] && IsGeminationConsonant( sLower[i] ) ){
				vTokens.push_back( L"っ" );
				i += 1;
				continue;
			}

			size_t	nMatchedLen = 0;
			for( size_t k = 0; k < map.vKeysDesc.size(); ++k ){
				const std::wstring&	sKey = map.vKeysDesc[k];
				if( 0 == sLower.compare( i, sKey.size(), sKey ) ){
					nMatchedLen = sKey.size();
					break;
				}
			}
			if( 0 < nMatchedLen ){
				vTokens.push_back( sQuery.substr( i, nMatchedLen ) );
				i += nMatchedLen;
			}else{
				std::wstring	sTok = sQuery.substr( i, 1 );
				// 直前がバラの"n"(ん確定)で今回もバラの"n"になった場合("nn"の2文字目が
				// 母音等に続けられずそれ単体で「ん」にしかならなかった場合)は、"nn"を
				// 明示的な1つの「ん」の確定とみなし重複させない。"konnichiwa"のように
				// 2文字目が母音等に続けられる場合は上のキー一致で"na"等の2文字トークンに
				// なるためこの分岐には来ず、影響しない 20260819
				if( !vTokens.empty() && 1 == vTokens.back().size() && L'n' == ToLowerAscii( vTokens.back()[0] )
				 && L'n' == ToLowerAscii( sTok[0] ) ){
					i += 1;
					continue;
				}
				vTokens.push_back( sTok );
				i += 1;
			}
		}
		return vTokens;
	}

}  // namespace


bool FuzzyMatchJapanese( const std::wstring& sQuery, const std::wstring& sText, int* pOutScore )
{
	if( sQuery.empty() ){
		if( NULL != pOutScore ){ *pOutScore = 0; }
		return true;
	}

	// クエリに漢字が含まれる場合、その漢字自体が検索対象に含まれていることを必須にする。
	// 漢字ヒューリスティックは「読みの先頭モーラ」だけの粗い変換のため、素通しすると
	// 例えば単独の「右」がその漢字を含まない無関係な文字列("UTF-8"等、たまたま
	// 読みの短いローマ字表記と一致してしまうもの)にまでヒットしてしまう。
	// IME等で漢字そのものを直接入力した場合は、素直にその漢字を含む項目だけに
	// 絞り込めた方が期待通りの挙動になる 20260821
	if( !ContainsAllQueryKanji( sQuery, sText ) ){
		return false;
	}

	std::wstring	sQueryExpanded = NormalizeHyphenToChoonpu( sQuery );
	std::wstring	sTextExpanded  = NormalizeHyphenToChoonpu( sText );
#ifdef NKMM_COMMAND_PALETTE_ROMAJI_KANJI
	// 漢字ヒューリスティック: 分かる範囲で漢字をひらがな読みへ展開してから正規化する。
	// クエリ側にも同じ展開をかけるのは、IME等で漢字そのものを直接入力された場合でも
	// (展開後の表記が一致するので)引き続き一致できるようにするため 20260820
	sQueryExpanded = ExpandKanjiReadings( sQueryExpanded );
	sTextExpanded  = ExpandKanjiReadings( sTextExpanded );
#endif // NKMM_COMMAND_PALETTE_ROMAJI_KANJI

	fuzzy::NormalizedText	pattern = fuzzy::NormalizeForSearch( WStrToUtf8( sQueryExpanded ) );
	fuzzy::NormalizedText	target  = fuzzy::NormalizeForSearch( WStrToUtf8( sTextExpanded ) );

	/*	util/RomajiFuzzyMatch.hppのFuzzyMatchNormalized()(部分列マッチ+ギャップ減点)は
		そのままだと「間が空いていても順序さえ合っていれば一致」を許してしまう。
		漢字ヒューリスティックで1漢字が複数文字のローマ字に展開されることもあり、
		無関係な別の漢字の読みへギャップ越しにまたがって一致してしまう実例が
		繰り返し見つかった:
		  - ">kensaku"が「(矩形選択)単語の左端に移動」にヒット(「選」が複数モーラ
		    読みテーブルで強制的に"えら"に展開され、遠く離れた文字と繋がっていた)
		  - ">shu"が「小文字」にヒット(小→"shi"の"sh"から、間の"i"を飛ばして
		    隣の文字→"fu"の"u"へ食い込んでいた。1文字ずつなら別々の漢字の読み)
		ギャップの許容量を数値で細かく調整しても同種の偶然一致が次々見つかるため、
		方式そのものを変更した: パターンは正規化後の文字列(normalized)に対して
		「連続した部分文字列」として現れることを必須にする(出現位置そのものは
		文字列中のどこでもよい)。これなら異なる漢字の読み同士が偶然繋がって
		一致することはなくなる。そのぶんVSCode風の離れた頭文字だけのマッチ
		(例: "gc"→"GetChanges")はできなくなるが、このパレットで実際に必要に
		なった例(kensaku/modosu/migi/hidari/shita/mae/doro/kopi-等)はすべて
		「正規化後の文字列に連続して現れる」ケースだったため支障はない 20260821
	*/
	size_t	nPos = target.normalized.find( pattern.normalized );
	if( std::string::npos == nPos ){ return false; }

	if( NULL != pOutScore ){
		// 出現位置が先頭に近いほど、対象文字列全体が短く無駄が少ないほど高スコア
		int	nScore = 0;
		nScore -= (int)nPos;
		nScore -= (int)( target.normalized.size() - pattern.normalized.size() );
		if( 0 == nPos ){ nScore += 50; }
		*pOutScore = nScore;
	}
	return true;
}


std::wstring ConvertRomajiToKana( const std::wstring& sText )
{
	const RomaToHiraMap&	map = GetRomaToHiraMap();
	std::vector<std::wstring>	vTokens = TokenizeQuery( sText );

	std::wstring	sResult;
	sResult.reserve( sText.size() );
	for( size_t i = 0; i < vTokens.size(); ++i ){
		const std::wstring&	sToken = vTokens[i];
		if( 1 == sToken.size() && L'-' == sToken[0] ){
			sResult += L'ー';
			continue;
		}
		std::wstring	sLower = ToLowerAsciiStr( sToken );
		std::map<std::wstring, std::vector<const KanaRomaEntry*> >::const_iterator	it = map.mapRomaToEntries.find( sLower );
		if( map.mapRomaToEntries.end() != it && !it->second.empty() ){
			// 確定できたモーラ(拗音等の2文字表記含む)はかなへ変換する
			sResult += it->second[0]->pszHira;
		}else{
			// まだ1モーラに満たない末尾の断片や、促音「っ」・かな・記号等はそのまま残す
			sResult += sToken;
		}
	}
	return sResult;
}

#endif // NKMM_COMMAND_PALETTE_ROMAJI
