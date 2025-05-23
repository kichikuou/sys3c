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
#include "sys3dc.h"
#include <stdlib.h>
#include <string.h>

#define NODE_POOL_SIZE 1024

bool sys0dc_offby1_error;

static Cali node_pool[NODE_POOL_SIZE];
static Cali *free_node;

static Cali *new_node(int type, int val, Cali *lhs, Cali *rhs) {
	Cali *n = (free_node > node_pool) ? --free_node : calloc(1, sizeof(Cali));
	n->type = type;
	n->val = val;
	n->lhs = lhs;
	n->rhs = rhs;
	return n;
}

static Cali *parse(const uint8_t **code, bool is_lhs) {
	Cali *stack[256];
	Cali **top = stack;
	const uint8_t *p = *code;
	do {
		uint8_t op = *p++;
		switch (op) {
		case OP_END:
			if (top == stack)
				error_at(p, "empty expression");
			if (top - 2 >= stack)
				error_at(p, "unexpected end of expression");
			*code = p;
			return *--top;

		case OP_MUL:
			if (config.sys_ver == SYSTEM1)
				goto operand;
			// fallthrough
		case OP_DIV:
			if (config.sys_ver == SYSTEM1)
				op = OP_MUL;
			// fallthrough
		case OP_ADD:
		case OP_SUB:
		case OP_EQ:
		case OP_LT:
		case OP_GT:
		case OP_NE:
			{
				if (top - 2 < stack)
					error_at(p, "stack underflow");
				Cali *rhs = *--top;
				Cali *lhs = *--top;
				if (op == OP_ADD &&
					lhs->type == NODE_NUMBER && lhs->val == 16383 &&
					rhs->type == NODE_NUMBER && rhs->val <= 65535-16383) {
					lhs->val += rhs->val;
					*top++ = lhs;
				} else {
					*top++ = new_node(NODE_OP, op, lhs, rhs);
				}
			}
			break;

		default:
		operand:
			if (op & 0x80) {
				int var = op & 0x3f;
				if (op >= 0xc0)
					var = var << 8 | *p++;
				*top++ = new_node(NODE_VARIABLE, var, NULL, NULL);
			} else {
				int val = op & 0x3f;
				if (op < 0x40) {
					val = val << 8 | *p++;
					if (val <= 0x33)
						error_at(p, "unknown code 00 %02x", val);
					if ((config.sys_ver == SYSTEM1 && val == 0x37) ||
					    (config.sys_ver >= SYSTEM2 && val == 0x36))
						sys0dc_offby1_error = true;
				}
				*top++ = new_node(NODE_NUMBER, val, NULL, NULL);
			}
			break;
		}
	} while (!is_lhs);

	if (top == stack)
		error_at(p, "empty expression");
	if (--top != stack)
		warning_at(p, "unexpected end of expression");
	Cali *node = *top;
	if (node->type != NODE_VARIABLE)
		error_at(p, "unexpected left-hand-side for assignment %d", node->type);
	*code = p;
	return node;
}

static int precedence(int op) {
	switch (op) {
	case OP_MUL:    return 4;
	case OP_DIV:    return 4;
	case OP_ADD:    return 3;
	case OP_SUB:    return 3;
	case OP_LT:     return 1;
	case OP_GT:     return 1;
	case OP_EQ:     return 0;
	case OP_NE:     return 0;
	case OP_END:    return 0;
	default:
		error("BUG: unknown operator %d", op);
	}
}

Cali *parse_cali(const uint8_t **code, bool is_lhs) {
	free_node = node_pool + NODE_POOL_SIZE;
	return parse(code, is_lhs);
}

void print_cali_prec(Cali *node, int out_prec, Vector *variables, FILE *out) {
	switch (node->type) {
	case NODE_NUMBER:
		fprintf(out, "%d", node->val);
		break;

	case NODE_VARIABLE:
		while (variables->len <= node->val)
			vec_push(variables, NULL);
		if (!variables->data[node->val]) {
			char buf[10];
			sprintf(buf, "VAR%d", node->val);
			variables->data[node->val] = strdup(buf);
		}
		fputs(variables->data[node->val], out);
		break;

	case NODE_OP:
		{
			int prec = precedence(node->val);
			if (out_prec > prec)
				fputc('(', out);
			print_cali_prec(node->lhs, prec, variables, out);
			switch (node->val) {
			case OP_MUL:   fputs(" * ", out); break;
			case OP_DIV:   fputs(" / ", out); break;
			case OP_ADD:   fputs(" + ", out); break;
			case OP_SUB:   fputs(" - ", out); break;
			case OP_EQ:    fputs(" = ", out); break;
			case OP_LT:    fputs(" < ", out); break;
			case OP_GT:    fputs(" > ", out); break;
			case OP_NE:    fputs(" \\ ", out); break;
			case OP_END:   fputs(" $ ", out); break;
			default:
				error("BUG: unknown operator %d", node->val);
			}
			print_cali_prec(node->rhs, prec + 1, variables, out);
			if (out_prec > prec)
				fputc(')', out);
			break;
		}
	}
}

void print_cali(Cali *node, Vector *variables, FILE *out) {
	print_cali_prec(node, 0, variables, out);
}
