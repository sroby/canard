#ifndef canard_h
#define canard_h

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef CANARD_MAX_NAMESPACES
#define CANARD_MAX_NAMESPACES 16
#endif

#ifndef CANARD_MAX_BUFFER
#define CANARD_MAX_BUFFER 8
#endif

#ifndef CANARD_MAX_CMDLINE
#define CANARD_MAX_CMDLINE 128
#endif

#ifndef CANARD_MAX_ARGS
#define CANARD_MAX_ARGS 10
#endif

#define END_CMD_DECL {NULL, NULL, NULL}
#define END_VAR_DECL {NULL, NULL, 0, NULL, NULL}

typedef struct Console Console;
typedef struct CnNamespace CnNamespace;

typedef struct CnStatement {
    int argc;
    char *argv[CANARD_MAX_ARGS];
    char raw[CANARD_MAX_CMDLINE];
} CnStatement;

typedef union CnVarValue {
    bool b_val;
    int i_val;
    char *str;
} CnVarValue;

typedef enum CnVarType {
    CVAR_BOOL,
    CVAR_INT,
    CVAR_STRING,
} CnVarType;

typedef void (*CnVarCallback)(void *, Console *, CnVarValue *);

typedef struct CnVarDecl {
    const char *name;
    CnVarCallback func;
    CnVarType type;
    const void *default_value;
    const char *description;
} CnVarDecl;

typedef struct CnVariable {
    CnVarCallback func;
    CnVarType type;
    CnVarValue default_value;
    CnVarValue value;
} CnVariable;

typedef bool (*CnCmdExec)(void *, Console *, const CnStatement *);

typedef struct CnCmdDecl {
    const char *name;
    CnCmdExec func;
    const char *description;
} CnCmdDecl;

typedef struct CnCommand {
    CnCmdExec func;
} CnCommand;

typedef enum CnObjectType {
    COBJ_CMD,
    COBJ_VAR,
} CnObjectType;

typedef union CnSubObject {
    CnCommand cmd;
    CnVariable var;
} CnSubObject;

typedef struct CnObject {
    const char *name;
    const char *description;
    CnObjectType type;
    CnSubObject sub;
    CnNamespace *ns;
} CnObject;

typedef struct CnNamespace {
    const char *name;
    void *handler;
    int t_objs;
    CnObject *objs;
    CnStatement buffer[CANARD_MAX_BUFFER];
} CnNamespace;

typedef struct Console {
    const char *app_name;
    FILE *output;
    CnNamespace nss[CANARD_MAX_NAMESPACES];
} Console;

/**
* Initialize a Console struct.
* @param con Newly created Console struct to initialize.
* @param app_name A simple name for the application, to be used when a unique
                  identifier is needed, for instance as the settings directory.
                  Should be alphanumerical-only if possible.
*/
void canard_init(Console *con, const char *app_name);

/**
 * Perform cleanup tasks on a Console object.
 * @param con The Console object to teardown.
 */
void canard_teardown(Console *con);

/**
 * Create a namespace.
 * @param con Required. The Console struct that will hold the new namespace.
 * @param name Required. String identifier of the new namespace.
 * @return The newly created namespace struct, or NULL if the namespace table
 *         has been exhaused, or one of the required parameters was NULL.
 */
CnNamespace *canard_create_namespace(Console *con, const char *name,
                                     const CnCmdDecl *cmds,
                                     const CnVarDecl *vars);

/**
 * Create a command within a given namespace.
 * @param ns Required. The namespace that will hold the new command.
 * @param name Required. String identifier of the new command.
 * @param func Optional. The execution function to call upon usage.
 * @param description Required. Description of the new command.
 * @return The newly created variable struct, or NULL if the namespace is full,
 *         or the name is already used, or one of the required parameters was
 *         NULL.
 */
CnObject *canard_create_command(CnNamespace *ns, const char *name,
                                CnCmdExec func,
                                const char *description);

/**
* Create a variable within a given namespace.
* @param ns Required. The namespace that will hold the new variable.
* @param name Required. String identifier of the new variable.
* @param type The variable type of the new variable.
* @param func Optional. The change callback function to be called whenever the
*             variable has been changed (and the namespace handler has been
*             defined).
* @return The newly created variable struct, or NULL if the namespace is full,
*         or the name is already used, or one of the required parameters was
*         NULL.
*/
CnObject *canard_create_variable(CnNamespace *ns, const char *name,
                                 CnVarType type, CnVarCallback func,
                                 const char *description);

/**
 * Define (or remove) a handler pointer for a given namespace. Until said handler is
 * defined, all variable change callbacks are ignored, and all command
 * executions are buffered until this function is called. Whenever one of those
 * functions are called, their first parameter will be set to that handler.
 * @param ns Required.
 * @param handler Optional. The handler pointer to set, or NULL to remove the
 *                current one.
 */
void canard_namespace_set_handler(CnNamespace *ns, void *handler);

/**
 * Execute a given console statement.
 */
void canard_exec(Console *con, const char *cmdline);

bool canard_get_cvar_bool(CnVariable *cvar);
void canard_set_cvar_bool(Console *con, CnVariable *cvar, bool value);
bool canard_toggle_cvar_bool(Console *con, CnVariable *cvar);

int canard_get_cvar_int(Console *con, CnVariable *cvar);
void canard_set_cvar_int(Console *con, CnVariable *cvar, int value);

const char *canard_get_cvar_str(CnVariable *cvar);
void canard_set_cvar_str(Console *con, CnVariable *cvar, const char *value);

void canard_reset_cvar(Console *con, CnVariable *cvar);

/**
 * Parse the command-line arguments passed to the application's main(), and
 * convert them into console statements. See [TODO] for more details on how the
 * arguments are interpreted.
 * This function should be called after parsing the config, so that any
 * arguments will override the config, or else it will be mostly pointless.
 * Use of this function is entirely optional, if you'd rather parse the args
 * yourself (or not parse them at all), you can simply not call it.
 * @param default_command Optional. The command or variable that will be
 *                        invoked for stray arguments (see [TODO] for
 *                        explanation).
 */
void canard_parse_args(Console *con, int argc, const char **argv,
                       const char *default_command);

/**
 * Define (and possibly create) the default path to use for saving files,
 * such as the config file. If path is NULL, a Unix-style path in the form
 * "$HOME/.<app_name>/" will be attempted to be created.
 * If your application is using SDL, you should call SDL_GetPrefPath() and then
 * pass its result to this function.
 */
bool canard_set_save_path(Console *con, const char *path);

CnNamespace *canard_find_namespace(Console *con, const char *name);
CnObject *canard_find_object(CnNamespace *ns, const char *name);

#endif /* canard_h */
