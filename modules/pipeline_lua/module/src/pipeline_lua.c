/****************************************************************
 *
 *        Copyright 2014, Big Switch Networks, Inc.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 ****************************************************************/

#include <pipeline/pipeline.h>
#include <stdlib.h>

#include <ivs/ivs.h>
#include <loci/loci.h>
#include <OVSDriver/ovsdriver.h>
#include <indigo/indigo.h>
#include <indigo/of_state_manager.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <pthread.h>

#include "pipeline_lua_int.h"

#define AIM_LOG_MODULE_NAME pipeline_lua
#include <AIM/aim_log.h>

AIM_LOG_STRUCT_DEFINE(AIM_LOG_OPTIONS_DEFAULT, AIM_LOG_BITS_DEFAULT, NULL, 0);

static void pipeline_lua_finish(void);

static lua_State *lua;
static pthread_mutex_t lua_lock = PTHREAD_MUTEX_INITIALIZER;

static void
pipeline_lua_init(const char *name)
{
    pipeline_lua_code_gentable_init();

    lua = luaL_newstate();
    if (lua == NULL) {
        AIM_DIE("failed to allocate Lua state");
    }

    luaL_openlibs(lua);
}

static void
pipeline_lua_finish(void)
{
    lua_close(lua);
    lua = NULL;

    pipeline_lua_code_gentable_finish();
}

indigo_error_t
pipeline_lua_process(struct ind_ovs_parsed_key *key,
                     struct xbuf *stats,
                     struct action_context *actx)
{
    pthread_mutex_lock(&lua_lock);

    lua_getglobal(lua, "ingress");

    if (lua_pcall(lua, 0, 0, 0) != 0) {
        AIM_LOG_ERROR("Failed to execute script: %s", lua_tostring(lua, -1));
    }

    pthread_mutex_unlock(&lua_lock);

    return INDIGO_ERROR_NONE;
}

static struct pipeline_ops pipeline_lua_ops = {
    .init = pipeline_lua_init,
    .finish = pipeline_lua_finish,
    .process = pipeline_lua_process,
};

void
pipeline_lua_load_code(const char *filename, const uint8_t *data, uint32_t size)
{
    ind_ovs_fwd_write_lock();

    if (luaL_loadbuffer(lua, (char *)data, size, filename) != 0) {
        AIM_LOG_ERROR("Failed to load code: %s", lua_tostring(lua, -1));
        ind_ovs_fwd_write_unlock();
        return;
    }

    if (lua_pcall(lua, 0, 0, 0) != 0) {
        AIM_LOG_ERROR("Failed to execute code %s: %s", filename, lua_tostring(lua, -1));
    }

    ind_ovs_fwd_write_unlock();
}

void
__pipeline_lua_module_init__(void)
{
    AIM_LOG_STRUCT_REGISTER();
    pipeline_register("lua", &pipeline_lua_ops);
}
