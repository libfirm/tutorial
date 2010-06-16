#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <libfirm/firm.h>

ir_mode *d_mode;
ir_type *d_type;

ir_node *cur_store;

char *id_str;
double num_val;
int cur_token;

// ************************** Error *****************************

void error(char *msg)
{
	fprintf(stderr, "Error: %s.\n", msg);
}

// ************ Lexer ************************************

// Token enum for the lexer
enum token_t {
	TOK_EOF = -1,	// End of file
	TOK_DEF = -2,	// Function definition
	TOK_EXT = -3,	// Extern function
	TOK_ID = -4,	// Identifier
	TOK_NUM = -5,	// Number
};

// Returns the current token
int get_token()
{
	int ret = 0;
	static int ch = ' ';
	char buffer[256];

	// Skip whitespace
	while (isspace(ch))
		ch = getchar();
		
	if (isalpha(ch)) {
		int i = 0;

		do {
			buffer[i++] = ch;
			ch = getchar();
		} while (isalnum(ch));
		buffer[i] = '\0';

		if (strcmp(buffer, "def") == 0)
			ret = TOK_DEF;
		else if (strcmp(buffer, "extern") == 0)
			ret = TOK_EXT;
		else {
			id_str = malloc((strlen(buffer) + 1) * sizeof(char));
			strcpy(id_str, buffer);
			ret = TOK_ID;
		}
	}
	else if (isdigit(ch)) {
		int i = 0;

		do {
			buffer[i++] = ch;
			ch = getchar();
		} while (isdigit(ch) || ch == '.');
		buffer[i] = '\0';

		num_val = atof(buffer);
		ret = TOK_NUM;
	}
	else if (ch == '#') {
		// skip comments
		do ch = getchar();
		while (ch != EOF && ch != '\n' && ch != '\r');

		if (ch == EOF)
			ret = TOK_EOF;
		else
			ret = get_token();
	}
	else if (ch == EOF)
		ret = TOK_EOF;
	else {
		ret = ch;
		ch = getchar();
	}

	return ret;
}

int next_token()
{
	cur_token = get_token();
	return cur_token;
}

// ************************** AST ***************************************

typedef enum expr_id {
	EXPR_NUM,
	EXPR_VAR,
	EXPR_BIN,
	EXPR_CALL,
} expr_id;

typedef struct expr_t {
	void *expr;
	expr_id which;
} expr_t;

typedef struct num_expr_t {
	double val;
} num_expr_t;

typedef struct var_expr_t {
	char *name;
} var_expr_t;

typedef struct bin_expr_t {
	char op;
	expr_t *lhs, *rhs;
} bin_expr_t;

typedef struct call_expr_t {
	char *callee;
	expr_t **argv;
	int argc;
} call_expr_t;

typedef struct prototype_t {
	char *name;
	char **argv;
	int argc;
} prototype_t;

typedef struct function_t {
	prototype_t *proto;
	expr_t *body;
} function_t;

expr_t *new_expr(void *expr, expr_id which)
{
	expr_t *a = malloc(sizeof(expr_t));
	a->expr = expr;
	a->which = which;
	return a;
}

var_expr_t *new_var_expr(char *name)
{
	var_expr_t *result = malloc(sizeof(var_expr_t));
	result->name = name;
	return result;
}

bin_expr_t *new_bin_expr(char op, expr_t *lhs, expr_t *rhs)
{
	bin_expr_t *a = malloc(sizeof(bin_expr_t));
	a->op = op;
	a->lhs = lhs;
	a->rhs = rhs;
	return a;
}

call_expr_t *new_call_expr(char *callee, expr_t **argv, int argc)
{
	call_expr_t *a = malloc(sizeof(call_expr_t));
	a->callee = callee;
	a->argv = argv;
	a->argc = argc;
	return a;
}

prototype_t *new_prototype(char *name, char **argv, int argc)
{
	prototype_t *a = malloc(sizeof(prototype_t));
	a->name = name;
	a->argv = argv;
	a->argc = argc;
	return a;
}

function_t *new_function(prototype_t *proto, expr_t *body)
{
	function_t *a = malloc(sizeof(function_t));
	a->proto = proto;
	a->body = body;
	return a;
}

// ****************************** Util **********************************

typedef struct expr_vector_t {
	expr_t **exprs;
	int p;
	int size;
} expr_vector_t;

typedef struct str_vector_t {
	char **strs;
	int p;
	int size;
} str_vector_t;

typedef struct irg_vector_t {
	ir_graph **graphs;
	int p;
	int size;
} irg_vector_t;

expr_vector_t *new_expr_vector()
{
	expr_vector_t *v = malloc(sizeof(expr_vector_t));
	v->exprs = malloc(5 * sizeof(expr_t *));
	v->p = 0;
	v->size = 5;
	return v;
}

str_vector_t *new_str_vector()
{
	str_vector_t *v = malloc(sizeof(str_vector_t));
	v->strs = malloc(5 * sizeof(char *));
	v->p = 0;
	v->size = 5;
	return v;
}

irg_vector_t *new_irg_vector()
{
	irg_vector_t *v = malloc(sizeof(irg_vector));
	v-> graphs = malloc(sizeof(irg_vector_t));
	v-

void push_back_expr(expr_vector_t *v, expr_t *expr)
{
	v->exprs[v->p++] = expr;

	if (v->p == v->size) {
		expr_t **new = malloc(2 * v->size * sizeof(expr_t *));

		for (int i = 0; i < v->size; i++)
			new[i] = v->exprs[i];

		free(v->exprs);
		v->exprs = new;
		v->size *= 2;
	}
}

void push_back_str(str_vector_t *v, char *str)
{
	v->strs[v->p++] = str;

	if (v->p == v->size) {
		char **new = malloc(2 * v->size * sizeof(char *));

		for (int i = 0; i < v->size; i++)
			new[i] = v->strs[i];

		free(v->strs);
		v->strs = new;
		v->size *= 2;
	}
}

// ********************* Parser *************************

expr_t *parse_expr();

int get_tok_prec()
{
	switch (cur_token) {
	default: return -1;
	case '<': return 10;
	case '+':
	case '-': return 20;
	case '*': return 40;
	}
}

expr_t *parse_id_expr()
{
	char *identifier = id_str;
	expr_t *id_expr = NULL;
	expr_vector_t *args = NULL;
	bool arg_error = false;

	next_token();

	if (cur_token != '(')
		id_expr = new_expr(new_var_expr(identifier), EXPR_VAR);
	else {
		next_token();
		if (cur_token != ')') {
			args = new_expr_vector();
			while (1) {
				expr_t *e = parse_expr();

				if (e == NULL) {
					args = NULL;
					break;
				}

				push_back_expr(args, e);

				if (cur_token == ')')
					break;

				if (cur_token != ',') {
					error("Expected ')' or ',' in argument list");
					arg_error = true;
					break;
				}
				
				next_token();
			}
		}

		next_token();

		if (args != NULL) {
			if (!arg_error)
				id_expr = new_expr(new_call_expr(identifier, args->exprs, args->p), EXPR_CALL);
			free(args);
		} else {
			id_expr = new_expr(new_call_expr(identifier, NULL, 0), EXPR_CALL);
		}
	}

	return id_expr;
}

expr_t *parse_num_expr()
{
	num_expr_t *expr = malloc(sizeof(num_expr_t));
	expr->val = num_val;
	next_token();

	return new_expr(expr, EXPR_NUM);
}

expr_t *parse_paren_expr()
{
	next_token();		// eat the (
	expr_t *result = parse_expr();

	if (cur_token != ')') {
		error("')' expected");
		result = NULL;
	}

	return result;
}

expr_t *parse_primary()
{
	switch (cur_token) {
	case TOK_ID:
		return parse_id_expr();
	case TOK_NUM:
		return parse_num_expr();
	case '(':
		return parse_paren_expr();
	default:
		error("unknown token when expecting an expression");
		return NULL;
	}
}

expr_t *parse_bin_rhs(int expr_prec, expr_t *lhs)
{
	while(1) {
		int tok_prec = get_tok_prec();

		if (tok_prec < expr_prec)
			return lhs;

		int bin_op = cur_token;
		next_token();

		expr_t *rhs = parse_primary();
		if (!rhs)
			return NULL;

		int next_prec = get_tok_prec();
		if (tok_prec < next_prec) {
			rhs = parse_bin_rhs(tok_prec + 1, rhs);
			if (!rhs)
				return NULL;
		}

		lhs = new_expr(new_bin_expr(bin_op, lhs, rhs), EXPR_BIN);
	}
}

prototype_t *parse_prototype()
{
	prototype_t *prototype = NULL;
	str_vector_t *arg_names = NULL;
	char *fn_name = NULL;

	if (cur_token != TOK_ID) {
		error("Expected function name in prototype");
	} else {
		fn_name = id_str;
		next_token();

		if (cur_token != '(') {
			error("Expected '(' in prototype");
		} else {
			arg_names = new_str_vector();
	
			while (next_token() == TOK_ID)
				push_back_str(arg_names, id_str);

			if (cur_token != ')') {
				error("Expected ')' in prototype");
				free(arg_names);
				arg_names = NULL;
			}
		}
	}

	next_token();

	if (arg_names != NULL) {
		prototype = new_prototype(fn_name, arg_names->strs, arg_names->p);
		free(arg_names);
	}

	return prototype;
}

function_t *parse_definition()
{
	function_t *fn = NULL;
	prototype_t *prototype = NULL;
	expr_t *body = NULL;
	
	next_token();

	prototype = parse_prototype();
	body = parse_expr();

	if (prototype != NULL && body != NULL)
		fn = new_function(prototype, body);

	return fn;
}

expr_t *parse_extern()
{
	next_token();
	return parse_prototype();
}

function_t *parse_top_lvl()
{
	function_t *fn = NULL;
	expr_t *expr = parse_expr();

	if (expr != NULL) {
		prototype_t *prototype = new_prototype("", NULL, 0);
		fn = new_function(prototype, expr);
	}

	return fn;
}

expr_t *parse_expr()
{
	expr_t *lhs = parse_primary();
	if (!lhs) return NULL;

	return parse_bin_rhs(0, lhs);
}

// ********************* to firm ***********************

typedef struct param_list {
	int n;
	char **name;
	ir_node *proj;
} param_list;

param_list *new_param_list(int n)
{
	param_list *plist = malloc(sizeof(param_list *));
	plist->n = n;
	plist->name = malloc(sizeof(char **) * n);
	plist->proj = malloc(sizeof(ir_node *) * n);

	return plist;
}

ir_node *get_from_param_list(param_list *plist, char *name)
{
	ir_node *proj = NULL;

	for (int i = 0; i < plist->n; i++) {
		if (strcmp(name, plist->name[i]) == 0) {
			proj = plist->proj[i];
			break;
		}
	}
	return proj;
}


void func_to_firm(function_t *fn)
{
	param_list *plist = NULL;
	int n_param = fn->proto->argc;
	// set up the type for the function
	ir_type *fun_type = new_type_method(n_param, 1);
	for (int i = 0; i < n_param; i++)
		set_method_param_type(fun_type, i, d_type);
	set_method_res_type(fun_type, 0, d_type);

	ir_entity *fun_entity = new_entity(get_glob_type(), new_id_from_str(fn->proto->name), fun_type);
	// now we can create the graph
	ir_graph *fun_graph = new_ir_graph(fun_entity, n_param);

	// set cur_store
	cur_store = get_irg_initial_mem(fung_graph);
	// create the projs for the parameters
	if (n_param < 0) {
		ir_node *b = get_irg_current_block(fun_graph);
		set_irg_current_block(fun_graph, get_irg_start_block(fun_graph));

		ir_node *args = get_irg_args(fun_graph);
		plist = new_param_list(n_param);

		for (int i = 0; i < n_param; i++) {
			plist->name[i] = fn->proto->argv[i];
			plist->proj[i] = new_Proj(args, d_mode, i);
		}

		set_irg_curent_block(fun_graph, b);
	}

	ir_node **result = &handle_expr(fn->body, plist);
	ir_node *ret = new_Return(cur_store, 1, result);
	mature_immBlock(get_irg_current_block(fun_graph));

	ir_node *end = get_irg_end_block(fun_graph);
	add_immBlock_pred(end, ret);
	mature_immBlock(end);
}

ir_node *handle_expr(expr_t *expr, param_list *plist)
{
	switch (expr->which) {
	case EXPR_NUM:
		return new_Const(new_tarval_from_double(expr->expr->val, d_mode));
	case EXPR_BIN:
		return handle_bin_expr((bin_expr_t *) expr->expr, plist);
	case EXPR_VAR:
		return handle_var_expr((var_expr_t *) expr->expr, plist);	
	case EXPR_CALL:
		return handle_call_expr((call_expr_t *) expr->expr, plist);
	default:
		return NULL;
	}
}

ir_node *handle_bin_expr(bin_expr_t *bin_ex, plist)
{
	// lhs and rhs are both expressions
	ir_node *lhs = handle_exp(bin_ex->lhs, plist);
	ir_node *rhs = handle_exp(bin_ex->rhs, plist);

	switch (bin_ex->op) {
	case '<':
		ir_node *cmp = new_Cmp(lhs, rhs);
		return new_Proj(cmp, d_mode, pn_Cmp_Lt);
	case '+':
		return new_Add(lhs, rhs, d_mode);
	case '-':
		return new_Sub(lhs, rhs, d_mode);
	case '*':
		return new_Mul(lhs, rhs, d_mode);
	default:
		error("Invalid binary expression.\n");
		return NULL;
	}
}

ir_node *handle_var_expr(var_expr_t *var, plist)
{
	return get_from_param_list(plist, var->name);
}

ir_node *handle_call_expr(call_expr_t *call_exp, plist)
{
	ir_node **in = NULL;
	if (call_exp->argc > 0)
		in = malloc(sizeof(ir_node **) * call_expr->argc);

	for (int i = 0; i < call_expr->argc; i++) {
		ir_node *p = get_from_param_list(plist, call_expr->argv[i]);

		if (p == NULL)
			p = new_Const(atof(new_tarval_from_double(call->expr->argv[i])));
	}
		
	ir_node *call = new_Call(cur_store, NULL, call_exp->argc, in, ) // !!!!!!!!!!
	cur_store = new_Proj(call, get_modeM(), pn_Generic_M);

	return new_Proj(call, d_mode, pn_Generic_X_regular);
}

// ******************* Main *************************

int loop()
{
	bool err = false;
	while (!err) {
		function_t *fn = NULL;
		switch (cur_token) {
		case TOK_EOF:
			return 0;
		case TOK_DEF:
			fn = parse_definition();
			func_to_firm(fn);
			break;
		case ';':
			next_token();
			break;
		default:
			fn = parse_top_lvl();
			func_to_firm(fn);
			break;
		}
	}

	return -1;
}

int main()
{
	ir_init();
	new_ir_prog("kaleidoscope");
	d_mode = get_modeD();
	d_type = new_type_primitive(d_mode);
	loop();

	ir_finish();
	return 0;
}

