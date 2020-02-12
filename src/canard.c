#include <stdlib.h>
#include <string.h>

#include "canard.h"

const char *cvar_type_names[] = {"boolean", "integer", "string"};

// MEMORY UTILITIES //

static void *malloc_zeroed(size_t size) {
    void *ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

static char *save_path_filename(Console *con, const char *fn) {
    CnVariable *cvar = &(canard_find_object(con->nss, "save_path")->sub.var);
    const char *save_path = canard_get_cvar_str(cvar);
    char *full_fn = malloc(strlen(save_path) + strlen(fn) + 1);
    strcpy(full_fn, save_path);
    strcat(full_fn, "/");
    strcat(full_fn, fn);
    return full_fn;
}

static void print_value(FILE *f, CnVarType type, CnVarValue *value) {
    switch (type) {
        case CVAR_BOOL:
            fprintf(f, "%s", (value->b_val ? "true" : "false"));
            break;
        case CVAR_INT:
            fprintf(f, "%d", (value->i_val));
            break;
        case CVAR_STRING:
            fprintf(f, "%s", (value->str));
            break;
    }
}

static void repr_value(FILE *f, CnVarType type, CnVarValue *value) {
    switch (type) {
        case CVAR_BOOL:
            fprintf(f, "%s", (value->b_val ? "1" : "0"));
            break;
        case CVAR_INT:
            fprintf(f, "%d", (value->i_val));
            break;
        case CVAR_STRING:
            fprintf(f, "\"%s\"", (value->str));
            break;
    }
}

static void describe_object(Console *con, CnNamespace *ns, CnObject *obj) {
    if (!ns || !obj) {
        return;
    }
    fprintf(con->output, "%s.%s", ns->name, obj->name);
    switch (obj->type) {
        case COBJ_CMD:
            fprintf(con->output, " %s\n", obj->description);
            break;
        case COBJ_VAR:
            fprintf(con->output, ": %s\nDefault: ",
                    cvar_type_names[obj->sub.var.type]);
            print_value(con->output, obj->sub.var.type,
                        &obj->sub.var.default_value);
            fprintf(con->output, "\nCurrent: ");
            print_value(con->output, obj->sub.var.type,
                        &obj->sub.var.value);
            fprintf(con->output, "\n%s\n", obj->description);
            break;
    }
}

static CnObject *resolve_object_name(Console *con, CnNamespace **return_ns,
                                     const char *name) {
    CnNamespace *ns = NULL;
    CnObject *obj = NULL;
    if (name) {
        char *s_name = strdup(name);
        char *post_dot = s_name;
        strsep(&post_dot, ".");
        if (post_dot) {
            ns = canard_find_namespace(con, s_name);
            if (ns) {
                obj = canard_find_object(ns, post_dot);
                if (!obj) {
                    fprintf(con->output, "%s: No such command or variable in "
                                         "namespace \"%s\"\n",
                            post_dot, s_name);
                }
            } else {
                fprintf(con->output, "%s: No such namespace\n", s_name);
            }
        } else {
            CnNamespace *matches[CANARD_MAX_NAMESPACES];
            int n_matches = 0;
            CnObject *candidate = NULL;
            for (int i = 0; i < CANARD_MAX_NAMESPACES; i++) {
                CnNamespace *candidate_ns = con->nss + i;
                if (!strcmp(candidate_ns->name, name)) {
                    fprintf(con->output, "%s: namespace", name);
                    const char *labels[] = {"Commands", "Variables"};
                    for (int j = 0; j < 2; j++) {
                        fprintf(con->output, "\n\t%s:", labels[j]);
                        bool none = true;
                        for (int k = 0; k < candidate_ns->t_objs; k++) {
                            CnObject *obj = candidate_ns->objs + k;
                            if (!obj->name) {
                                break;
                            }
                            if (obj->type == j) {
                                fprintf(con->output, " %s", obj->name);
                                none = false;
                            }
                        }
                        if (none) {
                            fprintf(con->output, " (none)");
                        }
                    }
                    fprintf(con->output, "\n");
                    n_matches = -1;
                    ns = candidate_ns;
                    break;
                }
                CnObject *match = canard_find_object(candidate_ns, name);
                if (match) {
                    candidate = match;
                    matches[n_matches++] = candidate_ns;
                }
            }
            switch (n_matches) {
                case -1:
                    break;
                case 0:
                    fprintf(con->output, "%s: No such command or variable\n",
                            name);
                    break;
                case 1:
                    ns = matches[0];
                    obj = candidate;
                    break;
                default:
                    fprintf(con->output,
                            "%s: Name is ambiguous for %d namespaces:\n",
                            name, n_matches);
                    for (int i = 0; i < n_matches; i++) {
                        fprintf(con->output, "\t%s.%s\n",
                                matches[i]->name, name);
                    }
            }
        }
        free(s_name);
    }
    if (return_ns) {
        *return_ns = ns;
    }
    return obj;
}

static bool var_is_changed(CnVariable *cvar) {
    switch (cvar->type) {
        case CVAR_BOOL:
            return cvar->default_value.b_val != cvar->value.b_val;
        case CVAR_INT:
            return cvar->default_value.i_val != cvar->value.i_val;
        case CVAR_STRING:
            return strcmp(cvar->default_value.str, cvar->value.str);
    }
    return false;
}

static void handle_cvar_change(Console *con, CnObject *obj) {
    if (obj->sub.var.func) {
        (*obj->sub.var.func)(obj->ns->handler, con, &obj->sub.var.value);
    }
}

// BUILT-IN COMMANDS //

static bool cmd_help(void *handler, Console *con, const CnStatement *stat) {
    if (stat->argc <= 1) {
        fprintf(con->output, "Available namespaces:");
        for (int i = 0; i < CANARD_MAX_NAMESPACES; i++) {
            if (!con->nss[i].name) {
                break;
            }
            fprintf(con->output, " %s", con->nss[i].name);
        }
        fprintf(con->output, "\n");
    } else {
        for (int i = 1; i < stat->argc; i++) {
            CnNamespace *ns = NULL;
            CnObject *obj = resolve_object_name(con, &ns, stat->argv[i]);
            if (obj) {
                describe_object(con, ns, obj);
            }
        }
    }
    return true;
}

static bool cmd_load(void *handler, Console *con, const CnStatement *stat) {
    if (stat->argc <= 1) {
        return false;
    }
    for (int i = 1; i < stat->argc; i++) {
        char *full_fn = save_path_filename(con, stat->argv[i]);
        FILE *rc = fopen(full_fn, "r");
        if (rc) {
            
            fclose(rc);
        } else {
            fprintf(con->output, "%s: Failed to open file for reading\n",
                    full_fn);
        }
        free(full_fn);
    }
    return true;
}

static bool cmd_save(void *handler, Console *con, const CnStatement *stat) {
    if (stat->argc != 2) {
        return false;
    }
    const char *save_path = canard_find_object(con->nss,
                                               "save_path")->sub.var.value.str;
    char full_path[strlen(save_path) + strlen(stat->argv[1]) + 1];
    strcpy(full_path, save_path);
    strcat(full_path, "/");
    strcat(full_path, stat->argv[1]);
    FILE *f = fopen(full_path, "w");
    if (f) {
        for (int i = 0; i < CANARD_MAX_NAMESPACES; i++) {
            CnNamespace *ns = con->nss + i;
            if (!ns->name) {
                break;
            }
            for (int j = 0; j < ns->t_objs; j++) {
                CnObject *obj = ns->objs + j;
                if (obj->type != COBJ_VAR) {
                    continue;
                }
                if (!var_is_changed(&obj->sub.var)) {
                    continue;
                }
                fprintf(f, "%s.%s ", ns->name, obj->name);
                repr_value(f, obj->sub.var.type, &obj->sub.var.value);
                fprintf(f, "\n");
            }
        }
        fclose(f);
    } else {
        fprintf(con->output, "%s: Failed to open file for writing\n",
                stat->argv[1]);
    }
    return true;
}

// BUILT-IN OBJECTS //

const CnCmdDecl builtin_cmds[] = {
    {"help", cmd_help,
        "<cmd-or-cvar...>\n"
        "Display description of a given command, variable or namespace.\n"
        "With no arguments, display list of all available namespaces."},
    {"load", cmd_load,
        "<filenames...>\n"
        "Open a file and parse each line as a console statement."},
    {"save", cmd_save,
        "<filename>\n"
        "Write console statements of all modified variables to a file."},
    END_CMD_DECL
};

const CnVarDecl builtin_vars[] = {
    {"save_path", NULL, CVAR_STRING, NULL,
        "Defines the application's main directory for storing settings"},
    END_VAR_DECL
};

// PUBLIC FUNCTIONS //

void canard_init(Console *con, const char *app_name) {
    memset(con, 0, sizeof(Console));
    
    if (!app_name) {
        app_name = "canard";
    }
    con->app_name = app_name;
    
    con->output = stdout;
    
    CnNamespace *ns = canard_create_namespace(con, "console", builtin_cmds,
                                              builtin_vars);
    canard_namespace_set_handler(ns, con);
}

void canard_teardown(Console *con) {
    // Free all string cvars
    for (int i = 0; i < CANARD_MAX_NAMESPACES; i++) {
        CnNamespace *ns = con->nss + i;
        if (!ns->name) {
            break;
        }
        for (int j = 0; j < ns->t_objs; j++) {
            CnObject *obj = ns->objs + j;
            if (obj->type == COBJ_VAR && obj->sub.var.type == CVAR_STRING) {
                free(ns->objs->sub.var.default_value.str);
                free(ns->objs->sub.var.value.str);
            }
        }
    }
}

CnNamespace *canard_create_namespace(Console *con, const char *name,
                                     const CnCmdDecl *cmds,
                                     const CnVarDecl *vars) {
    if (!name) {
        return NULL;
    }
    
    // Count total sum of commands and variables
    int total = 0;
    if (cmds) {
        const CnCmdDecl *decl = cmds;
        while (decl->name) {
            total++;
            decl++;
        }
    }
    if (vars) {
        const CnVarDecl *decl = vars;
        while (decl->name) {
            total++;
            decl++;
        }
    }
    if (!total) {
        return NULL;
    }
        
    for (int i = 0; i < CANARD_MAX_NAMESPACES; i++) {
        CnNamespace *ns = con->nss + i;
        if (!ns->name) {
            ns->name = name;
            ns->t_objs = total;
            ns->objs = malloc_zeroed(sizeof(CnObject) * total);
            CnObject *obj = ns->objs;
            if (vars) {
                const CnVarDecl *decl = vars;
                while (decl->name) {
                    obj->name = decl->name;
                    obj->description = (decl->description ? decl->description :
                                        "No help available");
                    obj->type = COBJ_VAR;
                    obj->sub.var.func = decl->func;
                    obj->sub.var.type = decl->type;
                    obj->ns = ns;
                    if (decl->default_value) {
                        switch (decl->type) {
                            case CVAR_BOOL:
                                obj->sub.var.default_value.b_val =
                                    *(bool *)decl->default_value;
                                break;
                            case CVAR_INT:
                                obj->sub.var.default_value.i_val =
                                    *(int *)decl->default_value;
                                break;
                            case CVAR_STRING:
                                obj->sub.var.default_value.str =
                                    strdup((char *)decl->default_value);
                                obj->sub.var.value.str =
                                    strdup((char *)decl->default_value);
                                break;
                        }
                        if (decl->type != CVAR_STRING) {
                            obj->sub.var.value = obj->sub.var.default_value;
                        }
                    }
                    obj++;
                    decl++;
                }
            }
            if (cmds) {
                const CnCmdDecl *decl = cmds;
                while (decl->name) {
                    obj->name = decl->name;
                    obj->description = decl->description;
                    obj->type = COBJ_CMD;
                    obj->sub.cmd.func = decl->func;
                    obj++;
                    decl++;
                }
            }
            return ns;
        }
        if (!strcmp(ns->name, name)) {
            break;
        }
    }
    return NULL;
}

void canard_namespace_set_handler(CnNamespace *ns, void *handler) {
    ns->handler = handler;
}

void canard_exec(Console *con, const char *cmdline) {
    CnStatement stat;
    strncpy(stat.raw, cmdline, CANARD_MAX_CMDLINE);
    stat.raw[CANARD_MAX_CMDLINE - 1] = 0;
    char *sep = stat.raw;
    do {
        char *arg = strsep(&sep, " ");
        if (!sep) {
            break;
        }
        if (*arg) {
            stat.argv[stat.argc++] = arg;
        }
    } while (stat.argc < CANARD_MAX_ARGS);
    if (stat.argc) {
        CnObject *obj = resolve_object_name(con, stat.argv[0], NULL);
        if (obj) {
            switch (obj->type) {
                case COBJ_CMD:
                    break;
                case COBJ_VAR:
                    if (stat.argc > 2) {
                        describe_object(con, obj);
                    }
                    break;
            }
        }
    }
}

bool canard_get_cvar_bool(CnVariable *cvar) {
    return cvar->value.b_val;
}

void canard_set_cvar_bool(Console *con, CnVariable *cvar, bool value) {
    if (cvar->type != CVAR_BOOL) {
        return;
    }
    if (cvar->value.b_val == value) {
        return;
    }
    cvar->value.b_val = value;
    handle_cvar_change(con, cvar);
}

const char *canard_get_cvar_str(CnVariable *cvar) {
    return cvar->value.str;
}

void canard_set_cvar_str(Console *con, CnVariable *cvar, const char *value) {
    if (cvar->type != CVAR_STRING) {
        return;
    }
    if (!strcmp(cvar->value.str, value)) {
        return;
    }
    
}

void canard_reset_cvar(Console *con, CnVariable *cvar) {
    switch (cvar->type) {
        case CVAR_BOOL:
            canard_set_cvar_bool(con, cvar, cvar->default_value.b_val);
            break;
        case CVAR_INT:
            canard_set_cvar_int(con, cvar, cvar->default_value.i_val);
            break;
        case CVAR_STRING:
            canard_set_cvar_str(con, cvar, cvar->default_value.str);
            break;
    }
}

void canard_parse_args(Console *con, int argc, const char **argv,
                       const char *default_command) {
    bool post_dash = false;
    char cmdline[CANARD_MAX_CMDLINE] = "";
    char cmdline_stray[CANARD_MAX_CMDLINE];
    strlcpy(cmdline_stray, default_command, CANARD_MAX_CMDLINE);
    for (int i = 0; i < argc; i++) {
        const char *arg = argv[i];
        bool dashed = false;
        bool stat_end = false;
        const char *new_cmd = NULL;
        if (!post_dash) {
            while (arg[0] == '-') {
                dashed = true;
                arg++;
            }
        }
        if (strlen(arg)) {
            if (dashed) {
                new_cmd = cmdline;
                stat_end = true;
            } else {
                char *cmdline_cat = (strlen(cmdline) ? cmdline : cmdline_stray);
                strlcat(cmdline_cat, " ", CANARD_MAX_CMDLINE);
                strlcat(cmdline_cat, arg, CANARD_MAX_CMDLINE);
            }
        } else if (dashed) {
            post_dash = true;
            stat_end = true;
        }
        if (stat_end) {
            if (strlen(cmdline)) {
                canard_exec(con, cmdline);
            }
            if (new_cmd) {
                strlcpy(cmdline, new_cmd, CANARD_MAX_CMDLINE);
            } else {
                cmdline[0] = 0;
            }
        }
    }
    if (strlen(cmdline_stray) > strlen(default_command)) {
        canard_exec(con, cmdline_stray);
    }
}

bool canard_set_save_path(Console *con, const char *path) {
    if (path) {
        con->save_path = strdup(path);
    } else {
        
    }
    return true;
}

CnNamespace *canard_find_namespace(Console *con, const char *name) {
    if (name) {
        for (int i = 0; i < CANARD_MAX_NAMESPACES; i++) {
            CnNamespace *ns = con->nss + i;
            if (!ns->name) {
                break;
            }
            if (!strcmp(name, ns->name)) {
                return ns;
            }
        }
    }
    return NULL;
}

CnObject *canard_find_object(CnNamespace *ns, const char *name) {
    if (name) {
        for (int i = 0; i < ns->t_objs; i++) {
            CnObject *obj = ns->objs + i;
            if (!obj->name) {
                break;
            }
            if (!strcmp(name, obj->name)) {
                return obj;
            }
        }
    }
    return NULL;
}
