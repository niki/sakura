// ============================================================
// JScript版ベンチマークマクロ (WSHエンジン, 拡張子 .js)
//
// Benchmark.js / Benchmark.vbs / Benchmark.qjs は同じ処理内容
// (算術ループ・文字列連結・バブルソート)を行い、実行時間を
// InfoMsg で表示する。3エンジンの実行速度を比較するためのもの。
// ============================================================

var ARITH_N = 2000000;	// 算術ループの回数
var STR_N   = 20000;	// 文字列連結の回数
var SORT_N  = 2000;	// ソート対象配列の要素数

function now() {
	return new Date().getTime();
}

// 算術演算ループ
function benchArith() {
	var total = 0;
	for (var i = 1; i <= ARITH_N; i++) {
		var x = i * 3 - 7;
		if (x % 2 == 0) {
			total += x;
		} else {
			total -= x;
		}
	}
	return total;
}

// 文字列連結
function benchString() {
	var s = "";
	for (var j = 1; j <= STR_N; j++) {
		s += "x";
	}
	return s.length;
}

// バブルソート(降順配列を昇順に)
function benchSort() {
	var arr = new Array(SORT_N);
	for (var k = 0; k < SORT_N; k++) {
		arr[k] = SORT_N - k;
	}
	for (var a = 0; a < SORT_N - 1; a++) {
		for (var b = 0; b < SORT_N - 1 - a; b++) {
			if (arr[b] > arr[b + 1]) {
				var tmp = arr[b];
				arr[b] = arr[b + 1];
				arr[b + 1] = tmp;
			}
		}
	}
	return arr[0];
}

var t0 = now();
var rArith = benchArith();
var t1 = now();
var rString = benchString();
var t2 = now();
var rSort = benchSort();
var t3 = now();

var msg =
	"[JScript]\n" +
	"Arith : " + (t1 - t0) + " ms (result=" + rArith + ")\n" +
	"String: " + (t2 - t1) + " ms (len=" + rString + ")\n" +
	"Sort  : " + (t3 - t2) + " ms (arr[0]=" + rSort + ")\n" +
	"Total : " + (t3 - t0) + " ms";

InfoMsg(msg);
