/*!	@file
	@brief VBScript風マクロ(.vbs)をJavaScriptへ変換するトランスパイラ

	WSH(VBScript)が無い/使えない環境でも、VBScript風の構文を書けば
	QuickJSマクロエンジン(CQuickJSMacroMgr)がそのまま実行できるJavaScript
	ソースへヘッダオンリーで変換する。設計・全体構成は[[CPasToJsTranspiler.h]]
	(Pascal風マクロ用の姉妹トランスパイラ)にならい、字句解析(tokenize)→
	パースしながらJS文字列を組み立てる再帰下降パーサー、という構成を踏襲する。
	関数呼び出し(例: InsText(s))はそのまま同名のJS関数呼び出しとして出力する
	ため、CQuickJSIfObjBinderがグローバルへ登録するEditor系関数(修飾無し
	呼び出し)をVBScript風のコードから直接呼べる。

	--- 対応範囲(実用的なサブセット) ------------------------------------
	Dim/ReDim(1・2次元配列を含む)、代入/Set、If/ElseIf/Else/End If(単一行
	形式も含む)、For(To/Step)/For Each、Do/Loop(While/Until前置・後置)、
	While/Wend、Sub/Function(戻り値は関数名への代入で表現)、Call、
	Exit Sub/Function/For/Do、Const、コメント(' および Rem)、行継続(_)、
	四則演算・文字列連結(&)・比較・論理演算子(And/Or/Not/Xor)、
	True/False/Nothing/Null/Emptyリテラル。

	クラス(Class ... End Class)・COM/ActiveXオブジェクト(CreateObject等)・
	行ラベル/GoTo・On Error Resume Next の実際のエラー握りつぶし動作は非対応
	(詳細はファイル末尾の既知の制限、および[[changelog/NKMM_FIX_VBS_MACRO.md]]
	を参照)。

	--- VBScript特有の設計判断 -------------------------------------------
	- VBScriptは大文字小文字を区別しないが、[[CPasToJsTranspiler.h]]と同様に
	  字句をそのままJSへ出力するだけで正規化はしない(宣言と参照で表記が
	  食い違うとJS側でReferenceErrorになる、既知の制限)。
	- 識別子はアンダースコアを2文字目以降に含められる(例: S_InsText、
	  APP_NAME)。VBScriptの行継続記号"_"(行末の空白の後に置く)と識別子中の
	  '_'は、字句解析の「英字で始まる識別子は英数字と'_'を続けて読む」
	  というルールにより曖昧無く区別できる(識別子スキャン中に現れた'_'は
	  常に識別子の一部として読み進められ、行継続用の判定分岐まで落ちて
	  来るのは、識別子の外側=独立した位置に現れた'_'だけになるため)。
	- "="はVBScriptでは代入にも等価比較にも使われる(Pascalの":="のような
	  専用の代入演算子が無い)。文の先頭で「識別子の直後に"="が来たら代入」と
	  parseStatement側で先に判定し、それ以外(式の内部)は全てparseRelational
	  が等価比較として解釈する、という位置(文脈)による判定で対応する。
	- 配列要素アクセス`a(i)`と関数/Sub呼び出し`f(args)`はVBScript上は同じ
	  "識別子(...)"の構文で区別が付かない。Dim/ReDimで宣言済みの配列名を
	  m_arrayNamesに記録しておき、参照時にそこへ含まれるかどうかで
	  `a[i]`(配列アクセス)と`f(args)`(呼び出し)を出し分ける。単一パスの
	  変換のため、配列の宣言(Dim)が使用箇所より前に書かれている前提。
	- Function の戻り値はVBScriptでは「関数名自身への代入」で表現するが、
	  JS側でも同名のローカル変数(let 関数名)を関数本体内に作ると、その
	  時点で外側のfunction宣言への束縛がシャドウされ、再帰呼び出しが
	  自分自身(ローカル変数、未定義)を呼ぼうとして壊れる。これを避ける
	  ため、戻り値は常に予約名`__ret`という別変数に持たせ、関数名への
	  代入/参照だけをパース時に`__ret`へ機械的に読み替える(ResolveVarName)。

	@date 2026 NKMM_FIX_VBS_MACRO
*/
#ifndef SAKURA_CVBSTOJSTRANSPILER_9C2E4F1A_6B3D_4E8C_A1F5_2D9B6E4C8A3F_H_
#define SAKURA_CVBSTOJSTRANSPILER_9C2E4F1A_6B3D_4E8C_A1F5_2D9B6E4C8A3F_H_

#ifdef NKMM_FIX_VBS_MACRO

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <stdexcept>

//!	VBScript風マクロ言語 → JavaScript トランスパイラ
class CVbsToJsTranspiler {
public:
	//!	構文解析エラー。[[CPasToJsTranspiler::ParseError]]と同じ役割
	//!	(想定外のトークンでparseStatement/parsePrimaryが1トークンも消費
	//!	せず無限ループするのを防ぐため、フォールバック位置では必ずこれを
	//!	投げる)。
	struct ParseError : std::runtime_error {
		explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
	};

	enum class TokenType {
		End_Of_File,
		Identifier,
		Number,
		StringLit,
		Newline,	//!< 文の区切り。VBScriptは改行(または':')が文の区切りになるため、
					//!< Pascal版には無いトークン種別として持つ
		//	キーワード
		KwDim, KwReDim, KwPreserve,
		KwSub, KwFunction, KwEnd,
		KwIf, KwThen, KwElse, KwElseIf,
		KwFor, KwTo, KwStep, KwNext, KwEach, KwIn,
		KwDo, KwWhile, KwUntil, KwLoop, KwWend,
		KwAnd, KwOr, KwNot, KwXor, KwMod, KwIs,
		KwTrue, KwFalse, KwNothing, KwNull, KwEmpty,
		KwCall, KwExit, KwSet, KwConst,
		KwByVal, KwByRef, KwOption,
		KwOn, KwError, KwResume, KwGoTo,
		KwPublic, KwPrivate,
		//	記号・演算子
		Assign,     // =  (文脈により代入/等価比較の両方に使われる)
		NotEqual,   // <>
		Less,       // <
		LessEq,     // <=
		Greater,    // >
		GreaterEq,  // >=
		Plus,       // +
		Minus,      // -
		Multiply,   // *
		Divide,     // /
		IntDivide,  // \  整数除算
		Power,      // ^
		Concat,     // &  文字列連結
		Comma,      // ,
		Colon,      // :  文の区切り(改行と等価)
		LParen,     // (
		RParen,     // )
	};

	struct Token {
		TokenType type;
		std::string text;
		int line;
	};

private:
	std::string m_src;
	size_t m_pos = 0;
	int m_line = 1;
	std::vector<Token> m_tokens;
	size_t m_tokenIndex = 0;

	//!	Dim/ReDimで宣言済みの配列名(小文字化して保持)。"識別子(...)"が
	//!	配列要素アクセスか関数/Sub呼び出しかを判定するために使う
	std::set<std::string> m_arrayNames;
	//!	現在パース中のFunction名(小文字化)。空文字列ならFunctionの外
	//!	(Sub/トップレベル)。関数名自身への代入/参照を"__ret"へ読み替える
	//!	ために使う
	std::string m_currentFuncName;
	//!	Forループごとに一意なステップ用一時変数名を作るための連番
	int m_forCounter = 0;

	static std::string toLower(const std::string& str) {
		std::string s = str;
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
			return std::tolower(c);
		});
		return s;
	}

	//	呼び出し名の"S_"接頭辞(大文字小文字区別無し)をサクラエディタのマクロ
	//	関数呼び出しの目印として扱う。[[CPasToJsTranspiler::ToJsCallName]]と
	//	同じ規約。加えて、VBScript組み込み関数のうちJSの組み込み(Array/String)
	//	と同名になるものは、生成コード側の内部利用(new Array(...)等)と衝突
	//	しないよう専用のランタイム関数名へ読み替える
	static std::string ToJsCallName(const std::string& name) {
		std::string lower = toLower(name);
		if (lower == "array") return "__vbsArray";
		if (lower == "string") return "__vbsStringRepeat";
		if (name.size() > 2 && (name[0] == 'S' || name[0] == 's') && name[1] == '_') {
			return "Editor." + name.substr(2);
		}
		return name;
	}

	//	========================================================================
	//	1. 字句解析 (Lexer)
	//	========================================================================
	//!	各分岐の判定順序に意味がある([[CPasToJsTranspiler::tokenize]]と同じ
	//	方針)。空白/改行/コメント/行継続 → 16進数(&H)/8進数(&O)リテラル →
	//	数値 → 文字列リテラル → 識別子/キーワード → 2文字演算子(<> <= >=) →
	//	1文字演算子、の順で「もっとも長く・具体的にマッチするパターン」を
	//	先に判定する。
	void tokenize() {
		m_tokens.clear();
		m_pos = 0;
		m_line = 1;

		while (m_pos < m_src.size()) {
			char c = m_src[m_pos];

			if (c == '\r') { m_pos++; continue; } // CRLFのCRは無視(直後のLFで改行扱いする)
			if (c == '\n') {
				m_tokens.push_back({TokenType::Newline, "\n", m_line});
				m_line++;
				m_pos++;
				continue;
			}
			if (c == ' ' || c == '\t') { m_pos++; continue; }

			//	行コメント ' (アポストロフィ)
			if (c == '\'') {
				while (m_pos < m_src.size() && m_src[m_pos] != '\n') m_pos++;
				continue;
			}

			//	行継続 "_" (行末の空白の後、改行の直前に置く)
			//	識別子中の'_'は既に上の識別子スキャンで消費されているため、
			//	ここに来る'_'は識別子の一部ではない"独立した"'_'のみ
			//	(例: "S_InsText"の'_'は識別子スキャン側で処理されるが、
			//	"1 + 2 + _<改行>3"のような演算子直後の'_'はここに来る)
			if (c == '_') {
				size_t p = m_pos + 1;
				while (p < m_src.size() && (m_src[p] == ' ' || m_src[p] == '\t')) p++;
				if (p < m_src.size() && m_src[p] == '\r') p++;
				if (p < m_src.size() && m_src[p] == '\n') {
					m_pos = p + 1;
					m_line++;
					continue; // 改行トークンを出さずに次の物理行へ継続する
				}
				m_pos++; // 行末以外の'_'は寛容に読み飛ばす
				continue;
			}

			//	16進数リテラル &H.. / 8進数リテラル &O..
			if (c == '&' && m_pos + 1 < m_src.size() && (m_src[m_pos + 1] == 'H' || m_src[m_pos + 1] == 'h')) {
				m_pos += 2;
				std::string hex = "0x";
				while (m_pos < m_src.size() && std::isxdigit(static_cast<unsigned char>(m_src[m_pos]))) hex += m_src[m_pos++];
				m_tokens.push_back({TokenType::Number, hex, m_line});
				continue;
			}
			if (c == '&' && m_pos + 1 < m_src.size() && (m_src[m_pos + 1] == 'O' || m_src[m_pos + 1] == 'o')) {
				m_pos += 2;
				std::string oct = "0o";
				while (m_pos < m_src.size() && m_src[m_pos] >= '0' && m_src[m_pos] <= '7') oct += m_src[m_pos++];
				m_tokens.push_back({TokenType::Number, oct, m_line});
				continue;
			}

			//	数値(先頭が数字、または".5"のような先頭ドットの小数)
			if (std::isdigit(static_cast<unsigned char>(c)) ||
			    (c == '.' && m_pos + 1 < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos + 1])))) {
				std::string num;
				bool dotSeen = false;
				while (m_pos < m_src.size()) {
					char cc = m_src[m_pos];
					if (std::isdigit(static_cast<unsigned char>(cc))) { num += cc; m_pos++; continue; }
					if (cc == '.' && !dotSeen) { dotSeen = true; num += cc; m_pos++; continue; }
					break;
				}
				//	指数表記 (1.5e10, 1E+5 等)
				if (m_pos < m_src.size() && (m_src[m_pos] == 'e' || m_src[m_pos] == 'E')) {
					size_t save = m_pos;
					std::string exp;
					exp += m_src[m_pos++];
					if (m_pos < m_src.size() && (m_src[m_pos] == '+' || m_src[m_pos] == '-')) exp += m_src[m_pos++];
					bool hasDigit = false;
					while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) { exp += m_src[m_pos++]; hasDigit = true; }
					if (hasDigit) num += exp; else m_pos = save;
				}
				m_tokens.push_back({TokenType::Number, num, m_line});
				continue;
			}

			//	文字列リテラル "..." (VBScript形式: ""でエスケープ)
			if (c == '"') {
				m_pos++;
				std::string s = "\"";
				while (m_pos < m_src.size()) {
					if (m_src[m_pos] == '"') {
						if (m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '"') {
							s += "\\\"";
							m_pos += 2;
						} else {
							m_pos++;
							break;
						}
					} else if (m_src[m_pos] == '\\') {
						s += "\\\\";
						m_pos++;
					} else if (m_src[m_pos] == '\n') {
						//	VBScriptの文字列リテラルは本来改行をまたげないが、
						//	防御的に許容してJSの\nへ変換しておく
						s += "\\n";
						m_line++;
						m_pos++;
					} else {
						s += m_src[m_pos++];
					}
				}
				s += "\"";
				m_tokens.push_back({TokenType::StringLit, s, m_line});
				continue;
			}

			//	識別子 / キーワード (先頭は英字のみ。2文字目以降は英数字と'_'を許容する。
			//	行継続記号の'_'は、識別子の続きとして読まれなかった"独立した"'_'
			//	(=このalpha分岐に入らず、後述の'_'分岐まで落ちてくる場合)としてのみ
			//	検出されるため、"S_InsText"のような識別子中の'_'と曖昧にならない)
			if (std::isalpha(static_cast<unsigned char>(c))) {
				std::string ident;
				while (m_pos < m_src.size() && (std::isalnum(static_cast<unsigned char>(m_src[m_pos])) || m_src[m_pos] == '_')) {
					ident += m_src[m_pos++];
				}
				std::string lower = toLower(ident);

				//	"Rem"は行コメント(単語全体としてこの綴りの識別子のときのみ)
				if (lower == "rem") {
					while (m_pos < m_src.size() && m_src[m_pos] != '\n') m_pos++;
					continue;
				}

				TokenType t = TokenType::Identifier;
				if (lower == "dim") t = TokenType::KwDim;
				else if (lower == "redim") t = TokenType::KwReDim;
				else if (lower == "preserve") t = TokenType::KwPreserve;
				else if (lower == "sub") t = TokenType::KwSub;
				else if (lower == "function") t = TokenType::KwFunction;
				else if (lower == "end") t = TokenType::KwEnd;
				else if (lower == "if") t = TokenType::KwIf;
				else if (lower == "then") t = TokenType::KwThen;
				else if (lower == "else") t = TokenType::KwElse;
				else if (lower == "elseif") t = TokenType::KwElseIf;
				else if (lower == "for") t = TokenType::KwFor;
				else if (lower == "to") t = TokenType::KwTo;
				else if (lower == "step") t = TokenType::KwStep;
				else if (lower == "next") t = TokenType::KwNext;
				else if (lower == "each") t = TokenType::KwEach;
				else if (lower == "in") t = TokenType::KwIn;
				else if (lower == "do") t = TokenType::KwDo;
				else if (lower == "while") t = TokenType::KwWhile;
				else if (lower == "until") t = TokenType::KwUntil;
				else if (lower == "loop") t = TokenType::KwLoop;
				else if (lower == "wend") t = TokenType::KwWend;
				else if (lower == "and") t = TokenType::KwAnd;
				else if (lower == "or") t = TokenType::KwOr;
				else if (lower == "not") t = TokenType::KwNot;
				else if (lower == "xor") t = TokenType::KwXor;
				else if (lower == "mod") t = TokenType::KwMod;
				else if (lower == "is") t = TokenType::KwIs;
				else if (lower == "true") t = TokenType::KwTrue;
				else if (lower == "false") t = TokenType::KwFalse;
				else if (lower == "nothing") t = TokenType::KwNothing;
				else if (lower == "null") t = TokenType::KwNull;
				else if (lower == "empty") t = TokenType::KwEmpty;
				else if (lower == "call") t = TokenType::KwCall;
				else if (lower == "exit") t = TokenType::KwExit;
				else if (lower == "set") t = TokenType::KwSet;
				else if (lower == "const") t = TokenType::KwConst;
				else if (lower == "byval") t = TokenType::KwByVal;
				else if (lower == "byref") t = TokenType::KwByRef;
				else if (lower == "option") t = TokenType::KwOption;
				else if (lower == "on") t = TokenType::KwOn;
				else if (lower == "error") t = TokenType::KwError;
				else if (lower == "resume") t = TokenType::KwResume;
				else if (lower == "goto") t = TokenType::KwGoTo;
				else if (lower == "public") t = TokenType::KwPublic;
				else if (lower == "private") t = TokenType::KwPrivate;

				m_tokens.push_back({t, ident, m_line});
				continue;
			}

			//	2文字演算子
			if (c == '<' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '>') {
				m_tokens.push_back({TokenType::NotEqual, "<>", m_line});
				m_pos += 2;
				continue;
			}
			if (c == '<' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '=') {
				m_tokens.push_back({TokenType::LessEq, "<=", m_line});
				m_pos += 2;
				continue;
			}
			if (c == '>' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '=') {
				m_tokens.push_back({TokenType::GreaterEq, ">=", m_line});
				m_pos += 2;
				continue;
			}

			//	1文字演算子・記号
			switch (c) {
			case '=': m_tokens.push_back({TokenType::Assign, "=", m_line}); break;
			case '<': m_tokens.push_back({TokenType::Less, "<", m_line}); break;
			case '>': m_tokens.push_back({TokenType::Greater, ">", m_line}); break;
			case '+': m_tokens.push_back({TokenType::Plus, "+", m_line}); break;
			case '-': m_tokens.push_back({TokenType::Minus, "-", m_line}); break;
			case '*': m_tokens.push_back({TokenType::Multiply, "*", m_line}); break;
			case '/': m_tokens.push_back({TokenType::Divide, "/", m_line}); break;
			case '\\': m_tokens.push_back({TokenType::IntDivide, "\\", m_line}); break;
			case '^': m_tokens.push_back({TokenType::Power, "^", m_line}); break;
			case '&': m_tokens.push_back({TokenType::Concat, "&", m_line}); break;
			case ',': m_tokens.push_back({TokenType::Comma, ",", m_line}); break;
			case ':': m_tokens.push_back({TokenType::Colon, ":", m_line}); break;
			case '(': m_tokens.push_back({TokenType::LParen, "(", m_line}); break;
			case ')': m_tokens.push_back({TokenType::RParen, ")", m_line}); break;
			default: break; // 未対応文字は寛容に読み飛ばす
			}
			m_pos++;
		}
		m_tokens.push_back({TokenType::End_Of_File, "", m_line});
	}

	//	========================================================================
	//	2. 構文解析 → JSコード生成
	//	========================================================================
	Token peek() const { return m_tokens[m_tokenIndex]; }
	Token advance() { return m_tokens[m_tokenIndex++]; }
	bool match(TokenType type) {
		if (peek().type == type) { advance(); return true; }
		return false;
	}
	bool isStop(const std::vector<TokenType>& stopTypes) const {
		for (TokenType t : stopTypes) if (peek().type == t) return true;
		return false;
	}
	//!	文の区切り(改行・':')を0個以上読み飛ばす
	void skipSeparators() {
		while (peek().type == TokenType::Newline || peek().type == TokenType::Colon) advance();
	}

	//!	「文がここで終わる(この後に式は続かない)」とみなせるトークンかどうか。
	//!	括弧無しの手続き呼び出し(`InfoMsg msg`のようなVBScript伝統の書き方)で、
	//!	識別子の直後に引数が続くのか、それとも引数無しでここで文が終わるのかを
	//!	判定するために使う
	static bool isStatementBoundary(TokenType t) {
		switch (t) {
		case TokenType::Newline:
		case TokenType::Colon:
		case TokenType::End_Of_File:
		case TokenType::KwElse:
		case TokenType::KwElseIf:
		case TokenType::KwEnd:
		case TokenType::KwNext:
		case TokenType::KwLoop:
		case TokenType::KwWend:
			return true;
		default:
			return false;
		}
	}

	//!	現在Function本体をパース中で、かつ名前がfuncNameと一致するなら
	//!	戻り値保持用のローカル変数名"__ret"へ読み替える。理由はファイル
	//!	冒頭のコメント参照(関数名と同名のletで再帰呼び出しが壊れるのを防ぐ)
	std::string ResolveVarName(const std::string& name) const {
		if (!m_currentFuncName.empty() && toLower(name) == m_currentFuncName) return "__ret";
		return name;
	}

	std::string BuildIndexExpr(const std::string& base, const std::vector<std::string>& indices) const {
		std::string result = base;
		for (const auto& idx : indices) result += "[" + idx + "]";
		return result;
	}

	//!	Dim/ReDimの次元指定(各次元の上限値、0始まりで上限を含む)から、
	//!	初期化済みのJS配列(2次元まではネストした配列)を作る式を組み立てる
	std::string BuildNewArrayExpr(const std::vector<std::string>& bounds) {
		if (bounds.empty()) {
			//	"Dim arr()"のような境界未指定の動的配列宣言。後続の
			//	"ReDim arr(n)"でサイズが決まる前提の空配列にしておく
			return "[]";
		}
		if (bounds.size() == 1) {
			return "new Array((" + bounds[0] + ") + 1).fill(undefined)";
		}
		if (bounds.size() == 2) {
			return "Array.from({ length: (" + bounds[0] + ") + 1 }, function() { return new Array((" + bounds[1] + ") + 1).fill(undefined); })";
		}
		throw ParseError("3次元以上の配列には対応していません (line " + std::to_string(peek().line) + ")");
	}

	//	--- 式(Expression)の変換 -------------------------------------------
	//	VBScriptの演算子優先順位(弱い順): Xor < Or < And < Not < 比較(=<><Is等)
	//	< 連結(&) < 加減算(+ -) < Mod < 整数除算(\) < 乗除算(* /) < 単項(- +)
	//	< べき乗(^、右結合)。[[CPasToJsTranspiler]]と同じ「弱い優先順位の
	//	関数が強い優先順位の関数を呼ぶ」再帰下降による優先順位パース。
	std::string parseExpression() { return parseXor(); }

	std::string parseXor() {
		std::string expr = parseOr();
		while (peek().type == TokenType::KwXor) {
			advance();
			expr = "__vbsXor(" + expr + ", " + parseOr() + ")";
		}
		return expr;
	}
	std::string parseOr() {
		std::string expr = parseAnd();
		while (peek().type == TokenType::KwOr) {
			advance();
			expr = "(" + expr + " || " + parseAnd() + ")";
		}
		return expr;
	}
	std::string parseAnd() {
		std::string expr = parseNot();
		while (peek().type == TokenType::KwAnd) {
			advance();
			expr = "(" + expr + " && " + parseNot() + ")";
		}
		return expr;
	}
	//!	単項Not。二項演算子と違い「まず演算子の有無を見て、あれば剥がして
	//!	自分自身を再帰呼び出しする」形(Not Not xのような入れ子を素直に扱う)
	std::string parseNot() {
		if (peek().type == TokenType::KwNot) {
			advance();
			return "!(" + parseNot() + ")";
		}
		return parseRelational();
	}
	std::string parseRelational() {
		std::string expr = parseConcat();
		while (true) {
			TokenType tt = peek().type;
			if (tt == TokenType::Assign) {
				advance();
				//	式の内部の"="はVBScriptでは等価比較。JSの==は型変換を
				//	伴う緩い比較なので、意味が近い厳密等価(===)へ変換する
				expr = "(" + expr + " === " + parseConcat() + ")";
			} else if (tt == TokenType::NotEqual) {
				advance();
				expr = "(" + expr + " !== " + parseConcat() + ")";
			} else if (tt == TokenType::Less || tt == TokenType::LessEq ||
			           tt == TokenType::Greater || tt == TokenType::GreaterEq) {
				Token op = advance();
				expr = "(" + expr + " " + op.text + " " + parseConcat() + ")";
			} else if (tt == TokenType::KwIs) {
				advance();
				expr = "(" + expr + " === " + parseConcat() + ")";
			} else {
				break;
			}
		}
		return expr;
	}
	//!	文字列連結(&)。VBScriptの&は被演算子の型に関わらず常に文字列結合
	//!	になるため、両辺を__vbsStr()(Empty/Nullを""に正規化する文字列化
	//	ヘルパー、ランタイム側で定義)で包む
	std::string parseConcat() {
		std::string expr = parseAdditive();
		while (peek().type == TokenType::Concat) {
			advance();
			expr = "(__vbsStr(" + expr + ") + __vbsStr(" + parseAdditive() + "))";
		}
		return expr;
	}
	std::string parseAdditive() {
		std::string expr = parseModOp();
		while (peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
			Token op = advance();
			expr = expr + " " + op.text + " " + parseModOp();
		}
		return expr;
	}
	std::string parseModOp() {
		std::string expr = parseIntDiv();
		while (peek().type == TokenType::KwMod) {
			advance();
			expr = "(" + expr + " % " + parseIntDiv() + ")";
		}
		return expr;
	}
	std::string parseIntDiv() {
		std::string expr = parseMultiplicative();
		while (peek().type == TokenType::IntDivide) {
			advance();
			//	VBScriptの\は整数除算。JSの/は常に浮動小数点除算なので、
			//	結果をMath.truncで切り捨てる([[CPasToJsTranspiler]]のdivと同じ)
			expr = "Math.trunc((" + expr + ") / (" + parseMultiplicative() + "))";
		}
		return expr;
	}
	std::string parseMultiplicative() {
		std::string expr = parseUnary();
		while (peek().type == TokenType::Multiply || peek().type == TokenType::Divide) {
			Token op = advance();
			expr = expr + " " + op.text + " " + parseUnary();
		}
		return expr;
	}
	//!	単項 - / + 。VBScriptでは"-2^2"は"-(2^2)"(べき乗の方が単項マイナス
	//	より強い)と解釈されるため、parsePowerを内側に呼ぶ
	std::string parseUnary() {
		if (peek().type == TokenType::Minus) {
			advance();
			return "-(" + parseUnary() + ")";
		}
		if (peek().type == TokenType::Plus) {
			advance();
			return parseUnary();
		}
		return parsePower();
	}
	//!	べき乗(^、右結合)。指数側は単項マイナスを許すため(2^-2)、右辺は
	//	parsePrimaryではなくparseUnaryを呼ぶ
	std::string parsePower() {
		std::string base = parsePrimary();
		if (peek().type == TokenType::Power) {
			advance();
			return "Math.pow(" + base + ", " + parseUnary() + ")";
		}
		return base;
	}

	//!	識別子の直後の"(...)"を読み、配列要素アクセス(a[i])か関数/Sub
	//!	呼び出し(f(args))かを判定してJSコード片を返す。呼び出し時点で
	//!	'('の手前まで読み進んでいる想定(このメソッドが'('自体を消費する)
	std::string parsePostfixCallOrIndex(const Token& ident) {
		advance(); // '('
		std::vector<std::string> args;
		if (peek().type != TokenType::RParen) {
			args.push_back(parseExpression());
			while (match(TokenType::Comma)) args.push_back(parseExpression());
		}
		match(TokenType::RParen);

		if (m_arrayNames.count(toLower(ident.text))) {
			return BuildIndexExpr(ResolveVarName(ident.text), args);
		}
		std::string callArgs;
		for (size_t i = 0; i < args.size(); i++) callArgs += (i ? ", " : "") + args[i];
		return ToJsCallName(ident.text) + "(" + callArgs + ")";
	}

	//!	式の末端。数値・文字列・真偽/Nothing/Null/Emptyリテラル、変数参照、
	//	関数呼び出し・配列アクセス、"(式)"のいずれか
	std::string parsePrimary() {
		Token t = advance();
		if (t.type == TokenType::Number || t.type == TokenType::StringLit) return t.text;
		if (t.type == TokenType::KwTrue) return "true";
		if (t.type == TokenType::KwFalse) return "false";
		//	Nothing(オブジェクト参照の未設定)とNull(値の不定)はどちらも
		//	JSのnullへ畳み込む。COM/オブジェクトを扱わない本サブセットでは
		//	区別する意味が薄いための割り切り(既知の制限。ファイル末尾参照)
		if (t.type == TokenType::KwNothing) return "null";
		if (t.type == TokenType::KwNull) return "null";
		if (t.type == TokenType::KwEmpty) return "undefined";
		if (t.type == TokenType::Identifier) {
			if (peek().type == TokenType::LParen) return parsePostfixCallOrIndex(t);
			//	TimerはVBScriptでは括弧無しの"プロパティ風"参照で呼び出せる
			//	組み込み関数(引数を取らない)。他の識別子と同様にただの変数参照
			//	として扱うと、呼び出しではなく関数オブジェクトそのものを参照して
			//	しまうため、ここだけ特別に呼び出し式へ変換する。
			//	(Now/Date/TimeはJSのグローバルDateコンストラクタと名前が衝突する
			//	ため、Array/Stringと同様の理由で今は対象に含めていない)
			if (!m_arrayNames.count(toLower(t.text)) && toLower(t.text) == "timer") {
				return ResolveVarName(t.text) + "()";
			}
			return ResolveVarName(t.text);
		}
		if (t.type == TokenType::LParen) {
			std::string expr = parseExpression();
			match(TokenType::RParen);
			return "(" + expr + ")";
		}
		throw ParseError("unexpected token '" + t.text + "' at line " + std::to_string(t.line));
	}

	//	========================================================================
	//	文(Statement)の変換
	//	========================================================================
	//!	stopTypesに含まれるトークンに出会うかEOFに達するまで、文の区切り
	//	(改行/':')を挟みながらparseStatement()を繰り返す。begin/endが無い
	//	VBScriptでは、If/For/Do/Sub/Function等どのブロック構文の本体も
	//	この関数で読む(stopTypesにEnd/Next/Loop/Wend等、そのブロックを
	//	閉じるキーワードを渡す)
	std::string parseBlock(const std::vector<TokenType>& stopTypes) {
		std::ostringstream out;
		skipSeparators();
		while (!isStop(stopTypes) && peek().type != TokenType::End_Of_File) {
			out << parseStatement();
			skipSeparators();
		}
		return out.str();
	}

	//!	単一行If文(`If cond Then stmt : stmt Else stmt`)の本体用。改行を
	//	区切りとして消費してしまうparseBlockと異なり、':'だけを区切りとして
	//	扱い、改行の手前で止まる(単一行Ifは"End If"を持たず、その物理行
	//	だけがスコープのため)
	std::string parseSingleLineStatements(const std::vector<TokenType>& stopTypes) {
		std::ostringstream out;
		while (!isStop(stopTypes) && peek().type != TokenType::End_Of_File) {
			out << parseStatement();
			if (!match(TokenType::Colon)) break;
		}
		return out.str();
	}

	//!	If ... Then ... [ElseIf ...] [Else ...] [End If]。Thenの直後が
	//	改行ならブロック形式(End Ifが必須)、そうでなければ単一行形式
	//	(End Ifを書かない)と判定する
	std::string parseIf() {
		std::ostringstream out;
		advance(); // If
		std::string cond = parseExpression();
		match(TokenType::KwThen);

		if (peek().type == TokenType::Newline) {
			out << "if (" << cond << ") {\n";
			out << parseBlock({TokenType::KwElseIf, TokenType::KwElse, TokenType::KwEnd});
			while (peek().type == TokenType::KwElseIf) {
				advance();
				std::string elifCond = parseExpression();
				match(TokenType::KwThen);
				out << "} else if (" << elifCond << ") {\n";
				out << parseBlock({TokenType::KwElseIf, TokenType::KwElse, TokenType::KwEnd});
			}
			if (peek().type == TokenType::KwElse) {
				advance();
				out << "} else {\n";
				out << parseBlock({TokenType::KwEnd});
			}
			match(TokenType::KwEnd);
			match(TokenType::KwIf);
			out << "}\n";
		} else {
			out << "if (" << cond << ") { ";
			out << parseSingleLineStatements({TokenType::KwElse, TokenType::Newline});
			out << "}";
			if (peek().type == TokenType::KwElse) {
				advance();
				out << " else { " << parseSingleLineStatements({TokenType::Newline}) << "}";
			}
			out << "\n";
		}
		return out.str();
	}

	//!	For var = start To end [Step step] ... Next / For Each var In coll ... Next
	std::string parseFor() {
		std::ostringstream out;
		advance(); // For

		if (peek().type == TokenType::KwEach) {
			advance();
			std::string varName = (peek().type == TokenType::Identifier) ? advance().text : "";
			match(TokenType::KwIn);
			std::string coll = parseExpression();
			out << "for (const " << varName << " of (" << coll << ")) {\n";
			out << parseBlock({TokenType::KwNext});
			match(TokenType::KwNext);
			if (peek().type == TokenType::Identifier) advance(); // ループ変数の反復記載(Next i)は読み飛ばす
			out << "}\n";
			return out.str();
		}

		std::string varName = (peek().type == TokenType::Identifier) ? advance().text : "";
		match(TokenType::Assign);
		std::string startExpr = parseExpression();
		match(TokenType::KwTo);
		std::string endExpr = parseExpression();
		std::string stepExpr = "1";
		if (peek().type == TokenType::KwStep) {
			advance();
			stepExpr = parseExpression();
		}

		//	Stepは実行時の式でもよく、かつ負の値も取り得る。VBScriptの
		//	Forは「Step評価時の符号で継続条件(<=か>=か)が決まる」ため、
		//	一時変数へStepの値を保持して符号判定に使う
		int id = m_forCounter++;
		std::string stepVar = "__vbsStep" + std::to_string(id);
		out << "for (" << varName << " = " << startExpr << ", " << stepVar << " = (" << stepExpr << "); "
		    << "(" << stepVar << " >= 0) ? (" << varName << " <= (" << endExpr << ")) : (" << varName << " >= (" << endExpr << ")); "
		    << varName << " += " << stepVar << ") {\n";
		out << parseBlock({TokenType::KwNext});
		match(TokenType::KwNext);
		if (peek().type == TokenType::Identifier) advance();
		out << "}\n";
		return out.str();
	}

	//!	Do [While|Until cond] ... Loop [While|Until cond] (前置・後置・
	//	どちらも無し=Exit Doで抜ける前提の無限ループ、の3パターン)
	std::string parseDo() {
		std::ostringstream out;
		advance(); // Do

		bool hasPre = false, preIsUntil = false;
		std::string preCond;
		if (peek().type == TokenType::KwWhile || peek().type == TokenType::KwUntil) {
			hasPre = true;
			preIsUntil = (peek().type == TokenType::KwUntil);
			advance();
			preCond = parseExpression();
		}

		std::string body = parseBlock({TokenType::KwLoop});
		match(TokenType::KwLoop);

		bool hasPost = false, postIsUntil = false;
		std::string postCond;
		if (peek().type == TokenType::KwWhile || peek().type == TokenType::KwUntil) {
			hasPost = true;
			postIsUntil = (peek().type == TokenType::KwUntil);
			advance();
			postCond = parseExpression();
		}

		if (hasPre) {
			std::string cond = preIsUntil ? ("!(" + preCond + ")") : preCond;
			out << "while (" << cond << ") {\n" << body << "}\n";
		} else if (hasPost) {
			std::string cond = postIsUntil ? ("!(" + postCond + ")") : postCond;
			out << "do {\n" << body << "} while (" << cond << ");\n";
		} else {
			out << "while (true) {\n" << body << "}\n";
		}
		return out.str();
	}

	//!	While cond ... Wend
	std::string parseWhile() {
		std::ostringstream out;
		advance(); // While
		std::string cond = parseExpression();
		out << "while (" << cond << ") {\n";
		out << parseBlock({TokenType::KwWend});
		match(TokenType::KwWend);
		out << "}\n";
		return out.str();
	}

	//!	Sub name(params) ... End Sub / Function name(params) ... End Function
	std::string parseSubOrFunction(bool isFunction) {
		std::ostringstream out;
		advance(); // Sub / Function
		std::string name = (peek().type == TokenType::Identifier) ? advance().text : "";

		std::vector<std::string> params;
		if (match(TokenType::LParen)) {
			while (peek().type == TokenType::Identifier || peek().type == TokenType::KwByVal || peek().type == TokenType::KwByRef) {
				match(TokenType::KwByVal);
				match(TokenType::KwByRef);
				if (peek().type == TokenType::Identifier) params.push_back(advance().text);
				if (!match(TokenType::Comma)) break;
			}
			match(TokenType::RParen);
		}

		std::string savedFuncName = m_currentFuncName;
		m_currentFuncName = isFunction ? toLower(name) : std::string();

		out << "function " << name << "(";
		for (size_t i = 0; i < params.size(); i++) out << params[i] << (i + 1 < params.size() ? ", " : "");
		out << ") {\n";
		if (isFunction) out << "let __ret;\n";
		out << parseBlock({TokenType::KwEnd});
		match(TokenType::KwEnd);
		if (isFunction) match(TokenType::KwFunction); else match(TokenType::KwSub);
		if (isFunction) out << "return __ret;\n";
		out << "}\n";

		m_currentFuncName = savedFuncName;
		return out.str();
	}

	//!	Dim/ReDim [Preserve] name[(bounds)] [, name2[(bounds2)], ...]
	std::string parseDimOrRedim() {
		std::ostringstream out;
		bool isRedim = (peek().type == TokenType::KwReDim);
		advance();
		bool preserve = false;
		if (isRedim && peek().type == TokenType::KwPreserve) { advance(); preserve = true; }

		std::vector<std::string> plainNames;
		std::ostringstream arrOut;
		do {
			if (peek().type != TokenType::Identifier) break;
			std::string nm = advance().text;
			std::string lname = toLower(nm);
			if (match(TokenType::LParen)) {
				std::vector<std::string> bounds;
				if (peek().type != TokenType::RParen) {
					bounds.push_back(parseExpression());
					while (match(TokenType::Comma)) bounds.push_back(parseExpression());
				}
				match(TokenType::RParen);
				m_arrayNames.insert(lname);
				if (isRedim) {
					if (preserve) {
						if (bounds.size() != 1) {
							throw ParseError("ReDim Preserveは1次元配列のみ対応しています (line " + std::to_string(peek().line) + ")");
						}
						arrOut << nm << " = __vbsReDimPreserve(" << nm << ", (" << bounds[0] << ") + 1);\n";
					} else {
						arrOut << nm << " = " << BuildNewArrayExpr(bounds) << ";\n";
					}
				} else {
					arrOut << "let " << nm << " = " << BuildNewArrayExpr(bounds) << ";\n";
				}
			} else {
				//	括弧無しのReDim(通常は書かれない)は無視する。括弧無しのDimは
				//	単純なスカラー変数宣言として扱う
				if (!isRedim) plainNames.push_back(nm);
			}
		} while (match(TokenType::Comma));

		out << arrOut.str();
		if (!plainNames.empty()) {
			out << "let ";
			for (size_t i = 0; i < plainNames.size(); i++) out << plainNames[i] << (i + 1 < plainNames.size() ? ", " : ";\n");
		}
		return out.str();
	}

	//!	Const name = expr [, name2 = expr2, ...]
	std::string parseConst() {
		std::ostringstream out;
		advance(); // Const
		out << "const ";
		bool first = true;
		do {
			if (peek().type != TokenType::Identifier) break;
			std::string nm = advance().text;
			match(TokenType::Assign);
			std::string val = parseExpression();
			out << (first ? "" : ", ") << nm << " = " << val;
			first = false;
		} while (match(TokenType::Comma));
		out << ";\n";
		return out.str();
	}

	//!	On Error Resume Next / On Error GoTo 0。実際のエラー握りつぶし
	//	動作までは再現せず、無視するだけの空文へ変換する(既知の制限)
	std::string parseOnError() {
		advance(); // On
		match(TokenType::KwError);
		if (match(TokenType::KwResume)) {
			match(TokenType::KwNext);
			return "// On Error Resume Next (未対応のため無視されます)\n";
		}
		if (match(TokenType::KwGoTo)) {
			if (peek().type == TokenType::Number || peek().type == TokenType::Identifier) advance();
			return "// On Error GoTo (未対応のため無視されます)\n";
		}
		throw ParseError("unsupported 'On Error' form at line " + std::to_string(peek().line));
	}

	//!	1文をパースして対応するJSコード片を返す。次のトークンの種類を見て
	//	分岐するだけの素朴な実装([[CPasToJsTranspiler::parseStatement]]と
	//	同じ方針)
	std::string parseStatement() {
		//	Public/Private修飾子(モジュールレベルのDim/Sub/Function/Constの前
		//	に付けられる)はアクセス制御を再現する対象が無いため読み飛ばす
		match(TokenType::KwPublic);
		match(TokenType::KwPrivate);

		if (peek().type == TokenType::KwOption) {
			advance();
			if (peek().type == TokenType::Identifier) advance(); // "Explicit" 等
			return "";
		}
		if (peek().type == TokenType::KwDim || peek().type == TokenType::KwReDim) return parseDimOrRedim();
		if (peek().type == TokenType::KwConst) return parseConst();
		if (peek().type == TokenType::KwSub) return parseSubOrFunction(false);
		if (peek().type == TokenType::KwFunction) return parseSubOrFunction(true);
		if (peek().type == TokenType::KwIf) return parseIf();
		if (peek().type == TokenType::KwFor) return parseFor();
		if (peek().type == TokenType::KwDo) return parseDo();
		if (peek().type == TokenType::KwWhile) return parseWhile();
		if (peek().type == TokenType::KwOn) return parseOnError();

		if (peek().type == TokenType::KwCall) {
			advance();
			std::string expr = parseExpression();
			return expr + ";\n";
		}
		if (peek().type == TokenType::KwExit) {
			advance();
			if (match(TokenType::KwSub)) return "return;\n";
			if (match(TokenType::KwFunction)) return "return __ret;\n";
			if (match(TokenType::KwFor)) return "break;\n";
			if (match(TokenType::KwDo)) return "break;\n";
			throw ParseError("unsupported 'Exit' form at line " + std::to_string(peek().line));
		}

		//	代入(Set含む)、または配列要素アクセス/関数呼び出し文
		bool isSet = match(TokenType::KwSet);
		(void)isSet; // JS側では参照代入と値代入を区別しないため、Setは消費するだけでよい
		if (peek().type == TokenType::Identifier) {
			Token ident = advance();
			if (peek().type == TokenType::LParen) {
				std::string target = parsePostfixCallOrIndex(ident);
				if (match(TokenType::Assign)) {
					return target + " = " + parseExpression() + ";\n";
				}
				return target + ";\n";
			}
			if (match(TokenType::Assign)) {
				return ResolveVarName(ident.text) + " = " + parseExpression() + ";\n";
			}
			//	括弧無しの手続き呼び出し(`InfoMsg msg`のようなVBScript伝統の
			//	書き方)。識別子の直後がここで文が終わるトークンでなければ、
			//	'('を伴わないカンマ区切りの引数列が続くとみなして読み進める
			if (isStatementBoundary(peek().type)) {
				return ToJsCallName(ident.text) + "();\n";
			}
			{
				std::vector<std::string> args;
				args.push_back(parseExpression());
				while (match(TokenType::Comma)) args.push_back(parseExpression());
				std::string callArgs;
				for (size_t i = 0; i < args.size(); i++) callArgs += (i ? ", " : "") + args[i];
				return ToJsCallName(ident.text) + "(" + callArgs + ");\n";
			}
		}

		throw ParseError("unexpected token '" + peek().text + "' at line " + std::to_string(peek().line));
	}

public:
	//!	VBScript風マクロのソース(UTF-8)をJavaScriptソース(UTF-8)へ変換する
	std::string transpile(const std::string& vbsCode) {
		m_src = vbsCode;
		m_tokenIndex = 0;
		m_arrayNames.clear();
		m_currentFuncName.clear();
		m_forCounter = 0;
		tokenize();

		std::ostringstream js;
		js << "// Transpiled from VBScript-like macro (.vbs) to QuickJS\n";
		js << "(() => {\n";
		js << parseBlock({}); // 空のstop集合 = EOFまで読み進める(トップレベル)
		js << "})();\n";
		return js.str();
	}
};

#endif // NKMM_FIX_VBS_MACRO

#endif /* SAKURA_CVBSTOJSTRANSPILER_9C2E4F1A_6B3D_4E8C_A1F5_2D9B6E4C8A3F_H_ */
/*[EOF]*/
