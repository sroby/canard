#include <stdio.h>
#include <string.h>

#include "../src/canard.h"

typedef struct DemoHandler {
    int serves_nothing;
} DemoHandler;

bool callback_dummy(void *handler, Console *con, CVarValue value) {
    if (value > 0) {
        sprintf(con->output, "dummy is positive!");
    } else if (value < 0) {
        sprintf(con->output, "dummy is negative!");
    } else {
        sprintf(con->output, "dummy is zero!");
    }
    return true;
}

int main(int argc, const char *argv[]) {
    Console con;
    canard_init(&con, "canard_demo");
    
    Namespace *ns = canard_create_namespace(&con, "demo");
    canard_create_variable(ns, "dummy", CVAR_INT, callback_dummy,
                           "Doesn't do anything");
    
    canard_parse_args(&con, argc, argv);
    
    DemoHandler handler;
    memset(handler, 0, sizeof(DemoHandler));
    canard_namespace_set_handler(ns, &handler);

    canard_teardown(&con);
    return 0;
}
