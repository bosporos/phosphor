.globl ___vnza_chain_lookup

#include <Venice/Platform.h>
#if CPU(X86_64)

// __vnza_chain_lookup(u64 size) : i64 index
// rdi = size
___vnza_chain_lookup:
    // sets zero flag if %rdi is 0
    bsrq %rdi, %rax
    // size = 0 so error -1
    jz ___vnz_chain_lookup.zerror
    // rax is MSB index, with MSBI64 = 63
    // 2 * MSBI -> rdx
    leaq (,%rax,2), %rdx
    // rax - 1 -> rax
    decq %rax
    // aside: in the case that size = 1
    // we get MSBI = 0, so rdx=0, and rax will come out as -1=FFF...FFFh
    // because BT takes index mod operand size, -1u mod 64 = 63 is the MSB,
    // which will be 0 anyway ('cuz MSBI = 0 anyway), so it's okay not to treat
    // the edge case here

    // if second-most significant bit is set, CF is set
    btq %rax, %rdi
    // use 0 -> rax instead of rax xor rax -> rax because we need to preserve CF
    // mov doesn't affect flags, but xor clears CF, among other things
    movq $0, %rax
    // rdx + rax [initial value 0] + CF -> rax
    // = 2 * MSBI + (BIT MSBI - 1)
    adcq %rdx, %rax
    // Return 2 * MSBI + (BIT MSBI - 1)

    // Note from the future: adding a -1 here to correct for observed behaviour
    decq %rax
    retq
___vnz_chain_lookup.zerror:
    movq $-1, %rax
    // Return -1
    retq
#else
#error "venice: symbol __vnz_chain_lookup is not implemented on your architecture; please use one of: x86_64/amd64/ia64"
#endif
