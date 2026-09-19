#include <kuai/kuai_c/ku_types.h>

extern "C" const char *ku_status_string(ku_status_t status) {
    switch (status) {
#define X(name, value, text) \
    case KU_STATUS_##name:   \
        return text;
        KU_STATUS_DEFS(X)
#undef X
        default:
            return "unknown status";
    }
}
