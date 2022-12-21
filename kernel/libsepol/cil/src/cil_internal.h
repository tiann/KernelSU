/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#ifndef CIL_INTERNAL_H_
#define CIL_INTERNAL_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

#include <sepol/policydb/services.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/flask_types.h>

#include <cil/cil.h>

#include "cil_flavor.h"
#include "cil_tree.h"
#include "cil_symtab.h"
#include "cil_mem.h"

#define CIL_MAX_NAME_LENGTH 2048

#define CIL_DEGENERATE_INHERITANCE_DEPTH 10UL
#define CIL_DEGENERATE_INHERITANCE_MINIMUM (0x01 << CIL_DEGENERATE_INHERITANCE_DEPTH)
#define CIL_DEGENERATE_INHERITANCE_GROWTH 10UL

enum cil_pass {
	CIL_PASS_INIT = 0,

	CIL_PASS_TIF,
	CIL_PASS_IN_BEFORE,
	CIL_PASS_BLKIN_LINK,
	CIL_PASS_BLKIN_COPY,
	CIL_PASS_BLKABS,
	CIL_PASS_IN_AFTER,
	CIL_PASS_CALL1,
	CIL_PASS_CALL2,
	CIL_PASS_ALIAS1,
	CIL_PASS_ALIAS2,
	CIL_PASS_MISC1,
	CIL_PASS_MLS,
	CIL_PASS_MISC2,
	CIL_PASS_MISC3,

	CIL_PASS_NUM
};


/*
	Keywords
*/
extern char *CIL_KEY_CONS_T1;
extern char *CIL_KEY_CONS_T2;
extern char *CIL_KEY_CONS_T3;
extern char *CIL_KEY_CONS_R1;
extern char *CIL_KEY_CONS_R2;
extern char *CIL_KEY_CONS_R3;
extern char *CIL_KEY_CONS_U1;
extern char *CIL_KEY_CONS_U2;
extern char *CIL_KEY_CONS_U3;
extern char *CIL_KEY_CONS_L1;
extern char *CIL_KEY_CONS_L2;
extern char *CIL_KEY_CONS_H1;
extern char *CIL_KEY_CONS_H2;
extern char *CIL_KEY_AND;
extern char *CIL_KEY_OR;
extern char *CIL_KEY_NOT;
extern char *CIL_KEY_EQ;
extern char *CIL_KEY_NEQ;
extern char *CIL_KEY_CONS_DOM;
extern char *CIL_KEY_CONS_DOMBY;
extern char *CIL_KEY_CONS_INCOMP;
extern char *CIL_KEY_CONDTRUE;
extern char *CIL_KEY_CONDFALSE;
extern char *CIL_KEY_SELF;
extern char *CIL_KEY_OBJECT_R;
extern char *CIL_KEY_STAR;
extern char *CIL_KEY_TCP;
extern char *CIL_KEY_UDP;
extern char *CIL_KEY_DCCP;
extern char *CIL_KEY_SCTP;
extern char *CIL_KEY_AUDITALLOW;
extern char *CIL_KEY_TUNABLEIF;
extern char *CIL_KEY_ALLOW;
extern char *CIL_KEY_DONTAUDIT;
extern char *CIL_KEY_TYPETRANSITION;
extern char *CIL_KEY_TYPECHANGE;
extern char *CIL_KEY_CALL;
extern char *CIL_KEY_TUNABLE;
extern char *CIL_KEY_XOR;
extern char *CIL_KEY_ALL;
extern char *CIL_KEY_RANGE;
extern char *CIL_KEY_GLOB;
extern char *CIL_KEY_FILE;
extern char *CIL_KEY_DIR;
extern char *CIL_KEY_CHAR;
extern char *CIL_KEY_BLOCK;
extern char *CIL_KEY_SOCKET;
extern char *CIL_KEY_PIPE;
extern char *CIL_KEY_SYMLINK;
extern char *CIL_KEY_ANY;
extern char *CIL_KEY_XATTR;
extern char *CIL_KEY_TASK;
extern char *CIL_KEY_TRANS;
extern char *CIL_KEY_TYPE;
extern char *CIL_KEY_ROLE;
extern char *CIL_KEY_USER;
extern char *CIL_KEY_USERATTRIBUTE;
extern char *CIL_KEY_USERATTRIBUTESET;
extern char *CIL_KEY_SENSITIVITY;
extern char *CIL_KEY_CATEGORY;
extern char *CIL_KEY_CATSET;
extern char *CIL_KEY_LEVEL;
extern char *CIL_KEY_LEVELRANGE;
extern char *CIL_KEY_CLASS;
extern char *CIL_KEY_IPADDR;
extern char *CIL_KEY_MAP_CLASS;
extern char *CIL_KEY_CLASSPERMISSION;
extern char *CIL_KEY_BOOL;
extern char *CIL_KEY_STRING;
extern char *CIL_KEY_NAME;
extern char *CIL_KEY_SOURCE;
extern char *CIL_KEY_TARGET;
extern char *CIL_KEY_LOW;
extern char *CIL_KEY_HIGH;
extern char *CIL_KEY_LOW_HIGH;
extern char *CIL_KEY_GLBLUB;
extern char *CIL_KEY_HANDLEUNKNOWN;
extern char *CIL_KEY_HANDLEUNKNOWN_ALLOW;
extern char *CIL_KEY_HANDLEUNKNOWN_DENY;
extern char *CIL_KEY_HANDLEUNKNOWN_REJECT;
extern char *CIL_KEY_MACRO;
extern char *CIL_KEY_IN;
extern char *CIL_KEY_IN_BEFORE;
extern char *CIL_KEY_IN_AFTER;
extern char *CIL_KEY_MLS;
extern char *CIL_KEY_DEFAULTRANGE;
extern char *CIL_KEY_BLOCKINHERIT;
extern char *CIL_KEY_BLOCKABSTRACT;
extern char *CIL_KEY_CLASSORDER;
extern char *CIL_KEY_CLASSMAPPING;
extern char *CIL_KEY_CLASSPERMISSIONSET;
extern char *CIL_KEY_COMMON;
extern char *CIL_KEY_CLASSCOMMON;
extern char *CIL_KEY_SID;
extern char *CIL_KEY_SIDCONTEXT;
extern char *CIL_KEY_SIDORDER;
extern char *CIL_KEY_USERLEVEL;
extern char *CIL_KEY_USERRANGE;
extern char *CIL_KEY_USERBOUNDS;
extern char *CIL_KEY_USERPREFIX;
extern char *CIL_KEY_SELINUXUSER;
extern char *CIL_KEY_SELINUXUSERDEFAULT;
extern char *CIL_KEY_TYPEATTRIBUTE;
extern char *CIL_KEY_TYPEATTRIBUTESET;
extern char *CIL_KEY_EXPANDTYPEATTRIBUTE;
extern char *CIL_KEY_TYPEALIAS;
extern char *CIL_KEY_TYPEALIASACTUAL;
extern char *CIL_KEY_TYPEBOUNDS;
extern char *CIL_KEY_TYPEPERMISSIVE;
extern char *CIL_KEY_RANGETRANSITION;
extern char *CIL_KEY_USERROLE;
extern char *CIL_KEY_ROLETYPE;
extern char *CIL_KEY_ROLETRANSITION;
extern char *CIL_KEY_ROLEALLOW;
extern char *CIL_KEY_ROLEATTRIBUTE;
extern char *CIL_KEY_ROLEATTRIBUTESET;
extern char *CIL_KEY_ROLEBOUNDS;
extern char *CIL_KEY_BOOLEANIF;
extern char *CIL_KEY_NEVERALLOW;
extern char *CIL_KEY_TYPEMEMBER;
extern char *CIL_KEY_SENSALIAS;
extern char *CIL_KEY_SENSALIASACTUAL;
extern char *CIL_KEY_CATALIAS;
extern char *CIL_KEY_CATALIASACTUAL;
extern char *CIL_KEY_CATORDER;
extern char *CIL_KEY_SENSITIVITYORDER;
extern char *CIL_KEY_SENSCAT;
extern char *CIL_KEY_CONSTRAIN;
extern char *CIL_KEY_MLSCONSTRAIN;
extern char *CIL_KEY_VALIDATETRANS;
extern char *CIL_KEY_MLSVALIDATETRANS;
extern char *CIL_KEY_CONTEXT;
extern char *CIL_KEY_FILECON;
extern char *CIL_KEY_IBPKEYCON;
extern char *CIL_KEY_IBENDPORTCON;
extern char *CIL_KEY_PORTCON;
extern char *CIL_KEY_NODECON;
extern char *CIL_KEY_GENFSCON;
extern char *CIL_KEY_NETIFCON;
extern char *CIL_KEY_PIRQCON;
extern char *CIL_KEY_IOMEMCON;
extern char *CIL_KEY_IOPORTCON;
extern char *CIL_KEY_PCIDEVICECON;
extern char *CIL_KEY_DEVICETREECON;
extern char *CIL_KEY_FSUSE;
extern char *CIL_KEY_POLICYCAP;
extern char *CIL_KEY_OPTIONAL;
extern char *CIL_KEY_DEFAULTUSER;
extern char *CIL_KEY_DEFAULTROLE;
extern char *CIL_KEY_DEFAULTTYPE;
extern char *CIL_KEY_ROOT;
extern char *CIL_KEY_NODE;
extern char *CIL_KEY_PERM;
extern char *CIL_KEY_ALLOWX;
extern char *CIL_KEY_AUDITALLOWX;
extern char *CIL_KEY_DONTAUDITX;
extern char *CIL_KEY_NEVERALLOWX;
extern char *CIL_KEY_PERMISSIONX;
extern char *CIL_KEY_IOCTL;
extern char *CIL_KEY_UNORDERED;
extern char *CIL_KEY_SRC_INFO;
extern char *CIL_KEY_SRC_CIL;
extern char *CIL_KEY_SRC_HLL_LMS;
extern char *CIL_KEY_SRC_HLL_LMX;
extern char *CIL_KEY_SRC_HLL_LME;

/*
	Symbol Table Array Indices
*/
enum cil_sym_index {
	CIL_SYM_BLOCKS = 0,
	CIL_SYM_USERS,
	CIL_SYM_ROLES,
	CIL_SYM_TYPES,
	CIL_SYM_COMMONS,
	CIL_SYM_CLASSES,
	CIL_SYM_CLASSPERMSETS,
	CIL_SYM_BOOLS,
	CIL_SYM_TUNABLES,
	CIL_SYM_SENS,
	CIL_SYM_CATS,
	CIL_SYM_SIDS,
	CIL_SYM_CONTEXTS,
	CIL_SYM_LEVELS,
	CIL_SYM_LEVELRANGES,
	CIL_SYM_POLICYCAPS,
	CIL_SYM_IPADDRS,
	CIL_SYM_NAMES,
	CIL_SYM_PERMX,
	CIL_SYM_NUM,
	CIL_SYM_UNKNOWN,
	CIL_SYM_PERMS	// Special case for permissions. This symtab is not included in arrays
};

enum cil_sym_array {
	CIL_SYM_ARRAY_ROOT = 0,
	CIL_SYM_ARRAY_BLOCK,
	CIL_SYM_ARRAY_IN,
	CIL_SYM_ARRAY_MACRO,
	CIL_SYM_ARRAY_CONDBLOCK,
	CIL_SYM_ARRAY_NUM
};

extern const int cil_sym_sizes[CIL_SYM_ARRAY_NUM][CIL_SYM_NUM];

#define CIL_CLASS_SYM_SIZE	256
#define CIL_PERMS_PER_CLASS (sizeof(sepol_access_vector_t) * 8)

struct cil_db {
	struct cil_tree *parse;
	struct cil_tree *ast;
	struct cil_type *selftype;
	struct cil_list *sidorder;
	struct cil_list *classorder;
	struct cil_list *catorder;
	struct cil_list *sensitivityorder;
	struct cil_sort *netifcon;
	struct cil_sort *genfscon;
	struct cil_sort *filecon;
	struct cil_sort *nodecon;
	struct cil_sort *ibpkeycon;
	struct cil_sort *ibendportcon;
	struct cil_sort *portcon;
	struct cil_sort *pirqcon;
	struct cil_sort *iomemcon;
	struct cil_sort *ioportcon;
	struct cil_sort *pcidevicecon;
	struct cil_sort *devicetreecon;
	struct cil_sort *fsuse;
	struct cil_list *userprefixes;
	struct cil_list *selinuxusers;
	struct cil_list *names;
	int num_types_and_attrs;
	int num_classes;
	int num_cats;
	int num_types;
	int num_roles;
	int num_users;
	struct cil_type **val_to_type;
	struct cil_role **val_to_role;
	struct cil_user **val_to_user;
	int disable_dontaudit;
	int disable_neverallow;
	int attrs_expand_generated;
	unsigned attrs_expand_size;
	int preserve_tunables;
	int handle_unknown;
	int mls;
	int multiple_decls;
	int qualified_names;
	int target_platform;
	int policy_version;
};

struct cil_root {
	symtab_t symtab[CIL_SYM_NUM];
};

struct cil_sort {
	enum cil_flavor flavor;
	uint32_t count;
	uint32_t index;
	void **array;
};

struct cil_block {
	struct cil_symtab_datum datum;
	symtab_t symtab[CIL_SYM_NUM];
	uint16_t is_abstract;
	struct cil_list *bi_nodes;
};

struct cil_blockinherit {
	char *block_str;
	struct cil_block *block;
};

struct cil_blockabstract {
	char *block_str;
};

struct cil_in {
	symtab_t symtab[CIL_SYM_NUM];
	int is_after;
	char *block_str;
};

struct cil_optional {
	struct cil_symtab_datum datum;
};

struct cil_perm {
	struct cil_symtab_datum datum;
	unsigned int value;
	struct cil_list *classperms; /* Only used for map perms */
};

struct cil_class {
	struct cil_symtab_datum datum;
	symtab_t perms;
	unsigned int num_perms;
	struct cil_class *common; /* Only used for kernel class */
	uint32_t ordered; /* Only used for kernel class */
};

struct cil_classorder {
	struct cil_list *class_list_str;
};

struct cil_classperms_set {
	char *set_str;
	struct cil_classpermission *set;
};

struct cil_classperms {
	char *class_str;
	struct cil_class *class;
	struct cil_list *perm_strs;
	struct cil_list *perms;
};

struct cil_classpermission {
	struct cil_symtab_datum datum;
	struct cil_list *classperms;
};

struct cil_classpermissionset {
	char *set_str;
	struct cil_list *classperms;
};

struct cil_classmapping {
	char *map_class_str;
	char *map_perm_str;
	struct cil_list *classperms;
};

struct cil_classcommon {
	char *class_str;
	char *common_str;
};

struct cil_alias {
	struct cil_symtab_datum datum;
	void *actual;
};

struct cil_aliasactual {
	char *alias_str;
	char *actual_str;
};

struct cil_sid {
	struct cil_symtab_datum datum;
	struct cil_context *context;
	uint32_t ordered;
};

struct cil_sidcontext {
	char *sid_str;
	char *context_str;
	struct cil_context *context;
};

struct cil_sidorder {
	struct cil_list *sid_list_str;
};

struct cil_user {
	struct cil_symtab_datum datum;
	struct cil_user *bounds;
	ebitmap_t *roles;
	struct cil_level *dftlevel;
	struct cil_levelrange *range;
	int value;
};

struct cil_userattribute {
	struct cil_symtab_datum datum;
	struct cil_list *expr_list;
	ebitmap_t *users;
};

struct cil_userattributeset {
	char *attr_str;
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_userrole {
	char *user_str;
	void *user;
	char *role_str;
	void *role;
};

struct cil_userlevel {
	char *user_str;
	char *level_str;
	struct cil_level *level;
};

struct cil_userrange {
	char *user_str;
	char *range_str;
	struct cil_levelrange *range;
};

struct cil_userprefix {
	char *user_str;
	struct cil_user *user;
	char *prefix_str;
};

struct cil_selinuxuser {
	char *name_str;
	char *user_str;
	struct cil_user *user;
	char *range_str;
	struct cil_levelrange *range;
};

struct cil_role {
	struct cil_symtab_datum datum;
	struct cil_role *bounds;
	ebitmap_t *types;
	int value;
};

struct cil_roleattribute {
	struct cil_symtab_datum datum;
	struct cil_list *expr_list;
	ebitmap_t *roles;
};

struct cil_roleattributeset {
	char *attr_str;
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_roletype {
	char *role_str;
	void *role; /* role or attribute */
	char *type_str;
	void *type; /* type, alias, or attribute */
};

struct cil_type	{
	struct cil_symtab_datum datum;
	struct cil_type *bounds;
	int value;
};

#define CIL_ATTR_AVRULE		(1 << 0)
#define CIL_ATTR_NEVERALLOW	(1 << 1)
#define CIL_ATTR_CONSTRAINT	(1 << 2)
#define CIL_ATTR_EXPAND_TRUE	(1 << 3)
#define CIL_ATTR_EXPAND_FALSE	(1 << 4)
struct cil_typeattribute {
	struct cil_symtab_datum datum;
	struct cil_list *expr_list;
	ebitmap_t *types;
	int used;	// whether or not this attribute was used in a binary policy rule
	int keep;
};

struct cil_typeattributeset {
	char *attr_str;
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_expandtypeattribute {
	struct cil_list *attr_strs;
	struct cil_list *attr_datums;
	int expand;
};

struct cil_typepermissive {
	char *type_str;
	void *type; /* type or alias */
};

struct cil_name {
	struct cil_symtab_datum datum;
	char *name_str;
};

struct cil_nametypetransition {
	char *src_str;
	void *src; /* type, alias, or attribute */
	char *tgt_str;
	void *tgt; /* type, alias, or attribute */
	char *obj_str;
	struct cil_class *obj;
	char *name_str;
	struct cil_name *name;
	char *result_str;
	void *result; /* type or alias */

};

struct cil_rangetransition {
	char *src_str;
	void *src; /* type, alias, or attribute */
	char *exec_str;
	void *exec; /* type, alias, or attribute */
	char *obj_str;
	struct cil_class *obj;
	char *range_str;
	struct cil_levelrange *range;
};

struct cil_bool {
	struct cil_symtab_datum datum;
	uint16_t value;
};

struct cil_tunable {
	struct cil_symtab_datum datum;
	uint16_t value;
};

#define CIL_AVRULE_ALLOWED     1
#define CIL_AVRULE_AUDITALLOW  2
#define CIL_AVRULE_DONTAUDIT   8
#define CIL_AVRULE_NEVERALLOW 128
#define CIL_AVRULE_AV         (AVRULE_ALLOWED | AVRULE_AUDITALLOW | AVRULE_DONTAUDIT | AVRULE_NEVERALLOW)
struct cil_avrule {
	int is_extended;
	uint32_t rule_kind;
	char *src_str;
	void *src; /* type, alias, or attribute */
	char *tgt_str;	
	void *tgt; /* type, alias, or attribute */
	union {
		struct cil_list *classperms;
		struct {
			char *permx_str;
			struct cil_permissionx *permx;
		} x;
	} perms;
};

#define CIL_PERMX_KIND_IOCTL 1
struct cil_permissionx {
	struct cil_symtab_datum datum;
	uint32_t kind;
	char *obj_str;
	struct cil_class *obj;
	struct cil_list *expr_str;
	ebitmap_t *perms;
};

#define CIL_TYPE_TRANSITION 16
#define CIL_TYPE_MEMBER     32
#define CIL_TYPE_CHANGE     64
#define CIL_AVRULE_TYPE       (AVRULE_TRANSITION | AVRULE_MEMBER | AVRULE_CHANGE)
struct cil_type_rule {
	uint32_t rule_kind;
	char *src_str;
	void *src; /* type, alias, or attribute */
	char *tgt_str;
	void *tgt; /* type, alias, or attribute */
	char *obj_str;
	struct cil_class *obj;
	char *result_str;
	void *result; /* type or alias */
};

struct cil_roletransition {
	char *src_str;
	struct cil_role *src;
	char *tgt_str;	
	void *tgt; /* type, alias, or attribute */
	char *obj_str;
	struct cil_class *obj;
	char *result_str;
	struct cil_role *result;
};

struct cil_roleallow {
	char *src_str;
	void *src; /* role or attribute */
	char *tgt_str;
	void *tgt; /* role or attribute */
};

struct cil_sens {
	struct cil_symtab_datum datum;
	struct cil_list *cats_list;
	uint32_t ordered;
};

struct cil_sensorder {
	struct cil_list *sens_list_str;
};

struct cil_cat {
	struct cil_symtab_datum datum;
	uint32_t ordered;
	int value;
};

struct cil_cats {
	uint32_t evaluated;
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_catset {
	struct cil_symtab_datum datum;
	struct cil_cats *cats;
};

struct cil_catorder {
	struct cil_list *cat_list_str;
};

struct cil_senscat {
	char *sens_str;
	struct cil_cats *cats;
};

struct cil_level {
	struct cil_symtab_datum datum;
	char *sens_str;
	struct cil_sens *sens;
	struct cil_cats *cats;
};

struct cil_levelrange {
	struct cil_symtab_datum datum;
	char *low_str;
	struct cil_level *low;
	char *high_str;
	struct cil_level *high;
};

struct cil_context {
	struct cil_symtab_datum datum;
	char *user_str;
	struct cil_user *user;
	char *role_str;
	struct cil_role *role;
	char *type_str;
	void *type; /* type or alias */
	char *range_str;
	struct cil_levelrange *range;
};

enum cil_filecon_types {
	CIL_FILECON_ANY = 0,
	CIL_FILECON_FILE,
	CIL_FILECON_DIR,
	CIL_FILECON_CHAR,
	CIL_FILECON_BLOCK,
	CIL_FILECON_SOCKET,
	CIL_FILECON_PIPE,
	CIL_FILECON_SYMLINK,
};

struct cil_filecon {
	char *path_str;
	enum cil_filecon_types type;
	char *context_str;
	struct cil_context *context;
};

enum cil_protocol {
	CIL_PROTOCOL_UDP = 1,
	CIL_PROTOCOL_TCP,
	CIL_PROTOCOL_DCCP,
	CIL_PROTOCOL_SCTP
};

struct cil_ibpkeycon {
	char *subnet_prefix_str;
	uint32_t pkey_low;
	uint32_t pkey_high;
	char *context_str;
	struct cil_context *context;
};

struct cil_portcon {
	enum cil_protocol proto;
	uint32_t port_low;
	uint32_t port_high;
	char *context_str;
	struct cil_context *context;
};

struct cil_nodecon {
	char *addr_str;
	struct cil_ipaddr *addr;
	char *mask_str;
	struct cil_ipaddr *mask;
	char *context_str;
	struct cil_context *context;
};

struct cil_ipaddr {
	struct cil_symtab_datum datum;
	int family;
	union {
		struct in_addr v4;
		struct in6_addr v6;
	} ip;
};

struct cil_genfscon {
	char *fs_str;
	char *path_str;
	enum cil_filecon_types file_type;
	char *context_str;
	struct cil_context *context;
};

struct cil_netifcon {
	char *interface_str;
	char *if_context_str;
	struct cil_context *if_context;
	char *packet_context_str;
	struct cil_context *packet_context;
	char *context_str;
};

struct cil_ibendportcon {
	char *dev_name_str;
	uint32_t port;
	char *context_str;
	struct cil_context *context;
};
struct cil_pirqcon {
	uint32_t pirq;
	char *context_str;
	struct cil_context *context;
};

struct cil_iomemcon {
	uint64_t iomem_low;
	uint64_t iomem_high;
	char *context_str;
	struct cil_context *context;
};

struct cil_ioportcon {
	uint32_t ioport_low;
	uint32_t ioport_high;
	char *context_str;
	struct cil_context *context;
};

struct cil_pcidevicecon {
	uint32_t dev;
	char *context_str;
	struct cil_context *context;
};

struct cil_devicetreecon {
	char *path;
	char *context_str;
	struct cil_context *context;
};


/* Ensure that CIL uses the same values as sepol services.h */
enum cil_fsuse_types {
	CIL_FSUSE_XATTR = SECURITY_FS_USE_XATTR,
	CIL_FSUSE_TASK = SECURITY_FS_USE_TASK,
	CIL_FSUSE_TRANS = SECURITY_FS_USE_TRANS
};

struct cil_fsuse {
	enum cil_fsuse_types type;
	char *fs_str;
	char *context_str;
	struct cil_context *context;
};

#define CIL_MLS_LEVELS "l1 l2 h1 h2" 
#define CIL_CONSTRAIN_KEYS "t1 t2 r1 r2 u1 u2"
#define CIL_MLSCONSTRAIN_KEYS CIL_MLS_LEVELS CIL_CONSTRAIN_KEYS
#define CIL_CONSTRAIN_OPER "== != eq dom domby incomp not and or"
struct cil_constrain {
	struct cil_list *classperms;
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_validatetrans {
	char *class_str;
	struct cil_class *class;
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_param {
	char *str;
	enum cil_flavor flavor;
};

struct cil_macro {
	struct cil_symtab_datum datum;
	symtab_t symtab[CIL_SYM_NUM];
	struct cil_list *params;
};

struct cil_args {
	char *arg_str;
	struct cil_symtab_datum *arg;
	char *param_str;
	enum cil_flavor flavor;
};

struct cil_call {
	char *macro_str;
	struct cil_macro *macro;
	struct cil_tree *args_tree;
	struct cil_list *args;
	int copied;
};

#define CIL_TRUE	1
#define CIL_FALSE	0

struct cil_condblock {
	enum cil_flavor flavor;
	symtab_t symtab[CIL_SYM_NUM];
};

struct cil_booleanif {
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
	int preserved_tunable;
};

struct cil_tunableif {
	struct cil_list *str_expr;
	struct cil_list *datum_expr;
};

struct cil_policycap {
	struct cil_symtab_datum datum;
};

struct cil_bounds {
	char *parent_str;
	char *child_str;
};

/* Ensure that CIL uses the same values as sepol policydb.h */
enum cil_default_object {
	CIL_DEFAULT_SOURCE = DEFAULT_SOURCE,
	CIL_DEFAULT_TARGET = DEFAULT_TARGET,
};

/* Default labeling behavior for users, roles, and types */
struct cil_default {
	enum cil_flavor flavor;
	struct cil_list *class_strs;
	struct cil_list *class_datums;
	enum cil_default_object object;
};

/* Ensure that CIL uses the same values as sepol policydb.h */
enum cil_default_object_range {
	CIL_DEFAULT_SOURCE_LOW      = DEFAULT_SOURCE_LOW,
	CIL_DEFAULT_SOURCE_HIGH     = DEFAULT_SOURCE_HIGH,
	CIL_DEFAULT_SOURCE_LOW_HIGH = DEFAULT_SOURCE_LOW_HIGH,
	CIL_DEFAULT_TARGET_LOW      = DEFAULT_TARGET_LOW,
	CIL_DEFAULT_TARGET_HIGH     = DEFAULT_TARGET_HIGH,
	CIL_DEFAULT_TARGET_LOW_HIGH = DEFAULT_TARGET_LOW_HIGH,
	CIL_DEFAULT_GLBLUB          = DEFAULT_GLBLUB,
};

/* Default labeling behavior for range */
struct cil_defaultrange {
	struct cil_list *class_strs;
	struct cil_list *class_datums;
	enum cil_default_object_range object_range;
};

struct cil_handleunknown {
	int handle_unknown;
};

struct cil_mls {
	int value;
};

struct cil_src_info {
	char *kind;
	uint32_t hll_line;
	char *path;
};

void cil_db_init(struct cil_db **db);
void cil_db_destroy(struct cil_db **db);

void cil_root_init(struct cil_root **root);
void cil_root_destroy(struct cil_root *root);

void cil_destroy_data(void **data, enum cil_flavor flavor);

int cil_flavor_to_symtab_index(enum cil_flavor flavor, enum cil_sym_index *index);
const char * cil_node_to_string(struct cil_tree_node *node);

int cil_userprefixes_to_string(struct cil_db *db, char **out, size_t *size);
int cil_selinuxusers_to_string(struct cil_db *db, char **out, size_t *size);
int cil_filecons_to_string(struct cil_db *db, char **out, size_t *size);

void cil_symtab_array_init(symtab_t symtab[], const int symtab_sizes[CIL_SYM_NUM]);
void cil_symtab_array_destroy(symtab_t symtab[]);
void cil_destroy_ast_symtabs(struct cil_tree_node *root);
int cil_get_symtab(struct cil_tree_node *ast_node, symtab_t **symtab, enum cil_sym_index sym_index);
int cil_string_to_uint32(const char *string, uint32_t *value, int base);
int cil_string_to_uint64(const char *string, uint64_t *value, int base);

void cil_sort_init(struct cil_sort **sort);
void cil_sort_destroy(struct cil_sort **sort);
void cil_netifcon_init(struct cil_netifcon **netifcon);
void cil_ibendportcon_init(struct cil_ibendportcon **ibendportcon);
void cil_context_init(struct cil_context **context);
void cil_level_init(struct cil_level **level);
void cil_levelrange_init(struct cil_levelrange **lvlrange);
void cil_sens_init(struct cil_sens **sens);
void cil_block_init(struct cil_block **block);
void cil_blockinherit_init(struct cil_blockinherit **inherit);
void cil_blockabstract_init(struct cil_blockabstract **abstract);
void cil_in_init(struct cil_in **in);
void cil_class_init(struct cil_class **class);
void cil_classorder_init(struct cil_classorder **classorder);
void cil_classcommon_init(struct cil_classcommon **classcommon);
void cil_sid_init(struct cil_sid **sid);
void cil_sidcontext_init(struct cil_sidcontext **sidcontext);
void cil_sidorder_init(struct cil_sidorder **sidorder);
void cil_userrole_init(struct cil_userrole **userrole);
void cil_userprefix_init(struct cil_userprefix **userprefix);
void cil_selinuxuser_init(struct cil_selinuxuser **selinuxuser);
void cil_roleattribute_init(struct cil_roleattribute **attribute);
void cil_roleattributeset_init(struct cil_roleattributeset **attrset);
void cil_roletype_init(struct cil_roletype **roletype);
void cil_typeattribute_init(struct cil_typeattribute **attribute);
void cil_typeattributeset_init(struct cil_typeattributeset **attrset);
void cil_expandtypeattribute_init(struct cil_expandtypeattribute **expandattr);
void cil_alias_init(struct cil_alias **alias);
void cil_aliasactual_init(struct cil_aliasactual **aliasactual);
void cil_typepermissive_init(struct cil_typepermissive **typeperm);
void cil_name_init(struct cil_name **name);
void cil_nametypetransition_init(struct cil_nametypetransition **nametypetrans);
void cil_rangetransition_init(struct cil_rangetransition **rangetrans);
void cil_bool_init(struct cil_bool **cilbool);
void cil_boolif_init(struct cil_booleanif **bif);
void cil_condblock_init(struct cil_condblock **cb);
void cil_tunable_init(struct cil_tunable **ciltun);
void cil_tunif_init(struct cil_tunableif **tif);
void cil_avrule_init(struct cil_avrule **avrule);
void cil_permissionx_init(struct cil_permissionx **permx);
void cil_type_rule_init(struct cil_type_rule **type_rule);
void cil_roletransition_init(struct cil_roletransition **roletrans);
void cil_roleallow_init(struct cil_roleallow **role_allow);
void cil_catset_init(struct cil_catset **catset);
void cil_cats_init(struct cil_cats **cats);
void cil_senscat_init(struct cil_senscat **senscat);
void cil_filecon_init(struct cil_filecon **filecon);
void cil_ibpkeycon_init(struct cil_ibpkeycon **ibpkeycon);
void cil_portcon_init(struct cil_portcon **portcon);
void cil_nodecon_init(struct cil_nodecon **nodecon);
void cil_genfscon_init(struct cil_genfscon **genfscon);
void cil_pirqcon_init(struct cil_pirqcon **pirqcon);
void cil_iomemcon_init(struct cil_iomemcon **iomemcon);
void cil_ioportcon_init(struct cil_ioportcon **ioportcon);
void cil_pcidevicecon_init(struct cil_pcidevicecon **pcidevicecon);
void cil_devicetreecon_init(struct cil_devicetreecon **devicetreecon);
void cil_fsuse_init(struct cil_fsuse **fsuse);
void cil_constrain_init(struct cil_constrain **constrain);
void cil_validatetrans_init(struct cil_validatetrans **validtrans);
void cil_ipaddr_init(struct cil_ipaddr **ipaddr);
void cil_perm_init(struct cil_perm **perm);
void cil_classpermission_init(struct cil_classpermission **cp);
void cil_classpermissionset_init(struct cil_classpermissionset **cps);
void cil_classperms_set_init(struct cil_classperms_set **cp_set);
void cil_classperms_init(struct cil_classperms **cp);
void cil_classmapping_init(struct cil_classmapping **mapping);
void cil_user_init(struct cil_user **user);
void cil_userlevel_init(struct cil_userlevel **usrlvl);
void cil_userrange_init(struct cil_userrange **userrange);
void cil_role_init(struct cil_role **role);
void cil_type_init(struct cil_type **type);
void cil_cat_init(struct cil_cat **cat);
void cil_catorder_init(struct cil_catorder **catorder);
void cil_sensorder_init(struct cil_sensorder **sensorder);
void cil_args_init(struct cil_args **args);
void cil_call_init(struct cil_call **call);
void cil_optional_init(struct cil_optional **optional);
void cil_param_init(struct cil_param **param);
void cil_macro_init(struct cil_macro **macro);
void cil_policycap_init(struct cil_policycap **policycap);
void cil_bounds_init(struct cil_bounds **bounds);
void cil_default_init(struct cil_default **def);
void cil_defaultrange_init(struct cil_defaultrange **def);
void cil_handleunknown_init(struct cil_handleunknown **unk);
void cil_mls_init(struct cil_mls **mls);
void cil_src_info_init(struct cil_src_info **info);
void cil_userattribute_init(struct cil_userattribute **attribute);
void cil_userattributeset_init(struct cil_userattributeset **attrset);

#endif
