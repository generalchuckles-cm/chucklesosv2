#include "basic.h"
#include "hdd_fs.h" // For SAVE/LOAD
#include <stddef.h> // For NULL
#include "stdio.h"
#include "extrainclude.h"


// --- External OS Functions ---
// We need these functions from your kernel/shell
extern void print_string(const char* str);
extern void print_char(char c);
extern void print_int(int n);
extern void new_line();
extern void clear_screen();
extern int strcmp(const char *a, const char *b);
extern char* strncpy(char *dest, const char *src, size_t n);
extern size_t strlen(const char* str);
extern void* memset(void *s, int c, size_t n);

// This is a new function we need to add to kernel.c
// It should handle keyboard input and return a null-terminated string.
extern void get_user_input(char* buffer, int max_len);

// --- Configuration ---
#define MAX_LINE_LENGTH      128
#define MAX_PROGRAM_LINES    256
#define MAX_VARIABLES        26   // 'A' through 'Z'
#define MAX_FOR_LOOP_STACK   8
#define MAX_GOSUB_STACK      16
#define MAX_PROGRAM_FILE_SIZE (MAX_PROGRAM_LINES * MAX_LINE_LENGTH)

// --- Data Structures ---

// Stores a single line of the BASIC program
typedef struct {
    int line_number;
    char line_text[MAX_LINE_LENGTH];
} ProgramLine;

// Stores a variable's value (integers only for now)
typedef struct {
    int value;
    int is_set;
} Variable;

// For the FOR...NEXT loop stack
typedef struct {
    int variable_index;      // Which variable (0-25 for A-Z)
    int to_value;
    int step_value;
    int source_line_index;   // Where to loop back to
} ForLoop;

// --- Global State ---
static ProgramLine program[MAX_PROGRAM_LINES];
static int program_line_count = 0;

static Variable variables[MAX_VARIABLES];

static ForLoop for_loop_stack[MAX_FOR_LOOP_STACK];
static int for_loop_sp = 0; // Stack pointer

static int gosub_stack[MAX_GOSUB_STACK];
static int gosub_sp = 0; // Stack pointer

// Buffer for SAVE/LOAD operations
static char file_io_buffer[MAX_PROGRAM_FILE_SIZE + 1];

// --- Forward Declarations for the Parser ---
static int execute_line(const char* line, int* current_program_index);
static int parse_expression();

// --- Tokenizer ---
static const char* current_pos; // Current position in the line being parsed

// Skip whitespace
static void skip_whitespace() {
    while (*current_pos == ' ' || *current_pos == '\t') {
        current_pos++;
    }
}

// Check if current token matches and advance
static int match(const char* keyword) {
    skip_whitespace();
    int len = strlen(keyword);
    if (strncmp(current_pos, keyword, len) == 0) {
        // Check for word boundary
        char next_char = current_pos[len];
        if (next_char == ' ' || next_char == '\t' || next_char == '\0' || next_char == '\n' || next_char == '"' || (next_char >= '0' && next_char <= '9') || next_char == '(' || next_char == '-') {
             current_pos += len;
             return 1;
        }
    }
    return 0;
}


// --- Variable Management ---
static void clear_variables() {
    memset(variables, 0, sizeof(variables));
}


// --- Program Management ---
static void clear_program() {
    memset(program, 0, sizeof(program));
    program_line_count = 0;
    print_string("OK\n");
}

static void store_line(int num, const char* text) {
    int i, insert_pos;
    for (insert_pos = 0; insert_pos < program_line_count; insert_pos++) {
        if (program[insert_pos].line_number > num) {
            break;
        }
        if (program[insert_pos].line_number == num) {
            // Overwriting existing line
            strncpy(program[insert_pos].line_text, text, MAX_LINE_LENGTH - 1);
            return;
        }
    }

    if (program_line_count >= MAX_PROGRAM_LINES) {
        print_string("?PROGRAM FULL  ERROR\n");
        return;
    }

    // Shift lines to make space
    for (i = program_line_count; i > insert_pos; i--) {
        program[i] = program[i - 1];
    }
    
    program[insert_pos].line_number = num;
    strncpy(program[insert_pos].line_text, text, MAX_LINE_LENGTH - 1);
    program_line_count++;
}

static int find_line_index(int line_number) {
    for (int i = 0; i < program_line_count; i++) {
        if (program[i].line_number == line_number) {
            return i;
        }
    }
    return -1; // Not found
}

// --- Expression Parser (Recursive Descent) ---

static int parse_factor() {
    skip_whitespace();
    int val = 0;
    int sign = 1;

    if (*current_pos == '-') {
        sign = -1;
        current_pos++;
    }

    if (*current_pos >= '0' && *current_pos <= '9') {
        while (*current_pos >= '0' && *current_pos <= '9') {
            val = val * 10 + (*current_pos - '0');
            current_pos++;
        }
    } else if (*current_pos >= 'A' && *current_pos <= 'Z') {
        val = variables[*current_pos - 'A'].value;
        current_pos++;
    } else if (*current_pos == '(') {
        current_pos++;
        val = parse_expression();
        skip_whitespace();
        if (*current_pos == ')') {
            current_pos++;
        } else {
            print_string("?SYNTAX ERROR\n");
        }
    }
    return val * sign;
}

static int parse_term() {
    int left = parse_factor();
    while (1) {
        skip_whitespace();
        if (*current_pos == '*') {
            current_pos++;
            left *= parse_factor();
        } else if (*current_pos == '/') {
            current_pos++;
            int right = parse_factor();
            if (right == 0) {
                print_string("?DIVISION BY ZERO ERROR\n");
                return 0; // Error case
            }
            left /= right;
        } else {
            break;
        }
    }
    return left;
}

static int parse_expression() {
    int left = parse_term();
    while (1) {
        skip_whitespace();
        if (*current_pos == '+') {
            current_pos++;
            left += parse_term();
        } else if (*current_pos == '-') {
            current_pos++;
            left -= parse_term();
        } else {
            break;
        }
    }
    return left;
}


// --- Statement Handlers ---
static int handle_print() {
    skip_whitespace();
    while(*current_pos != '\0') {
        if (*current_pos == '"') {
            current_pos++;
            while (*current_pos != '"' && *current_pos != '\0') {
                print_char(*current_pos++);
            }
            if (*current_pos == '"') current_pos++;
        } else {
            print_int(parse_expression());
        }
        skip_whitespace();
    }
    new_line();
    return 0;
}

static int handle_let() {
    skip_whitespace();
    if (*current_pos < 'A' || *current_pos > 'Z') return -1; // Syntax Error
    int var_index = *current_pos - 'A';
    current_pos++;
    
    skip_whitespace();
    if (*current_pos != '=') return -1; // Syntax Error
    current_pos++;

    variables[var_index].value = parse_expression();
    variables[var_index].is_set = 1;
    return 0;
}

static int handle_input() {
    skip_whitespace();
    if (*current_pos < 'A' || *current_pos > 'Z') return -1; // Syntax Error
    int var_index = *current_pos - 'A';
    current_pos++;

    print_char('?');
    print_char(' ');
    
    char input_buf[MAX_LINE_LENGTH];
    get_user_input(input_buf, MAX_LINE_LENGTH);

    const char* p = input_buf;
    int val = 0, sign = 1;
    if (*p == '-') { sign = -1; p++; }
    while(*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    variables[var_index].value = val * sign;
    variables[var_index].is_set = 1;
    return 0;
}

static int handle_goto(int* current_program_index) {
    int target_line = parse_expression();
    int new_index = find_line_index(target_line);
    if (new_index == -1) {
        print_string("?UNDEF'D STATEMENT ERROR\n");
        return -1;
    }
    *current_program_index = new_index - 1; // -1 to account for loop increment
    return 0;
}

static int handle_if(int* current_program_index) {
    int left = parse_expression();
    skip_whitespace();
    
    int op = 0; // 1:=, 2:<, 3:>, 4:>=, 5:<=, 6:<>
    if (match("=")) op = 1;
    else if (match("<>")) op = 6;
    else if (match("<=")) op = 5;
    else if (match(">=")) op = 4;
    else if (match("<")) op = 2;
    else if (match(">")) op = 3;
    else return -1; // Syntax error

    int right = parse_expression();
    
    int condition_met = 0;
    switch(op) {
        case 1: if (left == right) condition_met = 1; break;
        case 2: if (left < right) condition_met = 1; break;
        case 3: if (left > right) condition_met = 1; break;
        case 4: if (left >= right) condition_met = 1; break;
        case 5: if (left <= right) condition_met = 1; break;
        case 6: if (left != right) condition_met = 1; break;
    }

    if (condition_met) {
        if (match("THEN")) {
            return execute_line(current_pos, current_program_index);
        } else return -1;
    }
    return 0;
}

static int handle_for(int current_line_index) {
    if (for_loop_sp >= MAX_FOR_LOOP_STACK) { print_string("?OUT OF MEMORY ERROR\n"); return -1; }
    
    skip_whitespace();
    if (*current_pos < 'A' || *current_pos > 'Z') return -1;
    int var_idx = *current_pos - 'A';
    current_pos++;

    if (!match("=")) return -1;
    int start_val = parse_expression();

    if (!match("TO")) return -1;
    int to_val = parse_expression();

    int step_val = 1;
    if (match("STEP")) {
        step_val = parse_expression();
    }

    variables[var_idx].value = start_val;
    variables[var_idx].is_set = 1;

    for_loop_stack[for_loop_sp].variable_index = var_idx;
    for_loop_stack[for_loop_sp].to_value = to_val;
    for_loop_stack[for_loop_sp].step_value = step_val;
    for_loop_stack[for_loop_sp].source_line_index = current_line_index;
    for_loop_sp++;
    
    return 0;
}

static int handle_next(int* current_program_index) {
    if (for_loop_sp == 0) { print_string("?NEXT WITHOUT FOR ERROR\n"); return -1; }

    ForLoop* loop = &for_loop_stack[for_loop_sp - 1];
    variables[loop->variable_index].value += loop->step_value;

    int finished = 0;
    if (loop->step_value > 0) {
        if (variables[loop->variable_index].value > loop->to_value) finished = 1;
    } else {
        if (variables[loop->variable_index].value < loop->to_value) finished = 1;
    }

    if (finished) {
        for_loop_sp--; // Pop from stack
    } else {
        *current_program_index = loop->source_line_index - 1; // Loop back
    }
    return 0;
}

static int handle_gosub(int current_line_index) {
    if (gosub_sp >= MAX_GOSUB_STACK) { print_string("?OUT OF MEMORY ERROR\n"); return -1; }
    gosub_stack[gosub_sp++] = current_line_index;
    
    // Essentially a GOTO, so we reuse that logic
    int dummy_idx;
    return handle_goto(&dummy_idx);
}

static int handle_return(int* current_program_index) {
    if (gosub_sp == 0) { print_string("?RETURN WITHOUT GOSUB ERROR\n"); return -1; }
    *current_program_index = gosub_stack[--gosub_sp];
    return 0;
}

// --- Top-Level Commands ---

static void handle_list() {
    for (int i = 0; i < program_line_count; i++) {
        print_int(program[i].line_number);
        print_char(' ');
        print_string(program[i].line_text);
        new_line();
    }
    print_string("OK\n");
}

static void handle_run() {
    clear_variables();
    for_loop_sp = 0;
    gosub_sp = 0;

    print_string("RUNNING...\n");
    for (int i = 0; i < program_line_count; i++) {
        if (execute_line(program[i].line_text, &i) != 0) {
            print_string("?ERROR IN LINE ");
            print_int(program[i].line_number);
            new_line();
            break; // Stop execution on error
        }
    }
    print_string("\nOK\n");
}

static void handle_save() {
    skip_whitespace();
    char* filename = (char*)current_pos;
    if (strlen(filename) == 0) {
        print_string("?MISSING FILENAME ERROR\n");
        return;
    }

    memset(file_io_buffer, 0, MAX_PROGRAM_FILE_SIZE + 1);
    char* writer = file_io_buffer;
    for(int i = 0; i < program_line_count; i++) {
        char line_buf[20];
        const char* p = line_buf;
        print_int(program[i].line_number); // This prints to screen, need an itoa
        
        // Simple itoa
        int n = program[i].line_number;
        int j = 0;
        if (n == 0) { line_buf[j++] = '0'; } else {
            char temp[10]; int k=0;
            while(n>0) { temp[k++] = (n%10)+'0'; n/=10; }
            while(k>0) line_buf[j++] = temp[--k];
        }
        line_buf[j] = '\0';
        // End of itoa

        strcpy(writer, p);
        writer += strlen(p);
        *writer++ = ' ';
        strcpy(writer, program[i].line_text);
        writer += strlen(program[i].line_text);
        *writer++ = '\n';
    }

    if (fs_write_file(filename, file_io_buffer, strlen(file_io_buffer)) == 0) {
        print_string("SAVED ");
        print_string(filename);
        new_line();
    } else {
        print_string("?SAVE ERROR\n");
    }
    print_string("OK\n");
}

static void handle_load() {
    skip_whitespace();
    char* filename = (char*)current_pos;
    if (strlen(filename) == 0) {
        print_string("?MISSING FILENAME ERROR\n");
        return;
    }
    
    int bytes_read = fs_read_file(filename, file_io_buffer);
    if (bytes_read > 0) {
        clear_program();
        print_string("LOADING...\n");

        char* line_start = file_io_buffer;
        while(line_start < file_io_buffer + bytes_read) {
            char* line_end = line_start;
            while(*line_end != '\n' && *line_end != '\0') {
                line_end++;
            }
            if (*line_end == '\n') {
                *line_end = '\0';
            }
            
            // Parse and store line from file
            int num = 0;
            const char* p = line_start;
            while(*p >= '0' && *p <= '9') { num = num * 10 + (*p - '0'); p++; }
            while(*p == ' ') p++;
            store_line(num, p);

            line_start = line_end + 1;
        }

    } else {
        print_string("?LOAD ERROR\n");
    }
    print_string("OK\n");
}


// --- Main Execution Logic ---
int execute_line(const char* line, int* current_program_index) {
    current_pos = line;
    skip_whitespace();
    if (match("PRINT")) return handle_print();
    if (match("LET")) return handle_let();
    if (match("INPUT")) return handle_input();
    if (match("GOTO")) return handle_goto(current_program_index);
    if (match("GOSUB")) return handle_gosub(*current_program_index);
    if (match("RETURN")) return handle_return(current_program_index);
    if (match("IF")) return handle_if(current_program_index);
    if (match("FOR")) return handle_for(*current_program_index);
    if (match("NEXT")) return handle_next(current_program_index);
    if (match("REM")) return 0; // Comment, do nothing
    if (match("END")) return 1; // Special code to stop program

    // Check for implied LET
    if (*current_pos >= 'A' && *current_pos <= 'Z') {
        const char* temp_pos = current_pos + 1;
        while(*temp_pos == ' ' || *temp_pos == '\t') temp_pos++;
        if (*temp_pos == '=') return handle_let();
    }

    print_string("?SYNTAX ERROR\n");
    return -1;
}

// --- Entry Point ---
void basic_start() {
    char input_buf[MAX_LINE_LENGTH];

    clear_screen();
    print_string("CHUCKLES BASIC 1.0\n");
    print_string("READY.\n");
    
    while(1) {
        print_char('>');
        get_user_input(input_buf, MAX_LINE_LENGTH);

        const char* p = input_buf;
        skip_whitespace();
        
        // Check for line number to store in program
        if (*p >= '0' && *p <= '9') {
            int line_num = 0;
            while(*p >= '0' && *p <= '9') {
                line_num = line_num * 10 + (*p - '0');
                p++;
            }
            skip_whitespace();
            store_line(line_num, p);
            continue;
        }

        current_pos = input_buf; // For immediate mode commands
        if (match("LIST")) handle_list();
        else if (match("RUN")) handle_run();
        else if (match("NEW")) clear_program();
        else if (match("SAVE")) handle_save();
        else if (match("LOAD")) handle_load();
        else if (match("EXIT")) {
            clear_screen(); // Clean up before returning to shell
            return;
        } else {
            // Immediate mode execution
            int dummy_idx = -1;
            execute_line(input_buf, &dummy_idx);
            print_string("OK\n");
        }
        
        print_string("READY.\n");
    }
}