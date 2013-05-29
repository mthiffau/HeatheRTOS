#include "test/test_xmemcpy.h"

#include "xbool.h"
#include "xassert.h"
#include "xarg.h"
#include "bwio.h"

#include "xdef.h"
#include "xmemcpy.h"

void
test_xmemcpy_all(void)
{
    test_xmemcpy_small();
    test_xmemcpy_unalign();
    test_xmemcpy_mismatch();
    test_xmemcpy_large();
    test_xmemcpy_large_unalign();
}

void
test_xmemcpy_small(void)
{
    char src[12]  = "foo bar baz";
    char dest[12] = "XXXXXXXXXXX";
    void *result;

    bwputstr(COM2, "test_xmemcpy_small...");

    result = xmemcpy(dest, src, 12);
    assert(result == dest);

    assert(src[0]  == 'f' && dest[0]  == 'f');
    assert(src[1]  == 'o' && dest[1]  == 'o');
    assert(src[2]  == 'o' && dest[2]  == 'o');
    assert(src[3]  == ' ' && dest[3]  == ' ');
    assert(src[4]  == 'b' && dest[4]  == 'b');
    assert(src[5]  == 'a' && dest[5]  == 'a');
    assert(src[6]  == 'r' && dest[6]  == 'r');
    assert(src[7]  == ' ' && dest[7]  == ' ');
    assert(src[8]  == 'b' && dest[8]  == 'b');
    assert(src[9]  == 'a' && dest[9]  == 'a');
    assert(src[10] == 'z' && dest[10] == 'z');
    assert(src[11] == '\0'&& dest[11] == '\0');

    bwputstr(COM2, "ok\n");
}

void
test_xmemcpy_unalign(void)
{
    char src[12]  = "foo bar baz";
    char dest[12] = "XXXXXXXXXXX";
    void *result;

    bwputstr(COM2, "test_xmemcpy_unalign...");

    result = xmemcpy(dest + 2, src + 2, 10);
    assert(result == dest + 2);

    assert(src[0]  == 'f');
    assert(src[1]  == 'o');
    assert(src[2]  == 'o' && dest[2]  == 'o');
    assert(src[3]  == ' ' && dest[3]  == ' ');
    assert(src[4]  == 'b' && dest[4]  == 'b');
    assert(src[5]  == 'a' && dest[5]  == 'a');
    assert(src[6]  == 'r' && dest[6]  == 'r');
    assert(src[7]  == ' ' && dest[7]  == ' ');
    assert(src[8]  == 'b' && dest[8]  == 'b');
    assert(src[9]  == 'a' && dest[9]  == 'a');
    assert(src[10] == 'z' && dest[10] == 'z');
    assert(src[11] == '\0'&& dest[11] == '\0');

    bwputstr(COM2, "ok\n");
}

void
test_xmemcpy_mismatch(void)
{
    char src[12]  = "foo bar baz";
    char dest[12] = "XXXXXXXXXXX";
    void *result;

    bwputstr(COM2, "test_xmemcpy_mismatch...");

    result = xmemcpy(dest + 1, src, 11);
    assert(result == dest + 1);

    assert(src[0]  == 'f' && dest[1]  == 'f');
    assert(src[1]  == 'o' && dest[2]  == 'o');
    assert(src[2]  == 'o' && dest[3]  == 'o');
    assert(src[3]  == ' ' && dest[4]  == ' ');
    assert(src[4]  == 'b' && dest[5]  == 'b');
    assert(src[5]  == 'a' && dest[6]  == 'a');
    assert(src[6]  == 'r' && dest[7]  == 'r');
    assert(src[7]  == ' ' && dest[8]  == ' ');
    assert(src[8]  == 'b' && dest[9]  == 'b');
    assert(src[9]  == 'a' && dest[10] == 'a');
    assert(src[10] == 'z' && dest[11] == 'z');
    assert(src[11] == '\0');

    bwputstr(COM2, "ok\n");
}

void
test_xmemcpy_large(void)
{
    char src[118], dest[118]; /* 80 + 20 + 12 + 4 + 2 */
    int i;
    void *result;

    bwputstr(COM2, "test_xmemcpy_large...");
    for (i = 0; i < 118; i++) {
        src[i] = i;
        dest[i] = 0xff;
    }

    result = xmemcpy(dest, src, 118);
    assert(result == dest);

    for (i = 0; i < 118; i++)
        assert(dest[i] == i);

    bwputstr(COM2, "ok\n");
}

void
test_xmemcpy_large_unalign(void)
{
    char src[118], dest[118]; /* 80 + 20 + 12 + 4 + 2 */
    int i;
    void *result;

    bwputstr(COM2, "test_xmemcpy_large_unalign...");
    for (i = 0; i < 118; i++) {
        src[i] = i;
        dest[i] = 0xff;
    }

    result = xmemcpy(dest + 1, src + 1, 118);
    assert(result == dest + 1);

    for (i = 1; i < 118; i++)
        assert(dest[i] == i);

    bwputstr(COM2, "ok\n");
}

