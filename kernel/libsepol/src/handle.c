// #include <stdlib.h>
// #include <assert.h>
#include "handle.h"
#include "debug.h"

sepol_handle_t *sepol_handle_create(void)
{

	sepol_handle_t *sh = malloc(sizeof(sepol_handle_t));
	if (sh == NULL)
		return NULL;

	/* Set callback */
	sh->msg_callback = sepol_msg_default_handler;
	sh->msg_callback_arg = NULL;

	/* by default do not disable dontaudits */
	sh->disable_dontaudit = 0;
	sh->expand_consume_base = 0;

	/* by default needless unused branch of tunables would be discarded  */
	sh->preserve_tunables = 0;

	return sh;
}

int sepol_get_preserve_tunables(sepol_handle_t *sh)
{
	assert(sh != NULL);
	return sh->preserve_tunables;
}

void sepol_set_preserve_tunables(sepol_handle_t * sh, int preserve_tunables)
{
	assert(sh !=NULL);
	sh->preserve_tunables = preserve_tunables;
}

int sepol_get_disable_dontaudit(sepol_handle_t *sh)
{
	assert(sh !=NULL);
	return sh->disable_dontaudit;
}

void sepol_set_disable_dontaudit(sepol_handle_t * sh, int disable_dontaudit)
{
	assert(sh !=NULL);
	sh->disable_dontaudit = disable_dontaudit;
}

void sepol_set_expand_consume_base(sepol_handle_t *sh, int consume_base)
{
	assert(sh != NULL);
	sh->expand_consume_base = consume_base;
}

void sepol_handle_destroy(sepol_handle_t * sh)
{
	free(sh);
}
