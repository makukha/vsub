#ifndef VSUB_H
#define VSUB_H

#define VSUB_VERSION "0.2.0"

#ifdef _WIN32
  #define VSUB_EXPORT __declspec(dllexport)
#else
  #define VSUB_EXPORT
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <cjson/cJSON.h>


// --- syntaxes

typedef struct VsubSyntax {
    const int id;
    const char *name;
    const char *title;
} VsubSyntax;

VSUB_EXPORT const VsubSyntax *vsub_FindSyntax(const char *name);  // find by name

#define VSUB_SX_COMPOSE243 0
#define VSUB_SX_ENVSUBST 1

extern const VsubSyntax VSUB_SYNTAXES[];  // using VSUB_SX_* as indexes
extern const size_t VSUB_SYNTAXES_COUNT;


// --- substitution context

typedef struct Vsub {
    // params
    const VsubSyntax *syntax;  // default: VSUB_SX_ENVSUBST
    char depth;     // max subst iter count; default: 1
    size_t maxinp;  // max length of input string; unlimited if set to 0
    size_t maxres;  // max length of result string; unlimited if set to 0
    // result
    char *res;      // result string
    int err;        // see error/success flags
    char *errvar;   // first variable name with error; default: NULL
    char *errmsg;   // variable error message; default: NULL
    bool trunc;     // whether result string was truncated because of maxinp or maxres
    size_t gcac;    // input bytes requested
    size_t gcbc;    // input bytes returned other than EOF
    size_t inpc;    // parsed input length; also serves as error location
    size_t resc;    // actual length of result str
    size_t subc;    // count of total substitutions made
    char iterc;     // count of subst iterations actually performed
    // internal data
    void *tsrc;
    void *vsrc;
    void *aux;
} Vsub;

VSUB_EXPORT bool vsub_init(Vsub *sub);
VSUB_EXPORT bool vsub_alloc(Vsub *sub);
VSUB_EXPORT bool vsub_run(Vsub *sub);
VSUB_EXPORT cJSON *vsub_results(const Vsub *sub, bool include_details);
VSUB_EXPORT void vsub_free(Vsub *sub);


// --- input sources

VSUB_EXPORT bool vsub_UseTextFromFile(Vsub *sub, FILE *fp);
VSUB_EXPORT bool vsub_UseTextFromStr(Vsub *sub, const char *s);

VSUB_EXPORT bool vsub_UseVarsFromArrays(Vsub *sub, size_t c, const char *k[], const char *v[]);
VSUB_EXPORT bool vsub_UseVarsFromEnv(Vsub *sub);
VSUB_EXPORT bool vsub_UseVarsFromKvarray(Vsub *sub, size_t c, const char *kv[]);


// --- output formats

VSUB_EXPORT int vsub_FindFormat(const char *name);
VSUB_EXPORT int vsub_OutputPlain(Vsub *sub, FILE *fp);
VSUB_EXPORT int vsub_OutputJson(Vsub *sub, FILE *fp, bool detailed);
VSUB_EXPORT int vsub_OutputPretty(Vsub *sub, FILE *fp, bool result, bool detailed);

#define VSUB_FMT_PLAIN 0
#define VSUB_FMT_JSON 1
#define VSUB_FMT_PRETTY 2

extern const char *VSUB_FORMAT[];  // using VSUB_FMT_* above as indexes
extern const size_t VSUB_FORMAT_COUNT;


// --- error handling

#define vsub_ErrMsg(NAME) VSUB_ERRORS[-VSUB_ERR_##NAME]

#define VSUB_SUCCESS 0
#define VSUB_ERR_FILE_READ -1  // match EOF
#define VSUB_ERR_FILE_WRITE -2
#define VSUB_ERR_FILE_OPEN -3
#define VSUB_ERR_MEMORY -4
#define VSUB_ERR_SYNTAX -5
#define VSUB_ERR_VARIABLE -6
#define VSUB_ERR_PARSER -7
#define VSUB_ERR_OUTPUT -8
#define VSUB_ERR_UNKNOWN -9

extern const char* VSUB_ERRORS[];  // using (- VSUB_ERR_*) above as indexes


#endif  // VSUB_H
