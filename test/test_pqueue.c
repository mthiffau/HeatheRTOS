#undef NOASSERT

#include "test/test_pqueue.h"

#include "config.h"
#include "xint.h"
#include "xdef.h"
#include "pqueue.h"

#include "xbool.h"
#include "xassert.h"
#include "array_size.h"

#include "xarg.h"
#include "bwio.h"

static void test_pqueue_add_peekmin(void);
static void test_pqueue_add_fail_val_oor(void);
static void test_pqueue_add_fail_duplicate0(void);
static void test_pqueue_add_fail_duplicate1(void);
static void test_pqueue_add2_peekmin_fst(void);
static void test_pqueue_add2_peekmin_snd(void);
static void test_pqueue_add2_popmin_peekmin_fst(void);
static void test_pqueue_add2_popmin_peekmin_snd(void);
static void test_pqueue_add2_popmin2_peekmin(void);
static void test_pqueue_peekmin_empty(void);
static void test_pqueue_many(void);

void
test_pqueue_all(void)
{
    test_pqueue_add_peekmin();
    test_pqueue_add_fail_val_oor();
    test_pqueue_add_fail_duplicate0();
    test_pqueue_add_fail_duplicate1();
    test_pqueue_add2_peekmin_fst();
    test_pqueue_add2_peekmin_snd();
    test_pqueue_add2_popmin_peekmin_fst();
    test_pqueue_add2_popmin_peekmin_snd();
    test_pqueue_add2_popmin2_peekmin();
    test_pqueue_peekmin_empty();
    test_pqueue_many();
}

static void
test_pqueue_add_peekmin(void)
{
    struct pqueue q;
    struct pqueue_node nodes[1];
    struct pqueue_entry *min;
    int rc;
    bwputstr(COM2, "test_pqueue_add_peekmin...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 0, 42);
    assert(rc == 0);
    min = pqueue_peekmin(&q);
    assert(min != NULL);
    assert(min->key == 42);
    assert(min->val ==  0);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add_fail_val_oor(void)
{
    struct pqueue q;
    struct pqueue_node nodes[1];
    int rc;
    bwputstr(COM2, "test_pqueue_add_fail_val_oor...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 1, 42);
    assert(rc == -1);
    rc = pqueue_add(&q, 2, 42);
    assert(rc == -1);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add_fail_duplicate0(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    int rc;
    bwputstr(COM2, "test_pqueue_add_fail_duplicate0...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 0, 42);
    assert(rc == 0);
    rc = pqueue_add(&q, 1, 3);
    assert(rc == 0);
    rc = pqueue_add(&q, 0, 55);
    assert(rc == -2);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add_fail_duplicate1(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    int rc;
    bwputstr(COM2, "test_pqueue_add_fail_duplicate1...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 0, 42);
    assert(rc == 0);
    rc = pqueue_add(&q, 1, 3);
    assert(rc == 0);
    rc = pqueue_add(&q, 1, 55);
    assert(rc == -2);
    bwputstr(COM2, "ok\n");
}


static void
test_pqueue_add2_peekmin_fst(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    struct pqueue_entry *min;
    int rc;
    bwputstr(COM2, "test_pqueue_add2_peekmin_fst...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 1, -1);
    assert(rc == 0);
    rc = pqueue_add(&q, 0, 0);
    assert(rc == 0);
    min = pqueue_peekmin(&q);
    assert(min != NULL);
    assert(min->key == -1);
    assert(min->val == 1);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add2_peekmin_snd(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    struct pqueue_entry *min;
    int rc;
    bwputstr(COM2, "test_pqueue_add2_peekmin_snd...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 0, 1);
    assert(rc == 0);
    rc = pqueue_add(&q, 1, 0);
    assert(rc == 0);
    min = pqueue_peekmin(&q);
    assert(min != NULL);
    assert(min->key == 0);
    assert(min->val == 1);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add2_popmin_peekmin_fst(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    struct pqueue_entry *min;
    int rc;
    bwputstr(COM2, "test_pqueue_add2_popmin_peekmin_fst...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 0, -1);
    assert(rc == 0);
    rc = pqueue_add(&q, 1, 0);
    pqueue_popmin(&q);
    assert(rc == 0);
    min = pqueue_peekmin(&q);
    assert(min != NULL);
    assert(min->key == 0);
    assert(min->val == 1);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add2_popmin_peekmin_snd(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    struct pqueue_entry *min;
    int rc;
    bwputstr(COM2, "test_pqueue_add2_popmin_peekmin_snd...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 1, 1);
    assert(rc == 0);
    rc = pqueue_add(&q, 0, 0);
    assert(rc == 0);
    pqueue_popmin(&q);
    min = pqueue_peekmin(&q);
    assert(min != NULL);
    assert(min->key == 1);
    assert(min->val == 1);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_add2_popmin2_peekmin(void)
{
    struct pqueue q;
    struct pqueue_node nodes[2];
    struct pqueue_entry *min;
    int rc;
    bwputstr(COM2, "test_pqueue_add2_popmin2_peekmin...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    rc = pqueue_add(&q, 0, 1);
    assert(rc == 0);
    rc = pqueue_add(&q, 1, 0);
    assert(rc == 0);
    pqueue_popmin(&q);
    pqueue_popmin(&q);
    min = pqueue_peekmin(&q);
    assert(min == NULL);
    bwputstr(COM2, "ok\n");
}

static void
test_pqueue_peekmin_empty(void)
{
    struct pqueue q;
    struct pqueue_node nodes[1];
    struct pqueue_entry *min;
    bwputstr(COM2, "test_pqueue_peekmin_empty...");
    pqueue_init(&q, ARRAY_SIZE(nodes), nodes);
    min = pqueue_peekmin(&q);
    assert(min == NULL);
    bwputstr(COM2, "ok\n");
}

static void test_pqueue_many(void)
{
    struct pqueue q;
    struct pqueue_node nodes[32];
    struct pqueue_entry *min;
    int keys[32] = {
        1, 20, 28, 0, 12, 12, 21, 29, 25, 22, 13, 18, 2, 31, 21, 8,
        13, 10, 9, 2, 21, 20, 14, 14, 14, 21, 20, 31, 18, 13, 3, 24
    };
    bool incl[32];
    int i, j, count, rc;

    for (i = 0; i < 32; i++)
        incl[i] = false;

    bwputstr(COM2, "test_pqueue_many...");
    pqueue_init(&q, 32, nodes);

    count = 0;
    i = 0;
    while (count <= 16 - 2) {
        rc = pqueue_add(&q, i, keys[i]);
        assert(rc == 0);
        incl[i++] = true;
        rc = pqueue_add(&q, i, keys[i]);
        assert(rc == 0);
        incl[i++] = true;
        min = pqueue_peekmin(&q);
        assert(min != NULL);
        assert(incl[min->val]);
        assert(keys[min->val] == min->key);
        for (j = 0; j < 32; j++)
            assert(!incl[j] || keys[j] >= min->key);
        incl[min->val] = false;
        pqueue_popmin(&q);
        count++;
    }

    while (count > 0) {
        min = pqueue_peekmin(&q);
        assert(min != NULL);
        assert(incl[min->val]);
        assert(keys[min->val] == min->key);
        for (j = 0; j < 32; j++)
            assert(!incl[j] || keys[j] >= min->key);
        incl[min->val] = false;
        pqueue_popmin(&q);
        count--;
        if (count > 0 && count % 2 == 0) {
            for (i = 0; i < 32; i++) {
                if (incl[i])
                    break;
            }
            keys[i] = -i;
            rc = pqueue_decreasekey(&q, i, -i);
            assert(rc == 0);
        }
    }

    min = pqueue_peekmin(&q);
    assert(min == NULL);
    bwputstr(COM2, "ok\n");
}
