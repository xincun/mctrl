/*
 * Copyright (c) 2009 Martin Mitas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "module.h"



/**************************
 *** Module abstraction ***
 **************************/
 
static CRITICAL_SECTION mod_lock;

void
module_init(void)
{
    InitializeCriticalSection(&mod_lock);
}

void
module_fini(void)
{
    DeleteCriticalSection(&mod_lock);
}


/* Module structure */
typedef struct module_tag module_t;
struct module_tag {
#ifdef DEBUG
    const char* name;
#endif
    int (*fn_init)(void);    /* init function */
    void (*fn_fini)(void);   /* cleaning function */
    UINT refs;               /* number of initializations */
};


static int
module_init_modules(module_t** modules, int n)
{
    int res = 0;
    int i = 0;

    EnterCriticalSection(&mod_lock);
    while(i < n) {
        if(modules[i]->refs == 0) {
            res = modules[i]->fn_init();
            if(MC_ERR(res != 0)) {
                MC_TRACE("module_init: %s_init() failed.", modules[i]->name);
                /* Rollback previous initializations */
                while(--i >= 0) {
                    modules[i]->refs--;
                    if(modules[i]->refs == 0)
                        modules[i]->fn_fini();
                }                
                break;
            }
        }
        modules[i]->refs++;
        i++;
    }
    LeaveCriticalSection(&mod_lock);
    return res;
}

static void
module_fini_modules(module_t** modules, int n)
{
    int i;

    EnterCriticalSection(&mod_lock);
    for(i = n-1; i >= 0; i--) {
        modules[i]->refs--;
        if(modules[i]->refs == 0)
            modules[i]->fn_fini();
    }
    LeaveCriticalSection(&mod_lock);
}



/**************************************
 *** Macros for module declarations ***
 **************************************/

/* Declaration of a module */
#ifdef DEBUG
    #define DEFINE_MODULE(mod_name)                                           \
        static module_t mod_##mod_name = { #mod_name, mod_name##_init, mod_name##_fini, 0 };
#else
    #define DEFINE_MODULE(mod_name)                                           \
        static module_t mod_##mod_name = { mod_name##_init, mod_name##_fini, 0 };
#endif

/* Macro implementing public interface for public modules */
#define DEFINE_PUBLIC_IFACE(mod_name, PublicName, deps)                       \
    BOOL MCTRL_API                                                            \
    mc##PublicName##_Initialize(void)                                         \
    {                                                                         \
        if(MC_ERR(module_init_modules(deps, MC_ARRAY_SIZE(deps)) != 0)) {     \
            MC_TRACE("mc%s_Initialize: module_init_modules() failed.",        \
                     MC_STRINGIZE(PublicName));                               \
            return FALSE;                                                     \
        }                                                                     \
        return TRUE;                                                          \
    }                                                                         \
                                                                              \
    void MCTRL_API                                                            \
    mc##PublicName##_Terminate(void)                                          \
    {                                                                         \
        module_fini_modules(deps, MC_ARRAY_SIZE(deps));                       \
    }


/*******************************
 *** The module declarations ***
 *******************************/
   
#include "misc.h"
#include "theme.h"
#include "button.h"
#include "html.h"
#include "mditab.h"
   
DEFINE_MODULE(mc)
DEFINE_MODULE(theme)
DEFINE_MODULE(button)
DEFINE_MODULE(html)
DEFINE_MODULE(mditab)

/* Dependecies for public modules */
static module_t* mod_button_deps[] = { &mod_mc, &mod_theme, &mod_button };
static module_t* mod_html_deps[] =   { &mod_mc, &mod_theme, &mod_html };
static module_t* mod_mditab_deps[] = { &mod_mc, &mod_theme, &mod_mditab };

/* Public modules interfaces */
DEFINE_PUBLIC_IFACE(button, Button, mod_button_deps)
DEFINE_PUBLIC_IFACE(html, Html, mod_html_deps)
DEFINE_PUBLIC_IFACE(mditab, Mditab, mod_mditab_deps)