/*
 * xBase3 - DBF Engine Tests
 */

#include "dbf.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#define TEST(name) printf("Testing %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); return 1; } while(0)

static const char *test_file = "/tmp/test_xbase3.dbf";

int main(void) {
    /* Test DBF creation */
    TEST("DBF create");
    {
        DBFField fields[3] = {
            {"NAME", 'C', 20, 0, 0},
            {"AGE", 'N', 3, 0, 0},
            {"ACTIVE", 'L', 1, 0, 0}
        };

        DBF *dbf = dbf_create(test_file, fields, 3);
        if (!dbf) FAIL("Failed to create DBF");
        if (dbf_reccount(dbf) != 0) FAIL("New DBF should have 0 records");
        if (dbf_field_count(dbf) != 3) FAIL("Should have 3 fields");
        dbf_close(dbf);
        PASS();
    }

    /* Test DBF open */
    TEST("DBF open");
    {
        DBF *dbf = dbf_open(test_file, false);
        if (!dbf) FAIL("Failed to open DBF");
        if (dbf_field_count(dbf) != 3) FAIL("Should have 3 fields");

        const DBFField *name_field = dbf_field_info(dbf, 0);
        if (!name_field) FAIL("NAME field not found");
        if (strcmp(name_field->name, "NAME") != 0) FAIL("Field name mismatch");
        if (name_field->type != 'C') FAIL("Field type mismatch");
        if (name_field->length != 20) FAIL("Field length mismatch");

        dbf_close(dbf);
        PASS();
    }

    /* Test append and read */
    TEST("DBF append and read");
    {
        DBF *dbf = dbf_open(test_file, false);
        if (!dbf) FAIL("Failed to open DBF");

        /* Append record */
        if (!dbf_append_blank(dbf)) FAIL("Failed to append");
        if (dbf_reccount(dbf) != 1) FAIL("Should have 1 record");

        /* Write fields */
        dbf_put_string(dbf, 0, "John Doe");
        dbf_put_double(dbf, 1, 25);
        dbf_put_logical(dbf, 2, true);
        dbf_flush(dbf);

        /* Read back */
        char name[21];
        double age;
        bool active;

        dbf_get_string(dbf, 0, name, sizeof(name));
        dbf_get_double(dbf, 1, &age);
        dbf_get_logical(dbf, 2, &active);

        str_trim_right(name);
        if (strcmp(name, "John Doe") != 0) FAIL("Name mismatch");
        if (age != 25) FAIL("Age mismatch");
        if (!active) FAIL("Active mismatch");

        dbf_close(dbf);
        PASS();
    }

    /* Test navigation */
    TEST("DBF navigation");
    {
        DBF *dbf = dbf_open(test_file, false);
        if (!dbf) FAIL("Failed to open DBF");

        /* Add more records */
        dbf_append_blank(dbf);
        dbf_put_string(dbf, 0, "Jane Smith");
        dbf_put_double(dbf, 1, 30);
        dbf_flush(dbf);

        dbf_append_blank(dbf);
        dbf_put_string(dbf, 0, "Bob Jones");
        dbf_put_double(dbf, 1, 35);
        dbf_flush(dbf);

        if (dbf_reccount(dbf) != 3) FAIL("Should have 3 records");

        /* Test GO TOP */
        dbf_go_top(dbf);
        if (dbf_recno(dbf) != 1) FAIL("GO TOP failed");
        if (dbf_bof(dbf)) FAIL("Should not be BOF after GO TOP");

        /* Test GO BOTTOM */
        dbf_go_bottom(dbf);
        if (dbf_recno(dbf) != 3) FAIL("GO BOTTOM failed");

        /* Test SKIP */
        dbf_go_top(dbf);
        dbf_skip(dbf, 1);
        if (dbf_recno(dbf) != 2) FAIL("SKIP 1 failed");

        dbf_skip(dbf, -1);
        if (dbf_recno(dbf) != 1) FAIL("SKIP -1 failed");

        /* Test EOF */
        dbf_go_bottom(dbf);
        dbf_skip(dbf, 1);
        if (!dbf_eof(dbf)) FAIL("Should be EOF");

        dbf_close(dbf);
        PASS();
    }

    /* Test delete and recall */
    TEST("DBF delete and recall");
    {
        DBF *dbf = dbf_open(test_file, false);
        if (!dbf) FAIL("Failed to open DBF");

        dbf_goto(dbf, 2);
        if (!dbf_delete(dbf)) FAIL("Delete failed");
        dbf_flush(dbf);

        if (!dbf_deleted(dbf)) FAIL("Should be deleted");

        if (!dbf_recall(dbf)) FAIL("Recall failed");
        dbf_flush(dbf);

        if (dbf_deleted(dbf)) FAIL("Should not be deleted");

        dbf_close(dbf);
        PASS();
    }

    /* Test pack */
    TEST("DBF pack");
    {
        DBF *dbf = dbf_open(test_file, false);
        if (!dbf) FAIL("Failed to open DBF");

        dbf_goto(dbf, 2);
        dbf_delete(dbf);
        dbf_flush(dbf);

        if (!dbf_pack(dbf)) FAIL("Pack failed");
        if (dbf_reccount(dbf) != 2) FAIL("Should have 2 records after pack");

        dbf_close(dbf);
        PASS();
    }

    /* Cleanup */
    unlink(test_file);

    printf("\nAll DBF tests passed!\n");
    return 0;
}
