#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
コマンドパレットに実在する全コマンド文字列(sakura_rc.rcのSTRINGTABLE、F_*)の
漢字のうち、g_aMultiMoraKanjiTable(手動で検証済みの上書きテーブル)に登録が無い
ものについて、NKMM_COMMAND_PALETTE_ROMAJI_KANJI_JIS1TABLEを有効にした場合の
自動選択結果(FindReadingFromJIS1Table()と同じルール: 最初の訓読みの活用語幹、
無ければ最初の音読み)が、現在使われている単独モーラ辞書(CKanjiReadingDict)の
バケットと食い違う漢字を洗い出す。

「食い違う」= 有効化すると今と挙動が変わる漢字、という意味。挙動が変わること
自体が即バグとは限らない(むしろ改善のことも多い)が、レビューせず有効化すると
気づかないまま退行するおそれがあるため、確認すべき候補として一覧化する。

使い方:
    python tools/audit_kanji_readings.py

このリポジトリのルートから実行する想定(パスはリポジトリルート基準の相対パス)。
"""

import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
RC_PATH = REPO_ROOT / "sakura_core" / "sakura_rc.rc"
FUZZY_CPP_PATH = REPO_ROOT / "sakura_core" / "util" / "CFuzzyMatchJp.cpp"
JIS1_HEADER_PATH = REPO_ROOT / "sakura_core" / "util" / "CKanjiReadingTableJIS1.h"
READING_DICT_PATH = REPO_ROOT / "sakura_core" / "util" / "CKanjiReadingDict.cpp"

HIRA_START, HIRA_END = 0x3041, 0x3096
KATA_START, KATA_END = 0x30A1, 0x30F6
KATA_TO_HIRA_OFFSET = 0x60


def load_command_strings():
	"""sakura_rc.rc(UTF-16LE)からF_*のSTRINGTABLE文字列を全件取り出す。"""
	data = RC_PATH.read_bytes().decode("utf-16")
	pat = re.compile(r'^\s*(F_[A-Za-z0-9_]+)\s+"(.*)"\s*$')
	entries = []
	for line in data.split("\n"):
		m = pat.match(line)
		if m:
			entries.append((m.group(1), m.group(2)))
	return entries


def load_manual_overrides():
	"""g_aMultiMoraKanjiTableに登録済みの漢字の集合を取り出す。"""
	data = FUZZY_CPP_PATH.read_text(encoding="utf-8-sig")
	return set(re.findall(r"\{ L'(.)', L\"", data))


def load_jis1_table():
	"""CKanjiReadingTableJIS1.hから{漢字: (onyomi, kunyomi)}を取り出す。"""
	data = JIS1_HEADER_PATH.read_text(encoding="utf-8-sig")
	pat = re.compile(r'\{ L\'(.)\', L"([^"]*)", L"([^"]*)" \}')
	table = {}
	for m in pat.finditer(data):
		table[m.group(1)] = (m.group(2), m.group(3))
	return table


def load_reading_dict_bucket():
	"""CKanjiReadingDict.cppの単独モーラ辞書から{漢字: バケットのモーラ}を取り出す。"""
	data = READING_DICT_PATH.read_text(encoding="utf-8-sig")
	pat = re.compile(r"\{ L'(.)', L\"([^\"]*)\" \}")
	kanji_to_mora = {}
	for m in pat.finditer(data):
		mora, kanjis = m.group(1), m.group(2)
		for k in kanjis:
			kanji_to_mora[k] = mora
	return kanji_to_mora


def kata_to_hira(ch: str) -> str:
	code = ord(ch)
	if KATA_START <= code <= KATA_END:
		return chr(code - KATA_TO_HIRA_OFFSET)
	return ch


def simulate_jis1_reading(onyomi: str, kunyomi: str) -> str:
	"""FindReadingFromJIS1Table()と同じ選択ルールをPython側で再現する。"""
	if kunyomi:
		first = kunyomi.split("・", 1)[0]
		return first.split("-", 1)[0]
	if onyomi:
		first = onyomi.split("・", 1)[0]
		return "".join(kata_to_hira(ch) for ch in first)
	return ""


def is_kanji(ch: str) -> bool:
	return "一" <= ch <= "鿿"


def main():
	entries = load_command_strings()
	manual = load_manual_overrides()
	jis1 = load_jis1_table()
	bucket = load_reading_dict_bucket()

	kanji_to_strings = {}
	for _id, s in entries:
		for ch in s:
			if is_kanji(ch) and ch not in manual:
				kanji_to_strings.setdefault(ch, set()).add(s)

	no_data = []     # JIS1テーブルにすら無い漢字
	changed = []     # JIS1TABLE有効化で先頭モーラが変わる漢字(要レビュー)
	unchanged = 0    # 先頭モーラが変わらない漢字(現状維持、低リスク)

	for kanji, strings in sorted(kanji_to_strings.items()):
		entry = jis1.get(kanji)
		if entry is None:
			no_data.append((kanji, strings))
			continue
		onyomi, kunyomi = entry
		simulated = simulate_jis1_reading(onyomi, kunyomi)
		current_mora = bucket.get(kanji, "")
		if not simulated:
			continue
		if simulated[0] != current_mora:
			changed.append((kanji, onyomi, kunyomi, current_mora, simulated, strings))
		else:
			unchanged += 1

	total_kanji = len(set(ch for _id, s in entries for ch in s if is_kanji(ch)))
	print(f"コマンドパレットの漢字: 全{total_kanji}字中、手動未登録{len(kanji_to_strings)}字")
	print(f"  うちJIS1TABLE有効化で先頭モーラが変わらない(低リスク): {unchanged}字")
	print(f"  うちJIS1テーブルにデータが無い: {len(no_data)}字")
	print(f"  うちJIS1TABLE有効化で先頭モーラが変わる(要レビュー): {len(changed)}字")
	print()

	if no_data:
		print(f"# JIS1テーブルにデータが無い漢字 ({len(no_data)}字)")
		for kanji, strings in no_data:
			sample = " / ".join(sorted(strings)[:2])
			print(f"  {kanji}: {sample}")
		print()

	print(f"# JIS1TABLE有効化で挙動が変わる漢字 ({len(changed)}字。"
		"変わる=即バグとは限らないが要確認)")
	for kanji, onyomi, kunyomi, current_mora, simulated, strings in changed:
		sample = " / ".join(sorted(strings)[:2])
		print(f"  {kanji}\t現在:{current_mora}\t有効化後:{simulated}\t"
			f"(音:{onyomi} 訓:{kunyomi})\t{sample}")


if __name__ == "__main__":
	main()
