/*
 *  conf_ini.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 02/10/08.
 *  Copyright (c) 2008 Michel DEPEIGE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 */

#define _GNU_SOURCE
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "conf.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_LIBLUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif
#include "main.h"

#ifdef HAVE_LIBLUA
/* prototypes */
int	conf_lua_get_data(lua_State *L, char *name, hash_t *hash);
int	conf_lua_get_globals(lua_State *L, hash_t *hash);
int	conf_lua_get_section(lua_State *L, char *name, hash_t *hash);

/*
 * load a lua file
 * attenpt to execute it, then load know global vars and know global tables
 */

int	conf_lua_read(char *file, conf_t *conf)
{
	lua_State *L;

	L = lua_open();
	luaL_openlibs(L);

	if (luaL_loadfile(L, file) || lua_pcall(L, 0, 0, 0)) {
		log_err("[c] cannot run lua configuration file: %s\n",
		                 lua_tostring(L, -1));
		lua_close(L);
		return ERROR;
	}

	conf_lua_get_globals(L, conf->global);
	conf_lua_get_section(L, "cmd", conf->cmd);
	conf_lua_get_section(L, "forward", conf->forward);
	conf_lua_get_section(L, "transmit", conf->transmit);
	if (g_mode & DEBUG) {
		fprintf(stdout, "[c] lua configuration dump below:\n");
		conf_dump(conf);
	}

	lua_close(L);
	return 0;
}

/* load a value for a specified variable, string or table of strings */

int	conf_lua_get_data(lua_State *L, char *name, hash_t *hash)
{
	lua_getglobal(L, name);
	/* this look like a string */
	if (! lua_isnil(L, -1) && lua_tostring(L, -1) != NULL) {
		hash_text_insert(hash, name, (char *)lua_tostring(L, -1));
	}
	/* this look like an array of strings */
	else if (! lua_isnil(L, -1) && lua_istable(L, -1)) {
		lua_pushnil(L); // first key
		while (lua_next(L, -2) != 0) {
        		/* 'key' is at index -2 and 'value' at index -1 */
			hash_text_insert(hash, name, (char *)lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
	              	lua_pop(L, 1);
	        }
	   	lua_pop(L,2);
	}
	lua_remove(L, -1);
	return 0;
}

/* load know global variables */

int	conf_lua_get_globals(lua_State *L, hash_t *hash)
{
	char	*keys[] = {
		"allow_direct", "allow_private_cmd", "bitlbee", "channels", "color",
		"nick",	"perform", "offset", "pass", "port", "quit_message", "realname",
		"server", "ssl", "sync_message", "sync_notice", "syslog", "throttling",
		"hooks", "syslog_listener", "syslog_port", "syslog_silent_drop",
		"udp_key", "udp_listener", "udp_port", "unix_listener", "unix_path",
		"username", NULL
	};
	unsigned short	i = 0;

	while (keys[i] != NULL) {
		conf_lua_get_data(L, keys[i], hash);
		i++;
	}
	return 0;
}

/*
 * load a "section" int he configuration file
 * it's equivalent to each section the INI format, and is an array of strings
 */

int	conf_lua_get_section(lua_State *L, char *section, hash_t *hash)
{
	char	*tmp;

	lua_getglobal(L, section);
	if (lua_isnil(L, -1) || ! lua_istable(L, -1))
		return -1;

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (lua_tostring(L, -1)) {
			hash_text_insert(hash, (char *)lua_tostring(L, -2),
					 (char *)lua_tostring(L, -1));
			lua_pop(L, 1);
		}
		else if (lua_istable(L, -1)) {
			tmp = (char *)lua_tostring(L, -2);
			lua_pushnil(L);
			while (! lua_isnil(L, -2) && lua_next(L, -2) != 0) {
				hash_text_insert(hash, tmp, (char *)lua_tostring(L, -1));
				lua_pop(L, 1);
			}
			lua_pop(L,1);
		}
	}
	lua_pop(L,2);
	return 0;
}

#endif
