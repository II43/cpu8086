; First program - Hello, World!
; Inspired by: https://www.itnetwork.cz/assembler/zaklady/assembler-prvni-program-hello-world

bits 16
org 0x0100

main:
    mov ax, cs
    mov ds, ax

    mov si, message
    call print_string

    mov si, newline
    call print_string

    mov cl, 0x00
count:
    inc cl
    mov al, cl
    add al, 0x30
    mov ah, 0x0E
    int 0x10
    mov si, newline
    call print_string
    cmp cl, 0x09
    jb count

quit_program:
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
newline db  0x0d, 0x0a, 0x00