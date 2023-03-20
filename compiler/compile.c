/* Copyright (C) 2020 <KichikuouChrome@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
*/
#include "sys3c.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static Compiler *compiler;
static const char *menu_item_start;
static bool compiling;
static Vector *branch_end_stack;

typedef enum {
	VARIABLE,
	CONST,
} SymbolType;

typedef struct {
	SymbolType type;
	int value;  // variable index or constant value
} Symbol;

static Symbol *new_symbol(SymbolType type, int value) {
	Symbol *s = calloc(1, sizeof(Symbol));
	s->type = type;
	s->value = value;
	return s;
}

static Map *labels;

static Buffer *out;

static int lookup_var(char *var, bool create) {
	Symbol *sym = hash_get(compiler->symbols, var);
	if (sym) {
		switch (sym->type) {
		case VARIABLE:
			return sym->value;
		case CONST:
			if (create)
				error_at(input - strlen(var), "'%s' is already defined as a constant", var);
			return -1;
		}
	}

	if (!create)
		return -1;
	sym = new_symbol(VARIABLE, compiler->variables->len);
	vec_push(compiler->variables, var);
	hash_put(compiler->symbols, var, sym);
	return sym->value;
}

static void expr(void);
static void expr_equal(void);
static void commands(void);

static void variable(char *id, bool create) {
	int var = lookup_var(id, create);
	if (compiling && var < 0)
		error_at(input - strlen(id), "Undefined variable '%s'", id);
	emit_var(out, var);
}

static void number(void) {
	emit_number(out, get_number());
}

// prim ::= '(' equal ')' | number | '#' filename | const | var
static void expr_prim(void) {
	if (consume('(')) {
		expr_equal();
		expect(')');
	} else if (isdigit(next_char())) {
		number();
	} else if (consume('#')) {
		const char *top = input;
		char *fname = get_filename();
		for (int i = 0; i < compiler->src_paths->len; i++) {
			if (!strcasecmp(fname, basename_utf8(compiler->src_paths->data[i]))) {
				emit_number(out, i);
				return;
			}
		}
		error_at(top, "reference to unknown source file: '%s'", fname);
	} else {
		char *id = get_identifier();
		if (!strcmp(id, "__LINE__")) {
			emit_number(out, input_line);
		} else {
			Symbol *sym = hash_get(compiler->symbols, id);
			if (sym && sym->type == CONST)
				emit_number(out, sym->value);
			else
				variable(id, false);
		}
	}
}

// mul ::= prim ('*' prim | '/' prim | '%' prim)*
static void expr_mul(void) {
	expr_prim();
	for (;;) {
		if (consume('*')) {
			expr_prim();
			emit(out, OP_MUL);
		} else if (consume('/')) {
			expr_prim();
			emit(out, OP_DIV);
		} else {
			break;
		}
	}
}

// add ::= mul ('+' mul | '-' mul)*
static void expr_add(void) {
	expr_mul();
	for (;;) {
		if (consume('+')) {
			expr_mul();
			emit(out, OP_ADD);
		} else if (consume('-')) {
			expr_mul();
			emit(out, OP_SUB);
		} else {
			break;
		}
	}
}

// compare ::= add ('<' add | '>' add | '<=' add | '>=' add)*
static void expr_compare(void) {
	expr_add();
	for (;;) {
		int op = 0;
		if (consume('<')) {
			op = OP_LT;
		} else if (consume('>')) {
			op = OP_GT;
		}
		if (!op)
			break;
		expr_add();
		emit(out, op);
	}
}

// equal ::= compare ('=' compare | '\' compare | '$' compare)*
static void expr_equal(void) {
	expr_compare();
	for (;;) {
		if (consume('=')) {
			expr_compare();
			emit(out, OP_EQ);
		} else if (consume('\\')) {
			expr_compare();
			emit(out, OP_NE);
		} else if (consume('$')) {
			expr_compare();
		} else {
			break;
		}
	}
}

// expr ::= equal
static void expr(void) {
	expr_equal();
	emit(out, OP_END);
}

// 'const' 'word' identifier '=' constexpr (',' identifier '=' constexpr)* ':'
static void define_const(void) {
	if (!consume_keyword("word"))
		error_at(input, "unknown const type");
	do {
		const char *top = input;
		char *id = get_identifier();
		consume('=');
		int val = get_number();  // TODO: Allow expressions
		if (!compiling) {
			Symbol *sym = hash_get(compiler->symbols, id);
			if (sym) {
				switch (sym->type) {
				case VARIABLE:
					error_at(top, "'%s' is already defined as a variable", id);
				case CONST:
					error_at(top, "constant '%s' redefined", id);
				}
			}
			hash_put(compiler->symbols, id, new_symbol(CONST, val));
		}
	} while (consume(','));
	expect(':');
}

static Label *lookup_label(char *id) {
	Label *l = map_get(labels, id);
	if (!l) {
		l = calloc(1, sizeof(Label));
		l->source_loc = input - strlen(id);
		map_put(labels, id, l);
	}
	return l;
}

static void add_label(void) {
	char *id = get_label();
	if (!compiling)
		return;
	Label *l = lookup_label(id);
	if (l->addr)
		error_at(input - strlen(id), "label '%s' redefined", id);
	l->addr = current_address(out);

	while (l->hole_addr)
		l->hole_addr = swap_word(out, l->hole_addr, l->addr);
}

static Label *label(void) {
	char *id = get_label();
	if (!compiling)
		return NULL;
	Label *l = lookup_label(id);
	if (!l->addr) {
		emit_word(out, l->hole_addr);
		l->hole_addr = current_address(out) - 2;
	} else {
		assert(!l->hole_addr);
		emit_word(out, l->addr);
	}
	return l;
}

static void verb_obj(void) {
	int loc = current_address(out);
	emit_word(out, 0);  // dummy
	label();
	expect(',');
	set_byte(out, loc, get_number());  // verb
	expect(',');
	set_byte(out, loc + 1, get_number());  // obj
	expect(':');
}

// Compile command arguments. Directives:
//  e: expression
//  n: number (ascii digits)
//  s: string (colon-terminated)
//  v: variable
//  z: string (zero-terminated)
static void arguments(const char *sig) {
	if (*sig == 'n') {
		emit(out, get_number());
		if (*++sig)
			consume(',');  // comma between subcommand num and next argument is optional
	}

	while (*sig) {
		switch (*sig) {
		case 'e':
			expr();
			break;
		case 'n':
			emit(out, get_number());
			break;
		case 's':
		case 'z':
			while (isspace(*input))
				input++;  // Do not consume full-width spaces here
			if (*input == '"') {
				expect('"');
				compile_string(out, '"', false, false);
			} else {
				compile_bare_string(out);
			}
			emit(out, *sig == 'z' ? 0 : ':');
			break;
		case 'o': // obfuscated string
			{
				emit(out, 0);
				expect('"');
				int start = current_address(out);
				compile_string(out, '"', false, false);
				for (int i = start; i < current_address(out); i++) {
					uint8_t b = get_byte(out, i);
					set_byte(out, i, b >> 4 | b << 4);
				}
				emit(out, 0);
			}
			break;
		case 'v':
			variable(get_identifier(), false);
			emit(out, OP_END);
			break;
		default:
			error("BUG: invalid arguments() template : %c", *sig);
		}
		if (*++sig) {
			if (consume(':'))
				error_at(input - 1, "too few arguments");
			expect(',');
		}
	}
	if (consume(','))
		error_at(input - 1, "too many arguments");
	expect(':');
}

// assign ::= '!' var [+-*/%&|^]? ':' expr '!'
static void assign(void) {
	int op = current_address(out);
	emit(out, '!');
	variable(get_identifier(), true);
	if (consume('+')) {
		set_byte(out, op, 0x10);
	} else if (consume('-')) {
		set_byte(out, op, 0x11);
	} else if (consume('*')) {
		set_byte(out, op, 0x12);
	} else if (consume('/')) {
		set_byte(out, op, 0x13);
	} else if (consume('%')) {
		set_byte(out, op, 0x14);
	} else if (consume('&')) {
		set_byte(out, op, 0x15);
	} else if (consume('|')) {
		set_byte(out, op, 0x16);
	} else if (consume('^')) {
		set_byte(out, op, 0x17);
	}
	expect(':');
	expr();
	expect('!');
}

// conditional ::= '{' expr ':' commands '}'
static void conditional(void) {
	emit(out, '{');
	expr();
	expect(':');
	int hole = current_address(out);
	emit_word(out, 0);

	if (branch_end_stack) {
		stack_push(branch_end_stack, hole);
		return;
	}

	commands();
	expect('}');
	swap_word(out, hole, current_address(out));
}

// while-loop ::= '<@' expr ':' commands '>'
static void while_loop(void) {
	int loop_addr = current_address(out);
	emit(out, '{');
	expr();
	expect(':');
	int end_hole = current_address(out);
	emit_word(out, 0);

	commands();

	expect('>');
	emit(out, '>');
	emit_word(out, loop_addr);

	swap_word(out, end_hole, current_address(out));
}

// for-loop ::= '<' var ',' expr ',' expr ',' expr ',' expr ':' commands '>'
static void for_loop(void) {
	emit(out, '!');
	int var_begin = current_address(out);
	variable(get_identifier(), true);  // for-loop can define a variable.
	int var_end = current_address(out);
	expect(',');

	expr();  // start
	expect(',');

	emit(out, '<');
	emit(out, 0x00);
	int loop_addr = current_address(out);
	emit(out, '<');
	emit(out, 0x01);

	int end_hole = current_address(out);
	emit_word(out, 0);

	// Copy the opcode for the variable.
	for (int i = var_begin; i < var_end; i++)
		emit(out, get_byte(out, i));
	emit(out, OP_END);

	expr();  // end
	expect(',');
	expr();  // sign
	expect(',');
	expr();  // step
	expect(':');

	commands();

	expect('>');
	emit(out, '>');
	emit_word(out, loop_addr);

	swap_word(out, end_hole, current_address(out));
}

static void pragma(void) {
	if (consume_keyword("ald_volume")) {
		compiler->scos[input_page].ald_volume = get_number();
		expect(':');
	} else if (consume_keyword("address")) {
		int address = get_number();
		if (out) {
			if (out->len > address)
				out->len = address;
			while (out->len < address)
				emit(out, 0);
			// Addresses in LINE debug table must be monotonically increasing,
			// but address pragma breaks this condition. So clear the LINE info
			// for this page.
			if (compiler->dbg_info)
				debug_line_reset(compiler->dbg_info);
		}
		expect(':');
	} else {
		error_at(input, "unknown pragma");
	}
}

static bool command(void) {
	skip_whitespaces();
	if (out && compiler->dbg_info)
		debug_line_add(compiler->dbg_info, input_line, current_address(out));

	const char *command_top = input;
	int cmd = get_command(out);

	switch (cmd) {
	case '\0':
		return false;

	case '\x1a': // DOS EOF
		break;

	case '\'': // Message
		compile_string(out, '\'', config.sys_ver == SYSTEM35, true);
		break;

	case '!':  // Assign
		assign();
		break;

	case '{':  // Branch
		conditional();
		break;

	case '}':
		if (branch_end_stack && branch_end_stack->len > 0) {
			expect('}');
			swap_word(out, stack_top(branch_end_stack), current_address(out));
			stack_pop(branch_end_stack);
		} else {
			return false;
		}
		break;

	case '*':  // Label
		add_label();
		expect(':');
		break;

	case '@':  // Label jump
		emit(out, cmd);
		label();
		expect(':');
		break;

	case '\\': // Label call
		emit(out, cmd);
		if (consume('0'))
			emit_word(out, 0);  // Return
		else {
			Label *l = label();
			if (l)
				l->is_function = true;
		}
		expect(':');
		break;

	case '&':  // Page jump
		emit(out, cmd);
		expr();
		expect(':');
		break;

	case '%':  // Page call / return
		emit(out, cmd);
		expr();
		expect(':');
		break;

	case '<':  // Loop
		if (consume('@'))
			while_loop();
		else
			for_loop();
		break;

	case '>':
		return false;

	case ']':  // Menu
		emit(out, cmd);
		break;

	case '$':  // Menu item
		emit(out, cmd);
		if (menu_item_start) {
			menu_item_start = NULL;
			break;
		}
		label();
		expect('$');
		if (!isascii(*input)) {
			compile_string(out, '$', config.sys_ver == SYSTEM35, true);
			emit(out, '$');
		} else {
			menu_item_start = command_top;
		}
		break;

	case '_':  // Label address as data
		label();
		expect(':');
		break;

	case '"':  // String data
		compile_string(out, '"', config.sys_ver == SYSTEM35, false);
		emit(out, 0);
		break;

	case '[':  // Verb-obj
		emit(out, cmd);
		verb_obj();
		break;

	case ':':  // Conditional verb-obj
		emit(out, cmd);
		expr();
		expect(',');
		verb_obj();
		break;

	case 'A': break;
	case 'B': arguments("neeeeee"); break;
	case 'E': arguments("eeeeee"); break;
	case 'F': break;
	case 'G': arguments("e"); break;
	case 'H': arguments("ne"); break;
	case 'I': arguments("eeeeee"); break;
	case 'J': arguments("ee"); break;
	case 'K': arguments("n"); break;
	case 'L': arguments("e"); break;
	case 'M': arguments("s"); break;
	case 'N': arguments("nee"); break;
	case 'O': arguments("ee"); break;
	case 'P': arguments("eeee"); break;
	case 'Q': arguments("e"); break;
	case 'R': break;
	case 'S': arguments("n"); break;
	case 'T': arguments("ee"); break;
	case 'U': arguments("ee"); break;
	case 'V': arguments("ee"); break;
	case 'W': arguments("eee"); break;
	case 'X': arguments("n"); break;
	case 'Y': arguments("ee"); break;
	case 'Z': arguments("ee"); break;

	case COMMAND_IF:
		expect('{');
		conditional();
		break;

	case COMMAND_CONST:
		define_const();
		break;

	case COMMAND_PRAGMA:
		pragma();
		break;

	default:
		goto unknown_command;
	}
	return true;
 unknown_command:
		error_at(command_top, "Unknown command %.*s", input - command_top, command_top);
}

// commands ::= command*
static void commands(void) {
	while (command())
		;
}

// toplevel ::= commands
static void toplevel(void) {
	if (config.unicode && input_page == 0) {
		// Inject "ZU 1:" command.
		skip_whitespaces();
		if (out && compiler->dbg_info)
			debug_line_add(compiler->dbg_info, input_line, current_address(out));
		emit(out, 'Z');
		emit(out, 'U');
		emit(out, 0x41);
		emit(out, 0x7f);
	}

	commands();
	if (*input)
		error_at(input, "unexpected '%c'", *input);
}

Compiler *new_compiler(Vector *src_paths, Vector *variables) {
	Compiler *comp = calloc(1, sizeof(Compiler));
	comp->src_paths = src_paths;
	comp->variables = variables ? variables : new_vec();
	comp->symbols = new_string_hash();
	comp->scos = calloc(src_paths->len, sizeof(Sco));

	for (int i = 0; i < comp->variables->len; i++)
		hash_put(comp->symbols, comp->variables->data[i], new_symbol(VARIABLE, i));

	return comp;
}

static void prepare(Compiler *comp, const char *source, int pageno) {
	compiler = comp;
	lexer_init(source, comp->src_paths->data[pageno], pageno);
	menu_item_start = NULL;
	branch_end_stack = (config.sys_ver == SYSTEM35) ? new_vec() : NULL;
}

static void check_undefined_labels(void) {
	for (int i = 0; i < labels->vals->len; i++) {
		Label *l = labels->vals->data[i];
		if (l->hole_addr)
			error_at(l->source_loc, "undefined label '%s'", labels->keys->data[i]);
	}
}

void preprocess(Compiler *comp, const char *source, int pageno) {
	prepare(comp, source, pageno);
	compiling = false;
	labels = NULL;

	toplevel();

	if (menu_item_start)
		error_at(menu_item_start, "unfinished menu item");
	if (branch_end_stack && branch_end_stack->len > 0)
		error_at(input, "'}' expected");
}

void preprocess_done(Compiler *comp) {
	if (config.sys_ver == SYSTEM39)
		comp->msg_buf = new_buf();
	comp->msg_count = 0;
}

Sco *compile(Compiler *comp, const char *source, int pageno) {
	prepare(comp, source, pageno);
	compiling = true;
	labels = new_map();

	comp->scos[pageno].ald_volume = 1;
	out = new_buf();
	sco_init(out, basename_utf8(comp->src_paths->data[pageno]), pageno);
	if (comp->dbg_info)
		debug_init_page(comp->dbg_info, pageno);

	toplevel();

	if (menu_item_start)
		error_at(menu_item_start, "unfinished menu item");
	check_undefined_labels();

	sco_finalize(out);
	if (comp->dbg_info)
		debug_finish_page(comp->dbg_info, labels);
	comp->scos[pageno].buf = out;
	out = NULL;
	return &comp->scos[pageno];
}
