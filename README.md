# xBase3

A dBASE III+ compatible database system written in C.

## Features

- **Full DBF Support**: Read and write standard dBASE III+ .DBF files
- **XDX Index Engine**: Custom B-tree index format for fast lookups
- **Interactive REPL**: Classic dot-prompt interface
- **Expression Evaluator**: Arithmetic, string, logical, and relational operations
- **Built-in Functions**: String, numeric, date, and conversion functions
- **Memory Variables**: PUBLIC, PRIVATE, and LOCAL scope support

## Building

### Requirements

- C11 compatible compiler (clang or gcc)
- Make or CMake

### Build with Make

```bash
make
```

### Build with CMake

```bash
cmake -B build
cmake --build build
```

### Run Tests

```bash
make test
```

## Usage

### Interactive Mode

```bash
./build/xbase3
```

### Run a Script

```bash
./build/xbase3 script.prg
```

### Execute Single Command

```bash
./build/xbase3 -c "? 1 + 2"
```

## Supported Commands

| Command | Description |
|---------|-------------|
| `CREATE <file>` | Create new database (interactive field definition) |
| `USE <file>` | Open database file |
| `CLOSE` | Close current database |
| `APPEND BLANK` | Add new blank record |
| `REPLACE <field> WITH <value>` | Update field value |
| `DELETE` / `RECALL` | Mark/unmark record as deleted |
| `PACK` | Remove deleted records |
| `ZAP` | Delete all records |
| `GO <n>` / `GO TOP` / `GO BOTTOM` | Navigate to record |
| `SKIP [n]` | Move forward/backward |
| `LIST` / `DISPLAY` | Show records |
| `LOCATE FOR <condition>` | Find record |
| `CONTINUE` | Find next matching record |
| `INDEX ON <expr> TO <file>` | Create index on expression |
| `SET INDEX TO <file>` | Open existing index |
| `SET ORDER TO <n>` | Select controlling index |
| `SEEK <value>` | Find record by index key |
| `REINDEX` | Rebuild open indexes |
| `CLOSE INDEXES` | Close all indexes |
| `?` / `??` | Print expressions |
| `STORE <value> TO <var>` | Assign variable |
| `QUIT` | Exit program |

## Supported Field Types

| Type | Description | Example |
|------|-------------|---------|
| C | Character | `name,C,30` |
| N | Numeric | `age,N,3,0` or `price,N,10,2` |
| D | Date | `birthdate,D,8` |
| L | Logical | `active,L,1` |
| M | Memo | `notes,M,10` |

## Built-in Functions

### String Functions
- `LEN()`, `TRIM()`, `LTRIM()`, `RTRIM()`
- `UPPER()`, `LOWER()`
- `SUBSTR()`, `LEFT()`, `RIGHT()`
- `AT()`, `STUFF()`, `SPACE()`, `REPLICATE()`

### Numeric Functions
- `ABS()`, `INT()`, `ROUND()`
- `SQRT()`, `MOD()`, `MAX()`, `MIN()`

### Date Functions
- `DATE()`, `YEAR()`, `MONTH()`, `DAY()`
- `DOW()`, `CDOW()`, `CMONTH()`
- `DTOC()`, `CTOD()`

### Conversion Functions
- `STR()`, `VAL()`, `CHR()`, `ASC()`

### Database Functions
- `RECNO()`, `RECCOUNT()`, `EOF()`, `BOF()`
- `DELETED()`, `FIELD()`, `FCOUNT()`

### Other Functions
- `IIF()`, `TYPE()`, `EMPTY()`

## Example Session

```
xBase3 version 0.1.0
dBASE III+ Compatible Database System
Type QUIT to exit, ? expr to evaluate

. CREATE employees
Enter fields (name,type,length[,decimals]) - blank line to finish:
Field 1: name,C,30
Field 2: salary,N,10,2
Field 3:
Database employees.dbf created with 2 field(s)
EMPLOYEES> APPEND BLANK
Record 1 appended
EMPLOYEES> REPLACE name WITH "John Doe", salary WITH 50000
1 record(s) replaced
EMPLOYEES> APPEND BLANK
Record 2 appended
EMPLOYEES> REPLACE name WITH "Jane Smith", salary WITH 60000
1 record(s) replaced
EMPLOYEES> LIST
       1   John Doe                       50000.00
       2   Jane Smith                     60000.00
EMPLOYEES> ? SUM(salary)
110000.00
EMPLOYEES> QUIT
```

## File Formats

### DBF (Database Files)
Standard dBASE III+ format - fully compatible with other dBASE implementations.

### XDX (Index Files)
Custom B-tree index format:
- 512-byte header with key expression, type, and flags
- B-tree nodes with configurable order (default: 50 keys per node)
- Supports Character, Numeric, and Date key types
- Optional UNIQUE and DESCENDING flags

## Index Example

```
. USE customers
CUSTOMERS> INDEX ON name TO custname UNIQUE
100 record(s) indexed
CUSTOMERS> SEEK "Smith"
Found at record 42
CUSTOMERS> ? NAME
Smith, John
CUSTOMERS> SET INDEX TO
Indexes closed
```

## License

MIT License

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
