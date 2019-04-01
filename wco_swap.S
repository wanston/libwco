.text
.global WcoSwapContext
.type WcoSwapContext, @function
.att_syntax prefix

WcoSwapContext:
/* 1. 保存from_co的8个callee负责的寄存器。此时rsp应指向retaddr
 * 2. 把to_co的8+2个寄存器值加载进来。之所以加两个，是为了在最初的时候，让封装的协程函数成功运行。
 * 3. ret
 *  reg[0] 到 reg[9] 依次为：
 *    RSP
 *    RBP
 *    RBX
 *    R12
 *    R13
 *    R14
 *    R15
 *    FPU
 *    RDI
 *    RSI
*/

    // 此时，rsp指向的应该是该函数的栈帧中retaddr的地址。
    movq %rsp, 0x0(%rdi)
    movq %rbp, 0x8(%rdi)
    movq %rbx, 0x10(%rdi)
    movq %r12, 0x18(%rdi)
    movq %r13, 0x20(%rdi)
    movq %r14, 0x28(%rdi)
    movq %r15, 0x30(%rdi)
    fnstcw 0x38(%rdi)
    stmxcsr 0x3c(%rdi)

    movq 0x0(%rsi), %rsp
    movq 0x8(%rsi), %rbp
    movq 0x10(%rsi),%rbx
    movq 0x18(%rsi),%r12
    movq 0x20(%rsi),%r13
    movq 0x28(%rsi),%r14
    movq 0x30(%rsi),%r15
    fldcw 0x38(%rsi)
    ldmxcsr 0x3c(%rsi)
    movq 0x40(%rsi),%rdi
    movq 0x48(%rsi),%rsi

    ret


.globl WcoSaveFpucwMxcsr
.type  WcoSaveFpucwMxcsr, @function
.att_syntax prefix

WcoSaveFpucwMxcsr:
    fnstcw  (%rdi)
    stmxcsr 0x4(%rdi)
    ret