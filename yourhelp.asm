; Assembled with NASM
; From: http://www.fysnet.net/yourhelp.htm

bits 16
org 0x0100

; push all registers
pushf               ; flags    
pusha               ; (ax,cx,dx,bx,sp,bp,si,di)
push cs             ; CS so I can verify DX
mov  di, buff       ;     
mov  si, msg1       ;
mov  cx,10          ; 10 registers to print

Loop1:     
movsw               ; "print" register name
mov  al,'='         ;  and =
stosb               ;
pop  ax             ; get next register value to print
mov  bx,04          ; "print" 4 hex digits
PLoop:     
rol  ax,04          ;
push ax             ;
and  al,0Fh         ;
daa                 ;
add  al,0F0h        ;
adc  al,40h         ;
stosb               ;
pop  ax             ;
dec  bx             ;
jnz  short PLoop    ;
mov  ax,0D0Ah       ; "print" CRLF
stosw               ;
loop Loop1          ; do CX times

; if your current DOS version does not support this function,
;  please change to print a valid string given in 'buff'
mov  al,24h         ; place eol marker
stosb               ;
mov  dx,buff        ; print the buffer
mov  ah,09          ;
int  21h            ;

; if your current DOS version does not support this function,
;  please change to exit to DOS
mov  ah,4Ch         ;
int  21h

msg1       db  'CSDISIBPSPBXDXCXAXFL'
buff: times 100 db 0
