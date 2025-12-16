/*
 * xBase3 - dBASE III+ Compatible Database System
 * xdx.c - XDX B-tree index engine implementation
 */

#include "xdx.h"
#include "dbf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Internal structures for navigation stack */
typedef struct {
    uint32_t node_offset;
    int key_index;
} StackEntry;

typedef struct {
    StackEntry *entries;
    int count;
    int capacity;
} NavStack;

/* Calculate node size in bytes */
static size_t node_size(XDX *xdx) {
    /* Header + (order * (key + recno + child)) + right_child */
    size_t entry_size = xdx->header.key_length + sizeof(uint32_t);
    if (xdx->header.order > 0) {
        /* Internal nodes have child pointers */
        entry_size += sizeof(uint32_t);
    }
    return sizeof(XDXNodeHeader) +
           (xdx->header.order * entry_size) +
           sizeof(uint32_t);  /* right child */
}

/* Calculate leaf node size (no child pointers) */
static size_t leaf_node_size(XDX *xdx) {
    size_t entry_size = xdx->header.key_length + sizeof(uint32_t);
    return sizeof(XDXNodeHeader) +
           (xdx->header.order * entry_size);
}

/* Allocate a new node in memory */
static XDXNode *node_alloc(XDX *xdx) {
    XDXNode *node = xcalloc(1, sizeof(XDXNode));
    node->entries = xcalloc(xdx->header.order, sizeof(XDXKeyEntry));

    for (int i = 0; i < xdx->header.order; i++) {
        node->entries[i].key = xcalloc(1, xdx->header.key_length);
    }

    return node;
}

/* Free a node */
static void node_free(XDXNode *node, int order) {
    if (!node) return;

    if (node->entries) {
        for (int i = 0; i < order; i++) {
            free(node->entries[i].key);
        }
        free(node->entries);
    }
    free(node);
}

/* Read a node from file */
static XDXNode *node_read(XDX *xdx, uint32_t offset) {
    if (offset == 0) return NULL;

    XDXNode *node = node_alloc(xdx);
    node->file_offset = offset;

    if (fseek(xdx->fp, (long)offset, SEEK_SET) != 0) {
        node_free(node, xdx->header.order);
        return NULL;
    }

    /* Read header */
    if (fread(&node->header, sizeof(XDXNodeHeader), 1, xdx->fp) != 1) {
        node_free(node, xdx->header.order);
        return NULL;
    }

    /* Read key entries */
    for (int i = 0; i < node->header.key_count; i++) {
        /* Read key */
        if (fread(node->entries[i].key, xdx->header.key_length, 1, xdx->fp) != 1) {
            node_free(node, xdx->header.order);
            return NULL;
        }

        /* Read record number */
        if (fread(&node->entries[i].recno, sizeof(uint32_t), 1, xdx->fp) != 1) {
            node_free(node, xdx->header.order);
            return NULL;
        }

        /* Read child pointer for internal nodes */
        if (!node->header.is_leaf) {
            if (fread(&node->entries[i].child_offset, sizeof(uint32_t), 1, xdx->fp) != 1) {
                node_free(node, xdx->header.order);
                return NULL;
            }
        }
    }

    /* Read right child for internal nodes */
    if (!node->header.is_leaf) {
        if (fread(&node->right_child, sizeof(uint32_t), 1, xdx->fp) != 1) {
            node_free(node, xdx->header.order);
            return NULL;
        }
    }

    return node;
}

/* Write a node to file */
static bool node_write(XDX *xdx, XDXNode *node) {
    if (!node) return false;

    if (fseek(xdx->fp, (long)node->file_offset, SEEK_SET) != 0) {
        return false;
    }

    /* Write header */
    if (fwrite(&node->header, sizeof(XDXNodeHeader), 1, xdx->fp) != 1) {
        return false;
    }

    /* Write key entries */
    for (int i = 0; i < node->header.key_count; i++) {
        /* Write key */
        if (fwrite(node->entries[i].key, xdx->header.key_length, 1, xdx->fp) != 1) {
            return false;
        }

        /* Write record number */
        if (fwrite(&node->entries[i].recno, sizeof(uint32_t), 1, xdx->fp) != 1) {
            return false;
        }

        /* Write child pointer for internal nodes */
        if (!node->header.is_leaf) {
            if (fwrite(&node->entries[i].child_offset, sizeof(uint32_t), 1, xdx->fp) != 1) {
                return false;
            }
        }
    }

    /* Write right child for internal nodes */
    if (!node->header.is_leaf) {
        if (fwrite(&node->right_child, sizeof(uint32_t), 1, xdx->fp) != 1) {
            return false;
        }
    }

    node->dirty = false;
    return true;
}

/* Allocate a new node in the file */
static uint32_t node_create(XDX *xdx, bool is_leaf) {
    /* Seek to end of file */
    if (fseek(xdx->fp, 0, SEEK_END) != 0) {
        return 0;
    }

    uint32_t offset = (uint32_t)ftell(xdx->fp);

    /* Create empty node */
    XDXNode *node = node_alloc(xdx);
    node->file_offset = offset;
    node->header.key_count = 0;
    node->header.is_leaf = is_leaf ? 1 : 0;
    node->header.parent_offset = 0;
    node->dirty = true;

    /* Write to file */
    if (!node_write(xdx, node)) {
        node_free(node, xdx->header.order);
        return 0;
    }

    /* Pad to proper size */
    size_t size = is_leaf ? leaf_node_size(xdx) : node_size(xdx);
    size_t written = sizeof(XDXNodeHeader);
    uint8_t zero = 0;
    while (written < size) {
        fwrite(&zero, 1, 1, xdx->fp);
        written++;
    }

    node_free(node, xdx->header.order);
    xdx->header.node_count++;

    return offset;
}

/* Compare two keys */
int xdx_key_compare(XDX *xdx, const void *key1, const void *key2) {
    int result;

    switch (xdx->header.key_type) {
        case XDX_KEY_NUMERIC: {
            double d1, d2;
            /* Keys stored as strings, convert to double */
            char buf1[32], buf2[32];
            memcpy(buf1, key1, xdx->header.key_length);
            buf1[xdx->header.key_length] = '\0';
            memcpy(buf2, key2, xdx->header.key_length);
            buf2[xdx->header.key_length] = '\0';
            d1 = atof(buf1);
            d2 = atof(buf2);
            if (d1 < d2) result = -1;
            else if (d1 > d2) result = 1;
            else result = 0;
            break;
        }

        case XDX_KEY_DATE:
            /* Dates stored as YYYYMMDD strings */
            result = memcmp(key1, key2, 8);
            break;

        case XDX_KEY_CHAR:
        default:
            result = memcmp(key1, key2, xdx->header.key_length);
            break;
    }

    /* Reverse for descending order */
    if (xdx->header.flags & XDX_FLAG_DESCENDING) {
        result = -result;
    }

    return result;
}

/* Find position for key in node (binary search) */
static int find_key_pos(XDX *xdx, XDXNode *node, const void *key) {
    int left = 0;
    int right = node->header.key_count - 1;

    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = xdx_key_compare(xdx, key, node->entries[mid].key);

        if (cmp == 0) {
            return mid;  /* Exact match */
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    return left;  /* Insert position */
}

/* Split a full node */
static bool split_node(XDX *xdx, XDXNode *node, XDXNode *parent, int parent_idx) {
    int mid = node->header.key_count / 2;

    /* Create new right sibling */
    uint32_t new_offset = node_create(xdx, node->header.is_leaf);
    if (new_offset == 0) return false;

    XDXNode *sibling = node_read(xdx, new_offset);
    if (!sibling) return false;

    /* Copy right half of keys to sibling */
    sibling->header.key_count = node->header.key_count - mid - 1;
    sibling->header.is_leaf = node->header.is_leaf;

    for (int i = 0; i < sibling->header.key_count; i++) {
        memcpy(sibling->entries[i].key, node->entries[mid + 1 + i].key,
               xdx->header.key_length);
        sibling->entries[i].recno = node->entries[mid + 1 + i].recno;
        sibling->entries[i].child_offset = node->entries[mid + 1 + i].child_offset;
    }

    if (!node->header.is_leaf) {
        sibling->right_child = node->right_child;
    }

    /* Middle key goes up to parent */
    uint8_t *mid_key = xmalloc(xdx->header.key_length);
    memcpy(mid_key, node->entries[mid].key, xdx->header.key_length);
    uint32_t mid_recno = node->entries[mid].recno;

    /* Shrink original node */
    node->header.key_count = mid;
    if (!node->header.is_leaf) {
        node->right_child = node->entries[mid].child_offset;
    }

    /* Write both nodes */
    node->dirty = true;
    sibling->dirty = true;
    node_write(xdx, node);
    node_write(xdx, sibling);

    /* Insert middle key into parent */
    if (parent == NULL) {
        /* Create new root */
        uint32_t new_root_offset = node_create(xdx, false);
        XDXNode *new_root = node_read(xdx, new_root_offset);

        new_root->header.key_count = 1;
        memcpy(new_root->entries[0].key, mid_key, xdx->header.key_length);
        new_root->entries[0].recno = mid_recno;
        new_root->entries[0].child_offset = node->file_offset;
        new_root->right_child = sibling->file_offset;

        node_write(xdx, new_root);

        /* Update header */
        xdx->header.root_offset = new_root_offset;
        xdx->modified = true;

        /* Update cached root */
        if (xdx->root) {
            node_free(xdx->root, xdx->header.order);
        }
        xdx->root = new_root;
    } else {
        /* Insert into parent - shift keys right */
        for (int i = parent->header.key_count; i > parent_idx; i--) {
            memcpy(parent->entries[i].key, parent->entries[i-1].key,
                   xdx->header.key_length);
            parent->entries[i].recno = parent->entries[i-1].recno;
            parent->entries[i].child_offset = parent->entries[i-1].child_offset;
        }

        memcpy(parent->entries[parent_idx].key, mid_key, xdx->header.key_length);
        parent->entries[parent_idx].recno = mid_recno;
        parent->entries[parent_idx].child_offset = node->file_offset;

        /* Update right child pointer */
        if (parent_idx == parent->header.key_count) {
            parent->right_child = sibling->file_offset;
        } else {
            parent->entries[parent_idx + 1].child_offset = sibling->file_offset;
        }

        parent->header.key_count++;
        parent->dirty = true;
        node_write(xdx, parent);
    }

    free(mid_key);
    node_free(sibling, xdx->header.order);

    return true;
}

/* Navigation stack operations */
static void stack_init(NavStack *stack) {
    stack->entries = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static void stack_push(NavStack *stack, uint32_t offset, int idx) {
    if (stack->count >= stack->capacity) {
        stack->capacity = stack->capacity ? stack->capacity * 2 : 16;
        stack->entries = xrealloc(stack->entries,
                                   stack->capacity * sizeof(StackEntry));
    }
    stack->entries[stack->count].node_offset = offset;
    stack->entries[stack->count].key_index = idx;
    stack->count++;
}

static bool stack_pop(NavStack *stack, uint32_t *offset, int *idx) {
    if (stack->count == 0) return false;
    stack->count--;
    *offset = stack->entries[stack->count].node_offset;
    *idx = stack->entries[stack->count].key_index;
    return true;
}

static void stack_free(NavStack *stack) {
    free(stack->entries);
    stack->entries = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

/*
 * Public API Implementation
 */

XDX *xdx_create(const char *filename, const char *key_expr,
                char key_type, uint16_t key_length,
                bool unique, bool descending) {

    XDX *xdx = xcalloc(1, sizeof(XDX));
    strncpy(xdx->filename, filename, MAX_PATH_LEN - 1);

    /* Open file for writing */
    xdx->fp = fopen(filename, "w+b");
    if (!xdx->fp) {
        error_set(ERR_FILE_CREATE, "Cannot create index file");
        free(xdx);
        return NULL;
    }

    /* Initialize header */
    memset(&xdx->header, 0, sizeof(XDXHeader));
    memcpy(xdx->header.magic, XDX_MAGIC, 4);
    xdx->header.version = XDX_VERSION;
    xdx->header.key_type = key_type;
    xdx->header.key_length = key_length;
    xdx->header.order = XDX_DEFAULT_ORDER;
    xdx->header.flags = 0;

    if (unique) xdx->header.flags |= XDX_FLAG_UNIQUE;
    if (descending) xdx->header.flags |= XDX_FLAG_DESCENDING;

    strncpy(xdx->header.key_expr, key_expr, XDX_MAX_EXPR_LEN - 1);

    /* Write header */
    if (fwrite(&xdx->header, sizeof(XDXHeader), 1, xdx->fp) != 1) {
        error_set(ERR_FILE_WRITE, "Cannot write index header");
        fclose(xdx->fp);
        free(xdx);
        return NULL;
    }

    /* Pad header to XDX_HEADER_SIZE */
    uint8_t zero = 0;
    for (size_t i = sizeof(XDXHeader); i < XDX_HEADER_SIZE; i++) {
        fwrite(&zero, 1, 1, xdx->fp);
    }

    /* Create empty root node (leaf) */
    uint32_t root_offset = node_create(xdx, true);
    if (root_offset == 0) {
        error_set(ERR_FILE_WRITE, "Cannot create root node");
        fclose(xdx->fp);
        free(xdx);
        return NULL;
    }

    xdx->header.root_offset = root_offset;

    /* Rewrite header with root offset */
    fseek(xdx->fp, 0, SEEK_SET);
    fwrite(&xdx->header, sizeof(XDXHeader), 1, xdx->fp);

    /* Allocate key buffer */
    xdx->key_buffer = xcalloc(1, key_length);

    /* Load root */
    xdx->root = node_read(xdx, root_offset);

    fflush(xdx->fp);
    return xdx;
}

XDX *xdx_open(const char *filename) {
    XDX *xdx = xcalloc(1, sizeof(XDX));
    strncpy(xdx->filename, filename, MAX_PATH_LEN - 1);

    /* Open file for read/write */
    xdx->fp = fopen(filename, "r+b");
    if (!xdx->fp) {
        error_set(ERR_FILE_READ, "Cannot open index file");
        free(xdx);
        return NULL;
    }

    /* Read header */
    if (fread(&xdx->header, sizeof(XDXHeader), 1, xdx->fp) != 1) {
        error_set(ERR_FILE_READ, "Cannot read index header");
        fclose(xdx->fp);
        free(xdx);
        return NULL;
    }

    /* Validate magic */
    if (memcmp(xdx->header.magic, XDX_MAGIC, 3) != 0) {
        error_set(ERR_INVALID_INDEX, "Invalid index file format");
        fclose(xdx->fp);
        free(xdx);
        return NULL;
    }

    /* Validate version */
    if (xdx->header.version != XDX_VERSION) {
        error_set(ERR_INVALID_INDEX, "Unsupported index version");
        fclose(xdx->fp);
        free(xdx);
        return NULL;
    }

    /* Allocate key buffer */
    xdx->key_buffer = xcalloc(1, xdx->header.key_length);

    /* Load root */
    xdx->root = node_read(xdx, xdx->header.root_offset);

    return xdx;
}

void xdx_close(XDX *xdx) {
    if (!xdx) return;

    xdx_flush(xdx);

    if (xdx->root) {
        node_free(xdx->root, xdx->header.order);
    }

    free(xdx->key_buffer);

    if (xdx->fp) {
        fclose(xdx->fp);
    }

    free(xdx);
}

bool xdx_flush(XDX *xdx) {
    if (!xdx || !xdx->fp) return false;

    if (xdx->modified) {
        /* Write header */
        fseek(xdx->fp, 0, SEEK_SET);
        if (fwrite(&xdx->header, sizeof(XDXHeader), 1, xdx->fp) != 1) {
            return false;
        }
        xdx->modified = false;
    }

    if (xdx->root && xdx->root->dirty) {
        node_write(xdx, xdx->root);
    }

    fflush(xdx->fp);
    return true;
}

bool xdx_insert(XDX *xdx, const void *key, uint32_t recno) {
    if (!xdx || !key) return false;

    /* Start at root */
    XDXNode *node = xdx->root;
    XDXNode *parent = NULL;
    int parent_idx = 0;

    /* Traverse to leaf */
    while (!node->header.is_leaf) {
        int pos = find_key_pos(xdx, node, key);

        /* Check for duplicate in unique index */
        if ((xdx->header.flags & XDX_FLAG_UNIQUE) &&
            pos < node->header.key_count &&
            xdx_key_compare(xdx, key, node->entries[pos].key) == 0) {
            error_set(ERR_DUPLICATE_KEY, "Duplicate key in unique index");
            if (parent && parent != xdx->root) node_free(parent, xdx->header.order);
            if (node != xdx->root) node_free(node, xdx->header.order);
            return false;
        }

        uint32_t child_offset;
        if (pos < node->header.key_count) {
            child_offset = node->entries[pos].child_offset;
        } else {
            child_offset = node->right_child;
        }

        if (parent && parent != xdx->root) {
            node_free(parent, xdx->header.order);
        }
        parent = node;
        parent_idx = pos;

        node = node_read(xdx, child_offset);
        if (!node) {
            if (parent != xdx->root) node_free(parent, xdx->header.order);
            return false;
        }
    }

    /* Check for duplicate in leaf */
    if (xdx->header.flags & XDX_FLAG_UNIQUE) {
        for (int i = 0; i < node->header.key_count; i++) {
            if (xdx_key_compare(xdx, key, node->entries[i].key) == 0) {
                error_set(ERR_DUPLICATE_KEY, "Duplicate key in unique index");
                if (parent && parent != xdx->root) node_free(parent, xdx->header.order);
                if (node != xdx->root) node_free(node, xdx->header.order);
                return false;
            }
        }
    }

    /* Find insert position */
    int pos = find_key_pos(xdx, node, key);

    /* Check if node is full */
    if (node->header.key_count >= xdx->header.order - 1) {
        /* Split the node */
        if (!split_node(xdx, node, parent, parent_idx)) {
            if (parent && parent != xdx->root) node_free(parent, xdx->header.order);
            if (node != xdx->root) node_free(node, xdx->header.order);
            return false;
        }

        /* Re-find the correct node after split */
        /* Reload root if it changed */
        if (xdx->root->file_offset != xdx->header.root_offset) {
            node_free(xdx->root, xdx->header.order);
            xdx->root = node_read(xdx, xdx->header.root_offset);
        }

        /* Start over from root */
        if (parent && parent != xdx->root) node_free(parent, xdx->header.order);
        if (node != xdx->root) node_free(node, xdx->header.order);

        return xdx_insert(xdx, key, recno);  /* Retry */
    }

    /* Insert key at position */
    for (int i = node->header.key_count; i > pos; i--) {
        memcpy(node->entries[i].key, node->entries[i-1].key,
               xdx->header.key_length);
        node->entries[i].recno = node->entries[i-1].recno;
    }

    memcpy(node->entries[pos].key, key, xdx->header.key_length);
    node->entries[pos].recno = recno;
    node->header.key_count++;
    node->dirty = true;

    /* Write node */
    bool result = node_write(xdx, node);

    /* Update root cache if needed */
    if (node->file_offset == xdx->header.root_offset) {
        if (xdx->root && xdx->root != node) {
            node_free(xdx->root, xdx->header.order);
        }
        xdx->root = node;
    } else {
        node_free(node, xdx->header.order);
    }

    if (parent && parent != xdx->root) {
        node_free(parent, xdx->header.order);
    }

    return result;
}

bool xdx_delete(XDX *xdx, const void *key, uint32_t recno) {
    if (!xdx || !key) return false;

    /* Find the key */
    XDXNode *node = xdx->root;
    NavStack stack;
    stack_init(&stack);

    while (!node->header.is_leaf) {
        int pos = find_key_pos(xdx, node, key);
        stack_push(&stack, node->file_offset, pos);

        uint32_t child_offset;
        if (pos < node->header.key_count) {
            child_offset = node->entries[pos].child_offset;
        } else {
            child_offset = node->right_child;
        }

        if (node != xdx->root) {
            node_free(node, xdx->header.order);
        }

        node = node_read(xdx, child_offset);
        if (!node) {
            stack_free(&stack);
            return false;
        }
    }

    /* Find key in leaf */
    bool found = false;
    int del_pos = -1;

    for (int i = 0; i < node->header.key_count; i++) {
        if (xdx_key_compare(xdx, key, node->entries[i].key) == 0 &&
            node->entries[i].recno == recno) {
            found = true;
            del_pos = i;
            break;
        }
    }

    if (!found) {
        if (node != xdx->root) node_free(node, xdx->header.order);
        stack_free(&stack);
        return false;  /* Key not found */
    }

    /* Remove key by shifting */
    for (int i = del_pos; i < node->header.key_count - 1; i++) {
        memcpy(node->entries[i].key, node->entries[i+1].key,
               xdx->header.key_length);
        node->entries[i].recno = node->entries[i+1].recno;
    }
    node->header.key_count--;
    node->dirty = true;

    /* Write node */
    node_write(xdx, node);

    /* Note: A full B-tree implementation would handle underflow and
       rebalancing here. For simplicity, we just leave possibly
       underfilled nodes. The tree remains valid but may be suboptimal.
       REINDEX can be used to rebuild a balanced tree. */

    if (node != xdx->root) {
        node_free(node, xdx->header.order);
    }

    stack_free(&stack);
    return true;
}

bool xdx_seek(XDX *xdx, const void *key) {
    if (!xdx || !key) return false;

    XDXNode *node = xdx->root;
    xdx->found = false;
    xdx->current_recno = 0;

    /* Traverse tree */
    while (node) {
        int pos = find_key_pos(xdx, node, key);

        if (node->header.is_leaf) {
            /* Check for exact match */
            if (pos < node->header.key_count) {
                int cmp = xdx_key_compare(xdx, key, node->entries[pos].key);
                if (cmp == 0) {
                    xdx->found = true;
                    xdx->current_recno = node->entries[pos].recno;
                } else if (cmp < 0) {
                    /* Key would be before this position */
                    xdx->current_recno = node->entries[pos].recno;
                } else {
                    /* Key would be after all keys */
                    xdx->current_recno = 0;  /* EOF */
                }
            } else {
                xdx->current_recno = 0;  /* EOF */
            }

            if (node != xdx->root) {
                node_free(node, xdx->header.order);
            }
            break;
        }

        /* Descend to child */
        uint32_t child_offset;
        if (pos < node->header.key_count &&
            xdx_key_compare(xdx, key, node->entries[pos].key) == 0) {
            /* Exact match at internal node */
            xdx->found = true;
            xdx->current_recno = node->entries[pos].recno;
            if (node != xdx->root) {
                node_free(node, xdx->header.order);
            }
            break;
        }

        if (pos < node->header.key_count) {
            child_offset = node->entries[pos].child_offset;
        } else {
            child_offset = node->right_child;
        }

        XDXNode *child = node_read(xdx, child_offset);
        if (node != xdx->root) {
            node_free(node, xdx->header.order);
        }
        node = child;
    }

    return xdx->found;
}

uint32_t xdx_recno(XDX *xdx) {
    return xdx ? xdx->current_recno : 0;
}

bool xdx_found(XDX *xdx) {
    return xdx ? xdx->found : false;
}

bool xdx_go_top(XDX *xdx) {
    if (!xdx) return false;

    XDXNode *node = xdx->root;

    /* Go to leftmost leaf */
    while (node && !node->header.is_leaf) {
        uint32_t child_offset = node->entries[0].child_offset;
        XDXNode *child = node_read(xdx, child_offset);
        if (node != xdx->root) {
            node_free(node, xdx->header.order);
        }
        node = child;
    }

    if (node && node->header.key_count > 0) {
        xdx->current_recno = node->entries[0].recno;
        xdx->found = true;
        if (node != xdx->root) {
            node_free(node, xdx->header.order);
        }
        return true;
    }

    xdx->current_recno = 0;
    xdx->found = false;
    return false;
}

bool xdx_go_bottom(XDX *xdx) {
    if (!xdx) return false;

    XDXNode *node = xdx->root;

    /* Go to rightmost leaf */
    while (node && !node->header.is_leaf) {
        uint32_t child_offset = node->right_child;
        XDXNode *child = node_read(xdx, child_offset);
        if (node != xdx->root) {
            node_free(node, xdx->header.order);
        }
        node = child;
    }

    if (node && node->header.key_count > 0) {
        xdx->current_recno = node->entries[node->header.key_count - 1].recno;
        xdx->found = true;
        if (node != xdx->root) {
            node_free(node, xdx->header.order);
        }
        return true;
    }

    xdx->current_recno = 0;
    xdx->found = false;
    return false;
}

bool xdx_skip(XDX *xdx, int count) {
    if (!xdx || count == 0) return xdx_found(xdx);

    /* Simple implementation: just do sequential seeks
       A full implementation would track position in tree */
    /* For now, return false - full navigation requires more state */
    (void)count;
    return false;
}

bool xdx_eof(XDX *xdx) {
    return !xdx || xdx->current_recno == 0;
}

bool xdx_bof(XDX *xdx) {
    return !xdx || xdx->current_recno == 0;
}

bool xdx_reindex(XDX *xdx, DBF *dbf,
                 bool (*eval_key)(DBF *dbf, void *key, void *ctx),
                 void *ctx) {
    if (!xdx || !dbf || !eval_key) return false;

    /* Clear the index by creating new root */
    /* Truncate file after header */
    if (fseek(xdx->fp, XDX_HEADER_SIZE, SEEK_SET) != 0) {
        return false;
    }

    /* Note: ftruncate would be ideal here but isn't portable */
    /* Instead we just overwrite with new nodes */

    xdx->header.node_count = 0;

    /* Create new empty root */
    uint32_t new_root = node_create(xdx, true);
    if (new_root == 0) return false;

    xdx->header.root_offset = new_root;
    xdx->modified = true;

    /* Reload root */
    if (xdx->root) {
        node_free(xdx->root, xdx->header.order);
    }
    xdx->root = node_read(xdx, new_root);

    /* Iterate through all records and insert keys */
    uint32_t reccount = dbf_reccount(dbf);
    uint8_t *key = xcalloc(1, xdx->header.key_length);

    for (uint32_t recno = 1; recno <= reccount; recno++) {
        if (!dbf_goto(dbf, recno)) continue;
        if (dbf_deleted(dbf)) continue;

        /* Evaluate key expression */
        memset(key, ' ', xdx->header.key_length);
        if (!eval_key(dbf, key, ctx)) continue;

        /* Insert into index */
        xdx_insert(xdx, key, recno);
    }

    free(key);
    xdx_flush(xdx);

    return true;
}

const char *xdx_key_expr(XDX *xdx) {
    return xdx ? xdx->header.key_expr : NULL;
}

char xdx_key_type(XDX *xdx) {
    return xdx ? xdx->header.key_type : 0;
}

uint16_t xdx_key_length(XDX *xdx) {
    return xdx ? xdx->header.key_length : 0;
}

bool xdx_is_unique(XDX *xdx) {
    return xdx && (xdx->header.flags & XDX_FLAG_UNIQUE);
}

bool xdx_is_descending(XDX *xdx) {
    return xdx && (xdx->header.flags & XDX_FLAG_DESCENDING);
}
