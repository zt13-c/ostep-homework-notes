#include <stdio.h>
#include "mem.h"

int main(void) {
    if (Mem_Init(4096) == -1) {
        printf("Mem_Init failed, m_error=%d\n", m_error);
        return 1;
    }

    void *p1 = Mem_Alloc(100, FIRSTFIT);
    void *p2 = Mem_Alloc(200, FIRSTFIT);

    printf("After two allocations:\n");
    Mem_Dump();

    printf("\nFree p1:\n");
    if (Mem_Free(p1) == -1) {
        printf("Mem_Free(p1) failed, m_error=%d\n", m_error);
    }
    Mem_Dump();

    printf("\nDouble free p1:\n");
    if (Mem_Free(p1) == -1) {
        printf("Double free detected, m_error=%d\n", m_error);
    }

    printf("\nFree invalid stack pointer:\n");
    int x = 10;
    if (Mem_Free(&x) == -1) {
        printf("Invalid pointer detected, m_error=%d\n", m_error);
    }

    printf("\nFree middle of p2:\n");
    if (Mem_Free((char *)p2 + 10) == -1) {
        printf("Middle pointer detected, m_error=%d\n", m_error);
    }

    printf("\nFree p2 correctly:\n");
    if (Mem_Free(p2) == -1) {
        printf("Mem_Free(p2) failed, m_error=%d\n", m_error);
    }
    Mem_Dump();

    return 0;
}