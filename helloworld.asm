; First program - Hello, World!
; From: https://www.itnetwork.cz/assembler/zaklady/assembler-prvni-program-hello-world

bits 16
org 0x0100

main:
mov ax, cs
mov ds, ax

mov si, message
call print_string

ret

print_string:
mov ah, 0x0e
xor bh, bh

.print_char:
lodsb
or al, al
jz short .return
int 0x10
jmp short .print_char

.return:
ret

message db "Hello, World!", 0x00