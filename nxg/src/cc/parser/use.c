#include <nxg/cc/module.h>
#include <nxg/cc/use.h>
#include <nxg/utils/utils.h>
#include <string.h>

bool auto_import(settings_t *settings, expr_t *mod, char *name)
{
	size_t i;

	static char *std_io[] = {"print", "io"};

	static char *std_os[] = {"path", "os"};

	for (i = 0; i < LEN(std_io); i++)
		if (!strcmp(std_io[i], name)) {
			module_std_import(settings, mod, "std/io");
			return true;
		}

	for (i = 0; i < LEN(std_os); i++)
		if (!strcmp(std_os[i], name)) {
			module_std_import(settings, mod, "std/os");
			return true;
		}

	return false;
}
