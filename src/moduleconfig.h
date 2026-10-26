#ifndef __MODULECONFIG_H__
#define __MODULECONFIG_H__

#define MODULE_NAME "Steam Owner"
#define MODULE_VERSION "1.1.0"
#define MODULE_AUTHOR "Soul"
#define MODULE_URL ""
#define MODULE_LOGTAG "STEAMOWNER"
#define MODULE_LIBRARY "steam_owner"
#define MODULE_LIBCLASS ""

#ifdef __DATE__
#define MODULE_DATE __DATE__
#else
#define MODULE_DATE "Unknown"
#endif

#define FN_AMXX_ATTACH OnAmxxAttach
#define FN_AMXX_DETACH OnAmxxDetach
#define FN_AMXX_PLUGINSLOADED OnPluginsLoaded
#define FN_AMXX_PLUGINSUNLOADED OnPluginsUnloaded

#endif // __MODULECONFIG_H__
