/*
 * xBase3 - dBASE III+ Compatible Database System
 * dbf.c - DBF file engine implementation
 */

#include "dbf.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

/* Read DBF header from file */
static bool read_header(DBF *dbf) {
    uint8_t buf[32];

    if (fseek(dbf->fp, 0, SEEK_SET) != 0) return false;
    if (fread(buf, 1, 32, dbf->fp) != 32) return false;

    dbf->header.version = buf[0];
    dbf->header.year = buf[1];
    dbf->header.month = buf[2];
    dbf->header.day = buf[3];
    dbf->header.record_count = read_u32_le(&buf[4]);
    dbf->header.header_size = read_u16_le(&buf[8]);
    dbf->header.record_size = read_u16_le(&buf[10]);

    /* Validate version */
    if (dbf->header.version != DBF_VERSION_DBASE3 &&
        dbf->header.version != DBF_VERSION_DBASE3_MEMO) {
        error_set(ERR_INVALID_DBF, "Unsupported DBF version: 0x%02X", dbf->header.version);
        return false;
    }

    return true;
}

/* Write DBF header to file */
static bool write_header(DBF *dbf) {
    uint8_t buf[32] = {0};

    /* Update date */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    dbf->header.year = (uint8_t)(tm->tm_year % 100);
    dbf->header.month = (uint8_t)(tm->tm_mon + 1);
    dbf->header.day = (uint8_t)tm->tm_mday;

    /* Ensure version is set correctly */
    if (dbf->header.version != DBF_VERSION_DBASE3 &&
        dbf->header.version != DBF_VERSION_DBASE3_MEMO) {
        dbf->header.version = DBF_VERSION_DBASE3;
    }

    buf[0] = dbf->header.version;
    buf[1] = dbf->header.year;
    buf[2] = dbf->header.month;
    buf[3] = dbf->header.day;
    write_u32_le(&buf[4], dbf->header.record_count);
    write_u16_le(&buf[8], dbf->header.header_size);
    write_u16_le(&buf[10], dbf->header.record_size);

    if (fseek(dbf->fp, 0, SEEK_SET) != 0) return false;
    if (fwrite(buf, 1, 32, dbf->fp) != 32) return false;

    return true;
}

/* Read field descriptors */
static bool read_fields(DBF *dbf) {
    int max_fields = (dbf->header.header_size - 32 - 1) / 32;
    if (max_fields <= 0 || max_fields > MAX_FIELDS) {
        error_set(ERR_INVALID_DBF, "Invalid field count");
        return false;
    }

    dbf->fields = xcalloc((size_t)max_fields, sizeof(DBFField));
    dbf->field_count = 0;

    uint8_t buf[32];
    uint16_t offset = 1;  /* First byte is delete flag */

    for (int i = 0; i < max_fields; i++) {
        if (fread(buf, 1, 32, dbf->fp) != 32) break;

        /* Check for header terminator */
        if (buf[0] == DBF_HEADER_TERM) break;

        DBFField *field = &dbf->fields[dbf->field_count];

        /* Field name (11 bytes, null-terminated) */
        memcpy(field->name, buf, 10);
        field->name[10] = '\0';
        str_trim_right(field->name);
        str_upper(field->name);

        field->type = (char)toupper(buf[11]);
        field->length = buf[16];
        field->decimals = buf[17];
        field->offset = offset;

        offset += field->length;
        dbf->field_count++;
    }

    /* Verify record size matches */
    if (offset != dbf->header.record_size) {
        error_set(ERR_INVALID_DBF, "Record size mismatch");
        return false;
    }

    return true;
}

/* Write field descriptors */
static bool write_fields(DBF *dbf) {
    uint8_t buf[32];

    for (int i = 0; i < dbf->field_count; i++) {
        DBFField *field = &dbf->fields[i];
        memset(buf, 0, 32);

        /* Field name */
        strncpy((char *)buf, field->name, 10);

        buf[11] = (uint8_t)field->type;
        buf[16] = (uint8_t)field->length;
        buf[17] = field->decimals;

        if (fwrite(buf, 1, 32, dbf->fp) != 32) return false;
    }

    /* Header terminator */
    uint8_t term = DBF_HEADER_TERM;
    if (fwrite(&term, 1, 1, dbf->fp) != 1) return false;

    return true;
}

/* Read current record into buffer */
static bool read_record(DBF *dbf) {
    if (dbf->current_record == 0 || dbf->current_record > dbf->header.record_count) {
        return false;
    }

    long offset = dbf->header.header_size +
                  (long)(dbf->current_record - 1) * dbf->header.record_size;

    if (fseek(dbf->fp, offset, SEEK_SET) != 0) return false;
    if (fread(dbf->record_buffer, 1, dbf->header.record_size, dbf->fp)
        != dbf->header.record_size) return false;

    dbf->deleted = (dbf->record_buffer[0] == DBF_RECORD_DELETED);
    dbf->modified = false;

    return true;
}

/* Write current record from buffer */
static bool write_record(DBF *dbf) {
    if (dbf->readonly) {
        error_set(ERR_FILE_WRITE, "Database is read-only");
        return false;
    }

    if (dbf->current_record == 0 || dbf->current_record > dbf->header.record_count) {
        return false;
    }

    long offset = dbf->header.header_size +
                  (long)(dbf->current_record - 1) * dbf->header.record_size;

    if (fseek(dbf->fp, offset, SEEK_SET) != 0) return false;
    if (fwrite(dbf->record_buffer, 1, dbf->header.record_size, dbf->fp)
        != dbf->header.record_size) return false;

    dbf->modified = false;
    return true;
}

/* Open existing DBF file */
DBF *dbf_open(const char *filename, bool readonly) {
    DBF *dbf = xcalloc(1, sizeof(DBF));

    dbf->fp = fopen(filename, readonly ? "rb" : "r+b");
    if (!dbf->fp) {
        error_set(ERR_FILE_NOT_FOUND, "%s", filename);
        xfree(dbf);
        return NULL;
    }

    strncpy(dbf->filename, filename, MAX_PATH_LEN - 1);
    dbf->readonly = readonly;

    /* Read header */
    if (!read_header(dbf)) {
        fclose(dbf->fp);
        xfree(dbf);
        return NULL;
    }

    /* Read field descriptors */
    if (!read_fields(dbf)) {
        fclose(dbf->fp);
        xfree(dbf->fields);
        xfree(dbf);
        return NULL;
    }

    /* Allocate record buffer */
    dbf->record_buffer = xcalloc(dbf->header.record_size, 1);

    /* Set alias from filename */
    file_basename(dbf->alias, filename);
    str_upper(dbf->alias);

    /* Position at first record */
    dbf->current_record = 0;
    dbf->bof = true;
    dbf->eof = (dbf->header.record_count == 0);

    if (dbf->header.record_count > 0) {
        dbf_go_top(dbf);
    }

    return dbf;
}

/* Create new DBF file */
DBF *dbf_create(const char *filename, const DBFField *fields, int field_count) {
    if (field_count <= 0 || field_count > MAX_FIELDS) {
        error_set(ERR_INVALID_FIELD, "Invalid field count");
        return NULL;
    }

    DBF *dbf = xcalloc(1, sizeof(DBF));

    dbf->fp = fopen(filename, "w+b");
    if (!dbf->fp) {
        error_set(ERR_FILE_CREATE, "%s", filename);
        xfree(dbf);
        return NULL;
    }

    strncpy(dbf->filename, filename, MAX_PATH_LEN - 1);
    dbf->readonly = false;

    /* Initialize header */
    dbf->header.version = DBF_VERSION_DBASE3;
    dbf->header.record_count = 0;
    dbf->header.header_size = (uint16_t)(32 + field_count * 32 + 1);

    /* Copy and validate fields, calculate record size */
    dbf->fields = xcalloc((size_t)field_count, sizeof(DBFField));
    dbf->field_count = field_count;

    uint16_t record_size = 1;  /* Delete flag */
    for (int i = 0; i < field_count; i++) {
        DBFField *dst = &dbf->fields[i];
        const DBFField *src = &fields[i];

        strncpy(dst->name, src->name, MAX_FIELD_NAME - 1);
        str_upper(dst->name);
        dst->type = (char)toupper(src->type);
        dst->length = src->length;
        dst->decimals = src->decimals;
        dst->offset = record_size;

        /* Validate field */
        switch (dst->type) {
            case FIELD_TYPE_CHAR:
                if (dst->length == 0 || dst->length > MAX_FIELD_LEN) {
                    error_set(ERR_INVALID_FIELD, "Invalid character field length");
                    goto error;
                }
                break;
            case FIELD_TYPE_NUMERIC:
                if (dst->length == 0 || dst->length > 20) {
                    error_set(ERR_INVALID_FIELD, "Invalid numeric field length");
                    goto error;
                }
                break;
            case FIELD_TYPE_DATE:
                dst->length = 8;
                dst->decimals = 0;
                break;
            case FIELD_TYPE_LOGICAL:
                dst->length = 1;
                dst->decimals = 0;
                break;
            case FIELD_TYPE_MEMO:
                dst->length = 10;
                dst->decimals = 0;
                break;
            default:
                error_set(ERR_INVALID_FIELD, "Unknown field type: %c", dst->type);
                goto error;
        }

        record_size += dst->length;
    }

    dbf->header.record_size = record_size;

    /* Write header */
    if (!write_header(dbf)) goto error;

    /* Write field descriptors */
    if (!write_fields(dbf)) goto error;

    /* Write EOF marker */
    uint8_t eof = DBF_EOF_MARKER;
    if (fwrite(&eof, 1, 1, dbf->fp) != 1) goto error;

    fflush(dbf->fp);

    /* Allocate record buffer */
    dbf->record_buffer = xcalloc(dbf->header.record_size, 1);

    /* Set alias from filename */
    file_basename(dbf->alias, filename);
    str_upper(dbf->alias);

    dbf->current_record = 0;
    dbf->bof = true;
    dbf->eof = true;

    return dbf;

error:
    fclose(dbf->fp);
    xfree(dbf->fields);
    xfree(dbf);
    return NULL;
}

/* Close DBF file */
void dbf_close(DBF *dbf) {
    if (!dbf) return;

    if (dbf->modified) {
        write_record(dbf);
    }

    if (dbf->fp) {
        fflush(dbf->fp);
        fclose(dbf->fp);
    }

    xfree(dbf->record_buffer);
    xfree(dbf->fields);
    xfree(dbf);
}

/* Navigation functions */
bool dbf_goto(DBF *dbf, uint32_t recno) {
    if (!dbf) return false;

    /* Flush any pending changes */
    if (dbf->modified) {
        write_record(dbf);
    }

    if (recno == 0) {
        dbf->current_record = 0;
        dbf->bof = true;
        dbf->eof = false;
        memset(dbf->record_buffer, ' ', dbf->header.record_size);
        dbf->record_buffer[0] = DBF_RECORD_ACTIVE;
        return true;
    }

    if (recno > dbf->header.record_count) {
        dbf->current_record = dbf->header.record_count + 1;
        dbf->bof = false;
        dbf->eof = true;
        memset(dbf->record_buffer, ' ', dbf->header.record_size);
        dbf->record_buffer[0] = DBF_RECORD_ACTIVE;
        return true;
    }

    dbf->current_record = recno;
    dbf->bof = false;
    dbf->eof = false;

    return read_record(dbf);
}

bool dbf_skip(DBF *dbf, int count) {
    if (!dbf) return false;

    if (count == 0) return true;

    uint32_t new_recno;
    if (count > 0) {
        new_recno = dbf->current_record + (uint32_t)count;
    } else {
        if ((uint32_t)(-count) >= dbf->current_record) {
            new_recno = 0;
        } else {
            new_recno = dbf->current_record - (uint32_t)(-count);
        }
    }

    return dbf_goto(dbf, new_recno);
}

bool dbf_go_top(DBF *dbf) {
    if (!dbf) return false;

    if (dbf->header.record_count == 0) {
        dbf->bof = true;
        dbf->eof = true;
        dbf->current_record = 0;
        return true;
    }

    return dbf_goto(dbf, 1);
}

bool dbf_go_bottom(DBF *dbf) {
    if (!dbf) return false;

    if (dbf->header.record_count == 0) {
        dbf->bof = true;
        dbf->eof = true;
        dbf->current_record = 0;
        return true;
    }

    return dbf_goto(dbf, dbf->header.record_count);
}

/* Status functions */
bool dbf_eof(DBF *dbf) {
    return dbf ? dbf->eof : true;
}

bool dbf_bof(DBF *dbf) {
    return dbf ? dbf->bof : true;
}

uint32_t dbf_recno(DBF *dbf) {
    return dbf ? dbf->current_record : 0;
}

uint32_t dbf_reccount(DBF *dbf) {
    return dbf ? dbf->header.record_count : 0;
}

bool dbf_deleted(DBF *dbf) {
    return dbf ? dbf->deleted : false;
}

/* Append blank record */
bool dbf_append_blank(DBF *dbf) {
    if (!dbf || dbf->readonly) {
        error_set(ERR_FILE_WRITE, "Cannot append to read-only database");
        return false;
    }

    /* Flush any pending changes */
    if (dbf->modified) {
        write_record(dbf);
    }

    /* Initialize blank record */
    memset(dbf->record_buffer, ' ', dbf->header.record_size);
    dbf->record_buffer[0] = DBF_RECORD_ACTIVE;

    /* Position at end of file (before EOF marker) */
    long offset = dbf->header.header_size +
                  (long)dbf->header.record_count * dbf->header.record_size;

    if (fseek(dbf->fp, offset, SEEK_SET) != 0) return false;

    /* Write blank record */
    if (fwrite(dbf->record_buffer, 1, dbf->header.record_size, dbf->fp)
        != dbf->header.record_size) return false;

    /* Write EOF marker */
    uint8_t eof = DBF_EOF_MARKER;
    if (fwrite(&eof, 1, 1, dbf->fp) != 1) return false;

    /* Update header */
    dbf->header.record_count++;
    if (!write_header(dbf)) return false;

    fflush(dbf->fp);

    /* Position at new record */
    dbf->current_record = dbf->header.record_count;
    dbf->bof = false;
    dbf->eof = false;
    dbf->deleted = false;
    dbf->modified = false;

    return true;
}

/* Delete current record */
bool dbf_delete(DBF *dbf) {
    if (!dbf || dbf->readonly) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    dbf->record_buffer[0] = DBF_RECORD_DELETED;
    dbf->deleted = true;
    dbf->modified = true;

    return true;
}

/* Recall (undelete) current record */
bool dbf_recall(DBF *dbf) {
    if (!dbf || dbf->readonly) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    dbf->record_buffer[0] = DBF_RECORD_ACTIVE;
    dbf->deleted = false;
    dbf->modified = true;

    return true;
}

/* Flush changes to disk */
bool dbf_flush(DBF *dbf) {
    if (!dbf) return false;

    if (dbf->modified) {
        if (!write_record(dbf)) return false;
    }

    fflush(dbf->fp);
    return true;
}

/* Field access */
int dbf_field_index(DBF *dbf, const char *name) {
    if (!dbf || !name) return -1;

    for (int i = 0; i < dbf->field_count; i++) {
        if (str_casecmp(dbf->fields[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

const DBFField *dbf_field_info(DBF *dbf, int index) {
    if (!dbf || index < 0 || index >= dbf->field_count) return NULL;
    return &dbf->fields[index];
}

int dbf_field_count(DBF *dbf) {
    return dbf ? dbf->field_count : 0;
}

/* Get field value as string */
bool dbf_get_string(DBF *dbf, int field_index, char *buffer, size_t bufsize) {
    if (!dbf || !buffer || bufsize == 0) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    size_t len = field->length < bufsize - 1 ? field->length : bufsize - 1;

    memcpy(buffer, &dbf->record_buffer[field->offset], len);
    buffer[len] = '\0';

    return true;
}

/* Get field value as double */
bool dbf_get_double(DBF *dbf, int field_index, double *value) {
    if (!dbf || !value) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    if (field->type != FIELD_TYPE_NUMERIC) {
        error_set(ERR_TYPE_MISMATCH, "Field is not numeric");
        return false;
    }

    char buf[32];
    size_t len = field->length < 31 ? field->length : 31;
    memcpy(buf, &dbf->record_buffer[field->offset], len);
    buf[len] = '\0';

    return str_to_num(buf, value);
}

/* Get field value as logical */
bool dbf_get_logical(DBF *dbf, int field_index, bool *value) {
    if (!dbf || !value) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    if (field->type != FIELD_TYPE_LOGICAL) {
        error_set(ERR_TYPE_MISMATCH, "Field is not logical");
        return false;
    }

    char c = dbf->record_buffer[field->offset];
    *value = (c == 'T' || c == 't' || c == 'Y' || c == 'y');

    return true;
}

/* Get field value as date (YYYYMMDD) */
bool dbf_get_date(DBF *dbf, int field_index, char *buffer) {
    if (!dbf || !buffer) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    if (field->type != FIELD_TYPE_DATE) {
        error_set(ERR_TYPE_MISMATCH, "Field is not date");
        return false;
    }

    memcpy(buffer, &dbf->record_buffer[field->offset], 8);
    buffer[8] = '\0';

    return true;
}

/* Set field value as string */
bool dbf_put_string(DBF *dbf, int field_index, const char *value) {
    if (!dbf || dbf->readonly) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];

    /* Clear field with spaces */
    memset(&dbf->record_buffer[field->offset], ' ', field->length);

    if (value) {
        size_t len = strlen(value);
        if (len > field->length) len = field->length;
        memcpy(&dbf->record_buffer[field->offset], value, len);
    }

    dbf->modified = true;
    return true;
}

/* Set field value as double */
bool dbf_put_double(DBF *dbf, int field_index, double value) {
    if (!dbf || dbf->readonly) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    if (field->type != FIELD_TYPE_NUMERIC) {
        error_set(ERR_TYPE_MISMATCH, "Field is not numeric");
        return false;
    }

    char buf[64];
    num_to_str(value, buf, field->length, field->decimals);

    /* Right-align in field */
    memset(&dbf->record_buffer[field->offset], ' ', field->length);
    size_t len = strlen(buf);
    if (len > field->length) len = field->length;
    size_t offset = field->length - len;
    memcpy(&dbf->record_buffer[field->offset + offset], buf, len);

    dbf->modified = true;
    return true;
}

/* Set field value as logical */
bool dbf_put_logical(DBF *dbf, int field_index, bool value) {
    if (!dbf || dbf->readonly) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    if (field->type != FIELD_TYPE_LOGICAL) {
        error_set(ERR_TYPE_MISMATCH, "Field is not logical");
        return false;
    }

    dbf->record_buffer[field->offset] = value ? 'T' : 'F';
    dbf->modified = true;

    return true;
}

/* Set field value as date (YYYYMMDD) */
bool dbf_put_date(DBF *dbf, int field_index, const char *value) {
    if (!dbf || dbf->readonly) return false;
    if (field_index < 0 || field_index >= dbf->field_count) return false;
    if (dbf->current_record == 0 || dbf->eof) return false;

    const DBFField *field = &dbf->fields[field_index];
    if (field->type != FIELD_TYPE_DATE) {
        error_set(ERR_TYPE_MISMATCH, "Field is not date");
        return false;
    }

    /* Clear field with spaces */
    memset(&dbf->record_buffer[field->offset], ' ', 8);

    if (value && strlen(value) == 8) {
        memcpy(&dbf->record_buffer[field->offset], value, 8);
    }

    dbf->modified = true;
    return true;
}

/* Pack database (remove deleted records) */
bool dbf_pack(DBF *dbf) {
    if (!dbf || dbf->readonly) {
        error_set(ERR_FILE_WRITE, "Cannot pack read-only database");
        return false;
    }

    /* Flush any pending changes */
    if (dbf->modified) {
        write_record(dbf);
    }

    uint8_t *buffer = xmalloc(dbf->header.record_size);
    uint32_t write_recno = 0;

    for (uint32_t read_recno = 1; read_recno <= dbf->header.record_count; read_recno++) {
        /* Read record */
        long read_offset = dbf->header.header_size +
                          (long)(read_recno - 1) * dbf->header.record_size;
        if (fseek(dbf->fp, read_offset, SEEK_SET) != 0) {
            xfree(buffer);
            return false;
        }
        if (fread(buffer, 1, dbf->header.record_size, dbf->fp) != dbf->header.record_size) {
            xfree(buffer);
            return false;
        }

        /* Skip deleted records */
        if (buffer[0] == DBF_RECORD_DELETED) continue;

        write_recno++;

        /* Write record at new position if needed */
        if (write_recno != read_recno) {
            long write_offset = dbf->header.header_size +
                               (long)(write_recno - 1) * dbf->header.record_size;
            if (fseek(dbf->fp, write_offset, SEEK_SET) != 0) {
                xfree(buffer);
                return false;
            }
            if (fwrite(buffer, 1, dbf->header.record_size, dbf->fp) != dbf->header.record_size) {
                xfree(buffer);
                return false;
            }
        }
    }

    xfree(buffer);

    /* Update record count */
    dbf->header.record_count = write_recno;

    /* Truncate file */
    long new_size = dbf->header.header_size +
                   (long)dbf->header.record_count * dbf->header.record_size + 1;

    /* Write EOF marker */
    if (fseek(dbf->fp, new_size - 1, SEEK_SET) != 0) return false;
    uint8_t eof = DBF_EOF_MARKER;
    if (fwrite(&eof, 1, 1, dbf->fp) != 1) return false;

    /* Update header */
    if (!write_header(dbf)) return false;

    fflush(dbf->fp);

    /* Reposition to first record */
    dbf_go_top(dbf);

    return true;
}

/* Zap database (remove all records) */
bool dbf_zap(DBF *dbf) {
    if (!dbf || dbf->readonly) {
        error_set(ERR_FILE_WRITE, "Cannot zap read-only database");
        return false;
    }

    /* Update record count */
    dbf->header.record_count = 0;

    /* Write EOF marker after header */
    if (fseek(dbf->fp, dbf->header.header_size, SEEK_SET) != 0) return false;
    uint8_t eof = DBF_EOF_MARKER;
    if (fwrite(&eof, 1, 1, dbf->fp) != 1) return false;

    /* Update header */
    if (!write_header(dbf)) return false;

    fflush(dbf->fp);

    /* Reset position */
    dbf->current_record = 0;
    dbf->bof = true;
    dbf->eof = true;
    dbf->deleted = false;
    dbf->modified = false;

    memset(dbf->record_buffer, ' ', dbf->header.record_size);
    dbf->record_buffer[0] = DBF_RECORD_ACTIVE;

    return true;
}

/* Set/get alias */
void dbf_set_alias(DBF *dbf, const char *alias) {
    if (!dbf || !alias) return;
    strncpy(dbf->alias, alias, MAX_FIELD_NAME - 1);
    dbf->alias[MAX_FIELD_NAME - 1] = '\0';
    str_upper(dbf->alias);
}

const char *dbf_get_alias(DBF *dbf) {
    return dbf ? dbf->alias : NULL;
}
