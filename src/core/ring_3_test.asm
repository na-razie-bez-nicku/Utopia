bits 64 

default rel

global ring_3_program_start
global ring_3_program_end
global ring_3_program 

ring_3_program_start:

ring_3_program:
    mov rax, 1
    mov rdi, 1
    mov rsi, msg 
    mov rdx, msg_len
    
    syscall
.loop:
    jmp .loop 

msg db "it works lets goooooooo" 
msg_len equ $ - msg

ring_3_program_end:
