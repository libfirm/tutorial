#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <libfirm/firm.h>

// ****************************** Util **********************************

typedef struct parameter_t parameter_t;

struct parameter_t {
	char *name;
	ir_node *proj;

	parameter_t *next;
};

static parameter_t *new_parameter(char *name)
{
	parameter_t *p = calloc(1, sizeof(parameter_t));
	p->name = name;
	return p;
}

static ir_node *get_arg(parameter_t *params, char *name)
{
	for (parameter_t *p = params; p != NULL; p = p->next) {
		if (!strcmp(p->name, name)) return p->proj;
	}
	return NULL;
}

// ******************* Global *******************************

typedef struct expr_t expr_t;
typedef struct prototype_t prototype_t;
typedef struct function_t function_t;

// the source file
static FILE *file;

// we only need to get those once
static ir_mode *d_mode;
static ir_type *d_type;

// keeps track of the current store
static ir_node *cur_store;

//NEW
static expr_t *main_funs;
static expr_t *last_main_fun;
static prototype_t *prototypes;
static function_t *functions;

// the identifier string
static char *id_str;
// contains the value of any numbers
static double num_val;
// our current token
static int cur_token;

// ************************** Error *****************************

// obvious ;D
static void error(char *msg, char *info)
{
	fprintf(stderr, "Error: %s%s.\n", msg, info);
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
static int get_token(void)
{
	int ret = 0;
	static int ch = ' ';
	char buffer[256];

	// Skip whitespace
	while (isspace(ch))
		ch = fgetc(file);
		
	if (isalpha(ch)) {
		int i = 0;
		do {
			buffer[i++] = ch;
			ch = fgetc(file);
		} while (isalnum(ch));
		buffer[i] = '\0';

		if (strcmp(buffer, "def") == 0)
			ret = TOK_DEF;
		else if (strcmp(buffer, "extern") == 0)
			ret = TOK_EXT;
		else {
			id_str = calloc(strlen(buffer) + 1, sizeof(char));
			strcpy(id_str, buffer);
			ret = TOK_ID;
		}
	} else if (isdigit(ch)) {
		int i = 0;

		do {
			buffer[i++] = ch;
			ch = fgetc(file);
		} while (isdigit(ch) || ch == '.');
		buffer[i] = '\0';

		num_val = atof(buffer);
		ret = TOK_NUM;
	} else if (ch == '#') {
		// skip comments
		do ch = fgetc(file);
		while (ch != EOF && ch != '\n' && ch != '\r');

		if (ch == EOF)
			ret = TOK_EOF;
		else
			ret = get_token();
	} else if (ch == EOF)
		ret = TOK_EOF;
	else {
		ret = ch;
		ch = fgetc(file);
	}

	return ret;
}

// wrapper around get_token()
static int next_token(void)
{
	cur_token = get_token();
	return cur_token;
}

// ************************** AST ***************************************

// needed to identify the expressions in expr_t
typedef enum expr_kind {
	EXPR_NUM,
	EXPR_VAR,
	EXPR_BIN,
	EXPR_CALL,
} expr_kind;

// container struct fur all kinds of expressions
struct expr_t {
	void *expr;
	expr_kind which;

	expr_t *next;
};

// structs representing the different kinds of expressions
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
	expr_t *args;
	int argc;
} call_expr_t;

struct prototype_t {
	char *name;
	parameter_t *args;
	int argc;
	ir_entity *ent;

	prototype_t *next;
};

struct function_t {
	prototype_t *head;
	expr_t *body;

	function_t *next;
};

// constructors for the various expression structures
static expr_t *new_expr(void *expr, expr_kind which)
{
	expr_t *e = calloc(1, sizeof(expr_t));
	e->expr = expr;
	e->which = which;
	return e;
}

static var_expr_t *new_var_expr(char *name)
{
	var_expr_t *v = calloc(1, sizeof(var_expr_t));
	v->name = name;
	return v;
}

static bin_expr_t *new_bin_expr(char op, expr_t *lhs, expr_t *rhs)
{
	bin_expr_t *b = calloc(1, sizeof(bin_expr_t));
	b->op = op;
	b->lhs = lhs;
	b->rhs = rhs;
	return b;
}

static call_expr_t *new_call_expr(char *callee, expr_t *args, int argc)
{
	call_expr_t *c = calloc(1, sizeof(call_expr_t));
	c->callee = callee;
	c->args = args;
	c->argc = argc;
	return c;	
}

static prototype_t *new_prototype(char *name, parameter_t *args, int argc)
{
	prototype_t *p = calloc(1, sizeof(prototype_t));
	p->name = name;
	p->args = args;
	p->argc = argc;
	return p;
}

static function_t *new_function(prototype_t *head, expr_t *body)
{
	function_t *f = calloc(1, sizeof(function_t));
	f->head = head;
	f->body = body;
	return f;
}

// ********************* Parser *************************

static expr_t *parse_expr(void);

// returns the precedence of the current token
static int get_tok_prec(void)
{
	switch (cur_token) {
	default: return -1;
	case '<': return 10;
	case '+':
	case '-': return 20;
	case '*': return 40;
	}
}

static bool check_call(call_expr_t *call)
{
	for (prototype_t *p = prototypes; p != NULL; p = p->next) {
		if (!strcmp(call->callee, p->name) && call->argc == p->argc)
			return true;
	}
	return false;
}

// parses an identifier expression
static expr_t *parse_id_expr(void)
{
	char *identifier = id_str;
	expr_t *id_expr = NULL;

	next_token();

	if (cur_token != '(') {	// it's not a call
		id_expr = new_expr(new_var_expr(identifier), EXPR_VAR);
	} else {	// it's a call
		call_expr_t *call;
		expr_t *args = NULL;
		int c = 0;

		next_token();
		if (cur_token != ')') {
			while (1) {
				expr_t *e = parse_expr();

				if (e == NULL) {
					args = NULL;
					break;
				}

				e->next = args;
				args = e;
				c++;

				if (cur_token == ')')
					break;

				if (cur_token != ',') {
					error("Expected ')' or ',' in argument list", "");
					break;
				}
				
				next_token();
			}
		}

		next_token();

		call = new_call_expr(identifier, args, c);
		if (check_call(call))
			id_expr = new_expr(call, EXPR_CALL);
	}

	return id_expr;
}

// parse a number expression
static expr_t *parse_num_expr(void)
{
	num_expr_t *expr = calloc(1, sizeof(num_expr_t));
	expr->val = num_val;
	//next_token();

	return new_expr(expr, EXPR_NUM);
}

// parse a parantheses expression
static expr_t *parse_paren_expr(void)
{
	next_token();		// eat the (
	expr_t *result = parse_expr();

	if (cur_token != ')') {
		error("')' expected", "");
		result = NULL;
	}

	return result;
}

// parse a primary expression
static expr_t *parse_primary(void)
{
	switch (cur_token) {
	case TOK_ID:
		return parse_id_expr();
	case TOK_NUM:
		return parse_num_expr();
	case '(':
		return parse_paren_expr();
	default:
	{
		char info[2] = { cur_token, '\0' };
		error("Unknown token when expecting an expression: ", info);
		return NULL;
	}
	}
}

// parse the right hand side of a binary expression
static expr_t *parse_bin_rhs(int expr_prec, expr_t *lhs)
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

// parse a prototype
static prototype_t *parse_prototype(void)
{
	prototype_t *proto = NULL;
	parameter_t *args = NULL;
	int argc;
	char *fn_name = NULL;

	if (cur_token != TOK_ID) {
		error("Expected function name in prototype", "");
	} else {
		fn_name = id_str;
		next_token();

		if (cur_token != '(') {
			error("Expected '(' in prototype", "");
		} else {
			while (next_token() == TOK_ID) {
				parameter_t *arg = new_parameter(id_str);
				arg->next = args;
				args = arg;
				argc++;
			}
			if (cur_token != ')') {
				error("Expected ')' in prototype", "");
				args = NULL;
			}
		}
	}

	next_token();

	proto = new_prototype(fn_name, args, argc);

	proto->next = prototypes;
	prototypes = proto;

	printf("DEBUG: parsed prototype of %s\n", proto->name);

	return proto;
}

// parse the definition of a function
static void parse_definition(void)
{
	prototype_t *prototype = NULL;
	expr_t *body = NULL;
	
	next_token();

	prototype = parse_prototype();
	body = parse_expr();

	if (prototype != NULL && body != NULL) {
		 function_t *fn = new_function(prototype, body);
		 fn->next = functions;
		 functions = fn;
	}

	printf("DEBUG: parsed function %s\n", prototype->name);
}

// parse a top level expression
static void parse_top_lvl(void)
{
	expr_t *expr = parse_expr();
	if (expr == NULL)
		return;

	if (last_main_fun != NULL) {
		last_main_fun->next = expr;
	} else {
		main_funs = expr;
	}
	last_main_fun = expr;
}

// parse an expression
static expr_t *parse_expr()
{
	expr_t *lhs = parse_primary();
	if (!lhs) return NULL;

	return parse_bin_rhs(0, lhs);
}

// the main parser loop
static int parser_loop(void)
{
	bool err = false;
	while (!err) {
		next_token();
		switch (cur_token) {
		case TOK_EOF:
			return 0;
		case TOK_DEF:
			parse_definition();
			break;
		case TOK_EXT:
			next_token();
			parse_prototype();
			break;
		case ';':
			next_token();
			break;
		default:
			parse_top_lvl();
			break;
		}
	}

	return -1;
}

// ********************* to firm ***********************

static ir_node *handle_expr(expr_t *expr, parameter_t *args);

// generates the node for a binary expression
static ir_node *handle_bin_expr(bin_expr_t *bin_ex, parameter_t *args)
{
	// lhs and rhs are both expressions
	ir_node *lhs = handle_expr(bin_ex->lhs, args);
	ir_node *rhs = handle_expr(bin_ex->rhs, args);

	switch (bin_ex->op) {
	case '<':
	{
		ir_node *cmp = new_Cmp(lhs, rhs);
		return new_Proj(cmp, d_mode, pn_Cmp_Lt);
	}
	case '+':
		return new_Add(lhs, rhs, d_mode);
	case '-':
		return new_Sub(lhs, rhs, d_mode);
	case '*':
		return new_Mul(lhs, rhs, d_mode);
	default:
		error("Invalid binary expression", "");
		return NULL;
	}
}

// returns the proj for the given parameter
static ir_node *handle_var_expr(var_expr_t *var, parameter_t *args)
{
	return get_arg(args, var->name);
}

// generates the nodes for the given call expression
static ir_node *handle_call_expr(call_expr_t *call, parameter_t *args)
{
	printf("DEBUG: call to %s\n", call->callee);

	ir_node *callee = NULL;
	ir_node **in = NULL;
	ir_node *result = NULL;
	ir_entity *ent = NULL;

	if (call->argc > 0)
		in = calloc(call->argc, sizeof(ir_node **));

	for (prototype_t *p = prototypes; p != NULL; p = p->next) {
		if (!strcmp(p->name, call->callee) && p->argc == call->argc) {
			ent = p->ent;
			callee = new_SymConst(get_modeP(), (symconst_symbol) ent, symconst_addr_ent);
			break;
		}
	}

	if (callee != NULL) {
		int i = 0;
		for (expr_t *e = call->args; e != NULL; e = e->next) {
			in[i++] = handle_expr(e, args);
		}

		ir_node *call_node = new_Call(cur_store, callee, call->argc, in, get_entity_type(ent));
		cur_store = new_Proj(call_node, get_modeM(), pn_Generic_M);
		ir_node *tup = new_Proj(call_node, get_modeT(), pn_Call_T_result);
		result = new_Proj(tup, d_mode, 0);
	} else {
		error("Cannot call unknown function: ", call->callee);
	}

	return result;
}

// decides what to do with the given expression
static ir_node *handle_expr(expr_t *expr, parameter_t *args)
{
	switch (expr->which) {
	case EXPR_NUM:
	{
		num_expr_t *num = (num_expr_t *) expr->expr;
		return new_Const(new_tarval_from_double(num->val, d_mode));
	}
	case EXPR_BIN:
		return handle_bin_expr((bin_expr_t *) expr->expr, args);
	case EXPR_VAR:
		return handle_var_expr((var_expr_t *) expr->expr, args);	
	case EXPR_CALL:
		return handle_call_expr((call_expr_t *) expr->expr, args);
	default:
		return NULL;
	}
}

// creates the entities to all functions
static void create_func_entities(void)
{
	for (function_t *fn = functions; fn != NULL; fn = fn->next) {
		ir_type *fun_type = new_type_method(fn->head->argc, 1);
		for (int i = 0; i < fn->head->argc; i++)
			set_method_param_type(fun_type, i, d_type);
		set_method_res_type(fun_type, 0, d_type);

		fn->head->ent = new_entity(get_glob_type(), new_id_from_str(fn->head->name), fun_type);
	}
}

// creates an ir_graph for the given function
static void create_func_graphs(void)
{
	for (function_t *fn = functions; fn != NULL; fn = fn->next) {
		printf("DEBUG: in function %s\n", fn->head->name);
		
		int n_param = fn->head->argc;
		ir_graph *fun_graph = new_ir_graph(fn->head->ent, n_param);

		// set cur_store
		cur_store = get_irg_initial_mem(fun_graph);
		// create the projs for the parameters
		if (n_param > 0) {
			ir_node *block = get_irg_current_block(fun_graph);
			set_irg_current_block(fun_graph, get_irg_start_block(fun_graph));

			ir_node *args = get_irg_args(fun_graph);
			// initialize the parameter list
			int i = 0;
			for (parameter_t *p = fn->head->args; p != NULL; p = p->next) {
				p->proj = new_Proj(args, d_mode, i++);
			}

			set_irg_current_block(fun_graph, block);
		}

		// handle the function body
		ir_node *node = handle_expr(fn->body, fn->head->args);
		ir_node **result = &node; 
		ir_node *ret = new_Return(cur_store, 1, result);
		mature_immBlock(get_irg_current_block(fun_graph));

		ir_node *end = get_irg_end_block(fun_graph);
		add_immBlock_pred(end, ret);
		mature_immBlock(end);

		// push the function on the function list and dump it
		dump_ir_block_graph(fun_graph, "");
	}
}

// create the main graph
static void create_main(void)
{
	ir_entity *ent = new_entity(get_glob_type(), new_id_from_str("main"), new_type_method(0, 0));
	ir_graph *fn_main = new_ir_graph(ent, 0);
	cur_store = get_irg_initial_mem(fn_main);

	for (expr_t *e = main_funs; e != NULL; e = e->next) {
		handle_expr(e, NULL);
	}

	ir_node *ret = new_Return(cur_store, 0, NULL);
	add_immBlock_pred(get_irg_end_block(fn_main), ret);
	mature_immBlock(get_irg_end_block(fn_main));
	// set it as the main function
	set_irp_main_irg(fn_main);
	// and dump it
	dump_ir_block_graph(fn_main, "");
}

// ******************* Memory ***********************


// ******************* Main *************************

int main(int argc, char **argv)
{
	// open the source file and run the parser loop
	if (argc == 2) {
		file = fopen(argv[1], "r");
	} else {
		error("No source file found", "");
		return -1;
	}

	parser_loop();

	ir_init(NULL);
	new_ir_prog("kaleidoscope");
	d_mode = get_modeD();
	d_type = new_type_primitive(d_mode);

	create_func_entities();
	create_func_graphs();
	create_main();

	ir_finish();
	return 0;
}

