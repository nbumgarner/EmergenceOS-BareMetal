.set MAGIC,    0xe85250d6
.set ARCH,     0
.set LENGTH,   multiboot_header_end - multiboot_header
.set CHECKSUM, -(MAGIC + ARCH + LENGTH)

.section .multiboot
.align 8
multiboot_header:
    .long MAGIC
    .long ARCH
    .long LENGTH
    .long CHECKSUM
    .align 8
    .short 1
    .short 0
    .long 12
    .long 8     /* Request Framebuffer */
    .align 8
    .short 5
    .short 0
    .long 20
    .long 1024
    .long 768
    .long 32
    .align 8
    .short 0
    .short 0
    .long 8
multiboot_header_end:

.section .bss
.align 4096
p4_table: .skip 4096
p3_table: .skip 4096
p2_table: .skip 16384 /* 4GB map */
stack_bottom: .skip 16384
stack_top:

.section .rodata
.global gdt64
gdt64:
    .quad 0
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)
.global gdt64_ptr
gdt64_ptr:
    .short . - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
_start:
    cli
    /* PRESERVE MB2 REGS IMMEDIATELY */
    mov %eax, %edi /* rdi = magic */
    mov %ebx, %esi /* rsi = info */

    mov $stack_top, %esp

    /* 1. SETUP PAGING */
    mov $p3_table, %eax
    or $3, %eax
    mov %eax, (p4_table)

    mov $0, %ecx
.fill_p3:
    mov $p2_table, %eax
    mov %ecx, %edx
    shl $12, %edx
    add %edx, %eax
    or $3, %eax
    mov %eax, p3_table(,%ecx,8)
    inc %ecx
    cmp $4, %ecx
    jne .fill_p3

    mov $0, %ecx
.map_p2:
    mov $0x200000, %eax
    imul %ecx, %eax
    or $0x83, %eax
    mov %eax, p2_table(,%ecx,8)
    inc %ecx
    cmp $2048, %ecx
    jne .map_p2

    /* 2. ENABLE PAE */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* 3. SET P4 */
    mov $p4_table, %eax
    mov %eax, %cr3

    /* 4. ENABLE SSE */
    mov %cr0, %eax
    and $0xFFFB, %ax  /* clear coprocessor emulation CR0.EM */
    or $0x2, %ax      /* set coprocessor monitoring  CR0.MP */
    mov %eax, %cr0
    mov %cr4, %eax
    or $0x600, %eax   /* set CR4.OSFXSR and CR4.OSXMMEXCPT */
    mov %eax, %cr4

    /* 5. ENABLE LONG MODE */
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    /* 5. ENABLE PAGING */
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0

    /* 6. LOAD GDT & JUMP */
    lgdt (gdt64_ptr)
    ljmp $0x08, $_start64

.code64
_start64:
    mov $0, %ax
    mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov $stack_top, %rsp
    
    /* MAGIC/INFO ARE ALREADY IN EDI/ESI */
    call kmain

    cli
1:  hlt
    jmp 1b

.global exception_handler_ud
exception_handler_ud:
    push %rax
    push %rbx
    mov %rsp, %rax
    add $16, %rax   /* Pointer to saved RAX */
    mov %rsp, %rdx
    add $8, %rdx    /* Pointer to saved RBX */
    push %rcx       /* Padding */
    lea 24(%rsp), %rdi /* Pointer to RIP */
    mov %rax, %rsi     /* Pointer to saved RAX */
    call resolve_hic
    pop %rcx
    pop %rbx
    pop %rax
    iretq

.global exception_handler_gp
exception_handler_gp: 
    pop %rsi /* Error code */
    mov (%rsp), %rdi /* RIP is now at the top of the stack */
    call handle_gp_fault
    cli
    hlt

.global exception_handler_nm
exception_handler_nm:
    lea (%rsp), %rdi
    call handle_nm_fault
    iretq

.global exception_handler_df
exception_handler_df: cli; hlt

.global timer_interrupt_handler
timer_interrupt_handler:
    push %rax
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    call handle_timer_pulse
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rax
    
    /* EOI (End of Interrupt) to PIC */
    push %rax
    mov $0x20, %al
    out %al, $0x20
    pop %rax
    
    iretq

.global physical_opcode_stub
physical_opcode_stub:
    push %rbp
    mov %rsp, %rbp
    # rdi contains the opcode byte
    # We construct the 0x0F [op] ret sequence in a local buffer or just switch
    # For bare-metal robustness, we use a simple dispatch
    cmp $0xFF, %rdi
    je .do_ff
    # Fallback for generic testing
    .byte 0x0F
    .byte 0x0B /* UD2 default */
    jmp .done
.do_ff:
    .byte 0x0F, 0xFF
.done:
    pop %rbp
    ret
