;-------------------------------------------------------------------------------
;  ______                            ______                 _
; (_____ \                          |  ___ \               | |
;  _____) )___   _ _ _   ____   ___ | | _ | |  ___   ____  | |  _  ____  _   _
; |  ____// _ \ | | | | / _  ) / __)| || || | / _ \ |  _ \ | | / )/ _  )| | | |
; | |    | |_| || | | |( (/ / | |   | || || || |_| || | | || |< (( (/ / | |_| |
; |_|     \___/  \____| \____)|_|   |_||_||_| \___/ |_| |_||_| \_)\____) \__  |
;                                                                       (____/
; Copyright (C) 2021-2022 Ivan Dimkovic. All rights reserved.
;
; All trademarks, logos and brand names are the property of their respective
; owners. All company, product and service names used are for identification
; purposes only. Use of these names, trademarks and brands does not imply
; endorsement.
;
; SPDX-License-Identifier: Apache-2.0
; Full text of the license is available in project root directory (LICENSE)
;
; WARNING: This code is a proof of concept for educative purposes. It can
; modify internal computer configuration parameters and cause malfunctions or
; even permanent damage. It has been tested on a limited range of target CPUs
; and has minimal built-in failsafe mechanisms, thus making it unsuitable for
; recommended use by users not skilled in the art. Use it at your own risk.
;
;-------------------------------------------------------------------------------

%define MAGIC_HIDWORD                                               0xbaad0000
%define MAGIC_LODWORD                                               0xdeadc0de
%define MAGIC_MASK32HI                                              0xffff0000
%define MAGIC_MASKVECHI                                             0x000000ff

    BITS 64
    DEFAULT REL
    STACKSPACE  equ 0x8000
    
    section .text
    ALIGN 8

;------------------------------------------------------------------------------
;  This is the common Interrupt Service Routine that will handle all hooked
;  IRQs. Its job is to call the appropriate C handler (if any) and return the
;  CPU back to execution after the faulting instruction + signal to the code
;  resuming that the fault occured so it can handle it as "caught"
;
;  Optional C handler can do something like log, print, etc. We will then
;  also proceed to hijack CR2 register, as we are running on bare metal with
;  no paging (so a page fault would be most unusual), and use CR2 to signal to
;  our 'enlightened' routine that it messed up... it can then (maybe) clean
;  itself up and continue pretending this never happened. 
;
;  But since we are talking about potentially bringing CPU out of its safe
;  operating limits, or poking something that just triggers an uncorrectable 
;  fault (or an undocumented instruction? :-) one simply cannot reply on this
;  to be always succesful, or correct! Sometimes, system might just die hard.
;
;  ---------------------------------------------------------------------------
;
;  CR2 Heist - poor man's "poke" protection (hardware debugger is way better)
;
;  Because this code runs on bare metal with no underlying OS to interfere,
;  one can simply steal CR2 like a smooth criminal, and use it on their own.
;  There is a small chance that aggressive undervolt might trigger a page
;  fault where it should never happen. But in that case you are doomed anyway 
;  so no biggie, that session is already FUBAR and reboot is the only exit.
;
;  We will use CR2 as follows: when exception occurs, our ISR will use CR2 to
;  store information about exception vector and performed action in CR2, using 
;  the following encoding:
;
;           63              47      40      31                              0
;           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
;  OFFSETS  |     63..48    | 47.40 | 39.32 |             31.0              |
;           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
;   FIELDS  |    RESERVED   | PARAM |  VEC  |        MAGIC SIGNATURE        |
;           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
;  EXAMPLE  |      BAAD     |  00   |  06   |           DEADC0DE            |
;           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
; REGISTER  |              HIJAC..."BORROWED" CR2 (64 BIT)                  |
;           +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
;
;  MEANING   Invalid Opcode Exception (0x6) caught, no action taken by ISR
;
;------------------------------------------------------------------------------

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void get_current_idtr(void *pidtr)
;
; DESCRIPTION:
;
;   Returns current IDTR
;
;------------------------------------------------------------------------------

        global get_current_idtr
        get_current_idtr:

        ;
        ; Store current IDT

        sidt [rcx]
        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void stop_interrupts_on_this_cpu
;
; DESCRIPTION:
;
;       Portable solution (in an "operating environment" sense) as UEFI
;       compiler flags prohibit _enable() / _disable() x86 intrinsics
;
;------------------------------------------------------------------------------

        global stop_interrupts_on_this_cpu
        stop_interrupts_on_this_cpu:
        ;
        ; Stop interrupts and return

        cli
        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void resume_interrupts_on_this_cpu
;
; DESCRIPTION:
;
;       Portable solution (in an "operating environment" sense) as UEFI
;       compiler flags prohibit _enable() / _disable() x86 intrinsics
;
;------------------------------------------------------------------------------

        global resume_interrupts_on_this_cpu
        resume_interrupts_on_this_cpu:
        ;
        ; Resume interrupts and return

        sti
        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void hlp_atomic_increment_u64
;
; DESCRIPTION:
;
;   Increases 64-bit variable atomically and returns the new value
;
;------------------------------------------------------------------------------

        global hlp_atomic_increment_u64
        hlp_atomic_increment_u64:

        mov rax, rcx
        
        cli
        
        lock inc qword [rax]
        mov rax, [rax]
        
        sti
        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void hlp_atomic_increment_u32
;
; DESCRIPTION:
;
;   Increases 32-bit variable atomically and returns the new value
;
;------------------------------------------------------------------------------

        global hlp_atomic_increment_u32
        hlp_atomic_increment_u32:

        mov r8, rcx
        
        cli
        
        lock inc dword [r8]
        mov eax, dword [r8]
        
        sti
        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void hlp_atomic_decrement_u64
;
; DESCRIPTION:
;
;   Decrement 64-bit variable atomically and returns the new value
;
;------------------------------------------------------------------------------

        global hlp_atomic_decrement_u64
        hlp_atomic_decrement_u64:

        mov r8, rcx
        
        cli
        
        lock dec qword [r8]
        mov rax, [r8]
        
        sti
        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   void hlp_atomic_decrement_u32
;
; DESCRIPTION:
;
;   Increases 32-bit variable atomically and returns the new value
;
;------------------------------------------------------------------------------

        global hlp_atomic_decrement_u32
        hlp_atomic_decrement_u32:

        mov r8, rcx
        
        cli
        
        lock dec dword [r8]
        mov eax, dword [r8]
        
        sti
        ret

;------------------------------------------------------------------------------
; In order not to repeat boilerplate code by hand, we need three macros:
;
; (1) - for handling exceptions with no error code pushed to ISR invocation
; (2) - for handling exceptions where error code was already pushed
; (3) - for remaining interrupts, which we do not intend to hijack here (yet)
;------------------------------------------------------------------------------

;
; Exceptions where error code is not pushed to us

%macro EXCEPT_ISR_ERRCODE_ABSENT 1
    global monkey_isr_ %+ %1
    monkey_isr_ %+ %1 :
        align 8
        push rax
        mov rax, %1
        mov cr2, rax
        pop rax
        push qword 0               ; Dummy error code to maintain aligment
        jmp safer_isr_common       ; CR2 temporarily holds exception vector #
%endmacro

;
; Exceptions where error code is pushed

%macro EXCEPT_ISR_ERRCODE_PUSHED 1
    global monkey_isr_ %+ %1
    monkey_isr_ %+ %1 :
        align 8
        push rax
        mov rax, %1
        mov cr2, rax
        pop rax
        jmp safer_isr_common       ; CR2 temporarily holds exception vector #
%endmacro

;
; Other interrupts

%macro ISR_GENERIC_INTERRUPT 1
    global monkey_isr_ %+ %1
    monkey_isr_ %+ %1 :
        align 8
        push rax
        mov rax, %1
        mov cr2, rax
        pop rax 
        push qword 0                ; Dummy error code
        jmp safer_isr_common
%endmacro


;
; Error Handler (isError state stored on reg)

%macro ERROR_HANDLER_REG 2
    %1 %+ _err_handler :
    mov   r8, cr2
    cmp   r8d, MAGIC_LODWORD
    jne   %1 %+ _noerr

    xor   r8, r8
    mov   cr2, r8
    mov   %2, 1                               ; error flag to reg %2
    jmp   %1 %+ _done
%endmacro

;
; Error Handler (isError state stored in memory pointed by [reg])

%macro ERROR_HANDLER_MEM 2
    %1 %+ _err_handler :
    mov   r8, cr2
    cmp   r8d, MAGIC_LODWORD
    jne   %1 %+ _noerr

    xor   r8, r8
    mov   cr2, r8
    mov   [%2], 1                               ; error flag to [%2]
    jmp   %1 %+ _done
%endmacro

;------------------------------------------------------------------------------
;  TEMPLATED INTERRUPT SERVICE ROUTINE STUBS
;  Generated by three macros written just above, will be entry points for
;  their respective IRQs 
;------------------------------------------------------------------------------

;
; 0x00 - 0x20 = Exceptions your CPU could throw at you...

EXCEPT_ISR_ERRCODE_ABSENT       0       ; #DE - Division by Zero
EXCEPT_ISR_ERRCODE_ABSENT       1       ; #DB - Debug
EXCEPT_ISR_ERRCODE_ABSENT       2       ; *** - Non-maskable Interrupt
EXCEPT_ISR_ERRCODE_ABSENT       3       ; #BP - Breakpoint 
EXCEPT_ISR_ERRCODE_ABSENT       4       ; #OF - Overflow
EXCEPT_ISR_ERRCODE_ABSENT       5       ; #BR - Bound Range Exceeded
EXCEPT_ISR_ERRCODE_ABSENT       6       ; #UD - Invalid Opcode
EXCEPT_ISR_ERRCODE_ABSENT       7       ; #NM - Device Not Available
EXCEPT_ISR_ERRCODE_PUSHED       8       ; #DF - Double Fault
EXCEPT_ISR_ERRCODE_ABSENT       9       ; *** - Nobody cares anymore
EXCEPT_ISR_ERRCODE_PUSHED       10      ; #TS - Invalid TSS
EXCEPT_ISR_ERRCODE_PUSHED       11      ; #NP - Segment not present
EXCEPT_ISR_ERRCODE_PUSHED       12      ; #SS - Stack Segment Fault
EXCEPT_ISR_ERRCODE_PUSHED       13      ; #GP - General Protection Fault
EXCEPT_ISR_ERRCODE_PUSHED       14      ; #PF - Page Fault
EXCEPT_ISR_ERRCODE_ABSENT       15      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       16      ; #MF - x87 Floating Point Exception
EXCEPT_ISR_ERRCODE_ABSENT       17      ; #AC - Aligment Check
EXCEPT_ISR_ERRCODE_ABSENT       18      ; #MC - Machine Check
EXCEPT_ISR_ERRCODE_ABSENT       19      ; #XM or #XF - SIMD FP Exception
EXCEPT_ISR_ERRCODE_ABSENT       20      ; #VE - Virtualization Exception 
EXCEPT_ISR_ERRCODE_ABSENT       21      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       22      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       23      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       24      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       25      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       26      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       27      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       28      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       29      ; *** - Reserved
EXCEPT_ISR_ERRCODE_ABSENT       30      ; #SX - Security Exception
EXCEPT_ISR_ERRCODE_ABSENT       31      ; *** - Reserved

;
; 0x20 - 0xFF = Other interrupts
; Although we do not anticipate having to deal with them in this app

%assign i 20h 
%rep    100h-20h 
        ISR_GENERIC_INTERRUPT i
%assign i i+1 
%endrep

;------------------------------------------------------------------------------
;  Common ISR "Sink"
;
;  Below code is reached by all above boilerplate ISR stubs. It will save
;  registers on stack, encode information about the exception in borrowed CR2 
;  and call C ISR (if installed). After that, it will restore registers as they 
;  were and return execution to the offending code to handle its error state
;------------------------------------------------------------------------------

align   8

safer_isr_common:

        ;
        ; Save Registers

        push rax
        push rcx
        push rdx
        push rbx
        push rbp
        push rsi
        push rdi
        push r8
        push r9
        push r10
        push r11
        push r12
        push r13
        push r14
        push r15
        push qword 0       ; Maintain 16-byte aligment
                           ; OK to remove if we also remove 'CR2' padding

        xor rax, rax
        mov ax, ds         ; DS
        push rax

        mov ax, es         ; ES
        push rax

        push fs            ; FS
        push gs            ; GS

        push qword 0       ; We will not push CR2, as we stole it

        mov rax, cr3       ; CR3
        push rax

        ;
        ; And now, encode error for the faulty routine:
        ; we use CR2 to do that

        mov rax, cr2
        and eax, MAGIC_MASK32HI
        or  eax, MAGIC_HIDWORD
        shl rax, 32
        mov eax, MAGIC_LODWORD
        mov cr2, rax

        ;
        ; Call registered C ISR handler
        ; (if one exists)

        mov r10, safer_c_isr_fptr

        test r10, r10
        jz no_c_handler_ptr

        mov rdi, rsp                      ; RDI points to saved regs
        call r10

no_c_handler_ptr:

        ;
        ; Cleanup before we attempt to 
        ; resume normal operations:
        ; Restore context from stack

        add rsp, 8           ; Ignore padding added for aligment
        pop rax              ; Discard CR2, we stole it for us

        pop rax
        mov cr3, rax          ; CR3

        pop gs                ; GS
        pop fs                ; FS
     
        pop rax
        mov es, ax            ; ES

        pop rax
        mov ds, ax            ; DS

        pop r15
        pop r14
        pop r13
        pop r12
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdi
        pop rsi
        pop rbp
        pop rbx
        pop rdx
        pop rcx
        pop rax

        ;
        ; RETURN TO THE DARKNESS...
        ;
        ; Works only because crash-generating code is "enlightened" enough 
        ; for basic error recovery. This is >not< the way to handle faults
        ; in other circumstances...

        iretq

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   uint32_t get_pciex_base_addr(void);
;
; DESCRIPTION:
;
;   Gets the base address of PCIe Configuration Space ('ECAM')
;
; RETURN:
;
;   [EAX] Address of the beginning of PCIe Configuration Space
;
; FURTHER INFO:
;
;   https://wiki.osdev.org/PCI_Express - oh and yes, we are doing it the OG
;   way here using the good old PCI cfg. address space I/O ports CF8/CFC
;   because I am an old f4rt. Cool kidz can use ACPI MCFG table (Cool? ACPI?)
;   or find some GUIDs and UEFI protocols if they exist... which is likely.
;
;------------------------------------------------------------------------------

        global get_pciex_base_addr
        get_pciex_base_addr:

        push rdx

        cli
    
        mov eax, 0x80000060
        mov dx, word 0x0cf8
        out dx, eax

        mov dx, word 0x0cfc
        in eax, dx
    
        sti
    
        pop rdx

        ret

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   uint64_t safer_rdmsr64(const uint32_t msr_idx, uint32_t *is_err)
;
; DESCRIPTION:
;
;   Read MSR register and return 64-bit result value. In case of exception
;   (e.g. invalid / unsupported MSR), error flag will be set to 1
;
; ARGUMENTS:
;
;   [RCX] Index of the MSR to read (only lower 32-bits will be used)
;   [RDX] Optional pointer to "is_err" variable that will receive status
;
; RETURN:
;
;   [RAX] 64-bit value read from the MSR.
;
; FURTHER INFO:
;
;   This routine will attempt to catch exceptions resulting from attempts to
;   read invalid/unsupported MSRs. If such error is detected, it will return
;   grafefully with error status. Please note that it is not possible to 
;   guarantee that every invalid attempt will be caught. The purpose of this
;   approach is to reduce number of crashes / hangs when probing "somewhat
;   known" hw. resources. Its job is >not< to be a failsafe for a fuzzer. Or?
;
;------------------------------------------------------------------------------

        global safer_rdmsr64
        safer_rdmsr64:
    
        xor     r10d, r10d                          ; Local "is_err"

        rdmsr
    
        ;
        ; ERROR RECOVERY

        mov   r8, cr2
        cmp   r8d, MAGIC_LODWORD
        jne   rdmsr64_noerr

        xor   r8, r8
        mov   cr2, r8
        mov   r10d, 1                               ; error flag

rdmsr64_noerr:

        shl   rdx, 0x20
        or    rax, rdx

        test  edx, edx
        jz    rdmsr64_justret
    
        mov   dword [edx], r10d

rdmsr64_justret:

        ret                                         ; RAX = Returned MSR value

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   uint32_t safer_wrmsr64(const uint32_t msr_idx, const uint64_t value)
;
; DESCRIPTION:
;
;   Writes 64-bit value to the designated MSR, with error recovery (attempt)
;
; ARGUMENTS:
;
;   [RCX] Index of the MSR to write (only lower 32-bits will be used)
;   [RDX] 64-bit value to be written to the designated MSR
;
; RETURN:
;
;   [RAX] 0 if no errors occured, 1 if exception caught
;
; FURTHER INFO:
;
;   This routine will attempt to catch exceptions resulting from attempts to
;   read invalid/unsupported MSRs. If such error is detected, it will return
;   grafefully with error status. Please note that it is not possible to 
;   guarantee that every invalid attempt will be caught. The purpose of this
;   approach is to reduce number of crashes / hangs when probing "somewhat
;   known" hw. resources. Its job is >not< to be a failsafe for a fuzzer.
;
;------------------------------------------------------------------------------

        global safer_wrmsr64
        safer_wrmsr64:
    
        mov rax, rdx                  ; Distribute RDX to RAX
        shr rdx, 0x20                 ; [EDX:EAX] are now holding 64-bit value
    
        wrmsr

        ;
        ; ERROR RECOVERY

        ERROR_HANDLER_REG safer_wrmsr64, eax

safer_wrmsr64_noerr:

        xor eax, eax

safer_wrmsr64_done:
    
        ret                            ; [EAX] = Error code 

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   uint32_t safer_mmio_write32(const uint32_t addr, const uint32_t value)
;
; DESCRIPTION:
;
;   Writes a 32-bit value to the memory mapped I/O space
;
; ARGUMENTS:
;
;   [RCX:32] Address of the memory mapped I/O to write to (32-bit)
;   [RDX:32] 64-bit value to be written to the designated address (32-bit)
;
; RETURN:
;
;   [RAX:32] 0 if no errors occured, 1 if exception caught
;
; FURTHER INFO:
;
;   I am not sure if a MMIO can fail hard, but since we are dealing with the
;   CPU configuration, I wrote mmio_write to be error-correctible as well. 
;   In any case, there are no real costs of doing writes to MMIO this way, 
;   so why not(tm).
;
;------------------------------------------------------------------------------

        global safer_mmio_write32    
        safer_mmio_write32:
    
        mov esi, ecx
        and esi, 3                    ; Ensure that the address is aligned
        jnz safer_mmio_write32_done   ; "Soft" error, just refuse writing
    
        mov [ecx], edx                ; Perform 32-bit write

        ;
        ; ERROR RECOVERY

        ERROR_HANDLER_REG safer_mmio_write32, eax

safer_mmio_write32_noerr:    

        xor eax, eax                  ; Return "no error" (0)

safer_mmio_write32_done:

        ret                           ; [EAX] = Error code 

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   uint32_t safer_mmio_read32(const uint32_t addr, uint32_t *is_err)
;
; DESCRIPTION:
;
;   Reads a 32-bit value from the designated memory mapped I/O address
;
; ARGUMENTS:
;
;   [RCX:32] Memory Mapped I/O Address from which to read 32-bits
;   [RDX] Optional pointer to location where we can write status
;
; RETURN:
;
;   [RAX:32] 32-bit value read from the requested location
;
; FURTHER INFO:
;
;   Same as info for safer_mmio_write32
;
;------------------------------------------------------------------------------

        global safer_mmio_read32
        safer_mmio_read32:
    
        xor r9d, r9d                  ; Error flag

        mov esi, ecx
        and esi, 3                    ; Ensure that the address is aligned
        jnz smr_error
    
        mov eax, dword [ecx]          ; Perform read

        ;
        ; ERROR RECOVERY

        mov   r8, cr2
        cmp   r8d, MAGIC_LODWORD
        jne   smr_done

        xor   r8, r8
        mov   cr2, r8

smr_error:

        mov   r9d, 1                  ; error flag

smr_done:
    
        test rdx, rdx                 ; Do we have is_err ptr?
        jz smr_end
    
        mov dword [rdx], r9d

smr_end:

        ret                           ; [EAX] = Value

;------------------------------------------------------------------------------
;
; ROUTINE:
;
;   uint32_t safer_mmio_or32(const uint32_t addr, const uint32_t value)
;
; DESCRIPTION:
;
;   Performs 'logical or' between a 32-bit value from the designated  MMIO
;   address and caller supplier 32-bit value. Stores result of the operation
;   to the same MMIO adrress.
;
; ARGUMENTS:
;
;   [RCX:32] Memory Mapped I/O Address from which to read/write 32-bits
;   [RDX:32] 32-bit value to be used as second OR operand
;
; RETURN:
;
;   [RAX:32] 0 if no errors occured, 1 if exception caught
;
; FURTHER INFO:
;
;   Same as info for safer_mmio_write32
;
;------------------------------------------------------------------------------

        global safer_mmio_or32    
        safer_mmio_or32:
    
        mov esi, ecx
        and esi, 3                    ; Ensure that the address is aligned
        jnz smo_error

        mov edi, dword [ecx]          ; MMIO read
        or edi, edx                   ; result |= val
        mov dword [ecx], edi          ; MMIO write

        ;
        ; ERROR RECOVERY

        mov   r8, cr2
        cmp   r8d, MAGIC_LODWORD
        jne   smo_noerror

        xor   r8, r8
        mov   cr2, r8    

smo_noerror:    

        xor eax, eax                  ; Return "no error" (0)
        jmp smo_done

smo_error:    

        mov eax, 1

smo_done:

        ret                           ; [EAX] = Error code

;------------------------------------------------------------------------------
; memset override?
;
; This is to prevent EDK2 builds from failing. MSVC is dead set on creating
; object code that calls memset, which is non-existant in UEFI environment.
; 
; This is not the fastest (by far) method to do this, we use it because we are
; zaroing out <100-500 bytes of local stuff
;------------------------------------------------------------------------------
        
        ;
        ; memset(void* dst, int val, size_t cnt)
        ;
        ;   dst = RCX
        ;   val = EDX
        ;   cnt = R8

        global memset
        memset:
        
        push    rdi

        mov     eax, edx
        mov     rdi, rcx
        mov     r9, rcx
        mov     rcx, r8

        rep     stosb

        mov     rax, r9
        pop     rdi

        ret
;------------------------------------------------------------------------------

;  void
;  _pm_cpuid (
;    in  u32  func,
;    out u32 *regs          // EAX EBX ECX EDX, as array of 32-bit integers
;   );
;------------------------------------------------------------------------------

        global _pm_cpuid
        _pm_cpuid:

        push    rbx

        mov     r10, rdx
        mov     eax, ecx
        xor     ecx, ecx
        push    rax

        cpuid

        mov     [r10],    eax
        mov     [r10+4 ], ebx
        mov     [r10+8 ], ecx 
        mov     [r10+12], edx 
    
        pop     rax
        pop     rbx
        
        ret

;------------------------------------------------------------------------------
;  void
;  _pm_cpuid_ex (
;    in  u32  func,
;    in  u32  subfunc,
;    out u32 *regs           // EAX EBX ECX EDX, as array of 32-bit integers
;  );
;------------------------------------------------------------------------------

        global _pm_cpuid_ex
        _pm_cpuid_ex:

        push    rbx        
        mov     eax, ecx
        mov     ecx, edx
        push    rax

        cpuid

        mov     [r8],    eax
        mov     [r8+4 ], ebx
        mov     [r8+8 ], ecx 
        mov     [r8+12], edx 
    
        pop     rax
        pop     rbx
        ret

section .data
global safer_c_isr_fptr
safer_c_isr_fptr: dq 0
