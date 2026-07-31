/*! @file
	@brief libs/mimalloc/src/static.c 専用の ForcedIncludeFile

	NKMM_USE_MIMALLOC_OVERRIDE が有効なときだけ MI_MALLOC_OVERRIDE を定義し、
	malloc/free/calloc/realloc 等を mimalloc へ完全に上書きする。

	vendor元の libs/mimalloc/src/static.c は無改造のまま保つため、この定義だけを
	ここに切り出している。static.c は C としてコンパイルされ、プロジェクト全体の
	ForcedIncludeFiles(my.h、C++専用の silica を含む)は使えないため、C として安全な
	my_config.h だけを読み込む専用の ForcedIncludeFile として本ファイルを用意した。

	@date 20260731
*/
#include "my_config.h"

#if defined(NKMM_USE_MIMALLOC_OVERRIDE) && !defined(MI_MALLOC_OVERRIDE)
#define MI_MALLOC_OVERRIDE 1
#endif
