; アセンブラ(MASM) 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL)
.386
.model flat, stdcall

main proc
    mov eax, 123        ; 10進数
    mov ebx, 0FFh        ; 16進数(先頭0が必要、末尾h) ← 新規対応
    mov ecx, 1Ah         ; 16進数(先頭が10進数字)
    mov edx, 0BEEFh       ; Bで始まる16進数(2進数サフィックスと紛れやすい例) ← 新規対応

    mov esi, 17o          ; 8進数(末尾o) ← 新規対応
    mov edi, 17q           ; 8進数(末尾q)

    mov eax, 1010b          ; 2進数(末尾b) ← 新規対応
    mov ebx, 1010y            ; 2進数(末尾y)

    mov ecx, 123d               ; 明示的な10進数サフィックス(省略可) ← 新規対応
    ret
main endp
end main
