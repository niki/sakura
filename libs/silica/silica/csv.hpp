// -*- mode:c++; coding:utf-8-ws -*-
//====================================================================
//! @file  csv.hpp
//! @brief CSV操作
//!
//! @author (C) 2017, Niki.
//====================================================================
#ifndef SILICA_CSV_HPP
#define SILICA_CSV_HPP

#include "basis.h"
#include "util.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

namespace si {

//==================================================================
// CSV
//==================================================================
namespace csv {

class Csv {
public:
	//------------------------------------------------------------------
	// 列クラス
	//------------------------------------------------------------------
	class Column {
	public:
		explicit Column(const std::string &s) : line_buffer_(s) {}

		//------------------------------------------------------------------
		// ｎ番目の列を取得
		//------------------------------------------------------------------
		std::string GetX(int n) {
			std::string token;
			std::istringstream stream(line_buffer_);

			for (int i = 0; i <= n; i++) {
				getline(stream, token, ',');
			}

			return token;
		}

	private:
		std::string line_buffer_;
	};

public:
	Csv() : lines_() {}
	explicit Csv(const std::string &fname, bool writeMode = false) : Csv() { Open(fname, writeMode); }

	//------------------------------------------------------------------
	// CSVファイルを開く
	//------------------------------------------------------------------
	bool Open(const std::string &fname, bool writeMode = false) {
		if (writeMode) {
			ofs_.open(fname.c_str(), std::ios::out | std::ios::trunc);
		} else {
			ifs_.open(fname.c_str(), std::ios::in);

			// 内容を読み込む
			std::string line_buffer;
			while (std::getline(ifs_, line_buffer)) {
				if (line_buffer.empty() || line_buffer.at(0) == '#') {
					continue;
				}

				lines_.push_back(Column(line_buffer));
			}
		}

		return true;
	}

	//------------------------------------------------------------------
	// 行配列を取得
	//------------------------------------------------------------------
	std::vector<Column> GetLines() { return lines_; }

private:
	std::ifstream ifs_;
	std::ofstream ofs_;

	std::vector<Column> lines_;
};

} /* namespace of csv */

} /* namespace of si */

#endif /* SILICA_UTIL_HPP */
