@ RUN: llvm-mc -triple=arm-linux-gnueabi -filetype=obj < %s | llvm-objdump -t - | FileCheck %s

@ Implementation-detail test (unfortunately): values that are relocated do not
@ go via MCStreamer::EmitBytes; make sure they still emit a mapping symbol.
        add r0, r0, r0
        .word somewhere
        add r0, r0, r0

@ CHECK: 00000000 .text 00000000 $a
@ CHECK-NEXT: 00000008 .text 00000000 $a
@ CHECK-NEXT: 00000004 .text 00000000 $d
