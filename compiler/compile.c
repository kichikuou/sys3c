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

#define DUPLICATE 1000

static Compiler *compiler;
static const char *menu_item_start;

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
	if (var < 0)
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
			const char *path = compiler->src_paths->data[i];
			if (path && !strcasecmp(fname, basename_utf8(path))) {
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
			emit(out, config.sys_ver == SYSTEM1 ? OP_DIV : OP_MUL);
		} else if (consume('/')) {
			if (config.sys_ver == SYSTEM1)
				error_at(input - 1, "division is not supported in System 1");
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
	Label *l = lookup_label(id);
	if (l->addr)
		error_at(input - strlen(id), "label '%s' redefined", id);
	l->addr = current_address(out);

	while (l->hole_addr)
		l->hole_addr = swap_word(out, l->hole_addr, l->addr);
}

static Label *label(void) {
	char *id = get_label();
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

static int verb_or_obj(Vector *list, HashMap *map, const char *type) {
	skip_whitespaces();
	const char *top = input;
	if (*input == '"') {
		char *key = get_string();
		int idx = (intptr_t)hash_get(map, key) - 1;
		if (idx < 0)
			error_at(top, "undefined %s: \"%s\"", type, key);
		if (idx == DUPLICATE)
			error_at(top, "\"%s\" is not unique in %ss.txt. use an index instead", key, type);
		return idx;
	}
	int idx = get_number();
	if (idx >= list->len)
		error_at(top, "invalid %s index %d", type, idx);
	return idx;
}

static void verb_obj(void) {
	int loc = current_address(out);
	emit_word(out, 0);  // dummy
	label();
	expect(',');

	set_byte(out, loc, verb_or_obj(compiler->verb_list, compiler->verb_map, "verb"));

	if (consume(',')) {
		set_byte(out, loc + 1, verb_or_obj(compiler->obj_list, compiler->obj_map, "object"));
	}
	expect(':');
}

// Compile command arguments. Directives:
//  e: expression
//  n: number (ascii digits)
//  s: string (colon-terminated)
//  v: variable
static void arguments(const char *sig) {
	if (!sig)
		error_at(input, "invalid command");

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
			while (isspace(*input))
				input++;  // Do not consume full-width spaces here
			if (*input == '"') {
				expect('"');
				if (config.quoted_strings) {
					emit(out, '\'');
					compile_string(out, '"', STRING_ESCAPE_SQUOTE);
					emit(out, '\'');
				} else {
					compile_string(out, '"', 0);
					emit(out, ':');
				}
			} else {
				compile_bare_string(out);
				emit(out, ':');
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

static void arguments_by_sysver(const char *sig1, const char *sig2, const char *sig3, const char *sig3t) {
	switch (config.sys_ver) {
	case SYSTEM1: arguments(sig1); break;
	case SYSTEM2: arguments(sig2); break;
	case SYSTEM3: arguments(config.game_id < TOUSHIN2 ? sig3 : sig3t); break;
	}
}

// assign ::= '!' var ':' expr '!'
static void assign(void) {
	emit(out, '!');
	variable(get_identifier(), true);
	expect(':');
	expr();
	expect('!');
}

// conditional ::= '{' expr ':' commands '}'
static void conditional(void) {
	emit(out, '{');
	expr();
	expect(':');

	int hole;
	if (config.sys_ver == SYSTEM3) {
		hole = current_address(out);
		emit_word(out, 0);
	}

	commands();

	expect('}');
	if (config.sys_ver == SYSTEM3) {
		swap_word(out, hole, current_address(out));
	} else {
		emit(out, '}');
	}
}

static void pragma(void) {
	if (consume_keyword("dri_volume")) {
		char *volumes = get_identifier();
		compiler->scos[input_page].volume_bits = 0;
		for (char *p = volumes; *p; p++) {
			if (!isalpha(*p))
				error_at(input - strlen(p), "invalid volume letter '%c'", *p);
			compiler->scos[input_page].volume_bits |= 1 << (toupper(*p) - 'A' + 1);
		}
		expect(':');
	} else if (consume_keyword("default_address")) {
		int address = get_number();
		Label *l = lookup_label("default");
		if (l->addr)
			error_at(input, "label 'default' redefined");
		l->addr = address;
		while (l->hole_addr)
			l->hole_addr = swap_word(out, l->hole_addr, l->addr);
		expect(':');
	} else {
		error_at(input, "unknown pragma");
	}
}

static bool command(void) {
	skip_whitespaces();
	if (compiler->dbg_info)
		debug_line_add(compiler->dbg_info, input_line, current_address(out));

	const char *command_top = input;
	int cmd = get_command(out);

	switch (cmd) {
	case '\0':
		return false;

	case '\x1a':  // EOF
		emit(out, cmd);
		break;

	case '\'': // Message
		if (config.quoted_strings) {
			emit(out, cmd);
			compile_string(out, '\'', STRING_ESCAPE_SQUOTE);
			emit(out, cmd);
		} else {
			compile_string(out, '\'', STRING_COMPACT | STRING_FORBID_ASCII);
		}
		break;

	case '"':  // raw string
		compile_string(out, '"', 0);
		break;

	case '!':  // Assign
		assign();
		break;

	case '{':  // Branch
		conditional();
		break;

	case '}':
		return false;

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
		else
			label();
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
			compile_string(out, '$', STRING_COMPACT | STRING_FORBID_ASCII);
			emit(out, '$');
		} else {
			menu_item_start = command_top;
		}
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
	case 'B': arguments_by_sysver(NULL, "eeeeeee", "neeeeee", "neeeeee"); break;
	case 'D':
		arguments_by_sysver(NULL, config.game_id >= RANCE4_OPT ? "eee" : "eeeeeeee", NULL, NULL);
		break;
	case 'E': arguments_by_sysver(NULL, "eee", "eeeeee", "eeeeee"); break;
	case 'F': break;
	case 'G': arguments_by_sysver("n", "e", "e", "e"); break;
	case 'H': arguments_by_sysver(NULL, "ne", "ne", "ne"); break;
	case 'I': arguments_by_sysver(NULL, "eee", "eeeeee", "eee"); break;
	case 'J': arguments_by_sysver(NULL, "ee", "ee", "ee"); break;
	case 'K': arguments_by_sysver(NULL, "", "n", "n"); break;
	case 'L': arguments_by_sysver("n", "n", "e", "e"); break;
	case 'M': arguments_by_sysver(NULL, "s", "s", "s"); break;
	case 'N': arguments_by_sysver(NULL, "ee", "nee", "ee"); break;
	case 'O': arguments_by_sysver(NULL, "eee", "ev", "ev"); break;
	case 'P': arguments_by_sysver("n", "n", "eeee", "e"); break;
	case 'Q': arguments_by_sysver("n", "n", "e", "e"); break;
	case 'R': break;
	case 'S': arguments("n"); break;
	case 'T': arguments_by_sysver(NULL, "eee", "ee", "eee"); break;
	case 'U':
		arguments_by_sysver("nn", config.game_id >= YAKATA2 ? "ee" : "nn", "ee", "ee");
		break;
	case 'V': arguments_by_sysver(NULL, "neeeeeeeeeeeeeeeeeeeeeeeeeeeee", "ee", "ee"); break;
	case 'W': arguments_by_sysver(NULL, "eeee", "eee", "eee"); break;
	case 'X': arguments("n"); break;
	case 'Y': arguments("ee"); break;
	case 'Z': arguments_by_sysver("ee", "ee", "ee", "eee"); break;

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
	while (command()) {
		if (current_address(out) > 0xffff)
			error_at(input, "Maximum page size exceeded. Consider splitting the source file.");
	}
}

// toplevel ::= commands
static void toplevel(void) {
	if (config.rev_marker && input_page == 0) {
		skip_whitespaces();
		if (compiler->dbg_info)
			debug_line_add(compiler->dbg_info, input_line, current_address(out));
		emit(out, 'R');
		emit(out, 'E');
		emit(out, 'V');
	}

	commands();
	if (*input)
		error_at(input, "unexpected '%c'", *input);
}

HashMap *init_verbobj_hash(Vector *list) {
	HashMap *map = new_string_hash();
	if (!list)
		return map;

	for (int i = 0; i < list->len; i++) {
		intptr_t val = i;
		if (hash_get(map, list->data[i]))
			val = DUPLICATE;
		hash_put(map, list->data[i], (void*)(val + 1));  // +1 to avoid NULL
	}
	return map;
}

Compiler *new_compiler(Vector *src_paths, Vector *variables, Vector *verbs, Vector *objs) {
	Compiler *comp = calloc(1, sizeof(Compiler));
	comp->src_paths = src_paths;
	comp->variables = variables ? variables : new_vec();
	comp->symbols = new_string_hash();
	comp->scos = calloc(src_paths->len, sizeof(Sco));

	for (int i = 0; i < comp->variables->len; i++)
		hash_put(comp->symbols, comp->variables->data[i], new_symbol(VARIABLE, i));
	comp->verb_list = verbs ? verbs : new_vec();
	comp->verb_map = init_verbobj_hash(verbs);
	comp->obj_list = objs ? objs : new_vec();
	comp->obj_map = init_verbobj_hash(objs);

	return comp;
}

static void prepare(Compiler *comp, const char *source, int pageno) {
	compiler = comp;
	lexer_init(source, comp->src_paths->data[pageno], pageno);
	menu_item_start = NULL;
}

static void check_undefined_labels(void) {
	for (int i = 0; i < labels->vals->len; i++) {
		Label *l = labels->vals->data[i];
		if (l->hole_addr)
			error_at(l->source_loc, "undefined label '%s'", labels->keys->data[i]);
	}
}

Sco *compile(Compiler *comp, const char *source, int pageno) {
	prepare(comp, source, pageno);
	labels = new_map();

	comp->scos[pageno].volume_bits = 1 << 1;
	out = new_buf();
	emit_word(out, 0);  // Default address (to be filled later)
	if (comp->dbg_info)
		debug_init_page(comp->dbg_info, pageno);

	toplevel();

	if (menu_item_start)
		error_at(menu_item_start, "unfinished menu item");
	check_undefined_labels();

	Label *default_label = map_get(labels, "default");
	if (!default_label)
		fprintf(stderr, "%s: no default label\n", (char*)comp->src_paths->data[pageno]);
	swap_word(out, 0, default_label ? default_label->addr : out->len - 2);

	if (comp->dbg_info)
		debug_finish_page(comp->dbg_info);
	comp->scos[pageno].buf = out;
	out = NULL;
	return &comp->scos[pageno];
}
