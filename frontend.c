#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <libfirm/firm.h>

// we need these ahead declarations
typedef struct expr_t expr_t;
typedef struct prototype_t prototype_t;
typedef struct function_t function_t;

// the source file
static FILE *file;

// global variables needed by the parser
static char *id_str;				// the identifier string
static double num_val;				// contains the value of any numbers
static int cur_token;				// our current token

// the AST
static expr_t *main_exprs;			// a list containing the top level expressions
static expr_t *last_main_expr;		// the tail of that list
static prototype_t *prototypes;		// a list containing the prototypes
static function_t *functions;		// a list containing the functions

// global variables needed for the creation of the firm graphs
static ir_mode *d_mode;				// the double mode
static ir_type *d_type;				// the double type
static ir_node *cur_store;			// keeps track of the current store

// ************************** Error *****************************

// error printing
static void error(char *msg, char *info)
{
	fprintf(stderr, "Error: %s%s.\n", msg, info);
}

// ************ Lexer ************************************

// the lexer's tokens
enum token_t {
	TOK_EOF	= -1,		// end of file
	TOK_DEF	= -2,		// function definition
	TOK_EXT	= -3,		// extern function
	TOK_ID	= -4,		// identifier
	TOK_NUM	= -5,		// number
};

// returns the current token
static int get_token(void)
{
	int ret = 0;
	static int ch = ' ';
	char buffer[256];

	// skip whitespace
	while (isspace(ch))
		ch = fgetc(file);
		
	// keyword or identifier
	if (isalpha(ch)) {
		int i = 0;
		do {
			buffer[i++] = ch;
			ch = fgetc(file);
		} while (isalnum(ch));
		buffer[i] = '\0';

		if (strcmp(buffer, "def") == 0) {
			ret = TOK_DEF;
		}
		else if (strcmp(buffer, "extern") == 0) {
			ret = TOK_EXT;
		}
		else {
			id_str = calloc(strlen(buffer) + 1, sizeof(char));
			strcpy(id_str, buffer);
			ret = TOK_ID;
		}
	} else if (isdigit(ch)) {						// number
		int i = 0;

		do {
			buffer[i++] = ch;
			ch = fgetc(file);
		} while (isdigit(ch) || ch == '.');
		buffer[i] = '\0';

		num_val = atof(buffer);
		ret = TOK_NUM;
	} else if (ch == '#') {							// comments will be ignored
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

// ahead declaration
typedef struct parameter_t parameter_t;

// needed to identify the expressions in expr_t
typedef enum expr_kind {
	EXPR_NUM,				// number expression
	EXPR_VAR,				// variable expression
	EXPR_BIN,				// binary expression
	EXPR_CALL,				// call expression
} expr_kind;

// container struct for all kinds of expressions
struct expr_t {
	void *expr;				// the actual expression
	expr_kind which;		// it's kind

	expr_t *next;			// pointer to the next expression
};

// structs representing the different kinds of expressions
typedef struct num_expr_t {
	double val;				// the double value of a number
} num_expr_t;

typedef struct var_expr_t {
	char *name;				// the name of a variable
} var_expr_t;

typedef struct bin_expr_t {
	char op;				// the binary operator
	expr_t *lhs, *rhs;		// the left and right hand side
} bin_expr_t;

typedef struct call_expr_t {
	char *callee;			// the function to be called
	expr_t *args;			// the arguments of the call
	int argc;				// the number of arguments
} call_expr_t;


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

struct prototype_t {
	char *name;
	parameter_t *args;		// the argument names
	int argc;				// the number of arguments
	ir_entity *ent;			// the entity of the function represented by this prototype

	prototype_t *next;		// pointer to the next prototype
};

struct function_t {
	prototype_t *head;		// the head of the function
	expr_t *body;			// the function body, i.e. a expression

	function_t *next;		// pointer to the next function
};

// constructors for the various expression structures
static expr_t *new_expr(void *expr, expr_kind which)
{
	expr_t *e = calloc(1, sizeof(expr_t));
	e->expr = expr;
	e->which = which;
	return e;
}

static num_expr_t *new_num_expr(double val)
{
	num_expr_t *n = calloc(1, sizeof(num_expr_t));
	n->val = val;
	return n;
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

// ahead declaration
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

// check if a prototype exists for the called function
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

	if (cur_token != '(') {											// it's not a call
		id_expr = new_expr(new_var_expr(identifier), EXPR_VAR);		// hence it's a variable
	} else {
		call_expr_t *call;
		expr_t *args = NULL;
		int c = 0;

		next_token();
		if (cur_token != ')') {										// the call has arguments
			while (1) {
				expr_t *e = parse_expr();							// an argument is an expression

				if (e == NULL) {									// error checking
					args = NULL;
					break;
				}

				e->next = args;										// link the arguments
				args = e;
				c++;												// and count them

				if (cur_token == ')')								// we're done
					break;

				if (cur_token != ',') {								// error checking
					error("Expected ')' or ',' in argument list", "");
					break;
				}
				
				next_token();
			}
		}

		next_token();

		call = new_call_expr(identifier, args, c);
		if (check_call(call)) {
			id_expr = new_expr(call, EXPR_CALL);
		}
	}
	return id_expr;
}

// parse a number expression
static expr_t *parse_num_expr(void)
{
	num_expr_t *expr = new_num_expr(num_val);
	next_token();

	return new_expr(expr, EXPR_NUM);
}

// parse a parantheses expression
static expr_t *parse_paren_expr(void)
{
	next_token();													// eat the '('
	expr_t *result = parse_expr();									// parse the expression

	if (cur_token != ')') {											// error checking
		error("')' expected", "");
		result = NULL;
	}

	return result;
}

// 
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
	int argc = 0;
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

	// if a prototype with this name already exists, don't create another one
	for (prototype_t *p = prototypes; p != NULL; p = p->next) {
		if (!strcmp(p->name, fn_name)) {
			proto = p;
			p = NULL;
		}
	}

	if (proto == NULL) {
		// the prototype doesn't already exist
		proto = new_prototype(fn_name, args, argc);
		proto->next = prototypes;
		prototypes = proto;
	}
	return proto;
}

// parse the definition of a function
static bool parse_definition(void)
{
	prototype_t *prototype = NULL;
	expr_t *body = NULL;
	
	next_token();

	prototype = parse_prototype();								// its head is a prototype
	body = parse_expr();										// its body is an expression

	if (prototype != NULL && body != NULL) {
		 function_t *fn = new_function(prototype, body);
		 fn->next = functions;
		 functions = fn;
		 return true;
	} else {
		return false;
	}
}

// parse a top level expression
static bool parse_top_lvl(void)
{
	expr_t *expr = parse_expr();
	if (expr == NULL)
		return false;

	if (last_main_expr != NULL) {
		last_main_expr->next = expr;
	} else {
		main_exprs = expr;
	}
	last_main_expr = expr;

	return true;
}

// parse an expression
static expr_t *parse_expr(void)
{
	expr_t *lhs = parse_primary();
	if (!lhs) return NULL;

	return parse_bin_rhs(0, lhs);
}

// the main parser loop
static bool parser_loop(void)
{
	bool err = false;
	next_token();
	// parse the file till we reach the end
	while (!err) {
		switch (cur_token) {
		case TOK_EOF:
			return true;
		case TOK_DEF:
			err = !parse_definition();
			break;
		case TOK_EXT:
			next_token();
			err = (parse_prototype() == NULL);
			break;
		case ';':
			next_token();
			break;
		default:
			err = !parse_top_lvl();
			break;
		}
		if (err) error("Unexpected Parser Error!", "");
	}
	// an error occurred
	return false;
}

// ********************* to firm ***********************

// ahead declaration
static ir_node *handle_expr(expr_t *expr, parameter_t *args);

// generates the nodes for a binary expression
static ir_node *handle_bin_expr(bin_expr_t *bin_ex, parameter_t *args)
{
	// lhs and rhs are both expressions
	ir_node *lhs = handle_expr(bin_ex->lhs, args);
	ir_node *rhs = handle_expr(bin_ex->rhs, args);

	switch (bin_ex->op) {
	case '<':
	{
		ir_node *cmp = new_Cmp(lhs, rhs);			// compare lhs with rhs
		return new_Proj(cmp, d_mode, pn_Cmp_Lt);	// is lhs less than rhs?
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
	for (parameter_t *p = args; p != NULL; p = p->next) {
		if (!strcmp(p->name, var->name)) return p->proj;
	}
	return NULL;
}

// generates the nodes for the given call expression
static ir_node *handle_call_expr(call_expr_t *call, parameter_t *args)
{
	ir_node *callee = NULL;
	ir_node **in = NULL;
	ir_node *result = NULL;
	ir_entity *ent = NULL;

	// allocate space for an array referencing the arguments
	if (call->argc > 0)
		in = calloc(call->argc, sizeof(ir_node **));

	// find the corresponding prototype and create a symbolic constant
	for (prototype_t *p = prototypes; p != NULL; p = p->next) {
		if (!strcmp(p->name, call->callee)) {
			ent = p->ent;
			callee = new_SymConst(get_modeP(), (symconst_symbol) ent, symconst_addr_ent);
			break;
		}
	}

	if (callee != NULL) {
		// handle the arguments
		int i = 0;
		for (expr_t *e = call->args; e != NULL; e = e->next) {
			in[i++] = handle_expr(e, args);
		}
		// create the call
		ir_node *call_node = new_Call(cur_store, callee, call->argc, in, get_entity_type(ent));
		// update the current store
		cur_store = new_Proj(call_node, get_modeM(), pn_Generic_M);
		// get the result
		ir_node *tuple = new_Proj(call_node, get_modeT(), pn_Call_T_result);
		result = new_Proj(tuple, d_mode, 0);
	} else {
		error("Cannot call unknown function: ", call->callee);
	}

	if (in != NULL) {
		free(in);
	}
	return result;
}

// decides what to do with the given expression
static ir_node *handle_expr(expr_t *expr, parameter_t *args)
{
	switch (expr->which) {
	case EXPR_NUM:
	{
		num_expr_t *num = (num_expr_t *) expr->expr;				// cast to num_expr_t
		return new_Const(new_tarval_from_double(num->val, d_mode));	// create a constant
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

// creates an entity for each prototype
static void create_prototype_entities(void)
{
	for (prototype_t *proto = prototypes; proto != NULL; proto = proto->next) {
		ir_type *proto_type = new_type_method(proto->argc, 1);		// create a method type
		for (int i = 0; i < proto->argc; i++)						// for each parameter
			set_method_param_type(proto_type, i, d_type);			// set its type to double
		set_method_res_type(proto_type, 0, d_type);					// just like the result type
		// that's all we need to create the entity
		proto->ent = new_entity(get_glob_type(), new_id_from_str(proto->name), proto_type);
	}
}

// creates an ir_graph for each function
static void create_func_graphs(void)
{
	for (function_t *fn = functions; fn != NULL; fn = fn->next) {
		int n_param = fn->head->argc;
		ir_graph *fun_graph = new_ir_graph(fn->head->ent, n_param);	// create a new graph
		cur_store = get_irg_initial_mem(fun_graph);					// update the cur_store pointer
		// create the projs for the parameters
		if (n_param > 0) {
			// keep track of the current block
			ir_node *block = get_irg_current_block(fun_graph);
			// set the start block to be the current block
			set_irg_current_block(fun_graph, get_irg_start_block(fun_graph));
			// get a reference to the arguments node
			ir_node *args = get_irg_args(fun_graph);
			
			int i = 0;												// for each argument
			for (parameter_t *p = fn->head->args; p != NULL; p = p->next) {
				p->proj = new_Proj(args, d_mode, i++);				// create a projection node
			}

			set_irg_current_block(fun_graph, block);				// restore the current block
		}
		// the body is just an expression
		ir_node *node = handle_expr(fn->body, fn->head->args);		
		// the result of the function is the result of the body
		ir_node **result = &node;
		ir_node *ret = new_Return(cur_store, 1, result);			// create a return node
		ir_node *end = get_irg_end_block(fun_graph);				// get hold of the end block
		// set the return node to be its predecessor
		add_immBlock_pred(end, ret);								

		mature_immBlock(get_irg_current_block(fun_graph));			// mature the current block
		mature_immBlock(end);										// mature the end block

		irg_finalize_cons(fun_graph);								// finalize the construction
	}
}

// create the main graph
static void create_main(void)
{
	ir_type *type = new_type_method(0, 1);							// create the type
	set_method_res_type(type, 0, d_type);							// set the result type
	// create an entity
	ir_entity *ent = new_entity(get_glob_type(), new_id_from_str("main"), type);
	// create a fresh graph
	ir_graph *fn_main = new_ir_graph(ent, 0);
	cur_store = get_irg_initial_mem(fn_main);

	// handle each expression and keep a reference to the last one
	ir_node *node = NULL;
	for (expr_t *e = main_exprs; e != NULL; e = e->next) {
		node = handle_expr(e, NULL);
	}
	// get the result
	ir_node **result = &node;
	ir_node *ret = new_Return(cur_store, 1, result);
	add_immBlock_pred(get_irg_end_block(fn_main), ret);
	// mature the current and the end block
	mature_immBlock(get_irg_current_block(fn_main));
	mature_immBlock(get_irg_end_block(fn_main));
	// set it as the main function
	set_irp_main_irg(fn_main);
	irg_finalize_cons(fn_main);
}

// ******************* Main *************************

// extract the program name from the source file name
static char *gen_prog_name(char *source_name)
{
	int len = strlen(source_name);
	char *prog_name = calloc(len + 1, sizeof(char));

	int i = 0;
	// copy the prefix of the file name
	while (source_name[i] != '.' && source_name[i] != '\0') {
		prog_name[i] = source_name[i];
		i++;
	}
	prog_name[i] = '\0';

	return prog_name;
}

// generate a name for the assembly file
static char *gen_asm_name(char *prog_name)
{
	int len = strlen(prog_name);
	// allocate enough space for the name + ".s\0"
	char *asm_name = calloc(len + 3, sizeof(char));

	strcpy(asm_name, prog_name);
	strncat(asm_name, ".s", 2);

	return asm_name;
}

int main(int argc, char **argv)
{
	char *prog_name;
	char *src_file = NULL;
	bool opt_dump = false;						// '-d' as the first argument enables dumping

	// handle the arguments
	if (argc == 2) {
		src_file = argv[1];
	} else if (argc == 3 && !strcmp(argv[1], "-d")) {
		src_file = argv[2];
		opt_dump = true;
	} else {
		error("No source file provided", "");
		exit(1);
	}

	// open the source file
	prog_name = gen_prog_name(src_file);
	file = fopen(src_file, "r");

	// error checking
	if (file == NULL) {
		error("Could not open source file", "");
		exit(1);
	}

	if (parser_loop()) {						// run the parser loop
		ir_init(NULL);							// initialize libfirm
		new_ir_prog(prog_name);					// create a new program
		d_mode = get_modeD();					// set d_mode to the double mode
		d_type = new_type_primitive(d_mode);	// create the primitive double type
		// create the graphs
		create_prototype_entities();
		create_func_graphs();
		create_main();

		if (opt_dump) 							// dump all graphs
			dump_all_ir_graphs(&dump_ir_block_graph, "");

		char *asm_name = gen_asm_name(prog_name);
		FILE *out = NULL;
		if ((out = fopen(asm_name, "w")) == NULL) {
			error("Could not open output file ", asm_name);
			exit(1);
		}

		be_main(out, prog_name);				// emit code
		ir_finish();							// clean up
	}

	return 0;
}

