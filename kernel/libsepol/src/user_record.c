// #include <errno.h>
// #include <stdlib.h>
#include <linux/string.h>

#include "user_internal.h"
#include "debug.h"
#include "private.h"

struct sepol_user {
	/* This user's name */
	char *name;

	/* This user's mls level (only required for mls) */
	char *mls_level;

	/* This user's mls range (only required for mls) */
	char *mls_range;

	/* The role array */
	char **roles;

	/* The number of roles */
	unsigned int num_roles;
};

struct sepol_user_key {
	/* This user's name */
	char *name;
};

int sepol_user_key_create(sepol_handle_t * handle,
			  const char *name, sepol_user_key_t ** key_ptr)
{

	sepol_user_key_t *tmp_key =
	    (sepol_user_key_t *) malloc(sizeof(sepol_user_key_t));

	if (!tmp_key) {
		ERR(handle, "out of memory, "
		    "could not create selinux user key");
		return STATUS_ERR;
	}

	tmp_key->name = strdup(name);
	if (!tmp_key->name) {
		ERR(handle, "out of memory, could not create selinux user key");
		free(tmp_key);
		return STATUS_ERR;
	}

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;
}


void sepol_user_key_unpack(const sepol_user_key_t * key, const char **name)
{

	*name = key->name;
}


int sepol_user_key_extract(sepol_handle_t * handle,
			   const sepol_user_t * user,
			   sepol_user_key_t ** key_ptr)
{

	if (sepol_user_key_create(handle, user->name, key_ptr) < 0) {
		ERR(handle, "could not extract key from user %s", user->name);
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

void sepol_user_key_free(sepol_user_key_t * key)
{
	if (!key)
		return;
	free(key->name);
	free(key);
}

int sepol_user_compare(const sepol_user_t * user, const sepol_user_key_t * key)
{

	return strcmp(user->name, key->name);
}

int sepol_user_compare2(const sepol_user_t * user, const sepol_user_t * user2)
{

	return strcmp(user->name, user2->name);
}

/* Name */
const char *sepol_user_get_name(const sepol_user_t * user)
{

	return user->name;
}

int sepol_user_set_name(sepol_handle_t * handle,
			sepol_user_t * user, const char *name)
{

	char *tmp_name = strdup(name);
	if (!tmp_name) {
		ERR(handle, "out of memory, could not set name");
		return STATUS_ERR;
	}
	free(user->name);
	user->name = tmp_name;
	return STATUS_SUCCESS;
}


/* MLS */
const char *sepol_user_get_mlslevel(const sepol_user_t * user)
{

	return user->mls_level;
}


int sepol_user_set_mlslevel(sepol_handle_t * handle,
			    sepol_user_t * user, const char *mls_level)
{

	char *tmp_mls_level = strdup(mls_level);
	if (!tmp_mls_level) {
		ERR(handle, "out of memory, "
		    "could not set MLS default level");
		return STATUS_ERR;
	}
	free(user->mls_level);
	user->mls_level = tmp_mls_level;
	return STATUS_SUCCESS;
}


const char *sepol_user_get_mlsrange(const sepol_user_t * user)
{

	return user->mls_range;
}


int sepol_user_set_mlsrange(sepol_handle_t * handle,
			    sepol_user_t * user, const char *mls_range)
{

	char *tmp_mls_range = strdup(mls_range);
	if (!tmp_mls_range) {
		ERR(handle, "out of memory, "
		    "could not set MLS allowed range");
		return STATUS_ERR;
	}
	free(user->mls_range);
	user->mls_range = tmp_mls_range;
	return STATUS_SUCCESS;
}


/* Roles */
int sepol_user_get_num_roles(const sepol_user_t * user)
{

	return user->num_roles;
}

int sepol_user_add_role(sepol_handle_t * handle,
			sepol_user_t * user, const char *role)
{

	char *role_cp;
	char **roles_realloc = NULL;

	if (sepol_user_has_role(user, role))
		return STATUS_SUCCESS;

	role_cp = strdup(role);
	if (!role_cp)
		goto omem;

	roles_realloc = reallocarray(user->roles,
				     user->num_roles + 1,
				     sizeof(char *));
	if (!roles_realloc)
		goto omem;

	user->num_roles++;
	user->roles = roles_realloc;
	user->roles[user->num_roles - 1] = role_cp;

	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory, could not add role %s", role);
	free(role_cp);
	free(roles_realloc);
	return STATUS_ERR;
}


int sepol_user_has_role(const sepol_user_t * user, const char *role)
{

	unsigned int i;

	for (i = 0; i < user->num_roles; i++)
		if (!strcmp(user->roles[i], role))
			return 1;
	return 0;
}


int sepol_user_set_roles(sepol_handle_t * handle,
			 sepol_user_t * user,
			 const char **roles_arr, unsigned int num_roles)
{

	unsigned int i;
	char **tmp_roles = NULL;

	if (num_roles > 0) {

		/* First, make a copy */
		tmp_roles = (char **)calloc(1, sizeof(char *) * num_roles);
		if (!tmp_roles)
			goto omem;

		for (i = 0; i < num_roles; i++) {
			tmp_roles[i] = strdup(roles_arr[i]);
			if (!tmp_roles[i])
				goto omem;
		}
	}

	/* Apply other changes */
	for (i = 0; i < user->num_roles; i++)
		free(user->roles[i]);
	free(user->roles);
	user->roles = tmp_roles;
	user->num_roles = num_roles;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory, could not allocate roles array for"
	    "user %s", user->name);

	if (tmp_roles) {
		for (i = 0; i < num_roles; i++) {
			if (!tmp_roles[i])
				break;
			free(tmp_roles[i]);
		}
	}
	free(tmp_roles);
	return STATUS_ERR;
}

int sepol_user_get_roles(sepol_handle_t * handle,
			 const sepol_user_t * user,
			 const char ***roles_arr, unsigned int *num_roles)
{

	unsigned int i;
	const char **tmp_roles =
	    (const char **)calloc(user->num_roles, sizeof(char *));
	if (!tmp_roles)
		goto omem;

	for (i = 0; i < user->num_roles; i++)
		tmp_roles[i] = user->roles[i];

	*roles_arr = tmp_roles;
	*num_roles = user->num_roles;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory, could not "
	    "allocate roles array for user %s", user->name);
	free(tmp_roles);
	return STATUS_ERR;
}


void sepol_user_del_role(sepol_user_t * user, const char *role)
{

	unsigned int i;
	for (i = 0; i < user->num_roles; i++) {
		if (!strcmp(user->roles[i], role)) {
			free(user->roles[i]);
			user->roles[i] = NULL;
			user->roles[i] = user->roles[user->num_roles - 1];
			user->num_roles--;
		}
	}
}

/* Create */
int sepol_user_create(sepol_handle_t * handle, sepol_user_t ** user_ptr)
{

	sepol_user_t *user = (sepol_user_t *) malloc(sizeof(sepol_user_t));

	if (!user) {
		ERR(handle, "out of memory, "
		    "could not create selinux user record");
		return STATUS_ERR;
	}

	user->roles = NULL;
	user->num_roles = 0;
	user->name = NULL;
	user->mls_level = NULL;
	user->mls_range = NULL;

	*user_ptr = user;
	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_user_clone(sepol_handle_t * handle,
		     const sepol_user_t * user, sepol_user_t ** user_ptr)
{

	sepol_user_t *new_user = NULL;
	unsigned int i;

	if (sepol_user_create(handle, &new_user) < 0)
		goto err;

	if (sepol_user_set_name(handle, new_user, user->name) < 0)
		goto err;

	for (i = 0; i < user->num_roles; i++) {
		if (sepol_user_add_role(handle, new_user, user->roles[i]) < 0)
			goto err;
	}

	if (user->mls_level &&
	    (sepol_user_set_mlslevel(handle, new_user, user->mls_level) < 0))
		goto err;

	if (user->mls_range &&
	    (sepol_user_set_mlsrange(handle, new_user, user->mls_range) < 0))
		goto err;

	*user_ptr = new_user;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not clone selinux user record");
	sepol_user_free(new_user);
	return STATUS_ERR;
}

/* Destroy */
void sepol_user_free(sepol_user_t * user)
{

	unsigned int i;

	if (!user)
		return;

	free(user->name);
	for (i = 0; i < user->num_roles; i++)
		free(user->roles[i]);
	free(user->roles);
	free(user->mls_level);
	free(user->mls_range);
	free(user);
}

