# Test sources (shared with main)
set(TEST_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/util.c
    ${CMAKE_SOURCE_DIR}/src/dbf.c
    ${CMAKE_SOURCE_DIR}/src/lexer.c
    ${CMAKE_SOURCE_DIR}/src/ast.c
    ${CMAKE_SOURCE_DIR}/src/parser.c
    ${CMAKE_SOURCE_DIR}/src/expr.c
    ${CMAKE_SOURCE_DIR}/src/functions.c
    ${CMAKE_SOURCE_DIR}/src/variables.c
    ${CMAKE_SOURCE_DIR}/src/commands.c
)

# DBF engine tests
add_executable(test_dbf test_dbf.c ${TEST_COMMON_SOURCES})
target_include_directories(test_dbf PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(UNIX)
    target_link_libraries(test_dbf PRIVATE m)
endif()
add_test(NAME dbf_tests COMMAND test_dbf)

# Lexer tests
add_executable(test_lexer test_lexer.c ${TEST_COMMON_SOURCES})
target_include_directories(test_lexer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(UNIX)
    target_link_libraries(test_lexer PRIVATE m)
endif()
add_test(NAME lexer_tests COMMAND test_lexer)

# Parser tests
add_executable(test_parser test_parser.c ${TEST_COMMON_SOURCES})
target_include_directories(test_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(UNIX)
    target_link_libraries(test_parser PRIVATE m)
endif()
add_test(NAME parser_tests COMMAND test_parser)

# Expression evaluator tests
add_executable(test_expr test_expr.c ${TEST_COMMON_SOURCES})
target_include_directories(test_expr PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
if(UNIX)
    target_link_libraries(test_expr PRIVATE m)
endif()
add_test(NAME expr_tests COMMAND test_expr)
