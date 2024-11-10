#include <stdlib.h>
#include <string.h>
#include "vsub.h"
#include "syntax/default/parser.h"


// syntaxes

#define PARSER(pfx) {\
    .create=(void *(*)(void *))pfx##_create,\
    .parse=(int (*)(void *, void *))pfx##_parse,\
    .destroy=(void (*)(void *))pfx##_destroy\
}

const VsubSyntax VSUB_SYNTAXES[] = {
    {0, "default", "simple direct substitution"},
//    {1, "dc243",   "Docker Compose v2.4.3"},
//    {2, "ggenv",   "GNU gettext envsubst"},
};
const VsubParser VSUB_PARSERS[] = {
    PARSER(vsub_sx_default),
    PARSER(vsub_sx_default),  // todo: replace with PARSER(vsub_sx_dc243)
    PARSER(vsub_sx_default),  // todo: replace with PARSER(vsub_sx_ggenv)
};
const size_t VSUB_SYNTAXES_COUNT = sizeof(VSUB_SYNTAXES) / sizeof(VSUB_SYNTAXES[0]);

const VsubSyntax *vsub_syntax_lookup(const char *name) {
    for (size_t i = 0; i < VSUB_SYNTAXES_COUNT; i++) {
        if (strcmp(name, VSUB_SYNTAXES[i].name) == 0) {
            return &VSUB_SYNTAXES[i];
        }
    }
    return NULL;
}


// output formats

const char *VSUB_FORMAT[] = {"plain", "json"};
const size_t VSUB_FORMAT_COUNT = sizeof(VSUB_FORMAT) / sizeof(VSUB_FORMAT[0]);

int vsub_format_lookup(const char *name) {
    for (int i = 0; i < VSUB_FORMAT_COUNT; i++) {
        if (strcmp(name, VSUB_FORMAT[i]) == 0) {
            return i;
        }
    }
    return -1;
}


// memory management

static bool aux_request_resbuf(Auxil *aux, size_t sz) {
    Vsub *sub = aux->sub;
    if (sz <= aux->resz) {
        return true;
    }
    size_t newsz = sz + VSUB_BRES_INC;
    char *newbuf = realloc(aux->resbuf, sz);
    if (!newbuf) {
        sub->err = VSUB_ERROR_MEMORY;
        return false;
    }
    aux->resbuf = newbuf;
    aux->resz = newsz;
    return true;
}

static bool aux_request_errbuf(Auxil *aux, size_t sz) {
    Vsub *sub = aux->sub;
    if (sz <= aux->errz) {
        return true;
    }
    char *newbuf = realloc(aux->errbuf, sz);
    if (!newbuf) {
        sub->err = VSUB_ERROR_MEMORY;
        return false;
    }
    aux->errbuf = newbuf;
    aux->errz = sz;
    return true;
}


// input sources management

void vsub_set_tsrc(Vsub *sub, VsubTextSrc *src) {
    if (sub->tsrc) {
        free(sub->tsrc);
    }
    sub->tsrc = src;
}

void vsub_add_vsrc(Vsub *sub, VsubVarsSrc *src) {
    src->prev = sub->vsrc;
    sub->vsrc = src;
}


// aux syntax api

static int aux_getchar(Auxil *aux) {
    Vsub *sub = aux->sub;
    sub->gcac++;
    if (sub->maxinp == 0 || sub->gcac < sub->maxinp) {
        int c = ((VsubTextSrc*)(sub->tsrc))->getchar(sub->tsrc);
        if (c >= 0) {
            sub->gcbc++;
        }
        return c;
    }
    else {
        sub->trunc = true;
        return -1;
    }
}

static const char *aux_getvalue(Auxil *aux, const char *var) {
    VsubVarsSrc *vsrc = ((Vsub*)(aux->sub))->vsrc;
    while (vsrc) {
        const char *value = vsrc->getvalue(vsrc, var);
        if (value) {
            return value;
        }
        else {
            vsrc = vsrc->prev;
        }
    }
    return NULL;
}

static bool vsub_append(Vsub *sub, int epos, char *str) {
    sub->res = sub->aux.resbuf;  // make non-NULL on first append
    sub->inpc = epos;
    size_t current = sub->resc;
    size_t required = sub->resc + strlen(str);
    size_t allowed = required;
    if (sub->maxres > 0 && sub->maxres < required) {
        allowed = sub->maxres;
    }
    int delta = allowed - current;
    if (allowed < required) {
        sub->trunc = true;
    }
    if (delta <= 0) {
        return false;
    }
    if (!aux_request_resbuf(&(sub->aux), allowed + 1)) {
        return false;
    }
    char *end = stpncpy(sub->res + current, str, delta);
    *end = '\0';
    sub->resc += delta;
    return true;
}

static bool aux_append_orig(Auxil *aux, int epos, char *str) {
    return vsub_append(aux->sub, epos, str);
}

static bool aux_append_subst(Auxil *aux, int epos, char *str) {
    if (!vsub_append(aux->sub, epos, str)) {
        return false;
    }
    ((Vsub *)(aux->sub))->subc++;
    return true;
}

static bool aux_append_error(Auxil *aux, int epos, char *var, char *msg) {
    Vsub *sub = aux->sub;
    sub->errvar = aux->errbuf;  // make non-NULL when error is set
    sub->inpc = epos;
    if (!aux_request_errbuf(aux, strlen(var) + strlen(msg) + 2)) {
        return false;
    }
    sub->err = VSUB_ERROR_VARIABLE;
    sub->errmsg = stpcpy(sub->errvar, var) + 1;  // skip term zero
    strcpy(sub->errmsg, msg);
    return true;
}


// vsub management

static void vsub_prepare_to_run(Vsub *sub) {
    sub->res = NULL;
    sub->err = VSUB_SUCCESS;
    sub->errvar = NULL;
    sub->errmsg = NULL;
    sub->trunc = false;
    sub->gcac = 0;
    sub->gcbc = 0;
    sub->inpc = 0;
    sub->resc = 0;
    sub->subc = 0;
    sub->iterc = 0;
}

void vsub_init(Vsub *sub) {
    // vsub params
    sub->syntax = &VSUB_SYNTAXES[VSUB_SX_DEFAULT];
    sub->depth = 1;
    sub->maxinp = 0;
    sub->maxres = 0;
    // vsub result
    vsub_prepare_to_run(sub);

	// sources
    sub->tsrc = NULL;
    sub->vsrc = NULL;
    // aux
    sub->aux.sub = sub;
    // aux syntax methods
    sub->aux.getchar = (int (*)(void *))aux_getchar;
    sub->aux.getvalue = (const char *(*)(void *, const char *))aux_getvalue;
    sub->aux.append_orig = (bool (*)(void *, int, const char *))aux_append_orig;
    sub->aux.append_subst = (bool (*)(void *, int, const char *))aux_append_subst;
    sub->aux.append_error = (bool (*)(void *, int, const char *, const char *))aux_append_error;
    // data
    sub->aux.resbuf = NULL;
    sub->aux.resz = VSUB_BRES_MIN;
    sub->aux.errbuf = NULL;
    sub->aux.errz = VSUB_BERR_MIN;
    // parser
    sub->aux.parser = NULL;
    sub->aux.pctx = NULL;
}

bool vsub_alloc(Vsub *sub) {
    // result and error buffers
    if (!sub->aux.resbuf) {
        if (!(sub->aux.resbuf = malloc(sub->aux.resz))) {
            sub->err = VSUB_ERROR_MEMORY;
            return false;
        }
    }
    if (!sub->aux.errbuf) {
        if (!(sub->aux.errbuf = malloc(sub->aux.errz))) {
            sub->err = VSUB_ERROR_MEMORY;
            return false;
        }
    }
    // parser
    sub->aux.parser = &VSUB_PARSERS[sub->syntax->id];
    if (!sub->aux.pctx) {
        if (!(sub->aux.pctx = sub->aux.parser->create(&(sub->aux)))) {
            sub->err = VSUB_ERROR_MEMORY;
            return false;
        }
    }
    return true;
}

void vsub_free(Vsub *sub) {
    // result buffer
    free(sub->aux.resbuf);
    sub->res = sub->aux.resbuf = NULL;
    // result error buffer
    free(sub->aux.errbuf);
    sub->errvar = sub->errmsg = sub->aux.errbuf = NULL;
    // input text source
    free(sub->tsrc);
    sub->tsrc = NULL;
    // input vars sources
    VsubVarsSrc *vsrc = sub->vsrc;
    while (vsrc != NULL) {
        VsubVarsSrc *prev = vsrc->prev;
        free(vsrc);
        vsrc = prev;
    }
    sub->vsrc = NULL;
    // parser context
    if (sub->aux.parser) {
        sub->aux.parser->destroy(sub->aux.pctx);
    }
}

bool vsub_run(Vsub *sub) {
    vsub_prepare_to_run(sub);
    // first pass
    int ret = sub->aux.parser->parse(sub->aux.pctx, NULL);
    if (sub->err != VSUB_SUCCESS) {  // failed
        return false;
    }
    else if (ret == 0) {  // all consumed
        return true;
    }
    // second pass
    int resc = sub->resc;
    ret = sub->aux.parser->parse(sub->aux.pctx, NULL);  // second pass
    if (ret > 0 || resc < sub->resc || sub->err != VSUB_SUCCESS) {
        sub->err = VSUB_ERROR_PARSER;
        return false;
    }
    return true;
}
