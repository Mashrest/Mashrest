#include "API/msrest_api.h"
#include <stdlib.h>
#include <stdint.h>
#include <math.h>   
#include <string.h>  
#include <ctype.h>      

#define LIB_NAME "EVAL"


/* парсинг математики */

typedef struct Parser {
    const char* input;
    size_t pos;
    size_t len;
} Parser;

/* пропуск пробелов */
static void skip_spaces(Parser* p) {
    while (p->pos < p->len && isspace(p->input[p->pos])) {
        p->pos++;
    }
}

/* проверка, что текущий символ - цифра */
static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* парсинг числа */
static double parse_number(Parser* p) {
    skip_spaces(p);
    const char* start = p->input + p->pos;
    char* end;
    double num = strtod(start, &end);
    p->pos += (end - start);
    return num;
}

/* создание узла-числа */
static Value make_number_node(MsrestAPI* api, double value) {
    Value node = api->dict_new();
    api->dict_set(node, api->string_new_len("type", 4), api->string_new_len("number", 6));
    api->dict_set(node, api->string_new_len("value", 5), api->float_to_value(value));
    return node;
}

/* слздание узла-бинарной операции */
static Value make_binop_node(MsrestAPI* api, const char* op, Value left, Value right) {
    Value node = api->dict_new();
    api->dict_set(node, api->string_new_len("type", 4), api->string_new_len("binop", 5));
    api->dict_set(node, api->string_new_len("op", 2), api->string_new_len(op, strlen(op)));
    api->dict_set(node, api->string_new_len("left", 4), left);
    api->dict_set(node, api->string_new_len("right", 5), right);
    return node;
}

/* парсинг выражения (рекурсивный спуск) */
static Value parse_expression(MsrestAPI* api, Parser* p, int min_precedence);

/* парсинг первичного выражения (число или скобки) */
static Value parse_primary(MsrestAPI* api, Parser* p) {
    skip_spaces(p);
    
    if (p->pos >= p->len) {
        api->msrest_panic("[" LIB_NAME "] eval(): unexpected end of expression");
    }
    
    char c = p->input[p->pos];
    
    /* Число */
    if (is_digit(c) || c == '.') {
        double num = parse_number(p);
        return make_number_node(api, num);
    }
    
    /* скобки */
    if (c == '(') {
        p->pos++;  // пропускаем '('
        Value node = parse_expression(api, p, 0);
        skip_spaces(p);
        if (p->pos >= p->len || p->input[p->pos] != ')') {
            api->msrest_panic("[" LIB_NAME "] eval(): expected ')'");
        }
        p->pos++;  // пропускаем ')'
        return node;
    }
    
    api->msrest_panic("[" LIB_NAME "] eval(): unexpected character '%c'", c);
}

/* проверка, является ли символ оператором */
static int is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

/* получение приоритета оператора */
static int get_precedence(char op) {
    switch (op) {
        case '+':
        case '-': return 1;
        case '*':
        case '/': return 2;
        case '^': return 3;
        default: return 0;
    }
}

/* парсинг выражения с учётом приоритетов */
static Value parse_expression(MsrestAPI* api, Parser* p, int min_precedence) {
    /* парсим левую часть */
    Value left = parse_primary(api, p);
    
    while (1) {
        skip_spaces(p);
        if (p->pos >= p->len) break;
        
        char op = p->input[p->pos];
        if (!is_operator(op)) break;
        
        int prec = get_precedence(op);
        if (prec < min_precedence) break;
        
        p->pos++;  // пропускаем оператор
        
        /* парсим правую часть с учётом приоритета */
        Value right = parse_expression(api, p, prec + 1);
        
        /* создаём узел бинарной операции */
        char op_str[2] = {op, '\0'};
        left = make_binop_node(api, op_str, left, right);
    }
    
    return left;
}

/* экспортируемая функция eval */
EXPORT Value build_ast(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_STRING, args[0].type, LIB_NAME, "build_ast");
    
    const char* expr = api->string_data(args[0]);
    size_t len = api->string_bytes(args[0]);
    
    if (len == 0) {
        api->msrest_panic("[" LIB_NAME "] build_ast(): empty expression");
    }
    
    Parser parser;
    parser.input = expr;
    parser.pos = 0;
    parser.len = len;
    
    /* парсим выражение */
    Value ast = parse_expression(api, &parser, 0);
    
    /* проверяем, что дошли до конца */
    skip_spaces(&parser);
    if (parser.pos < parser.len) {
        api->msrest_panic("[" LIB_NAME "] build_ast(): unexpected characters after expression");
    }
    
    return ast;
}


/* вычисление AST */
static double eval_ast_node(MsrestAPI* api, Value node) {
    Value type_val = api->dict_get(node, api->string_new_len("type", 4));
    const char* type = api->string_data(type_val);
    
    if (strcmp(type, "number") == 0) {
        Value val = api->dict_get(node, api->string_new_len("value", 5));
        return api->value_to_double(val);
    }
    
    if (strcmp(type, "binop") == 0) {
        Value op_val = api->dict_get(node, api->string_new_len("op", 2));
        const char* op = api->string_data(op_val);
        
        Value left_node = api->dict_get(node, api->string_new_len("left", 4));
        Value right_node = api->dict_get(node, api->string_new_len("right", 5));
        
        double left = eval_ast_node(api, left_node);
        double right = eval_ast_node(api, right_node);
        
        switch (op[0]) {
            case '+': return left + right;
            case '-': return left - right;
            case '*': return left * right;
            case '/': return left / right;
            case '^': return pow(left, right);
            default:
                api->msrest_panic("[" LIB_NAME "] eval_ast(): unknown operator '%c'", op[0]);
        }
    }
    
    api->msrest_panic("[" LIB_NAME "] eval_ast(): unknown node type '%s'", type);
}

EXPORT Value eval_ast(MsrestAPI* api, Value* args, int argc) {
    api->check_argc(argc, 1);
    api->lib_check_type(MSREST_DICT, args[0].type, LIB_NAME, "eval_ast");
    
    double result = eval_ast_node(api, args[0]);
    return api->float_to_value(result);
}