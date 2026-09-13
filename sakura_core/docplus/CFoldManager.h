/*
	Copyright (C) 2026, Yu-zuki.

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such,
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/
#ifndef SAKURA_CFOLDMANAGER_H_
#define SAKURA_CFOLDMANAGER_H_

class CDocLine;
class CDocLineMgr;

//! 行に付加するコードフォールディング情報 20260911
class CLineFolded{
public:
	CLineFolded() : m_bFoldable(false), m_bFolded(false), m_bHidden(false), m_nEndLine(-1), m_nNameCol(-1), m_nNameLen(0), m_nArgsCol(-1), m_nArgsLen(0), m_bIsDeclaration(false) { }

	//! この行が折りたたみの開始行(関数/構造体等のヘッダ行)になりうるか
	bool GetFoldable() const { return m_bFoldable; }
	void SetFoldable(bool bSet) { m_bFoldable = bSet; }

	//! この行(開始行)が現在折りたたまれているか
	bool GetFolded() const { return m_bFolded; }
	void SetFolded(bool bSet) { m_bFolded = bSet; }

	//! この行が折りたたみ範囲内にあり非表示になっているか(開始行自身は対象外)
	bool GetHidden() const { return m_bHidden; }
	void SetHidden(bool bSet) { m_bHidden = bSet; }

	//! 開始行のみ意味を持つ、折りたたみ範囲の終了行(0オリジン、CRLF単位。未設定なら-1)
	int GetFoldEndLine() const { return m_nEndLine; }
	void SetFoldEndLine(int nEndLine) { m_nEndLine = nEndLine; }

	//! 開始行のみ意味を持つ、関数/メソッド名部分の開始桁(0オリジン、文字単位。特定できなければ-1)
	int GetNameCol() const { return m_nNameCol; }
	void SetNameCol(int nCol) { m_nNameCol = nCol; }
	//! 関数/メソッド名部分の文字数
	int GetNameLen() const { return m_nNameLen; }
	void SetNameLen(int nLen) { m_nNameLen = nLen; }

	//! 開始行のみ意味を持つ、引数リスト(丸括弧の中身)の開始桁(0オリジン、文字単位。特定できなければ-1)
	int GetArgsCol() const { return m_nArgsCol; }
	void SetArgsCol(int nCol) { m_nArgsCol = nCol; }
	//! 引数リスト(丸括弧の中身)の文字数
	int GetArgsLen() const { return m_nArgsLen; }
	void SetArgsLen(int nLen) { m_nArgsLen = nLen; }

	//! 開始行のみ意味を持つ、本体を持たない宣言(プロトタイプ)か
	//! (折りたたみ範囲(EndLine)には末尾の空行/コメント行が含まれることがあるため、
	//! 「範囲が1行だけか」では判定できず、実テキストを見て別途判定する 20260913)
	bool GetIsDeclaration() const { return m_bIsDeclaration; }
	void SetIsDeclaration(bool bSet) { m_bIsDeclaration = bSet; }

private:
	bool m_bFoldable;	// 折りたたみ開始行か
	bool m_bFolded;		// 折りたたみ中か(開始行のみ意味を持つ)
	bool m_bHidden;		// 折りたたみ範囲内で非表示か(開始行自身は対象外)
	int  m_nEndLine;	// 折りたたみ範囲の終了行(開始行のみ意味を持つ)
	int  m_nNameCol;	// 関数/メソッド名部分の開始桁(開始行のみ意味を持つ、未特定なら-1)
	int  m_nNameLen;	// 関数/メソッド名部分の文字数
	int  m_nArgsCol;	// 引数リスト(丸括弧の中身)の開始桁(開始行のみ意味を持つ、未特定なら-1)
	int  m_nArgsLen;	// 引数リスト(丸括弧の中身)の文字数
	bool m_bIsDeclaration;	// 本体を持たない宣言(プロトタイプ)か(開始行のみ意味を持つ)
};

//! 行全体のコードフォールディング情報アクセサ
class CFoldManager{
public:
	//状態
	bool GetLineFoldable(const CDocLine* pcDocLine) const;
	void SetLineFoldable(CDocLine* pcDocLine, bool bFlag);
	bool GetLineFolded(const CDocLine* pcDocLine) const;
	void SetLineFolded(CDocLine* pcDocLine, bool bFlag);
	bool GetLineFoldHidden(const CDocLine* pcDocLine) const;
	void SetLineFoldHidden(CDocLine* pcDocLine, bool bFlag);
	int  GetLineFoldEndLine(const CDocLine* pcDocLine) const;
	void SetLineFoldEndLine(CDocLine* pcDocLine, int nEndLine);
	int  GetLineFoldNameCol(const CDocLine* pcDocLine) const;
	void SetLineFoldNameCol(CDocLine* pcDocLine, int nCol);
	int  GetLineFoldNameLen(const CDocLine* pcDocLine) const;
	void SetLineFoldNameLen(CDocLine* pcDocLine, int nLen);
	int  GetLineFoldArgsCol(const CDocLine* pcDocLine) const;
	void SetLineFoldArgsCol(CDocLine* pcDocLine, int nCol);
	int  GetLineFoldArgsLen(const CDocLine* pcDocLine) const;
	void SetLineFoldArgsLen(CDocLine* pcDocLine, int nLen);
	bool GetLineFoldIsDeclaration(const CDocLine* pcDocLine) const;
	void SetLineFoldIsDeclaration(CDocLine* pcDocLine, bool bFlag);

	//一括操作
	void ResetAllFoldMark(CDocLineMgr* pcDocLineMgr);	// 折りたたみ情報をすべてリセット(非折りたたみ状態に戻す)
};

#endif /* SAKURA_CFOLDMANAGER_H_ */
/*[EOF]*/
