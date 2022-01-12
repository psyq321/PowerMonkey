DEFAULT REL
BITS 64

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
; "ComboHell AVX2" - CPU Stressor kernel for undervolt or overclock testing
;
; Copyright (C) 2021 Ivan Dimkovic.
; 
; WARNING: THIS IS A PROOF OF CONCEPT CODE, PROVIDED FOR EDUCATION / RESEARCH
; PURPOSES. NO TESTS OR VALIDATIONS HAVE BEEN CARRIED OUT IN THE PRODUCTION
; ENVIRONMENT. USAGE COULD DAMAGE HARDWARE OR VOID WARRANTIES!!!
;-------------------------------------------------------------------------------

section .text
align 16

;-------------------------------------------------------------------------------
; combohell_avx2_kernel - theory of operation:
;
; Perform a tight loop of instructions designed to utilize as much of
; available EUs as possible (ALUs, AGUs, etc.) with minimal dependencies
; so that we reach very high efficiency % (target >99.5%)
;
; On top of that, we want the routine to serve as a validator of correct CPU
; operation - so we will need to use some data transformation that is possible
; to attest for correctness.
;-------------------------------------------------------------------------------

        global combohell_avx2_kernel
        combohell_avx2_kernel:

        push rsi

        ;
        ; Need to enable AVX on this CPU core, since we run on the bare metal
        ; and UEFI does not care about AVX. We will restore original state
        ; after we are done.

        ;push rcx
        ;push rax

        ;xor rcx, rcx
        ;xgetbv
        
        ;mov [ComboHell_SavedRdx], rdx
        ;mov [ComboHell_SavedRax], rax

        ;or rax, 0x7                                 ; Enable AVX
        ;xsetbv

        ;pop rax;
        ;pop rcx;

        ;
        ; Initialize ComboHell_AVX2

        xor  rdx, rdx
        
        mov  r10, [ComboHell_StopRequestPtr]
        mov  r11, [ComboHell_ErrorCounterPtr]

        ;
        ; Load Init Vectors
        ; rdi points to float[72] array
                 
        vmovdqa ymm0, [rcx]
        vmovdqa ymm1, [rcx + 32]
        vmovdqa ymm2, [rcx + 64]
        vmovdqa ymm3, [rcx + 96]
        vmovdqa ymm4, [rcx + 128]
        vmovdqa ymm5, [rcx + 160]
        vmovdqa ymm6, [rcx + 192]
        vmovdqa ymm7, [rcx + 224]

        vmovaps ymm8,  [rcx + 256]
        vmovaps ymm9,  [rcx + 288]
        vmovaps ymm10, [rcx + 320]

        ;
        ; Will be used to validate ymm0-5

        vmovdqa ymm11, ymm0
        vmovdqa ymm12, ymm1
        vmovdqa ymm13, ymm2
        vmovdqa ymm14, ymm3
        
        ;
        ; Stressor Loop 
        ;
        ; We will run this sequence >very< tight
        ; 100M+ times, before checking for errors
                
        mov esi, nloops

combohell:

        ;
        ; For this job, this is a completely useless instruction 
        ; which we put inside to also keep AGUs busy while we CRUNCH...

        vmovaps ymm15, [rcx]

        ;
        ; Work packages suitable for all ALUs 

        vpxor ymm0, ymm4, ymm0
        vpxor ymm1, ymm5, ymm1
        vpxor ymm2, ymm6, ymm2
        vpxor ymm3, ymm7, ymm3

        ;
        ; If this is running on Skylake / Coffee Lake / Whatever Lake
        ; here is little something for more capable EUs...
        
        vrcpps ymm15, ymm15
        
        ;
        ; Let's burn common ALUs a bit longer since the pure
        ; FP tasks will take longer and we do not want inefficient pipe
        ; 
        
        vpxor ymm11, ymm4, ymm11
        vpxor ymm12, ymm5, ymm12
        vpxor ymm13, ymm6, ymm13
        vpxor ymm14, ymm7, ymm14

        ;
        ; Another heavy job for capable EUs

        vdpps ymm8, ymm9, ymm10, 0xFF        

        sub esi, 1
        jnz combohell

        ;
        ; Outer-loop

        add rdx, 1
        cmp rdx, [ComboHell_MaxRuns]
        jg done

errcheck:

        ;
        ; Compare:
        ; (ymm0==ymm11) && (ymm1==ymm12) &&
        ; (ymm2==ymm13) && (ymm3==ymm14)
        
        vpcmpeqd ymm11, ymm0, ymm11
        vpcmpeqd ymm12, ymm1, ymm12
        vpcmpeqd ymm13, ymm2, ymm13
        vpcmpeqd ymm14, ymm3, ymm14
        
        vpand ymm11, ymm11, ymm12
        vpand ymm11, ymm11, ymm13
        vpand ymm11, ymm11, ymm14

        vpmovmskb r8d, ymm11
        
        cmp r8d, 0xFFFFFFFF
        jne cmperr

        ;
        ; Restore destroyed values

        vmovdqa ymm11, ymm0
        vmovdqa ymm12, ymm1
        vmovdqa ymm13, ymm2
        vmovdqa ymm14, ymm3

        ;
        ; Check for stop request
        ; and continue if no stop is requested

        mov  r9d, [r10]
        test r9d, r9d
        jz combohell

        ;
        ; User requested to stop testing

        xor eax, eax

done:
        ;
        ; Restore original value of extended CR

        ;push rcx
        ;push rdx
        ;push rax

        ;xor rcx, rcx
        ;mov rdx, [ComboHell_SavedRdx]
        ;mov rax, [ComboHell_SavedRax]
        ;xsetbv

        ;pop rax;
        ;pop rdx;
        ;pop rcx;

        ;
        ; Bye...

        pop rsi
        ret
cmperr:
        
        ;
        ; We found an error, increase the error counter
        
        mov r8, 1
        lock xadd qword [r11], r8

        ;
        ; Shall we terminate immediately?

        mov r8, ComboHell_TerminateOnError
        test r8, r8
        jnz errcleanup        

        ;
        ; Check for stop request
        ; and continue if no stop is requested
        
        mov  r9d, [r10]
        test r9d, r9d
        jz combohell

seterr:
        ;
        ; Terminate with error code

        mov eax, 0xBADDC0DE                   ; nonzero = errors detected
        jmp done

errcleanup:

        ;
        ; This code is only reached if "stop on error"
        ; condition exists. In this case, we will do a small trick:
        ; we will set tue stop flag so all other threads exit.
        ; this is needed for enviroments with spartan MP support (UEFI)

        mov  qword [r10], 1
        jmp  seterr
        
section .data
align 16

global ComboHell_StopRequestPtr
ComboHell_StopRequestPtr: dq 0                ; Stop Request (external)

global ComboHell_ErrorCounterPtr
ComboHell_ErrorCounterPtr: dq 0               ; Number of errors detected

global ComboHell_MaxRuns
ComboHell_MaxRuns: dq 0                       ; Max. number of runs

global ComboHell_TerminateOnError
ComboHell_TerminateOnError: dq 0              ; Terminate on Error?

nloops:  equ    0x10000000                    ; Number of inner ComboHell runs

ComboHell_SavedRdx: dq 0                      ; Used for saving extended CR
ComboHell_SavedRax: dq 0                      ; Used for saving extended CR