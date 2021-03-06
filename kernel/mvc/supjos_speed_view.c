/*
  +----------------------------------------------------------------------+
  | Speed framework                                                      |
  +----------------------------------------------------------------------+
  | Copyright (c) 2017-2020 www.supjos.cn                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:  Josin <774542602@qq.com|www.supjos.cn>                      |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_speed.h"

#include <string.h>
#include "kernel/mvc/supjos_speed_view.h"

/* {{{
    Common function to render the file or the file with variables
    ret means to get the file data
*/
void speed_view_include_file(const char *templ, zval *vars, zval *variables, zval *ret)
{
    char real_path[MAXPATHLEN];
    if (!VCWD_REALPATH(templ, real_path)) {
        php_printf("Can't find the templeate file：[ %s ]", templ);
        return ;
    }

    /* Compile the included file into PHP opcodes */
    zend_file_handle t_file_handle;
    t_file_handle.filename = (const char *)templ;
    t_file_handle.opened_path = NULL;
    t_file_handle.handle.fp = NULL;
    t_file_handle.free_filename = 0;
    t_file_handle.type = ZEND_HANDLE_FILENAME;

    /* Zend opcodes */
    zend_op_array *op_array;
    op_array = zend_compile_file(&t_file_handle, ZEND_INCLUDE);
    /* If include the file SUCCESS, Add the file into the global variables */
    if (op_array) {
        if (t_file_handle.handle.stream.handle) {
            if (!t_file_handle.opened_path) {
                t_file_handle.opened_path = zend_string_init(ZEND_STRL(templ), 0);
            }
            zend_hash_add_empty_element(&EG(included_files), t_file_handle.opened_path);
        }
    }
    op_array->scope = speed_view_ce;

    /* start a zend vm code to finish the current include operation */
    zend_execute_data *include_call;
    zval speed_view_object;
    object_init_ex(&speed_view_object, speed_view_ce);
    include_call = zend_vm_stack_push_call_frame(ZEND_CALL_NESTED_CODE | ZEND_CALL_HAS_SYMBOL_TABLE, (zend_function *)op_array, 0, op_array->scope, Z_OBJ(speed_view_object));

    /* include_call->symbol_table = xxx*/
    zend_string *var_name;
    zval *value;
    zend_class_entry *scope = zend_get_executed_scope();
    zend_array *symbol_tables;
    symbol_tables = emalloc(sizeof(zend_array));
    zend_hash_init(symbol_tables, 8, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_real_init(symbol_tables, 0);
    /* if the given array data was not empty */
    if (vars && Z_TYPE_P(vars) == IS_ARRAY) {
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(vars), var_name, value) {
            /* GLOBALS protection */
            if (zend_string_equals_literal(var_name, "GLOBALS")) {
                continue;
            }
            if (zend_string_equals_literal(var_name, "this") && scope && ZSTR_LEN(scope->name) != 0) {
                continue;
            }
            /* Add the value into the current symbol-tables */
            if (EXPECTED(zend_hash_add_new(symbol_tables, var_name, value))) {
                Z_TRY_ADDREF_P(value);
            }
        } ZEND_HASH_FOREACH_END();
    }
    if (!ZVAL_IS_NULL(variables)) {
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(variables), var_name, value) {
            /* GLOBALS protection */
            if (zend_string_equals_literal(var_name, "GLOBALS")) {
                continue;
            }
            if (zend_string_equals_literal(var_name, "this") && scope && ZSTR_LEN(scope->name) != 0) {
                continue;
            }
            /* Add the value into the current symbol-tables */
            if (EXPECTED(zend_hash_add_new(symbol_tables, var_name, value))) {
                Z_TRY_ADDREF_P(value);
            }
        } ZEND_HASH_FOREACH_END();
    }
    include_call->symbol_table = symbol_tables;

    /* Execute the stack call */
    zval result;
    ZVAL_UNDEF(&result);

    if (ret && php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS) == FAILURE) {
        php_error_docref("ref.outcontrol", E_WARNING, "Failed to call ob_start()");
        return ;
    }

    zend_init_execute_data(include_call, op_array, &result);
    ZEND_ADD_CALL_FLAG(include_call, ZEND_CALL_TOP);
    zend_execute_ex(include_call);
    zend_vm_stack_free_call_frame(include_call);

    if (ret) { /* Store the data into the ret zval struct and discard the data in the output */
        if (php_output_get_contents(ret) == FAILURE) {
            php_output_end();
            php_error_docref(NULL, E_WARNING, "Can't fetch the ob_data");
            return ;
        }

        if (php_output_discard() != SUCCESS ) {
            return ;
        }
    }
}

/* {{{ ARG_INFO */
SPEED_BEGIN_ARG_INFO_EX(arginfo_speed_view_render, 0, 0, 1)
    SPEED_ARG_INFO(0, template)
    SPEED_ARG_INFO(0, vars)
SPEED_END_ARG_INFO()

SPEED_BEGIN_ARG_INFO_EX(arginfo_speed_view_partial, 0, 0, 1)
    SPEED_ARG_INFO(0, templ)
SPEED_END_ARG_INFO()

SPEED_BEGIN_ARG_INFO_EX(arginfo_speed_view_getrender, 0, 0, 1)
    SPEED_ARG_INFO(0, templ)
    SPEED_ARG_INFO(0, vars)
SPEED_END_ARG_INFO()

SPEED_BEGIN_ARG_INFO_EX(arginfo_speed_view_setvar, 0, 0, 2)
    SPEED_ARG_INFO(0, var_name)
    SPEED_ARG_INFO(0, var_value)
SPEED_END_ARG_INFO()
/*}}}*/

/* {{{ proto supjos\mvc\View::partial($templ)*/
SPEED_METHOD(View, partial)
{
    char *templ = NULL;
    size_t templ_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &templ, &templ_len) == FAILURE) {
        return ;
    }
    /* render the file */
    zval *variables = zend_read_property(speed_view_ce, getThis(), ZEND_STRL(SPEED_VIEW_VARIABLES), 1, NULL);
    speed_view_include_file(templ, NULL, variables, NULL);
}
/*}}}*/

/* {{{ proto supjos\mvc\View::render($tmpl, $vars)
    Use the View class to render the templates
*/
SPEED_METHOD(View, render)
{
    char *templ = NULL;
    size_t templ_len;
    zval *vars;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &templ, &templ_len, &vars) == FAILURE) {
        return ;
    }
    /* render the file */
    zval *variables = zend_read_property(speed_view_ce, getThis(), ZEND_STRL(SPEED_VIEW_VARIABLES), 1, NULL);
    speed_view_include_file(templ, vars, variables, NULL);
}
/* }}}*/

/* {{{ proto supjos\mvc\View::getRender($tmpl, $vars)
    Use the View class to render the templates and return the result
*/
SPEED_METHOD(View, getRender)
{
    char *templ = NULL;
    size_t templ_len;
    zval *vars;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|z", &templ, &templ_len, &vars) == FAILURE) {
        return ;
    }
    /* render the file */
    zval *variables = zend_read_property(speed_view_ce, getThis(), ZEND_STRL(SPEED_VIEW_VARIABLES), 1, NULL);
    zval ob_data;
    speed_view_include_file(templ, vars, variables, &ob_data);
    RETURN_ZVAL(&ob_data, 0, 0);
}
/* }}}*/

/* {{{ proto supjos\mvc\View::setVar($name, $value)
    Set the value into the template
*/
SPEED_METHOD(View, setVar)
{
    zend_string *var_name;
    zval *var_value;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Sz", &var_name, &var_value) == FAILURE) {
        return ;
    }
    if (strlen(ZSTR_VAL(var_name)) == 0) {
        RETURN_FALSE
    }
    zval *variables = zend_read_property(speed_view_ce, getThis(), ZEND_STRL(SPEED_VIEW_VARIABLES), 1, NULL);
    if (ZVAL_IS_NULL(variables)) {
        zval array_variables;
        array_init(&array_variables);
        zend_update_property(speed_view_ce, getThis(), ZEND_STRL(SPEED_VIEW_VARIABLES), &array_variables);
    }
    if (add_assoc_zval_ex(variables, ZSTR_VAL(var_name), ZSTR_LEN(var_name), var_value) == FAILURE) {
        RETURN_FALSE
    }
}
/*}}}*/

/* {{{
    All methods for the supjos\mvc\View class
*/
static const zend_function_entry speed_view_functions[] = {
    SPEED_ME(View, render , arginfo_speed_view_render , ZEND_ACC_PUBLIC)
    SPEED_ME(View, partial, arginfo_speed_view_partial, ZEND_ACC_PUBLIC)
    SPEED_ME(View, setVar , arginfo_speed_view_setvar , ZEND_ACC_PUBLIC)
    SPEED_ME(View, getRender , arginfo_speed_view_getrender , ZEND_ACC_PUBLIC)

    SPEED_FE_END
};
/*}}}*/

/* {{{
    Load the module when you want
*/
SPEED_STARTUP_FUNCTION(view)
{
    zend_class_entry ce;
    INIT_NS_CLASS_ENTRY(ce, "supjos\\mvc", "View", speed_view_functions);
    speed_view_ce = zend_register_internal_class(&ce);

    /* The property to store the variables in the template */
    zend_declare_property_null(speed_view_ce, ZEND_STRL(SPEED_VIEW_VARIABLES), ZEND_ACC_PROTECTED);
}
/*}}}*/



/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */