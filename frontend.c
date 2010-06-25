#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <libfirm/firm.h>

// some macros for type safe vectors
#define irg_vector_t	vector_t
#define expr_vector_t	vector_t
#define str_vector_t	vector_t

#define get_irg(v, i)	\
	(ir_graph *) v->content[i]

#define get_expressions(v)	\
	(expr_t **) v->content

#define get_strings(v)	\
	(char **) v->content

// an unbounded array
typedef struct vector_t {
	int p;
	int size;
	void **content;
} vector_t;

// ******************* Global *******************************

// the source file
FILE *file;

// we only need to get those once
ir_mode *d_mode;
ir_type *d_type;

// keeps track of the current store
ir_node *cur_store;

// contains the top level expressions
ir_graph *top_lvl;
// the top level store
ir_node *top_store;

// in here we keep track of all the functions
irg_vector_t *flist;

// the identifier string
char *id_str;
// contains the value of any numbers
double num_val;
// our current token
int cur_token;

// ************************** Error *****************************

// obvious ;D
void error(char *msg, char *info)
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
int get_token()
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
			id_str = malloc((strlen(buffer) + 1) * sizeof(char));
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
int next_token()
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
typedef struct expr_t {
	void *expr;
	expr_kind which;
} expr_t;

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
	expr_t **argv;
	int argc;
} call_expr_t;

typedef struct prototype_t {
	char *name;
	char **argv;
	int argc;
} prototype_t;

typedef struct function_t {
	prototype_t *head;
	expr_t *body;
} function_t;

// constructors for the various expression structures
expr_t *new_expr(void *expr, expr_kind which)
{
	expr_t *e = malloc(sizeof(expr_t));
	e->expr = expr;
	e->which = which;
	return e;
}

var_expr_t *new_var_expr(char *name)
{
	var_expr_t *var = malloc(sizeof(var_expr_t));
	var->name = name;
	return var;
}

bin_expr_t *new_bin_expr(char op, expr_t *lhs, expr_t *rhs)
{
	bin_expr_t *bin = malloc(sizeof(bin_expr_t));
	bin->op = op;
	bin->lhs = lhs;
	bin->rhs = rhs;
	return bin;
}

call_expr_t *new_call_expr(char *callee, expr_t **argv, int argc)
{
	call_expr_t *call = malloc(sizeof(call_expr_t));
	call->callee = callee;
	call->argv = argv;
	call->argc = argc;
	return call;
}

prototype_t *new_prototype(char *name, char **argv, int argc)
{
	prototype_t *p = malloc(sizeof(prototype_t));
	p->name = name;
	p->argv = argv;
	p->argc = argc;
	return p;
}

function_t *new_function(prototype_t *head, expr_t *body)
{
	function_t *fn = malloc(sizeof(function_t));
	fn->head = head;
	fn->body = body;
	return fn;
}

// ****************************** Util **********************************

// maps the parameter names to the corresponding projs
typedef struct param_list {
	int n;
	char **name;
	ir_node **proj;
} param_list;

param_list *new_param_list(int n)
{
	param_list *plist = malloc(sizeof(param_list *));
	plist->n = n;
	plist->name = malloc(sizeof(char **) * n);
	plist->proj = malloc(sizeof(ir_node *) * n);

	return plist;
}

// returns the corresponding proj for a name, NULL if name is not found
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

// constructor for vectors
vector_t *new_vector()
{
	vector_t *v = malloc(sizeof(vector_t));
	v->p = 0;
	v->size = 5;
	v->content = malloc(v->size * sizeof(vector_t));
	return v;
}

// used to push things at the end of the vector
void push_back(vector_t *v, void *element)
{
	v->content[v->p++] = element;

	if (v->p == v->size) {
		v->size *= 2;
		void **new = malloc(v->size * sizeof(void *));

		for (int i = 0; i < v->size; i++)
			new[i] = v->content[i];

		free(v->content);
		v->content = new;
	}
}

// ********************* Parser *************************

expr_t *parse_expr();

// returns the precedence of the current token
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

// parses an identifier expression
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
			args = new_vector();
			while (1) {
				expr_t *e = parse_expr();

				if (e == NULL) {
					args = NULL;
					break;
				}

				push_back(args, e);

				if (cur_token == ')')
					break;

				if (cur_token != ',') {
					error("Expected ')' or ',' in argument list", "");
					arg_error = true;
					break;
				}
				
				next_token();
			}
		}

		next_token();

		if (args != NULL) {
			if (!arg_error)
				id_expr = new_expr(new_call_expr(identifier, get_expressions(args), args->p), EXPR_CALL);
			free(args);
		} else {
			id_expr = new_expr(new_call_expr(identifier, NULL, 0), EXPR_CALL);
		}
	}

	return id_expr;
}

// parse a number expression
expr_t *parse_num_expr()
{
	num_expr_t *expr = malloc(sizeof(num_expr_t));
	expr->val = num_val;
	next_token();

	return new_expr(expr, EXPR_NUM);
}

// parse a parantheses expression
expr_t *parse_paren_expr()
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
		error("unknown token when expecting an expression", "");
		printf("DEBUG: %s - curtok: %i = %c\n", id_str, cur_token, cur_token);
		return NULL;
	}
}

// parse the right hand side of a binary expression
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

// parse a prototype
prototype_t *parse_prototype()
{
	prototype_t *prototype = NULL;
	str_vector_t *arg_names = NULL;
	char *fn_name = NULL;

	if (cur_token != TOK_ID) {
		error("Expected function name in prototype", "");
	} else {
		fn_name = id_str;
		next_token();

		if (cur_token != '(') {
			error("Expected '(' in prototype", "");
		} else {
			arg_names = new_vector();
	
			while (next_token() == TOK_ID)
				push_back(arg_names, id_str);

			if (cur_token != ')') {
				error("Expected ')' in prototype", "");
				free(arg_names);
				arg_names = NULL;
			}
		}
	}

	next_token();

	if (arg_names != NULL) {
		prototype = new_prototype(fn_name, get_strings(arg_names), arg_names->p);
		free(arg_names);
	}

	return prototype;
}

// parse the definition of a function
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

// parse a extern prototype
prototype_t *parse_extern()
{
	next_token();
	return parse_prototype();
}

// parse a top level expression
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

// parse an expression
expr_t *parse_expr()
{
	expr_t *lhs = parse_primary();
	if (!lhs) return NULL;

	return parse_bin_rhs(0, lhs);
}

// ********************* to firm ***********************

ir_node *handle_expr(expr_t *expr, param_list *plist);

// generates the node for a binary expression
ir_node *handle_bin_expr(bin_expr_t *bin_ex, param_list *plist)
{
	// lhs and rhs are both expressions
	ir_node *lhs = handle_expr(bin_ex->lhs, plist);
	ir_node *rhs = handle_expr(bin_ex->rhs, plist);

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
		error("Invalid binary expression.\n", "");
		return NULL;
	}
}

// returns the proj for the given parameter
ir_node *handle_var_expr(var_expr_t *var, param_list *plist)
{
	return get_from_param_list(plist, var->name);
}

// generates the nodes for the given call expression
ir_node *handle_call_expr(call_expr_t *call_expr, param_list *plist)
{
	printf("DEBUG: call to %s\n", call_expr->callee);

	ir_node *callee = NULL;
	ir_entity *ent = NULL;
	ir_node **in = NULL;
	ir_node *result = NULL;

	if (call_expr->argc > 0)
		in = malloc(sizeof(ir_node **) * call_expr->argc);

	for (int i = 0; i < flist->p; i++) {
		ent = get_irg_entity(get_irg(flist, i));

		if (!strcmp(get_entity_name(ent), call_expr->callee)) {
			callee = new_SymConst(get_modeP(), (symconst_symbol) ent, symconst_addr_ent);
			break;
		}
	}

	if (callee != NULL) {
		for (int i = 0; i < call_expr->argc; i++)
			in[i] = handle_expr(call_expr->argv[i], plist);

		ir_node *call = new_Call(cur_store, callee, call_expr->argc, in, get_entity_type(ent));
		cur_store = new_Proj(call, get_modeM(), pn_Generic_M);
		ir_node *tup = new_Proj(call, get_modeT(), pn_Call_T_result);
		result = new_Proj(tup, d_mode, 0);
	} else {
		error("Cannot call unknown function ", call_expr->callee);
	}

	return result;
}

// decides what to do with the given expression
ir_node *handle_expr(expr_t *expr, param_list *plist)
{
	switch (expr->which) {
	case EXPR_NUM:
	{
		num_expr_t *num = (num_expr_t *) expr->expr;
		return new_Const(new_tarval_from_double(num->val, d_mode));
	}
	case EXPR_BIN:
		return handle_bin_expr((bin_expr_t *) expr->expr, plist);
		// TODO? return proj auf ergebnis ?????
	case EXPR_VAR:
		return handle_var_expr((var_expr_t *) expr->expr, plist);	
	case EXPR_CALL:
		return handle_call_expr((call_expr_t *) expr->expr, plist);
		// auch proj ?
	default:
		return NULL;
	}
}

// creates an ir_graph for the given function
void func_to_firm(function_t *fn)
{
	printf("DEBUG: in function %s\n", fn->head->name);
	
	param_list *plist = NULL;
	int n_param = fn->head->argc;
	// set up the type for the function
	ir_type *fun_type = new_type_method(n_param, 1);
	for (int i = 0; i < n_param; i++)
		set_method_param_type(fun_type, i, d_type);
	set_method_res_type(fun_type, 0, d_type);

	ir_entity *fun_entity = new_entity(get_glob_type(), new_id_from_str(fn->head->name), fun_type);
	// now we can create the graph
	ir_graph *fun_graph = new_ir_graph(fun_entity, n_param);

	// set cur_store
	cur_store = get_irg_initial_mem(fun_graph);
	// create the projs for the parameters
	if (n_param > 0) {
		ir_node *b = get_irg_current_block(fun_graph);
		set_irg_current_block(fun_graph, get_irg_start_block(fun_graph));

		ir_node *args = get_irg_args(fun_graph);
		// initialize the parameter list
		plist = new_param_list(n_param);
		for (int i = 0; i < n_param; i++) {
			plist->name[i] = fn->head->argv[i];
			plist->proj[i] = new_Proj(args, d_mode, i);
		}

		set_irg_current_block(fun_graph, b);
	}

	// handle the function body
	ir_node *node = handle_expr(fn->body, plist);
	ir_node **result = &node; 
	ir_node *ret = new_Return(cur_store, 1, result);
	mature_immBlock(get_irg_current_block(fun_graph));

	ir_node *end = get_irg_end_block(fun_graph);
	add_immBlock_pred(end, ret);
	mature_immBlock(end);

	// push the function on the function list and dump it
	push_back(flist, fun_graph);
	dump_ir_block_graph(fun_graph, "");
}

// adds the given part to the top level ir_graph
void top_lvl_to_firm(function_t *fn)
{
	cur_store = top_store;
	set_current_ir_graph(top_lvl);
	handle_expr(fn->body, NULL);
	top_store = cur_store;
}

// ******************* Main *************************

// our main loop
int loop()
{
	bool err = false;
	while (!err) {
		function_t *fn = NULL;
		next_token();
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
			top_lvl_to_firm(fn);
			break;
		}
	}

	return -1;
}

int main(int argc, char **argv)
{
	ir_init(NULL);
	new_ir_prog("kaleidoscope");
	d_mode = get_modeD();
	d_type = new_type_primitive(d_mode);
	
	// prepare the top level ir_graph
	ir_type *top_type = new_type_method(0, 0);
	ir_entity *top_entity = new_entity(get_glob_type(), new_id_from_str("main"), top_type);
	top_lvl = new_ir_graph(top_entity, 0);
	top_store = get_irg_initial_mem(top_lvl);

	// initialize the file list, open the source file and run the main loop
	flist = new_vector();
	file = fopen(argv[1], "r");
	loop();

	// finish the top level ir_graph
	set_current_ir_graph(top_lvl);
	ir_node *ret = new_Return(top_store, 0, NULL);
	add_immBlock_pred(get_irg_end_block(top_lvl), ret);
	mature_immBlock(get_irg_end_block(top_lvl));
	// set it as the main function
	set_irp_main_irg(top_lvl);
	// and dump it
	dump_ir_block_graph(top_lvl, "");

	ir_finish();
	return 0;
}

