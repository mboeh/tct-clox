#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#define PANIC(msg) { fputs("PANIC: " msg, stderr); exit(2); }

typedef enum {
    OP_PUSH,
    OP_POP,
    OP_DUP,
    OP_SWAP,
    OP_ASSERT,
    OP_ADD,
    OP_SUBTRACT,
    OP_DIVIDE,
    OP_MULTIPLY,
    OP_EQUAL,
    OP_LAST
} opcode;

const char* keywords[] = {
    NULL,
    "pop",
    "dup",
    "swap",
    "assert",
    "+",
    "-",
    "/",
    "*",
    "=",
};

typedef struct {
    double* values;
    size_t size;
    size_t capacity;
} values;

bool values_alloc(values* a) {
    double* newv = realloc(a->values, sizeof(double)*a->capacity);
    if (!newv) {
        return false;
    } else {
        a->values = newv;
        return true;
    }
}

bool values_init(values* a) {
    a->values = NULL;
    a->size = 0;
    a->capacity = 8;
    return values_alloc(a);
}

void values_reset(values* a) {
    a->size = 0;
}

bool values_grow(values* a) {
    a->capacity *= 2;
    return values_alloc(a);
}

bool values_push(values* a, double v, size_t* slot_out) {
    if (a->size == (a->capacity-1)) {
        if (!values_grow(a)) {
            return false;
        }
    }
    a->values[a->size] = v;
    if (slot_out != NULL) {
        *slot_out = a->size;
    }
    a->size++;

    return true;
}

typedef uint64_t op_t;

typedef struct {
    op_t* ops;
    size_t size;
    size_t capacity;
    
    values values;
} op_chunk;

bool op_chunk_alloc(op_chunk* a) {
    op_t* newo = realloc(a->ops, sizeof(op_t)*a->capacity);
    if (!newo) {
        return false;
    } else {
        a->ops = newo;
        return true;
    }
}

bool op_chunk_init(op_chunk* a) {
    a->ops = NULL;
    a->size = 0;
    a->capacity = 32;
    if (!values_init(&a->values)) {
        return false;
    }
    return op_chunk_alloc(a);
}

void op_chunk_reset(op_chunk* a) {
    a->size = 0;
    values_reset(&a->values);
}

bool op_chunk_grow(op_chunk* a) {
    a->capacity *= 2;
    return op_chunk_alloc(a);
}

bool ops_push(op_chunk* a, op_t op) {
    if (a->size == (a->capacity-1)) {
        if (!op_chunk_grow(a)) {
            return false;
        }
    }
    a->ops[a->size] = op;
    a->size++;

    return true;
}

bool compile(op_chunk* chunk, char* src) {
    enum {
        C_SPACE,
        C_NUMBER,
        C_WORD
    } state = C_SPACE;
    char* tok_st = src;
    size_t tok_l = 0;

    // dumbest switch-based compiler I could make
    while (*src) {
        switch (state) {
        case C_SPACE:
            if (isdigit(*src)) {
                tok_st = src;
                tok_l = 0;
                state = C_NUMBER;
            } else if (isalpha(*src) || ispunct(*src)) {
                tok_st = src;
                tok_l = 0;
                state = C_WORD;
            } else if (isspace(*src)) {
                src++;
            } else {
                // compile error
                return false;
            }
            break;
        case C_NUMBER:
            if (isdigit(*src) || *src == '.') {
                tok_l++;
                src++;
            } else if (isspace(*src)) {
                // I said dumbest
                double v = atof(tok_st);
                size_t slot;
                if (!(
                  values_push(&chunk->values, v, &slot) &&
                  ops_push(chunk, OP_PUSH) &&
                  ops_push(chunk, (op_t) slot)
                  )) {
                    return false;
                }
                state = C_SPACE;
            } else {
                // compile error
                return false;
            }
            break;
        case C_WORD:
            if (isalpha(*src) || ispunct(*src)) {
                tok_l++;
                src++;
            } else if (isspace(*src)) {
                size_t op = 0;
                for(; op < OP_LAST; op++) {
                    if (keywords[op] == NULL) {
                        // no keyword for this op
                    } else if (strncmp(keywords[op], tok_st, tok_l) == 0) {
                        break;
                    }
                }
                if (op == OP_LAST) {
                    return false;
                }
                ops_push(chunk, op);
                state = C_SPACE;
            } else {
                // compile error
                return false;
            }
            break;
        }
    }
    return true;
}

void dump_chunk(op_chunk* chunk, FILE* f) {
    fprintf(f, "OPS: ");
    for (size_t i = 0; i < chunk->size; i++) {
        fprintf(f, "%llu ", chunk->ops[i]);
    }
    fprintf(f, "\n");

    fprintf(f, "VALUES: ");
    for (size_t i = 0; i < chunk->values.size; i++) {
        fprintf(f, "%f ", chunk->values.values[i]);
    }
    fprintf(f, "\n");
}

// ultra-dumb implementation I'm replacing
// yes this will overflow
// it's 30 minutes to bedtime
typedef struct {
    double stack[64];
    double* top;
} vm;

void dump_stack(vm* vm, FILE* f) {
    fprintf(f, "STACK: ");
    if (vm->stack == vm->top) {
        fprintf(f, "empty");
    } else {
        for (double* v = (vm->stack+1); v <= vm->top; v++) {
            fprintf(f, "%f ", *v);
        }
    }
    fprintf(f, "\n");
}

bool vm_init(vm* vm) {
    vm->top = vm->stack;

    return true;
}

bool vm_execute(vm* vm, op_chunk* chunk) {
    // note that this only works if ops can be executed in order
    // no flow control
    // again: ultra-dumb
    for (size_t i = 0; i < chunk->size; i++) {
        switch (chunk->ops[i]) {
        case OP_PUSH:
            // move on to the slot number
            i++;
            // retrieve the value, push the stack
            vm->top++;
            *vm->top = chunk->values.values[chunk->ops[i]];
            break;
        case OP_POP:
            if (vm->top == vm->stack) {
                PANIC("vm->stack empty on pop");
            }
            vm->top--;
            break;
        case OP_DUP:
            if (vm->top == vm->stack) {
                PANIC("vm->stack empty on dup");
            }
            *(vm->top+1) = *vm->top;
            vm->top++;
            break;
        case OP_SWAP:
            if (vm->top == vm->stack || vm->top == (vm->stack+1)) {
                PANIC("vm->stack too shallow on swap");
            }
            double tmp = *vm->top;
            *vm->top = *(vm->top-1);
            *(vm->top-1) = tmp;
            break;
        case OP_ADD:
            if (vm->top == vm->stack || vm->top == (vm->stack+1)) {
                PANIC("vm->stack too shallow on add");
            }
            *(vm->top-1) += *vm->top;
            vm->top--;
            break;
        case OP_SUBTRACT:
            if (vm->top == vm->stack || vm->top == (vm->stack+1)) {
                PANIC("vm->stack too shallow on subtract");
            }
            *(vm->top-1) -= *vm->top;
            vm->top--;
            break;
        case OP_DIVIDE:
            if (vm->top == vm->stack || vm->top == (vm->stack+1)) {
                PANIC("vm->stack too shallow on divide");
            }
            *(vm->top-1) /= *vm->top;
            vm->top--;
            break;
        case OP_MULTIPLY:
            if (vm->top == vm->stack || vm->top == (vm->stack+1)) {
                PANIC("vm->stack too shallow on multiply");
            }
            *(vm->top-1) *= *vm->top;
            vm->top--;
            break;
        case OP_EQUAL:
            if (vm->top == vm->stack || vm->top == (vm->stack+1)) {
                PANIC("vm->stack too shallow on equal");
            }
            // yes, there is a delta problem here
            if (*(vm->top-1) == *vm->top) {
                *(vm->top-1) = 1.0;
            } else {
                *(vm->top-1) = 0.0;
            }
            vm->top--;
            break;
        // this is a dumb opcode but it is for testing
        case OP_ASSERT:
            if (vm->top == vm->stack) {
                PANIC("vm->stack too shallow on assert");
            }
            // no booleans :(
            if (*vm->top == 0.0) {
                fprintf(stderr, "assertion failed!\n");
                return false;
            }
            // does NOT pop
            break;
        default:
            PANIC("unknown opcode");
        }
    }

    return true;
}

int main() {
    op_chunk chunk;
    vm vm;
    char linebuf[256];

    op_chunk_init(&chunk);
    vm_init(&vm);

    // if lines are longer than 256 characters bad stuff may happen; this is fine
    while (fgets(linebuf, 256, stdin) != NULL) {
        if (!compile(&chunk, linebuf)) {
            fputs("compile error", stderr);
            return 1;
        }
#ifdef DEBUG
        dump_chunk(&chunk, stderr);
#endif
        if (!vm_execute(&vm, &chunk)) {
            return 1;
        }
#ifdef DEBUG
        dump_stack(&vm, stderr);
#endif
        op_chunk_reset(&chunk);
    }

    return 0;
}
