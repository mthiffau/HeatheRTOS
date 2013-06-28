#ifdef TEST_XMEMCPY_H
#error "double-included test_xmemcpy.h"
#endif

void test_xmemcpy_all(void);

void test_xmemcpy_small(void);
void test_xmemcpy_unalign(void);
void test_xmemcpy_mismatch(void);
void test_xmemcpy_large(void);
void test_xmemcpy_large_unalign(void);
void test_memset(void);
