// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  split_string.h
//! @brief 文字列分割
//!
//! @author
//====================================================================
#ifndef SPLIT_STRING_H
#define SPLIT_STRING_H

#include <vector>

namespace si {

namespace text {

enum {
	kIgnoreToken = (1 << 0),
};

/*!
 * 分割文字列クラス
 */
class SplitString {
public:
	SplitString() {}
	SplitString(const std::tstring &text, TCHAR delimiter) { Split(text, delimiter); }
	~SplitString() {}

	/*!
	 * トークンの個数を取得
	 * @return トークンの個数 (無視されたトークンは含みません)
	 */
	int Size()
	{
		int c = 0;

		for (int i = 0; i < tokens_.size(); i++) {
			if ((flag_[i] & kIgnoreToken) == 0) {
				c++;
			}
		}

		return c;
	}

	/*!
	 * トークンを取得
	 * @param index インデックス (無視されたトークンはスキップします)
	 * @return トークン
	 */
	std::tstring &At(int index)
	{
		int c = 0;

		for (int i = 0; i < tokens_.size(); i++) {
			if (flag_[i] & kIgnoreToken) {
				continue;
			}

			if (c == index) {
				return tokens_[i];
			}

			c++;
		}
	}
	std::tstring operator[](int index) { return At(index); }

	/*!
	 * トークンの無視設定
	 * @param index インデックス (無視されたトークンはスキップします)
	 */
	void IgnoreToken(int index)
	{
		int c = 0;

		for (int i = 0; i < tokens_.size(); i++) {
			if (flag_[i] & kIgnoreToken) {
				continue;
			}

			if (c == index) {
				flag_[i] |= kIgnoreToken;
				return;
			}

			c++;
		}
	}

	/*!
	 * 分割
	 * @param text テキスト (NULLの場合はテキストを更新しない)
	 * @param delimiter デリミタ
	 */
	void Split(const std::tstring &text, TCHAR delimiter)
	{
		if (text.empty()) return;

		TCHAR *temp = new TCHAR[text.size() + 1];
		TCHAR *del_temp = temp;
		_tcscpy_s(temp, text.size() * sizeof(TCHAR), text.c_str());
		temp[text.size()] = 0;

		TCHAR *entry = temp;

		while (*temp) {
			TCHAR ch = *temp;

			if (ch == delimiter) {
				*temp = 0;
				tokens_.push_back(entry);

				temp++;
				entry = temp;
			}
			else {
				temp++;
			}
		}

		if (entry < temp) {
			tokens_.push_back(entry);
		}

		delete[] del_temp;
	}

private:
	std::vector<std::tstring> tokens_;
	uint32_t flag_[256] = {};
};

} // namespace text

} // namespace si

#endif // SPLIT_STRING_H
