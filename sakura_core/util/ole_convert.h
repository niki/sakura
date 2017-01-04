/*!	@file
	@brief OLE型（VARIANT, BSTRなど）の変換関数

*/
#ifndef SAKURA_OLE_CONVERT_208FE8C1_C742_4ED8_9C9C_25841915706BD_H_
#define SAKURA_OLE_CONVERT_208FE8C1_C742_4ED8_9C9C_25841915706BD_H_

#include <string>
#include "_os/OleTypes.h"

bool variant_to_wstr( VARIANT v, std::wstring& wstr );	// VARIANT変数をBSTRとみなし、wstringに変換する
bool variant_to_astr( VARIANT v, std::string& astr );	// VARIANT変数をBSTRとみなし、stringに変換する
bool variant_to_int( VARIANT v, int& n );	// VARIANT変数を整数とみなし、intに変換する

inline bool variant_to_auto_str( VARIANT v, std::wstring& wstr )
{
	return variant_to_wstr( v, wstr );
}

inline bool variant_to_auto_str( VARIANT v, std::string& astr )
{
	return variant_to_astr( v, astr );
}

#endif /* SAKURA_OLE_CONVERT_208FE8C1_C742_4ED8_9C9C_25841915706BD_H_ */
/*[EOF]*/
