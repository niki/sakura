/*!	@file
	@brief Pascal風マクロ(.pas)をJavaScriptへ変換するトランスパイラ

	NKMM_FIX_PASCAL_MACROが使う。PPA.DLL(Poor-Pascal for Application)が提供していた
	ようなPascal風の構文を、QuickJSマクロエンジン(CQuickJSMacroMgr)がそのまま実行できる
	JavaScriptソースへヘッダオンリーで変換する。生成したJSは、変数宣言・分岐・ループ等を
	素直に対応するJS構文へ置き換えるだけで、関数呼び出し(例: InsText(s))はそのまま
	同名のJS関数呼び出しとして出力するため、CQuickJSIfObjBinderがグローバルへ登録する
	Editor系関数(修飾無し呼び出し)をPascalのコードから直接呼べる。

	--- 全体の構成(コンパイラ/トランスパイラの標準的な2段構成) -----------------

	1) 字句解析(Lexer): tokenize()
	   Pascalソース文字列(m_src)を先頭から1文字ずつ読み、意味のある最小単位
	   ("token"、例: 識別子1個・数値1個・記号1個)に切り出して m_tokens に
	   並べる。この段階では文法(varの後は識別子が来る、等)は一切見ておらず、
	   「文字の並び」を「トークンの並び」に変換するだけ。

	2) 構文解析(Parser)とコード生成を同時に行うparseXxx()系
	   一般的なコンパイラは「(2a)トークン列から構文木(AST)を作る」→
	   「(2b)構文木を辿ってコード生成する」の2段階に分けることが多いが、
	   このトランスパイラは変換先がJSソース文字列で十分単純なため、構文木を
	   作らずに「パースしながらその場でJS文字列を組み立てて返す」一体型の
	   実装になっている(各parseXxxが「自分の担当する文法要素を消費して、
	   対応するJSコード片の文字列を返す」再帰下降パーサー)。

	   式(Expression)側は、演算子の優先順位ごとに関数を分けて呼び出し順序で
	   優先順位を表現する「再帰下降による演算子優先順位パース」の定石構成。
	   parseExpression (最優先度=or) → parseLogicalOr → parseLogicalAnd →
	   parseEquality → parseRelational → parseAdditive → parseMultiplicative →
	   parseUnary → parsePrimary (最優先度=リテラル/識別子/カッコ) の順に
	   呼び出しが下りていく。詳しくは各関数のコメント参照。

	   文(Statement)側は、次のトークンの種類(var/begin/if/for/...)を見て
	   分岐するだけの素朴な実装(parseStatement)。

	3) エントリポイント: transpile()
	   1)→2)を呼び出し、トップレベルの文を末尾(End_Of_File)まで繰り返し
	   パースしてJSソース全体を組み立てる。

	@date 2026 NKMM_FIX_PASCAL_MACRO
*/
#ifndef SAKURA_CPASTOJSTRANSPILER_1F3A6C2D_8B4E_4A5D_9E7F_3D6C9B8A2E1F_H_
#define SAKURA_CPASTOJSTRANSPILER_1F3A6C2D_8B4E_4A5D_9E7F_3D6C9B8A2E1F_H_

#ifdef NKMM_FIX_PASCAL_MACRO

#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <stdexcept>

//!	Pascal風マクロ言語 → JavaScript トランスパイラ
class CPasToJsTranspiler {
public:
	//!	構文解析エラー。想定外のトークンでparseStatement/parsePrimaryが
	//!	1トークンも消費せず無限ループするのを防ぐため、フォールバック位置では
	//!	必ずこれを投げる(呼び出し元のtranspile()はこれをそのまま伝播させる)。
	struct ParseError : std::runtime_error {
		explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
	};

	//!	トークンの種類。字句解析(tokenize)が出力し、構文解析(parseXxx)が
	//!	消費する。「識別子」「数値」のような大分類に加え、Pascalのキーワード
	//!	(var, begin, if, ...)は個別の種類として区別する(構文解析側で
	//!	peek().type==TokenType::KwIf のように直接分岐できるようにするため。
	//!	もしキーワードもIdentifierのままだったら、毎回文字列比較で
	//!	"これはifという名前のキーワードか、それとも変数名か"を判定する
	//!	必要が出てしまう)。
	enum class TokenType {
		End_Of_File,	//!< ソース末尾に達したことを示す番兵トークン
		Identifier,	//!< 変数名・関数名などキーワードに該当しない単語
		Number,		//!< 数値リテラル(整数・小数。テキストのまま保持しJS側にも数値リテラルとして出力)
		StringLit,	//!< 文字列リテラル。tokenize()の時点でJSのダブルクォート文字列表現に変換済み
		//	キーワード
		KwVar, KwBegin, KwEnd, KwIf, KwThen, KwElse,
		KwWhile, KwDo, KwFor, KwTo, KwDownto, KwRepeat, KwUntil,
		KwProcedure, KwFunction, KwAnd, KwOr, KwNot, KwMod, KwDiv,
		KwContinue, KwBreak, KwProgram,
		//	記号・演算子
		Assign,     // :=
		Equal,      // =
		NotEqual,   // <>
		Less,       // <
		LessEq,     // <=
		Greater,    // >
		GreaterEq,  // >=
		Plus,       // +
		Minus,      // -
		Multiply,   // *
		Divide,     // /
		Semicolon,  // ;
		Colon,      // :
		Comma,      // ,
		Dot,        // .
		LParen,     // (
		RParen,     // )
	};

	//!	1個のトークン。
	//!	text: そのトークンの生のテキスト(数値なら"123"、識別子なら"InsText"等)。
	//!	      StringLitだけは例外で、tokenize()の時点でJSの文字列リテラル表現
	//!	      (ダブルクォート込み)に変換済みのものが入る。
	//!	line: 元のPascalソース上の行番号(1始まり)。ParseError発生時に
	//!	      「何行目が悪いか」をユーザーに伝えるために使う。
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

	//	ヘルパー: 大文字小文字を区別しないキーワード判定用
	static std::string toLower(const std::string& str) {
		std::string s = str;
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
			return std::tolower(c);
		});
		return s;
	}

	//	呼び出し名の"S_"接頭辞(大文字小文字区別無し)をサクラエディタのマクロ
	//	関数呼び出しの目印として扱う。接頭辞を剥がして"Editor."を付けたJS呼び出し
	//	名に変換することで、Copy/InputBox/MessageBoxのようなPascal標準関数名との
	//	衝突を避けつつ、常にサクラ本来の実装(CQuickJSIfObjBinderが常に登録する
	//	"Editor"オブジェクト経由)を指すようにする。接頭辞が無ければPPA言語自体の
	//	組み込み関数(CPasMacroMgr側が用意するランタイムライブラリ)とみなし、
	//	そのままの名前で呼び出す。
	static std::string ToJsCallName(const std::string& name) {
		if (name.size() > 2 && (name[0] == 'S' || name[0] == 's') && name[1] == '_') {
			return "Editor." + name.substr(2);
		}
		return name;
	}

	//	========================================================================
	//	1. 字句解析 (Lexer)
	//	========================================================================
	//!	m_src(Pascalソース全体)を先頭から1文字ずつ読み進め、m_tokensへ
	//!	トークンを積んでいく。全体の構造は「while(まだ読める){ 今の1文字を見て
	//!	何のトークンか判定し、該当する分だけm_posを進めてpush_back、continue }」
	//!	という素朴な手書きループ(正規表現やパーサジェネレータは使わない)。
	//!	各分岐の判定順序に意味がある点に注意:
	//!	  空白・コメント(読み飛ばすだけ) → 文字列リテラル → 数値 →
	//!	  識別子/キーワード → 2文字の記号(:= <> <= >=) → 1文字の記号
	//!	この順で「もっとも長く・具体的にマッチするパターン」から先に判定する
	//!	ことで、例えば":="を":"と"="の2トークンに誤分割しない。
	void tokenize() {
		m_tokens.clear();
		m_pos = 0;
		m_line = 1;

		while (m_pos < m_src.size()) {
			char c = m_src[m_pos];

			if (c == '\n') {
				m_line++;
				m_pos++;
				continue;
			}
			if (std::isspace(static_cast<unsigned char>(c))) {
				m_pos++;
				continue;
			}

			//	行コメント //
			if (c == '/' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '/') {
				while (m_pos < m_src.size() && m_src[m_pos] != '\n') m_pos++;
				continue;
			}
			//	ブロックコメント { ... }
			if (c == '{') {
				m_pos++;
				while (m_pos < m_src.size() && m_src[m_pos] != '}') {
					if (m_src[m_pos] == '\n') m_line++;
					m_pos++;
				}
				if (m_pos < m_src.size()) m_pos++; // '}' をスキップ
				continue;
			}

			//	文字列リテラル '...' (Pascal形式: '' でエスケープ)
			//	PPAマクロでは 'CRLF := '\n';' のように、リテラル中に実際の
			//	改行文字を含めて複数行文字列として書く例がある。JSのダブル
			//	クォート文字列は生の改行を含められない(SyntaxErrorになる)ため、
			//	\r/\nはエスケープシーケンスへ変換する。
			if (c == '\'') {
				m_pos++;
				std::string s = "\"";
				while (m_pos < m_src.size()) {
					if (m_src[m_pos] == '\'') {
						if (m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '\'') {
							s += '\'';
							m_pos += 2;
						} else {
							m_pos++;
							break;
						}
					} else if (m_src[m_pos] == '\n') {
						s += "\\n";
						m_line++;
						m_pos++;
					} else if (m_src[m_pos] == '\r') {
						//	CRLFのCRは読み飛ばす(直後のLFを\nとして出力する)
						m_pos++;
					} else {
						if (m_src[m_pos] == '"' || m_src[m_pos] == '\\') s += '\\';
						s += m_src[m_pos];
						m_pos++;
					}
				}
				s += "\"";
				m_tokens.push_back({TokenType::StringLit, s, m_line});
				continue;
			}

			//	数値
			if (std::isdigit(static_cast<unsigned char>(c))) {
				std::string num;
				while (m_pos < m_src.size() && (std::isdigit(static_cast<unsigned char>(m_src[m_pos])) || m_src[m_pos] == '.')) {
					//	".." (範囲指定)を誤読しないようにチェック
					if (m_src[m_pos] == '.' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '.') break;
					num += m_src[m_pos++];
				}
				m_tokens.push_back({TokenType::Number, num, m_line});
				continue;
			}

			//	識別子 / キーワード
			if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
				std::string ident;
				while (m_pos < m_src.size() && (std::isalnum(static_cast<unsigned char>(m_src[m_pos])) || m_src[m_pos] == '_')) {
					ident += m_src[m_pos++];
				}
				std::string lower = toLower(ident);
				TokenType t = TokenType::Identifier;

				if (lower == "var") t = TokenType::KwVar;
				else if (lower == "begin") t = TokenType::KwBegin;
				else if (lower == "end") t = TokenType::KwEnd;
				else if (lower == "if") t = TokenType::KwIf;
				else if (lower == "then") t = TokenType::KwThen;
				else if (lower == "else") t = TokenType::KwElse;
				else if (lower == "while") t = TokenType::KwWhile;
				else if (lower == "do") t = TokenType::KwDo;
				else if (lower == "for") t = TokenType::KwFor;
				else if (lower == "to") t = TokenType::KwTo;
				else if (lower == "downto") t = TokenType::KwDownto;
				else if (lower == "repeat") t = TokenType::KwRepeat;
				else if (lower == "until") t = TokenType::KwUntil;
				else if (lower == "procedure") t = TokenType::KwProcedure;
				else if (lower == "function") t = TokenType::KwFunction;
				else if (lower == "and") t = TokenType::KwAnd;
				else if (lower == "or") t = TokenType::KwOr;
				else if (lower == "not") t = TokenType::KwNot;
				else if (lower == "mod") t = TokenType::KwMod;
				else if (lower == "div") t = TokenType::KwDiv;
				else if (lower == "continue") t = TokenType::KwContinue;
				else if (lower == "break") t = TokenType::KwBreak;
				else if (lower == "program") t = TokenType::KwProgram;

				m_tokens.push_back({t, ident, m_line});
				continue;
			}

			//	記号・演算子
			if (c == ':' && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '=') {
				m_tokens.push_back({TokenType::Assign, ":=", m_line});
				m_pos += 2;
				continue;
			}
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

			switch (c) {
			case '=':
				m_tokens.push_back({TokenType::Equal, "=", m_line});
				break;
			case '<':
				m_tokens.push_back({TokenType::Less, "<", m_line});
				break;
			case '>':
				m_tokens.push_back({TokenType::Greater, ">", m_line});
				break;
			case '+':
				m_tokens.push_back({TokenType::Plus, "+", m_line});
				break;
			case '-':
				m_tokens.push_back({TokenType::Minus, "-", m_line});
				break;
			case '*':
				m_tokens.push_back({TokenType::Multiply, "*", m_line});
				break;
			case '/':
				m_tokens.push_back({TokenType::Divide, "/", m_line});
				break;
			case ';':
				m_tokens.push_back({TokenType::Semicolon, ";", m_line});
				break;
			case ':':
				m_tokens.push_back({TokenType::Colon, ":", m_line});
				break;
			case ',':
				m_tokens.push_back({TokenType::Comma, ",", m_line});
				break;
			case '.':
				m_tokens.push_back({TokenType::Dot, ".", m_line});
				break;
			case '(':
				m_tokens.push_back({TokenType::LParen, "(", m_line});
				break;
			case ')':
				m_tokens.push_back({TokenType::RParen, ")", m_line});
				break;
			default:
				break;
			}
			m_pos++;
		}
		m_tokens.push_back({TokenType::End_Of_File, "", m_line});
	}

	//	========================================================================
	//	2. 構文解析 → JSコード生成
	//	========================================================================
	//!	現在位置(m_tokenIndex)のトークンを覗き見るだけ(読み進めない)。
	//!	「次のトークンが何であるかによって、どの構文か判断する」という
	//!	再帰下降パーサーの分岐判定は、すべてこのpeek()の結果を見て行う。
	Token peek() const {
		return m_tokens[m_tokenIndex];
	}
	//!	現在位置のトークンを返しつつ、読み取り位置を1つ進める(=消費する)。
	Token advance() {
		return m_tokens[m_tokenIndex++];
	}
	//!	次のトークンが指定した種類なら、それを消費してtrueを返す。
	//!	違えば何もせず(位置を進めず)falseを返す。「あれば読み飛ばす、
	//!	無ければ気にしない」という省略可能な区切り記号(';'や'('等)の
	//!	チェックに多用される。
	bool match(TokenType type) {
		if (peek().type == type) {
			advance();
			return true;
		}
		return false;
	}

	//	--- 式(Expression)の変換 -------------------------------------------
	//	演算子優先順位パーサー(precedence climbing)の素朴な実装。
	//	「1 + 2 * 3」を「(1 + 2) * 3」ではなく「1 + (2 * 3)」と正しく解釈
	//	させたい(*の方が+より優先順位が高い)場合の定石が、優先順位の低い
	//	演算子を扱う関数から、優先順位の高い演算子を扱う関数を呼び出す形で
	//	関数を積み重ねる、というこの構成。呼び出しの深さがそのまま優先順位の
	//	高さに対応する:
	//
	//	  parseExpression        (エントリポイント。中身はparseLogicalOr)
	//	    parseLogicalOr        優先度1(最弱): or
	//	      parseLogicalAnd      優先度2: and
	//	        parseEquality       優先度3: = <>
	//	          parseRelational    優先度4: < <= > >=
	//	            parseAdditive     優先度5: + -
	//	              parseMultiplicative 優先度6: * / mod div
	//	                parseUnary        優先度7: 単項 not, 単項 -
	//	                  parsePrimary     優先度8(最強): 数値・文字列・識別子・
	//	                                   関数呼び出し・( )で囲んだ式
	//
	//	各段の実装パターンは共通: 「まず1段階優先順位が高い関数を呼んで
	//	左辺(expr)を得る」→「自分の優先順位の演算子が続く限りwhileループで
	//	読み進め、そのたびに右辺も1段階優先順位が高い関数で取得し、
	//	exprを更新していく」。これにより同じ優先順位の演算子の連続
	//	(例: 1 - 2 - 3)は自然に左結合(=(1-2)-3)になる。
	std::string parseExpression() {
		return parseLogicalOr();
	}

	std::string parseLogicalOr() {
		std::string expr = parseLogicalAnd();
		while (peek().type == TokenType::KwOr) {
			advance();
			expr = "(" + expr + " || " + parseLogicalAnd() + ")";
		}
		return expr;
	}

	std::string parseLogicalAnd() {
		std::string expr = parseEquality();
		while (peek().type == TokenType::KwAnd) {
			advance();
			expr = "(" + expr + " && " + parseEquality() + ")";
		}
		return expr;
	}

	std::string parseEquality() {
		std::string expr = parseRelational();
		while (peek().type == TokenType::Equal || peek().type == TokenType::NotEqual) {
			Token op = advance();
			//	PascalのEqual(=)はJSの==ではなく===へ変換する。JSの==は
			//	型変換を伴う緩い比較("1"==1がtrueになる等)なので、Pascalの
			//	厳密な等価比較の意味に近い===(型変換なし)を使う。
			std::string jsOp = (op.type == TokenType::Equal) ? " === " : " !== ";
			expr = "(" + expr + jsOp + parseRelational() + ")";
		}
		return expr;
	}

	std::string parseRelational() {
		std::string expr = parseAdditive();
		while (peek().type == TokenType::Less || peek().type == TokenType::LessEq ||
		        peek().type == TokenType::Greater || peek().type == TokenType::GreaterEq) {
			Token op = advance();
			expr = "(" + expr + " " + op.text + " " + parseAdditive() + ")";
		}
		return expr;
	}

	std::string parseAdditive() {
		std::string expr = parseMultiplicative();
		while (peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
			Token op = advance();
			expr = expr + " " + op.text + " " + parseMultiplicative();
		}
		return expr;
	}

	std::string parseMultiplicative() {
		std::string expr = parseUnary();
		while (peek().type == TokenType::Multiply || peek().type == TokenType::Divide ||
		        peek().type == TokenType::KwMod || peek().type == TokenType::KwDiv) {
			Token op = advance();
			std::string rhs = parseUnary();
			if (op.type == TokenType::KwMod) {
				expr = "(" + expr + " % " + rhs + ")";
			} else if (op.type == TokenType::KwDiv) {
				//	Pascalのdivは整数除算。JSの/は常に浮動小数点除算なので、
				//	結果をMath.truncで切り捨てて整数除算の意味を再現する。
				expr = "Math.trunc(" + expr + " / (" + rhs + "))";
			} else {
				expr = expr + " " + op.text + " " + rhs;
			}
		}
		return expr;
	}

	//!	単項演算子(not X, -X)。二項演算子(+ - * / 等)と違い「左辺を先に
	//!	取ってからループで演算子を探す」形にはならず、「まず演算子の有無を
	//!	見て、あればそれを剥がしてから自分自身(parseUnary)を再帰呼び出しし、
	//!	結果を包む」形になる。これは"- - 5"(--5ではない、単項マイナス2つ)の
	//!	ような入れ子を素直に処理するため。演算子が無ければ、これ以上高い
	//!	優先順位は無いのでparsePrimary()へ落ちる。
	std::string parseUnary() {
		if (peek().type == TokenType::KwNot) {
			advance();
			return "!(" + parseUnary() + ")";
		}
		if (peek().type == TokenType::Minus) {
			advance();
			return "-" + parseUnary();
		}
		return parsePrimary();
	}

	//!	式の末端(これ以上分解できない最小の式)。数値・文字列リテラル、
	//!	単なる変数参照、関数呼び出し、"( 式 )"のいずれか。再帰下降パーサーの
	//!	末端らしく、ここでは基本的にparseExpression()を再帰的に呼ぶのは
	//!	"(...)"の中身と関数の実引数だけで、それ以外は何かを消費して即返す。
	std::string parsePrimary() {
		Token t = advance();
		if (t.type == TokenType::Number || t.type == TokenType::StringLit) {
			return t.text;
		}
		if (t.type == TokenType::Identifier) {
			std::string name = t.text;
			//	直後が'('なら関数呼び出し。引数はカンマ区切りで
			//	parseExpression()を繰り返し呼んで集める(=引数自体も
			//	式なので、四則演算や別の関数呼び出しを書ける)。
			if (peek().type == TokenType::LParen) {
				advance(); // '('
				std::string args;
				if (peek().type != TokenType::RParen) {
					args += parseExpression();
					while (match(TokenType::Comma)) {
						args += ", " + parseExpression();
					}
				}
				match(TokenType::RParen);
				//	ToJsCallNameで"S_"接頭辞の有無により
				//	"Editor.関数名"かそのままの名前かに振り分ける
				return ToJsCallName(name) + "(" + args + ")";
			}
			//	'('が続かなければ、ただの変数参照
			return name;
		}
		if (t.type == TokenType::LParen) {
			//	丸カッコで囲まれた部分式。優先順位を無視して中身を再帰的に
			//	丸ごと評価するため、ここだけ最上位のparseExpression()へ戻る
			//	(これが「丸カッコは優先順位を打ち破れる」所以)。
			std::string expr = parseExpression();
			match(TokenType::RParen);
			return "(" + expr + ")";
		}
		//	Number/StringLit/Identifier/LParenのいずれでもない
		//	=式の先頭に来てはいけないトークン。ParseErrorを投げて
		//	上位へ伝播させる(このtは既にadvance()済みなので、呼び出し元の
		//	ループが1トークンも進めずに無限ループする心配は無い)。
		throw ParseError("unexpected token '" + t.text + "' at line " + std::to_string(t.line));
	}

	//	========================================================================
	//	文(Statement)の変換
	//	========================================================================
	//!	式のような「優先順位に応じて関数を積み重ねる」構造ではなく、
	//!	次に来るトークンの種類をひとつずつif文で見て、該当する構文の
	//!	読み取り・JSコード組み立てを行う素朴な分岐構造(いわゆる
	//!	"再帰下降パーサーのstatement版"の定石)。1つのparseStatement()呼び出しで
	//!	Pascalの文1つぶんを消費し、対応するJSコード文字列を返す。
	//!	begin...end/if/for/while/repeatの中身は、この関数自身を再帰呼び出しして
	//!	1文ずつ処理する(=入れ子のブロックも同じロジックで扱える)。
	std::string parseStatement() {
		std::ostringstream out;

		//	program宣言(例: program PPALangCheck;)。実行には影響しないため無視する。
		if (peek().type == TokenType::KwProgram) {
			advance();
			if (peek().type == TokenType::Identifier) advance(); // プログラム名をスキップ
			match(TokenType::Semicolon);
			return "";
		}

		//	var 宣言
		//	Pascalのvarセクションは、型の異なる複数の宣言行を続けて書ける
		//	(例: var s: string; i: Integer;)。次のトークンが識別子である限り
		//	宣言行が続くとみなして読み進め、まとめて1つのJS "let"文にする。
		if (peek().type == TokenType::KwVar) {
			advance();
			std::vector<std::string> allVars;
			while (peek().type == TokenType::Identifier) {
				do {
					if (peek().type == TokenType::Identifier) {
						allVars.push_back(advance().text);
					}
				} while (match(TokenType::Comma));

				match(TokenType::Colon); // 型の直前の ':'
				if (peek().type == TokenType::Identifier) advance(); // 型名 (Integer, String等) をスキップ
				match(TokenType::Semicolon);
			}

			out << "let ";
			for (size_t i = 0; i < allVars.size(); i++) {
				out << allVars[i] << (i + 1 < allVars.size() ? ", " : ";\n");
			}
			return out.str();
		}

		//	procedure 宣言
		//	戻り値は無い(functionと違い、名前への代入で戻り値を返すことはしない)。
		//	定義はメイン処理のbeginより前(varと同じ宣言セクション)に書く。
		//	begin ... end; までが本体になる。JSのfunction宣言(hoistingされる)へ
		//	変換するため、メインのbeginブロックより前に出力されていれば、
		//	定義順に関わらずメイン処理から呼び出せる。
		if (peek().type == TokenType::KwProcedure) {
			advance();
			std::string name;
			if (peek().type == TokenType::Identifier) {
				name = advance().text;
			}

			std::vector<std::string> params;
			if (match(TokenType::LParen)) {
				//	引数リストもvarセクション同様、型の異なる複数グループを
				//	';'区切りで続けて書ける(例: (a, b: Integer; c: String))
				while (peek().type == TokenType::Identifier) {
					do {
						if (peek().type == TokenType::Identifier) {
							params.push_back(advance().text);
						}
					} while (match(TokenType::Comma));
					match(TokenType::Colon);
					if (peek().type == TokenType::Identifier) advance(); // 型名をスキップ
					match(TokenType::Semicolon);
				}
				match(TokenType::RParen);
			}
			match(TokenType::Semicolon);

			out << "function " << name << "(";
			for (size_t i = 0; i < params.size(); i++) {
				out << params[i] << (i + 1 < params.size() ? ", " : "");
			}
			out << ") {\n";
			//	本体側のローカルvar宣言(0個以上)。トップレベルのvarと同じ構文
			//	(既存のKwVar分岐)を、begin...endの前で繰り返し読む。
			while (peek().type == TokenType::KwVar) {
				out << parseStatement();
			}
			//	begin ... end ブロック本体。中括弧は上で開いた分だけを使うため、
			//	KwBegin分岐が生成する"{"/"}"は使わず中身だけを展開する。
			match(TokenType::KwBegin);
			while (peek().type != TokenType::KwEnd && peek().type != TokenType::End_Of_File) {
				out << parseStatement();
				match(TokenType::Semicolon);
			}
			match(TokenType::KwEnd);
			out << "}\n";
			return out.str();
		}

		//	begin ... end ブロック
		if (peek().type == TokenType::KwBegin) {
			advance();
			out << "{\n";
			while (peek().type != TokenType::KwEnd && peek().type != TokenType::End_Of_File) {
				out << parseStatement();
				match(TokenType::Semicolon);
			}
			match(TokenType::KwEnd);
			out << "}\n";
			return out.str();
		}

		//	if ... then ... else
		if (peek().type == TokenType::KwIf) {
			advance();
			std::string cond = parseExpression();
			match(TokenType::KwThen);
			out << "if (" << cond << ") " << parseStatement();
			if (peek().type == TokenType::KwElse) {
				advance();
				out << " else " << parseStatement();
			}
			return out.str();
		}

		//	for ループ (to / downto)
		if (peek().type == TokenType::KwFor) {
			advance();
			std::string varName = advance().text;
			match(TokenType::Assign);
			std::string startVal = parseExpression();
			bool isDown = false;
			if (peek().type == TokenType::KwDownto) {
				isDown = true;
				advance();
			}
			else {
				match(TokenType::KwTo);
			}
			std::string endVal = parseExpression();
			match(TokenType::KwDo);

			out << "for (" << varName << " = " << startVal << "; "
			    << varName << (isDown ? " >= " : " <= ") << endVal << "; "
			    << varName << (isDown ? "--" : "++") << ") ";
			out << parseStatement();
			return out.str();
		}

		//	while ループ
		if (peek().type == TokenType::KwWhile) {
			advance();
			std::string cond = parseExpression();
			match(TokenType::KwDo);
			out << "while (" << cond << ") " << parseStatement();
			return out.str();
		}

		//	repeat ... until ループ
		if (peek().type == TokenType::KwRepeat) {
			advance();
			out << "do {\n";
			while (peek().type != TokenType::KwUntil && peek().type != TokenType::End_Of_File) {
				out << parseStatement();
				match(TokenType::Semicolon);
			}
			match(TokenType::KwUntil);
			std::string cond = parseExpression();
			out << "} while (!(" << cond << "));\n";
			return out.str();
		}

		//	continue / break (forループ内のContinue;/Break;等)
		//	JSの予約語であり関数ではないため、他の識別子と同じ「手続き呼び出し」
		//	扱いにすると"continue();"のような不正なコードになる。専用に処理する。
		if (peek().type == TokenType::KwContinue) {
			advance();
			out << "continue;\n";
			return out.str();
		}
		if (peek().type == TokenType::KwBreak) {
			advance();
			out << "break;\n";
			return out.str();
		}

		//	代入文、または引数なし手続き呼び出し
		if (peek().type == TokenType::Identifier) {
			Token ident = advance();
			if (peek().type == TokenType::Assign) {
				advance(); // ':='
				std::string val = parseExpression();
				out << ident.text << " = " << val << ";\n";
				return out.str();
			} else {
				//	引数付き、または引数無しの手続き呼び出し
				std::string args;
				if (match(TokenType::LParen)) {
					if (peek().type != TokenType::RParen) {
						args += parseExpression();
						while (match(TokenType::Comma)) {
							args += ", " + parseExpression();
						}
					}
					match(TokenType::RParen);
				}
				out << ToJsCallName(ident.text) << "(" << args << ");\n";
				return out.str();
			}
		}

		//	どの文パターンにも該当しない場合、必ず例外を投げてトークンを消費させる。
		//	空文字列を返すと呼び出し元のbegin/repeatループが1トークンも進めずに
		//	無限ループする(未対応構文、例えばPascal形式の文字コードリテラル
		//	#13#10等を含むソースで実際に無限ループを確認した)。
		throw ParseError("unexpected token '" + peek().text + "' at line " + std::to_string(peek().line));
	}

public:
	//!	Pascal風マクロのソース(UTF-8)をJavaScriptソース(UTF-8)へ変換する
	std::string transpile(const std::string& pascalCode) {
		m_src = pascalCode;
		m_tokenIndex = 0;
		tokenize();

		std::ostringstream js;
		js << "// Transpiled from Pascal-like macro (.pas) to QuickJS\n";
		js << "(() => {\n";

		while (peek().type != TokenType::End_Of_File) {
			js << parseStatement();
			match(TokenType::Semicolon);
			match(TokenType::Dot); // プログラム末尾の 'end.' の '.' を無視
		}

		js << "})();\n";
		return js.str();
	}
};

#endif // NKMM_FIX_PASCAL_MACRO

#endif /* SAKURA_CPASTOJSTRANSPILER_1F3A6C2D_8B4E_4A5D_9E7F_3D6C9B8A2E1F_H_ */
/*[EOF]*/
