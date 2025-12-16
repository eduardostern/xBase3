/*
 * xBase3 - XDX Index Tests
 */

#include "xdx.h"
#include "dbf.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST(name) printf("Testing %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); return 1; } while(0)

static const char *test_dbf = "/tmp/test_xdx.dbf";
static const char *test_xdx = "/tmp/test_xdx.xdx";

int main(void) {
    /* Create test DBF */
    DBFField fields[2] = {
        {"NAME", 'C', 20, 0, 0},
        {"VALUE", 'N', 10, 2, 0}
    };

    DBF *dbf = dbf_create(test_dbf, fields, 2);
    if (!dbf) {
        printf("Failed to create test DBF\n");
        return 1;
    }

    /* Add test records */
    const char *names[] = {"Charlie", "Alice", "Bob", "David", "Eve"};
    double values[] = {300.0, 100.0, 200.0, 400.0, 500.0};

    for (int i = 0; i < 5; i++) {
        dbf_append_blank(dbf);
        dbf_put_string(dbf, 0, names[i]);
        dbf_put_double(dbf, 1, values[i]);
    }
    dbf_close(dbf);

    /* Test XDX create */
    TEST("XDX create");
    {
        XDX *xdx = xdx_create(test_xdx, "NAME", XDX_KEY_CHAR, 20, false, false);
        if (!xdx) FAIL("Create returned NULL");
        if (strcmp(xdx_key_expr(xdx), "NAME") != 0) FAIL("Key expression mismatch");
        if (xdx_key_type(xdx) != 'C') FAIL("Key type mismatch");
        if (xdx_key_length(xdx) != 20) FAIL("Key length mismatch");
        xdx_close(xdx);
        PASS();
    }

    /* Test XDX insert and seek */
    TEST("XDX insert and seek");
    {
        XDX *xdx = xdx_open(test_xdx);
        if (!xdx) FAIL("Open returned NULL");

        /* Insert keys */
        char key[20];
        for (int i = 0; i < 5; i++) {
            memset(key, ' ', 20);
            memcpy(key, names[i], strlen(names[i]));
            if (!xdx_insert(xdx, key, (uint32_t)(i + 1))) {
                FAIL("Insert failed");
            }
        }

        /* Seek for "Bob" */
        memset(key, ' ', 20);
        memcpy(key, "Bob", 3);
        if (!xdx_seek(xdx, key)) FAIL("Seek failed for Bob");
        if (xdx_recno(xdx) != 3) FAIL("Bob should be record 3");

        /* Seek for "Alice" */
        memset(key, ' ', 20);
        memcpy(key, "Alice", 5);
        if (!xdx_seek(xdx, key)) FAIL("Seek failed for Alice");
        if (xdx_recno(xdx) != 2) FAIL("Alice should be record 2");

        /* Seek for non-existent key */
        memset(key, ' ', 20);
        memcpy(key, "Frank", 5);
        xdx_seek(xdx, key);
        if (xdx_found(xdx)) FAIL("Frank should not be found");

        xdx_close(xdx);
        PASS();
    }

    /* Test XDX go_top and go_bottom */
    TEST("XDX navigation");
    {
        XDX *xdx = xdx_open(test_xdx);
        if (!xdx) FAIL("Open returned NULL");

        if (!xdx_go_top(xdx)) FAIL("go_top failed");
        /* First in alphabetical order should be Alice (record 2) */
        if (xdx_recno(xdx) != 2) FAIL("Top should be Alice (record 2)");

        if (!xdx_go_bottom(xdx)) FAIL("go_bottom failed");
        /* Last in alphabetical order should be Eve (record 5) */
        if (xdx_recno(xdx) != 5) FAIL("Bottom should be Eve (record 5)");

        xdx_close(xdx);
        PASS();
    }

    /* Test XDX unique index */
    TEST("XDX unique index");
    {
        const char *unique_xdx = "/tmp/test_unique.xdx";
        XDX *xdx = xdx_create(unique_xdx, "NAME", XDX_KEY_CHAR, 20, true, false);
        if (!xdx) FAIL("Create unique index failed");

        char key[20];
        memset(key, ' ', 20);
        memcpy(key, "Test", 4);

        /* First insert should succeed */
        if (!xdx_insert(xdx, key, 1)) FAIL("First insert failed");

        /* Second insert with same key should fail */
        if (xdx_insert(xdx, key, 2)) FAIL("Duplicate insert should fail");

        xdx_close(xdx);
        unlink(unique_xdx);
        PASS();
    }

    /* Test XDX delete */
    TEST("XDX delete");
    {
        const char *del_xdx = "/tmp/test_del.xdx";
        XDX *xdx = xdx_create(del_xdx, "NAME", XDX_KEY_CHAR, 20, false, false);
        if (!xdx) FAIL("Create failed");

        char key[20];
        memset(key, ' ', 20);
        memcpy(key, "DeleteMe", 8);

        /* Insert */
        if (!xdx_insert(xdx, key, 1)) FAIL("Insert failed");

        /* Verify it exists */
        if (!xdx_seek(xdx, key)) FAIL("Seek after insert failed");

        /* Delete */
        if (!xdx_delete(xdx, key, 1)) FAIL("Delete failed");

        /* Verify it's gone */
        xdx_seek(xdx, key);
        if (xdx_found(xdx)) FAIL("Key should be deleted");

        xdx_close(xdx);
        unlink(del_xdx);
        PASS();
    }

    /* Cleanup */
    unlink(test_dbf);
    unlink(test_xdx);

    printf("\nAll XDX tests passed!\n");
    return 0;
}
