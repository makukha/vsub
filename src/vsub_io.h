#ifndef VSUB_IO_H
#define VSUB_IO_H

#include "vsub.h"


typedef struct VsubTextSrc {
    const char *name;
    int (*getchar)(void *src);
} VsubTextSrc;

typedef struct VsubVarsSrc {
    const char *name;
    const char *(*getvalue)(void *src, const char *var);
    void *prev;
} VsubVarsSrc;

// input helpers
void vsub_set_tsrc(Vsub *sub, VsubTextSrc *src);
void vsub_add_vsrc(Vsub *sub, VsubVarsSrc *src);


#endif  // VSUB_IO_H