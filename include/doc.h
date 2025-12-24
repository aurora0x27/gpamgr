#pragma once

constexpr const auto MiniSQLDoc =

    // clang-format off
R"(
============================================================
                     MiniSQL Overview
============================================================

1. Introduction
------------------------------------------------------------
MiniSQL is a lightweight, educational SQL-like query language
designed for a single-table student grade management system.

The goal of MiniSQL is to:
- Provide a clear and minimal subset of SQL
- Avoid complex features such as JOINs and subqueries
- Map each statement directly to a sequence of executable plans
- Be easy to implement using a simple lexer, parser, and executor

MiniSQL focuses on clarity and executability rather than full SQL
compatibility.

------------------------------------------------------------

2. Data Model
------------------------------------------------------------
The system operates on exactly one active table at a time.

A table consists of:
- A schema (column name + type)
- A set of rows stored in memory
- Optional indexes on selected columns

Supported column types:
- INT      : signed integer
- FLOAT    : floating point number
- STRING   : text string

Each row represents one student record.

------------------------------------------------------------

3. Supported Statements
------------------------------------------------------------

3.1 SELECT
------------------------------------------------------------
Syntax:

  SELECT select_list
  FROM table_name
  [WHERE condition]
  [ORDER BY column [ASC | DESC]] ;

select_list:
  *                       select all columns
  col1, col2, ...         select specific columns

Examples:

  SELECT * FROM students;

  SELECT sid, name, math
  FROM student_scores
  WHERE math >= 60;

------------------------------------------------------------

3.2 WHERE Clause
------------------------------------------------------------
The WHERE clause filters rows based on boolean expressions.

Supported operators:

  =    !=    <    <=    >    >=

Logical operators:

  AND    OR

String matching:

  LIKE

Examples:

  WHERE math >= 60 AND english >= 60;

  WHERE name LIKE "Zhang%";

------------------------------------------------------------

3.3 ORDER BY
------------------------------------------------------------
Syntax:

  ORDER BY column [ASC | DESC]

Examples:

  ORDER BY math DESC;

  ORDER BY name ASC;

------------------------------------------------------------

4. Data Modification Statements
------------------------------------------------------------

4.1 INSERT
------------------------------------------------------------
Syntax:

  INSERT INTO table_name VALUES (value1, value2, ...);

Example:

  INSERT INTO student_scores
  VALUES (10001, "Zhang San", 90, 85, 88);

------------------------------------------------------------

4.2 DELETE
------------------------------------------------------------
Syntax:

  DELETE FROM table_name [WHERE condition];

Example:

  DELETE FROM student_scores
  WHERE sid = 10001;

------------------------------------------------------------

4.3 UPDATE
------------------------------------------------------------
Syntax:

  UPDATE table_name
  SET column = value
  [WHERE condition];

Example:

  UPDATE student_scores
  SET math = 95
  WHERE sid = 10001;

------------------------------------------------------------

5. Literals
------------------------------------------------------------

5.1 Numeric Literals
------------------------------------------------------------
- Integers:
    0, 42, 100

- Floating point numbers:
    3.14, 0.5, 78.25

Scientific notation is not supported.

------------------------------------------------------------

5.2 String Literals
------------------------------------------------------------
- Enclosed in double or single quotes
- No multiline strings

Examples:

  "Alice"
  'Bob'
  "Zhang San"

------------------------------------------------------------

6. Execution Model
------------------------------------------------------------
Each MiniSQL statement is translated into a linear execution plan.

Example:

  SELECT name FROM students WHERE math >= 60;

Execution plan:

  ScanPlan
    -> FilterPlan
    -> ProjectPlan

Each plan node:
- Performs exactly one operation
- Reads and writes intermediate results via ExecContext
- Is executed sequentially

------------------------------------------------------------

7. Error Handling
------------------------------------------------------------
- Lexical errors are detected during tokenization
- Syntax errors are reported during parsing
- Execution errors are reported at runtime

Error messages include precise source positions when possible.

------------------------------------------------------------

8. Design Philosophy
------------------------------------------------------------
- Simplicity over completeness
- Deterministic execution order
- Explicit and readable semantics
- Direct mapping from SQL syntax to execution plans

MiniSQL is intentionally limited, but structurally aligned with
real database query engines.

============================================================
)";
// clang-format on
