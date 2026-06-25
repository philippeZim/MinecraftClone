// Single translation unit that compiles the sokol header-only libraries.
// Keep this file tiny: it exists only to instantiate the implementations once.
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_time.h"
#define SOKOL_DEBUGTEXT_IMPL
#include "sokol_debugtext.h"
