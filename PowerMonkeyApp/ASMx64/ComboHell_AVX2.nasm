;-------------------------------------------------------------------------------
;  ______                            ______                 _
; (_____ \                          |  ___ \               | |
;  _____) )___   _ _ _   ____   ___ | | _ | |  ___   ____  | |  _  ____  _   _
; |  ____// _ \ | | | | / _  ) / __)| || || | / _ \ |  _ \ | | / )/ _  )| | | |
; | |    | |_| || | | |( (/ / | |   | || || || |_| || | | || |< (( (/ / | |_| |
; |_|     \___/  \____| \____)|_|   |_||_||_| \___/ |_| |_||_| \_)\____) \__  |
;                                                                       (____/
; Copyright (C) 2021 Ivan Dimkovic. All rights reserved.
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

;-------------------------------------------------------------------------------
; Theory of operation
;
; Simple: perform a tight loop of instructions designed to utilize as much of
; available EUs as possible (ALUs, AGUs, etc.) with minimal dependencies so
; that we reach very high efficiency % (target >99.5%)
;
; On top of that, we want the routine to serve as a validator of correct CPU
; operation - so we will need to use some data transformation that is possible
; to attest for correctness.
;-------------------------------------------------------------------------------

BITS 64
DEFAULT REL
        ;
        ; Globals

        global combohell_avx2_kernel

        section .text

combohell_avx2_kernel:

        push rsi

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
        ; This is a completely useless instruction which 
        ; is put inside to also keep AGUs busy while we crunch

        vmovaps ymm15, [rcx]

        ;
        ; Work packages posible for all ALUs 

        ;vpxor ymm11, ymm4, ymm11
        ;vpxor ymm12, ymm5, ymm12
        ;vpxor ymm13, ymm6, ymm13
        ;vpxor ymm14, ymm7, ymm14

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

        vdpps ymm8, ymm9, ymm10, 0xFF        

        sub esi, 1
        jnz combohell

errcheck:

        ;
        ; Compare:
        ; [ymm0==ymm11] [ymm1==ymm12]
        ; [ymm2==ymm13] [ymm3==ymm14]
        
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
        ; Check for stop request
        ; and continue if no stop is requested

        mov  r9d, [r10]
        test r9d, r9d
        jz combohell

        ;
        ; User requested to stop testing

        xor eax, eax

done:
        ; Store the result 
        ; (might be needed for printouts, debugging, etc.)

        vmovdqa [rdx], ymm0
        vmovdqa [rdx + 32], ymm1
        vmovdqa [rdx + 64], ymm2
        vmovdqa [rdx + 96], ymm3     

        pop rsi

        ret
cmperr:
        
        ;
        ; We found an error, increase the error counter
        
        lock inc qword [r11]

        ;
        ; Check for stop request
        ; and continue if no stop is requested

        mov  r9d, [r10]
        test r9d, r9d
        jz combohell

        ;
        ; Terminate with error code

        mov eax, 0xBADDC0DE                   ; nonzero = errors detected
        jmp done

       
        section .data

nloops:  equ    0x10000000                    ; Number of XorHell iterations

global ComboHell_StopRequestPtr
ComboHell_StopRequestPtr: dq 0

global ComboHell_ErrorCounterPtr
ComboHell_ErrorCounterPtr: dq 0
