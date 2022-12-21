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

#include <stdlib.h>
#include <stdio.h>

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/symtab.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_log.h"
#include "cil_mem.h"
#include "cil_tree.h"
#include "cil_list.h"
#include "cil_symtab.h"
#include "cil_build_ast.h"

#include "cil_parser.h"
#include "cil_build_ast.h"
#include "cil_resolve_ast.h"
#include "cil_fqn.h"
#include "cil_post.h"
#include "cil_binary.h"
#include "cil_policy.h"
#include "cil_strpool.h"
#include "cil_write_ast.h"

const int cil_sym_sizes[CIL_SYM_ARRAY_NUM][CIL_SYM_NUM] = {
	{64, 64, 64, 1 << 13, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64},
	{8, 8, 8, 32, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

char *CIL_KEY_CONS_T1;
char *CIL_KEY_CONS_T2;
char *CIL_KEY_CONS_T3;
char *CIL_KEY_CONS_R1;
char *CIL_KEY_CONS_R2;
char *CIL_KEY_CONS_R3;
char *CIL_KEY_CONS_U1;
char *CIL_KEY_CONS_U2;
char *CIL_KEY_CONS_U3;
char *CIL_KEY_CONS_L1;
char *CIL_KEY_CONS_L2;
char *CIL_KEY_CONS_H1;
char *CIL_KEY_CONS_H2;
char *CIL_KEY_AND;
char *CIL_KEY_OR;
char *CIL_KEY_NOT;
char *CIL_KEY_EQ;
char *CIL_KEY_NEQ;
char *CIL_KEY_CONS_DOM;
char *CIL_KEY_CONS_DOMBY;
char *CIL_KEY_CONS_INCOMP;
char *CIL_KEY_CONDTRUE;
char *CIL_KEY_CONDFALSE;
char *CIL_KEY_SELF;
char *CIL_KEY_OBJECT_R;
char *CIL_KEY_STAR;
char *CIL_KEY_TCP;
char *CIL_KEY_UDP;
char *CIL_KEY_DCCP;
char *CIL_KEY_SCTP;
char *CIL_KEY_AUDITALLOW;
char *CIL_KEY_TUNABLEIF;
char *CIL_KEY_ALLOW;
char *CIL_KEY_DONTAUDIT;
char *CIL_KEY_TYPETRANSITION;
char *CIL_KEY_TYPECHANGE;
char *CIL_KEY_CALL;
char *CIL_KEY_TUNABLE;
char *CIL_KEY_XOR;
char *CIL_KEY_ALL;
char *CIL_KEY_RANGE;
char *CIL_KEY_GLOB;
char *CIL_KEY_FILE;
char *CIL_KEY_DIR;
char *CIL_KEY_CHAR;
char *CIL_KEY_BLOCK;
char *CIL_KEY_SOCKET;
char *CIL_KEY_PIPE;
char *CIL_KEY_SYMLINK;
char *CIL_KEY_ANY;
char *CIL_KEY_XATTR;
char *CIL_KEY_TASK;
char *CIL_KEY_TRANS;
char *CIL_KEY_TYPE;
char *CIL_KEY_ROLE;
char *CIL_KEY_USER;
char *CIL_KEY_USERATTRIBUTE;
char *CIL_KEY_USERATTRIBUTESET;
char *CIL_KEY_SENSITIVITY;
char *CIL_KEY_CATEGORY;
char *CIL_KEY_CATSET;
char *CIL_KEY_LEVEL;
char *CIL_KEY_LEVELRANGE;
char *CIL_KEY_CLASS;
char *CIL_KEY_IPADDR;
char *CIL_KEY_MAP_CLASS;
char *CIL_KEY_CLASSPERMISSION;
char *CIL_KEY_BOOL;
char *CIL_KEY_STRING;
char *CIL_KEY_NAME;
char *CIL_KEY_SOURCE;
char *CIL_KEY_TARGET;
char *CIL_KEY_LOW;
char *CIL_KEY_HIGH;
char *CIL_KEY_LOW_HIGH;
char *CIL_KEY_GLBLUB;
char *CIL_KEY_HANDLEUNKNOWN;
char *CIL_KEY_HANDLEUNKNOWN_ALLOW;
char *CIL_KEY_HANDLEUNKNOWN_DENY;
char *CIL_KEY_HANDLEUNKNOWN_REJECT;
char *CIL_KEY_MACRO;
char *CIL_KEY_IN;
char *CIL_KEY_IN_BEFORE;
char *CIL_KEY_IN_AFTER;
char *CIL_KEY_MLS;
char *CIL_KEY_DEFAULTRANGE;
char *CIL_KEY_BLOCKINHERIT;
char *CIL_KEY_BLOCKABSTRACT;
char *CIL_KEY_CLASSORDER;
char *CIL_KEY_CLASSMAPPING;
char *CIL_KEY_CLASSPERMISSIONSET;
char *CIL_KEY_COMMON;
char *CIL_KEY_CLASSCOMMON;
char *CIL_KEY_SID;
char *CIL_KEY_SIDCONTEXT;
char *CIL_KEY_SIDORDER;
char *CIL_KEY_USERLEVEL;
char *CIL_KEY_USERRANGE;
char *CIL_KEY_USERBOUNDS;
char *CIL_KEY_USERPREFIX;
char *CIL_KEY_SELINUXUSER;
char *CIL_KEY_SELINUXUSERDEFAULT;
char *CIL_KEY_TYPEATTRIBUTE;
char *CIL_KEY_TYPEATTRIBUTESET;
char *CIL_KEY_EXPANDTYPEATTRIBUTE;
char *CIL_KEY_TYPEALIAS;
char *CIL_KEY_TYPEALIASACTUAL;
char *CIL_KEY_TYPEBOUNDS;
char *CIL_KEY_TYPEPERMISSIVE;
char *CIL_KEY_RANGETRANSITION;
char *CIL_KEY_USERROLE;
char *CIL_KEY_ROLETYPE;
char *CIL_KEY_ROLETRANSITION;
char *CIL_KEY_ROLEALLOW;
char *CIL_KEY_ROLEATTRIBUTE;
char *CIL_KEY_ROLEATTRIBUTESET;
char *CIL_KEY_ROLEBOUNDS;
char *CIL_KEY_BOOLEANIF;
char *CIL_KEY_NEVERALLOW;
char *CIL_KEY_TYPEMEMBER;
char *CIL_KEY_SENSALIAS;
char *CIL_KEY_SENSALIASACTUAL;
char *CIL_KEY_CATALIAS;
char *CIL_KEY_CATALIASACTUAL;
char *CIL_KEY_CATORDER;
char *CIL_KEY_SENSITIVITYORDER;
char *CIL_KEY_SENSCAT;
char *CIL_KEY_CONSTRAIN;
char *CIL_KEY_MLSCONSTRAIN;
char *CIL_KEY_VALIDATETRANS;
char *CIL_KEY_MLSVALIDATETRANS;
char *CIL_KEY_CONTEXT;
char *CIL_KEY_FILECON;
char *CIL_KEY_IBPKEYCON;
char *CIL_KEY_IBENDPORTCON;
char *CIL_KEY_PORTCON;
char *CIL_KEY_NODECON;
char *CIL_KEY_GENFSCON;
char *CIL_KEY_NETIFCON;
char *CIL_KEY_PIRQCON;
char *CIL_KEY_IOMEMCON;
char *CIL_KEY_IOPORTCON;
char *CIL_KEY_PCIDEVICECON;
char *CIL_KEY_DEVICETREECON;
char *CIL_KEY_FSUSE;
char *CIL_KEY_POLICYCAP;
char *CIL_KEY_OPTIONAL;
char *CIL_KEY_DEFAULTUSER;
char *CIL_KEY_DEFAULTROLE;
char *CIL_KEY_DEFAULTTYPE;
char *CIL_KEY_ROOT;
char *CIL_KEY_NODE;
char *CIL_KEY_PERM;
char *CIL_KEY_ALLOWX;
char *CIL_KEY_AUDITALLOWX;
char *CIL_KEY_DONTAUDITX;
char *CIL_KEY_NEVERALLOWX;
char *CIL_KEY_PERMISSIONX;
char *CIL_KEY_IOCTL;
char *CIL_KEY_UNORDERED;
char *CIL_KEY_SRC_INFO;
char *CIL_KEY_SRC_CIL;
char *CIL_KEY_SRC_HLL_LMS;
char *CIL_KEY_SRC_HLL_LMX;
char *CIL_KEY_SRC_HLL_LME;

static void cil_init_keys(void)
{
	/* Initialize CIL Keys into strpool */
	CIL_KEY_CONS_T1 = cil_strpool_add("t1");
	CIL_KEY_CONS_T2 = cil_strpool_add("t2");
	CIL_KEY_CONS_T3 = cil_strpool_add("t3");
	CIL_KEY_CONS_R1 = cil_strpool_add("r1");
	CIL_KEY_CONS_R2 = cil_strpool_add("r2");
	CIL_KEY_CONS_R3 = cil_strpool_add("r3");
	CIL_KEY_CONS_U1 = cil_strpool_add("u1");
	CIL_KEY_CONS_U2 = cil_strpool_add("u2");
	CIL_KEY_CONS_U3 = cil_strpool_add("u3");
	CIL_KEY_CONS_L1 = cil_strpool_add("l1");
	CIL_KEY_CONS_L2 = cil_strpool_add("l2");
	CIL_KEY_CONS_H1 = cil_strpool_add("h1");
	CIL_KEY_CONS_H2 = cil_strpool_add("h2");
	CIL_KEY_AND = cil_strpool_add("and");
	CIL_KEY_OR = cil_strpool_add("or");
	CIL_KEY_NOT = cil_strpool_add("not");
	CIL_KEY_EQ = cil_strpool_add("eq");
	CIL_KEY_NEQ = cil_strpool_add("neq");
	CIL_KEY_CONS_DOM = cil_strpool_add("dom");
	CIL_KEY_CONS_DOMBY = cil_strpool_add("domby");
	CIL_KEY_CONS_INCOMP = cil_strpool_add("incomp");
	CIL_KEY_CONDTRUE = cil_strpool_add("true");
	CIL_KEY_CONDFALSE = cil_strpool_add("false");
	CIL_KEY_SELF = cil_strpool_add("self");
	CIL_KEY_OBJECT_R = cil_strpool_add("object_r");
	CIL_KEY_STAR = cil_strpool_add("*");
	CIL_KEY_UDP = cil_strpool_add("udp");
	CIL_KEY_TCP = cil_strpool_add("tcp");
	CIL_KEY_DCCP = cil_strpool_add("dccp");
	CIL_KEY_SCTP = cil_strpool_add("sctp");
	CIL_KEY_AUDITALLOW = cil_strpool_add("auditallow");
	CIL_KEY_TUNABLEIF = cil_strpool_add("tunableif");
	CIL_KEY_ALLOW = cil_strpool_add("allow");
	CIL_KEY_DONTAUDIT = cil_strpool_add("dontaudit");
	CIL_KEY_TYPETRANSITION = cil_strpool_add("typetransition");
	CIL_KEY_TYPECHANGE = cil_strpool_add("typechange");
	CIL_KEY_CALL = cil_strpool_add("call");
	CIL_KEY_TUNABLE = cil_strpool_add("tunable");
	CIL_KEY_XOR = cil_strpool_add("xor");
	CIL_KEY_ALL = cil_strpool_add("all");
	CIL_KEY_RANGE = cil_strpool_add("range");
	CIL_KEY_TYPE = cil_strpool_add("type");
	CIL_KEY_ROLE = cil_strpool_add("role");
	CIL_KEY_USER = cil_strpool_add("user");
	CIL_KEY_USERATTRIBUTE = cil_strpool_add("userattribute");
	CIL_KEY_USERATTRIBUTESET = cil_strpool_add("userattributeset");
	CIL_KEY_SENSITIVITY = cil_strpool_add("sensitivity");
	CIL_KEY_CATEGORY = cil_strpool_add("category");
	CIL_KEY_CATSET = cil_strpool_add("categoryset");
	CIL_KEY_LEVEL = cil_strpool_add("level");
	CIL_KEY_LEVELRANGE = cil_strpool_add("levelrange");
	CIL_KEY_CLASS = cil_strpool_add("class");
	CIL_KEY_IPADDR = cil_strpool_add("ipaddr");
	CIL_KEY_MAP_CLASS = cil_strpool_add("classmap");
	CIL_KEY_CLASSPERMISSION = cil_strpool_add("classpermission");
	CIL_KEY_BOOL = cil_strpool_add("boolean");
	CIL_KEY_STRING = cil_strpool_add("string");
	CIL_KEY_NAME = cil_strpool_add("name");
	CIL_KEY_HANDLEUNKNOWN = cil_strpool_add("handleunknown");
	CIL_KEY_HANDLEUNKNOWN_ALLOW = cil_strpool_add("allow");
	CIL_KEY_HANDLEUNKNOWN_DENY = cil_strpool_add("deny");
	CIL_KEY_HANDLEUNKNOWN_REJECT = cil_strpool_add("reject");
	CIL_KEY_BLOCKINHERIT = cil_strpool_add("blockinherit");
	CIL_KEY_BLOCKABSTRACT = cil_strpool_add("blockabstract");
	CIL_KEY_CLASSORDER = cil_strpool_add("classorder");
	CIL_KEY_CLASSMAPPING = cil_strpool_add("classmapping");
	CIL_KEY_CLASSPERMISSIONSET = cil_strpool_add("classpermissionset");
	CIL_KEY_COMMON = cil_strpool_add("common");
	CIL_KEY_CLASSCOMMON = cil_strpool_add("classcommon");
	CIL_KEY_SID = cil_strpool_add("sid");
	CIL_KEY_SIDCONTEXT = cil_strpool_add("sidcontext");
	CIL_KEY_SIDORDER = cil_strpool_add("sidorder");
	CIL_KEY_USERLEVEL = cil_strpool_add("userlevel");
	CIL_KEY_USERRANGE = cil_strpool_add("userrange");
	CIL_KEY_USERBOUNDS = cil_strpool_add("userbounds");
	CIL_KEY_USERPREFIX = cil_strpool_add("userprefix");
	CIL_KEY_SELINUXUSER = cil_strpool_add("selinuxuser");
	CIL_KEY_SELINUXUSERDEFAULT = cil_strpool_add("selinuxuserdefault");
	CIL_KEY_TYPEATTRIBUTE = cil_strpool_add("typeattribute");
	CIL_KEY_TYPEATTRIBUTESET = cil_strpool_add("typeattributeset");
	CIL_KEY_EXPANDTYPEATTRIBUTE = cil_strpool_add("expandtypeattribute");
	CIL_KEY_TYPEALIAS = cil_strpool_add("typealias");
	CIL_KEY_TYPEALIASACTUAL = cil_strpool_add("typealiasactual");
	CIL_KEY_TYPEBOUNDS = cil_strpool_add("typebounds");
	CIL_KEY_TYPEPERMISSIVE = cil_strpool_add("typepermissive");
	CIL_KEY_RANGETRANSITION = cil_strpool_add("rangetransition");
	CIL_KEY_USERROLE = cil_strpool_add("userrole");
	CIL_KEY_ROLETYPE = cil_strpool_add("roletype");
	CIL_KEY_ROLETRANSITION = cil_strpool_add("roletransition");
	CIL_KEY_ROLEALLOW = cil_strpool_add("roleallow");
	CIL_KEY_ROLEATTRIBUTE = cil_strpool_add("roleattribute");
	CIL_KEY_ROLEATTRIBUTESET = cil_strpool_add("roleattributeset");
	CIL_KEY_ROLEBOUNDS = cil_strpool_add("rolebounds");
	CIL_KEY_BOOLEANIF = cil_strpool_add("booleanif");
	CIL_KEY_NEVERALLOW = cil_strpool_add("neverallow");
	CIL_KEY_TYPEMEMBER = cil_strpool_add("typemember");
	CIL_KEY_SENSALIAS = cil_strpool_add("sensitivityalias");
	CIL_KEY_SENSALIASACTUAL = cil_strpool_add("sensitivityaliasactual");
	CIL_KEY_CATALIAS = cil_strpool_add("categoryalias");
	CIL_KEY_CATALIASACTUAL = cil_strpool_add("categoryaliasactual");
	CIL_KEY_CATORDER = cil_strpool_add("categoryorder");
	CIL_KEY_SENSITIVITYORDER = cil_strpool_add("sensitivityorder");
	CIL_KEY_SENSCAT = cil_strpool_add("sensitivitycategory");
	CIL_KEY_CONSTRAIN = cil_strpool_add("constrain");
	CIL_KEY_MLSCONSTRAIN = cil_strpool_add("mlsconstrain");
	CIL_KEY_VALIDATETRANS = cil_strpool_add("validatetrans");
	CIL_KEY_MLSVALIDATETRANS = cil_strpool_add("mlsvalidatetrans");
	CIL_KEY_CONTEXT = cil_strpool_add("context");
	CIL_KEY_FILECON = cil_strpool_add("filecon");
	CIL_KEY_IBPKEYCON = cil_strpool_add("ibpkeycon");
	CIL_KEY_IBENDPORTCON = cil_strpool_add("ibendportcon");
	CIL_KEY_PORTCON = cil_strpool_add("portcon");
	CIL_KEY_NODECON = cil_strpool_add("nodecon");
	CIL_KEY_GENFSCON = cil_strpool_add("genfscon");
	CIL_KEY_NETIFCON = cil_strpool_add("netifcon");
	CIL_KEY_PIRQCON = cil_strpool_add("pirqcon");
	CIL_KEY_IOMEMCON = cil_strpool_add("iomemcon");
	CIL_KEY_IOPORTCON = cil_strpool_add("ioportcon");
	CIL_KEY_PCIDEVICECON = cil_strpool_add("pcidevicecon");
	CIL_KEY_DEVICETREECON = cil_strpool_add("devicetreecon");
	CIL_KEY_FSUSE = cil_strpool_add("fsuse");
	CIL_KEY_POLICYCAP = cil_strpool_add("policycap");
	CIL_KEY_OPTIONAL = cil_strpool_add("optional");
	CIL_KEY_DEFAULTUSER = cil_strpool_add("defaultuser");
	CIL_KEY_DEFAULTROLE = cil_strpool_add("defaultrole");
	CIL_KEY_DEFAULTTYPE = cil_strpool_add("defaulttype");
	CIL_KEY_MACRO = cil_strpool_add("macro");
	CIL_KEY_IN = cil_strpool_add("in");
	CIL_KEY_IN_BEFORE = cil_strpool_add("before");
	CIL_KEY_IN_AFTER = cil_strpool_add("after");
	CIL_KEY_MLS = cil_strpool_add("mls");
	CIL_KEY_DEFAULTRANGE = cil_strpool_add("defaultrange");
	CIL_KEY_GLOB = cil_strpool_add("*");
	CIL_KEY_FILE = cil_strpool_add("file");
	CIL_KEY_DIR = cil_strpool_add("dir");
	CIL_KEY_CHAR = cil_strpool_add("char");
	CIL_KEY_BLOCK = cil_strpool_add("block");
	CIL_KEY_SOCKET = cil_strpool_add("socket");
	CIL_KEY_PIPE = cil_strpool_add("pipe");
	CIL_KEY_SYMLINK = cil_strpool_add("symlink");
	CIL_KEY_ANY = cil_strpool_add("any");
	CIL_KEY_XATTR = cil_strpool_add("xattr");
	CIL_KEY_TASK = cil_strpool_add("task");
	CIL_KEY_TRANS = cil_strpool_add("trans");
	CIL_KEY_SOURCE = cil_strpool_add("source");
	CIL_KEY_TARGET = cil_strpool_add("target");
	CIL_KEY_LOW = cil_strpool_add("low");
	CIL_KEY_HIGH = cil_strpool_add("high");
	CIL_KEY_LOW_HIGH = cil_strpool_add("low-high");
	CIL_KEY_GLBLUB = cil_strpool_add("glblub");
	CIL_KEY_ROOT = cil_strpool_add("<root>");
	CIL_KEY_NODE = cil_strpool_add("<node>");
	CIL_KEY_PERM = cil_strpool_add("perm");
	CIL_KEY_ALLOWX = cil_strpool_add("allowx");
	CIL_KEY_AUDITALLOWX = cil_strpool_add("auditallowx");
	CIL_KEY_DONTAUDITX = cil_strpool_add("dontauditx");
	CIL_KEY_NEVERALLOWX = cil_strpool_add("neverallowx");
	CIL_KEY_PERMISSIONX = cil_strpool_add("permissionx");
	CIL_KEY_IOCTL = cil_strpool_add("ioctl");
	CIL_KEY_UNORDERED = cil_strpool_add("unordered");
	CIL_KEY_SRC_INFO = cil_strpool_add("<src_info>");
	CIL_KEY_SRC_CIL = cil_strpool_add("cil");
	CIL_KEY_SRC_HLL_LMS = cil_strpool_add("lms");
	CIL_KEY_SRC_HLL_LMX = cil_strpool_add("lmx");
	CIL_KEY_SRC_HLL_LME = cil_strpool_add("lme");
}

void cil_db_init(struct cil_db **db)
{
	*db = cil_malloc(sizeof(**db));

	cil_strpool_init();
	cil_init_keys();

	cil_tree_init(&(*db)->parse);
	cil_tree_init(&(*db)->ast);
	cil_root_init((struct cil_root **)&(*db)->ast->root->data);
	(*db)->sidorder = NULL;
	(*db)->classorder = NULL;
	(*db)->catorder = NULL;
	(*db)->sensitivityorder = NULL;
	cil_sort_init(&(*db)->netifcon);
	cil_sort_init(&(*db)->genfscon);
	cil_sort_init(&(*db)->filecon);
	cil_sort_init(&(*db)->nodecon);
	cil_sort_init(&(*db)->ibpkeycon);
	cil_sort_init(&(*db)->ibendportcon);
	cil_sort_init(&(*db)->portcon);
	cil_sort_init(&(*db)->pirqcon);
	cil_sort_init(&(*db)->iomemcon);
	cil_sort_init(&(*db)->ioportcon);
	cil_sort_init(&(*db)->pcidevicecon);
	cil_sort_init(&(*db)->devicetreecon);
	cil_sort_init(&(*db)->fsuse);
	cil_list_init(&(*db)->userprefixes, CIL_LIST_ITEM);
	cil_list_init(&(*db)->selinuxusers, CIL_LIST_ITEM);
	cil_list_init(&(*db)->names, CIL_LIST_ITEM);

	cil_type_init(&(*db)->selftype);
	(*db)->selftype->datum.name = CIL_KEY_SELF;
	(*db)->selftype->datum.fqn = CIL_KEY_SELF;
	(*db)->num_types_and_attrs = 0;
	(*db)->num_classes = 0;
	(*db)->num_types = 0;
	(*db)->num_roles = 0;
	(*db)->num_users = 0;
	(*db)->num_cats = 0;
	(*db)->val_to_type = NULL;
	(*db)->val_to_role = NULL;
	(*db)->val_to_user = NULL;

	(*db)->disable_dontaudit = CIL_FALSE;
	(*db)->disable_neverallow = CIL_FALSE;
	(*db)->attrs_expand_generated = CIL_FALSE;
	(*db)->attrs_expand_size = 1;
	(*db)->preserve_tunables = CIL_FALSE;
	(*db)->handle_unknown = -1;
	(*db)->mls = -1;
	(*db)->multiple_decls = CIL_FALSE;
	(*db)->qualified_names = CIL_FALSE;
	(*db)->target_platform = SEPOL_TARGET_SELINUX;
	(*db)->policy_version = POLICYDB_VERSION_MAX;
}

void cil_db_destroy(struct cil_db **db)
{
	if (db == NULL || *db == NULL) {
		return;
	}

	cil_tree_destroy(&(*db)->parse);
	cil_tree_destroy(&(*db)->ast);
	cil_list_destroy(&(*db)->sidorder, CIL_FALSE);
	cil_list_destroy(&(*db)->classorder, CIL_FALSE);
	cil_list_destroy(&(*db)->catorder, CIL_FALSE);
	cil_list_destroy(&(*db)->sensitivityorder, CIL_FALSE);
	cil_sort_destroy(&(*db)->netifcon);
	cil_sort_destroy(&(*db)->genfscon);
	cil_sort_destroy(&(*db)->filecon);
	cil_sort_destroy(&(*db)->nodecon);
	cil_sort_destroy(&(*db)->ibpkeycon);
	cil_sort_destroy(&(*db)->ibendportcon);
	cil_sort_destroy(&(*db)->portcon);
	cil_sort_destroy(&(*db)->pirqcon);
	cil_sort_destroy(&(*db)->iomemcon);
	cil_sort_destroy(&(*db)->ioportcon);
	cil_sort_destroy(&(*db)->pcidevicecon);
	cil_sort_destroy(&(*db)->devicetreecon);
	cil_sort_destroy(&(*db)->fsuse);
	cil_list_destroy(&(*db)->userprefixes, CIL_FALSE);
	cil_list_destroy(&(*db)->selinuxusers, CIL_FALSE);
	cil_list_destroy(&(*db)->names, CIL_TRUE);

	cil_destroy_type((*db)->selftype);

	cil_strpool_destroy();
	free((*db)->val_to_type);
	free((*db)->val_to_role);
	free((*db)->val_to_user);

	free(*db);
	*db = NULL;	
}

void cil_root_init(struct cil_root **root)
{
	struct cil_root *r = cil_malloc(sizeof(*r));
	cil_symtab_array_init(r->symtab, cil_sym_sizes[CIL_SYM_ARRAY_ROOT]);

	*root = r;
}

void cil_root_destroy(struct cil_root *root)
{
	if (root == NULL) {
		return;
	}
	cil_symtab_array_destroy(root->symtab);
	free(root);
}

int cil_add_file(cil_db_t *db, const char *name, const char *data, size_t size)
{
	char *buffer = NULL;
	int rc;

	cil_log(CIL_INFO, "Parsing %s\n", name);

	buffer = cil_malloc(size + 2);
	memcpy(buffer, data, size);
	memset(buffer + size, 0, 2);

	rc = cil_parser(name, buffer, size + 2, &db->parse);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed to parse %s\n", name);
		goto exit;
	}

	free(buffer);
	buffer = NULL;

	rc = SEPOL_OK;

exit:
	free(buffer);

	return rc;
}

int cil_compile(struct cil_db *db)
{
	int rc = SEPOL_ERR;

	if (db == NULL) {
		goto exit;
	}

	cil_log(CIL_INFO, "Building AST from Parse Tree\n");
	rc = cil_build_ast(db, db->parse->root, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to build AST\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Destroying Parse Tree\n");
	cil_tree_destroy(&db->parse);

	cil_log(CIL_INFO, "Resolving AST\n");
	rc = cil_resolve_ast(db, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to resolve AST\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Qualifying Names\n");
	rc = cil_fqn_qualify(db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to qualify names\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Compile post process\n");
	rc = cil_post_process(db);
	if (rc != SEPOL_OK ) {
		cil_log(CIL_ERR, "Post process failed\n");
		goto exit;
	}

exit:

	return rc;
}

int cil_write_parse_ast(FILE *out, cil_db_t *db)
{
	int rc = SEPOL_ERR;

	if (db == NULL) {
		goto exit;
	}

	cil_log(CIL_INFO, "Writing Parse AST\n");
	rc = cil_write_ast(out, CIL_WRITE_AST_PHASE_PARSE, db->parse->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to write parse ast\n");
		goto exit;
	}

exit:
	return rc;
}

int cil_write_build_ast(FILE *out, cil_db_t *db)
{
	int rc = SEPOL_ERR;

	if (db == NULL) {
		goto exit;
	}

	cil_log(CIL_INFO, "Building AST from Parse Tree\n");
	rc = cil_build_ast(db, db->parse->root, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to build ast\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Destroying Parse Tree\n");
	cil_tree_destroy(&db->parse);

	cil_log(CIL_INFO, "Writing Build AST\n");
	rc = cil_write_ast(out, CIL_WRITE_AST_PHASE_BUILD, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to write build ast\n");
		goto exit;
	}

exit:
	return rc;
}

int cil_write_resolve_ast(FILE *out, cil_db_t *db)
{
	int rc = SEPOL_ERR;

	if (db == NULL) {
		goto exit;
	}

	cil_log(CIL_INFO, "Building AST from Parse Tree\n");
	rc = cil_build_ast(db, db->parse->root, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to build ast\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Destroying Parse Tree\n");
	cil_tree_destroy(&db->parse);

	cil_log(CIL_INFO, "Resolving AST\n");
	rc = cil_resolve_ast(db, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to resolve ast\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Qualifying Names\n");
	rc = cil_fqn_qualify(db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to qualify names\n");
		goto exit;
	}

	cil_log(CIL_INFO, "Writing Resolve AST\n");
	rc = cil_write_ast(out, CIL_WRITE_AST_PHASE_RESOLVE, db->ast->root);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to write resolve ast\n");
		goto exit;
	}

exit:
	return rc;
}

int cil_build_policydb(cil_db_t *db, sepol_policydb_t **sepol_db)
{
	int rc;

	cil_log(CIL_INFO, "Building policy binary\n");
	rc = cil_binary_create(db, sepol_db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to generate binary\n");
		goto exit;
	}

exit:
	return rc;
}

void cil_write_policy_conf(FILE *out, struct cil_db *db)
{
	cil_log(CIL_INFO, "Writing policy.conf file\n");
	cil_gen_policy(out, db);
}

void cil_destroy_data(void **data, enum cil_flavor flavor)
{
	if (*data == NULL) {
		return;
	}

	switch(flavor) {
	case CIL_NONE:
		break;
	case CIL_ROOT:
		cil_root_destroy(*data);
		break;
	case CIL_NODE:
		break;
	case CIL_STRING:
		break;
	case CIL_DATUM:
		break;
	case CIL_LIST:
		cil_list_destroy(*data, CIL_FALSE);
		break;
	case CIL_LIST_ITEM:
		break;
	case CIL_PARAM:
		cil_destroy_param(*data);
		break;
	case CIL_ARGS:
		cil_destroy_args(*data);
		break;
	case CIL_BLOCK:
		cil_destroy_block(*data);
		break;
	case CIL_BLOCKINHERIT:
		cil_destroy_blockinherit(*data);
		break;
	case CIL_BLOCKABSTRACT:
		cil_destroy_blockabstract(*data);
		break;
	case CIL_IN:
		cil_destroy_in(*data);
		break;
	case CIL_MACRO:
		cil_destroy_macro(*data);
		break;
	case CIL_CALL:
		cil_destroy_call(*data);
		break;
	case CIL_OPTIONAL:
		cil_destroy_optional(*data);
		break;
	case CIL_BOOL:
		cil_destroy_bool(*data);
		break;
	case CIL_BOOLEANIF:
		cil_destroy_boolif(*data);
		break;
	case CIL_TUNABLE:
		cil_destroy_tunable(*data);
		break;
	case CIL_TUNABLEIF:
		cil_destroy_tunif(*data);
		break;
	case CIL_CONDBLOCK:
		cil_destroy_condblock(*data);
		break;
	case CIL_CONDTRUE:
		break;
	case CIL_CONDFALSE:
		break;
	case CIL_PERM:
	case CIL_MAP_PERM:
		cil_destroy_perm(*data);
		break;
	case CIL_COMMON:
	case CIL_CLASS:
	case CIL_MAP_CLASS:
		cil_destroy_class(*data);
		break;
	case CIL_CLASSORDER:
		cil_destroy_classorder(*data);
		break;
	case CIL_CLASSPERMISSION:
		cil_destroy_classpermission(*data);
		break;
	case CIL_CLASSCOMMON:
		cil_destroy_classcommon(*data);
		break;
	case CIL_CLASSMAPPING:
		cil_destroy_classmapping(*data);
		break;
	case CIL_CLASSPERMS:
		cil_destroy_classperms(*data);
		break;
	case CIL_CLASSPERMS_SET:
		cil_destroy_classperms_set(*data);
		break;
	case CIL_CLASSPERMISSIONSET:
		cil_destroy_classpermissionset(*data);
		break;
	case CIL_USER:
		cil_destroy_user(*data);
		break;
	case CIL_USERATTRIBUTE:
		cil_destroy_userattribute(*data);
		break;
	case CIL_USERATTRIBUTESET:
		cil_destroy_userattributeset(*data);
		break;
	case CIL_USERPREFIX:
		cil_destroy_userprefix(*data);
		break;
	case CIL_USERROLE:
		cil_destroy_userrole(*data);
		break;
	case CIL_USERLEVEL:
		cil_destroy_userlevel(*data);
		break;
	case CIL_USERRANGE:
		cil_destroy_userrange(*data);
		break;
	case CIL_USERBOUNDS:
		cil_destroy_bounds(*data);
		break;
	case CIL_SELINUXUSER:
	case CIL_SELINUXUSERDEFAULT:
		cil_destroy_selinuxuser(*data);
		break;
	case CIL_ROLE:
		cil_destroy_role(*data);
		break;
	case CIL_ROLEATTRIBUTE:
		cil_destroy_roleattribute(*data);
		break;
	case CIL_ROLEATTRIBUTESET:
		cil_destroy_roleattributeset(*data);
		break;
	case CIL_ROLETYPE:
		cil_destroy_roletype(*data);
		break;
	case CIL_ROLEBOUNDS:
		cil_destroy_bounds(*data);
		break;
	case CIL_TYPE:
		cil_destroy_type(*data);
		break;
	case CIL_TYPEATTRIBUTE:
		cil_destroy_typeattribute(*data);
		break;
	case CIL_TYPEALIAS:
		cil_destroy_alias(*data);
		break;
	case CIL_TYPEATTRIBUTESET:
		cil_destroy_typeattributeset(*data);
		break;
	case CIL_EXPANDTYPEATTRIBUTE:
		cil_destroy_expandtypeattribute(*data);
		break;
	case CIL_TYPEALIASACTUAL:
		cil_destroy_aliasactual(*data);
		break;
	case CIL_TYPEBOUNDS:
		cil_destroy_bounds(*data);
		break;
	case CIL_TYPEPERMISSIVE:
		cil_destroy_typepermissive(*data);
		break;
	case CIL_SENS:
		cil_destroy_sensitivity(*data);
		break;
	case CIL_SENSALIAS:
		cil_destroy_alias(*data);
		break;
	case CIL_SENSALIASACTUAL:
		cil_destroy_aliasactual(*data);
		break;
	case CIL_SENSITIVITYORDER:
		cil_destroy_sensitivityorder(*data);
		break;
	case CIL_SENSCAT:
		cil_destroy_senscat(*data);
		break;
	case CIL_CAT:
		cil_destroy_category(*data);
		break;
	case CIL_CATSET:
		cil_destroy_catset(*data);
		break;
	case CIL_CATALIAS:
		cil_destroy_alias(*data);
		break;
	case CIL_CATALIASACTUAL:
		cil_destroy_aliasactual(*data);
		break;
	case CIL_CATORDER:
		cil_destroy_catorder(*data);
		break;
	case CIL_LEVEL:
		cil_destroy_level(*data);
		break;
	case CIL_LEVELRANGE:
		cil_destroy_levelrange(*data);
		break;
	case CIL_SID:
		cil_destroy_sid(*data);
		break;
	case CIL_SIDORDER:
		cil_destroy_sidorder(*data);
		break;
	case CIL_NAME:
		cil_destroy_name(*data);
		break;
	case CIL_ROLEALLOW:
		cil_destroy_roleallow(*data);
		break;
	case CIL_AVRULE:
	case CIL_AVRULEX:
		cil_destroy_avrule(*data);
		break;
	case CIL_PERMISSIONX:
		cil_destroy_permissionx(*data);
		break;
	case CIL_ROLETRANSITION:
		cil_destroy_roletransition(*data);
		break;
	case CIL_TYPE_RULE:
		cil_destroy_type_rule(*data);
		break;
	case CIL_NAMETYPETRANSITION:
		cil_destroy_typetransition(*data);
		break;
	case CIL_RANGETRANSITION:
		cil_destroy_rangetransition(*data);
		break;
	case CIL_CONSTRAIN:
		cil_destroy_constrain(*data);
		break;
	case CIL_MLSCONSTRAIN:
		cil_destroy_constrain(*data);
		break;
	case CIL_VALIDATETRANS:
	case CIL_MLSVALIDATETRANS:
		cil_destroy_validatetrans(*data);
		break;
	case CIL_CONTEXT:
		cil_destroy_context(*data);
		break;
	case CIL_IPADDR:
		cil_destroy_ipaddr(*data);
		break;
	case CIL_SIDCONTEXT:
		cil_destroy_sidcontext(*data);
		break;
	case CIL_FSUSE:
		cil_destroy_fsuse(*data);
		break;
	case CIL_FILECON:
		cil_destroy_filecon(*data);
		break;
	case CIL_IBPKEYCON:
		cil_destroy_ibpkeycon(*data);
		break;
	case CIL_PORTCON:
		cil_destroy_portcon(*data);
		break;
	case CIL_IBENDPORTCON:
		cil_destroy_ibendportcon(*data);
		break;
	case CIL_NODECON:
		cil_destroy_nodecon(*data);
		break;
	case CIL_GENFSCON:
		cil_destroy_genfscon(*data);
		break;
	case CIL_NETIFCON:
		cil_destroy_netifcon(*data);
		break;
	case CIL_PIRQCON:
		cil_destroy_pirqcon(*data);
		break;
	case CIL_IOMEMCON:
		cil_destroy_iomemcon(*data);
		break;
	case CIL_IOPORTCON:
		cil_destroy_ioportcon(*data);
		break;
	case CIL_PCIDEVICECON:
		cil_destroy_pcidevicecon(*data);
		break;
	case CIL_DEVICETREECON:
		cil_destroy_devicetreecon(*data);
		break;
	case CIL_POLICYCAP:
		cil_destroy_policycap(*data);
		break;
	case CIL_DEFAULTUSER:
	case CIL_DEFAULTROLE:
	case CIL_DEFAULTTYPE:
		cil_destroy_default(*data);
		break;
	case CIL_DEFAULTRANGE:
		cil_destroy_defaultrange(*data);
		break;
	case CIL_HANDLEUNKNOWN:
		cil_destroy_handleunknown(*data);
		break;
	case CIL_MLS:
		cil_destroy_mls(*data);
		break;
	case CIL_SRC_INFO:
		cil_destroy_src_info(*data);
		break;
	case CIL_OP:
	case CIL_CONS_OPERAND:
		break;
	default:
		cil_log(CIL_INFO, "Unknown data flavor: %d\n", flavor);
		break;
	}

	*data = NULL;
}

int cil_flavor_to_symtab_index(enum cil_flavor flavor, enum cil_sym_index *sym_index)
{
	if (flavor < CIL_MIN_DECLARATIVE) {
		return SEPOL_ERR;
	}

	switch(flavor) {
	case CIL_BLOCK:
		*sym_index = CIL_SYM_BLOCKS;
		break;
	case CIL_MACRO:
		*sym_index = CIL_SYM_BLOCKS;
		break;
	case CIL_OPTIONAL:
		*sym_index = CIL_SYM_BLOCKS;
		break;
	case CIL_BOOL:
		*sym_index = CIL_SYM_BOOLS;
		break;
	case CIL_TUNABLE:
		*sym_index = CIL_SYM_TUNABLES;
		break;
	case CIL_PERM:
	case CIL_MAP_PERM:
		*sym_index = CIL_SYM_PERMS;
		break;
	case CIL_COMMON:
		*sym_index = CIL_SYM_COMMONS;
		break;
	case CIL_CLASS:
	case CIL_MAP_CLASS:
		*sym_index = CIL_SYM_CLASSES;
		break;
	case CIL_CLASSPERMISSION:
	case CIL_CLASSPERMISSIONSET:
		*sym_index = CIL_SYM_CLASSPERMSETS;
		break;
	case CIL_USER:
	case CIL_USERATTRIBUTE:
		*sym_index = CIL_SYM_USERS;
		break;
	case CIL_ROLE:
	case CIL_ROLEATTRIBUTE:
		*sym_index = CIL_SYM_ROLES;
		break;
	case CIL_TYPE:
	case CIL_TYPEALIAS:
	case CIL_TYPEATTRIBUTE:
		*sym_index = CIL_SYM_TYPES;
		break;
	case CIL_SENS:
	case CIL_SENSALIAS:
		*sym_index = CIL_SYM_SENS;
		break;
	case CIL_CAT:
	case CIL_CATSET:
	case CIL_CATALIAS:
		*sym_index = CIL_SYM_CATS;
		break;
	case CIL_LEVEL:
		*sym_index = CIL_SYM_LEVELS;
		break;
	case CIL_LEVELRANGE:
		*sym_index = CIL_SYM_LEVELRANGES;
		break;
	case CIL_SID:
		*sym_index = CIL_SYM_SIDS;
		break;
	case CIL_NAME:
		*sym_index = CIL_SYM_NAMES;
		break;
	case CIL_CONTEXT:
		*sym_index = CIL_SYM_CONTEXTS;
		break;
	case CIL_IPADDR:
		*sym_index = CIL_SYM_IPADDRS;
		break;
	case CIL_POLICYCAP:
		*sym_index = CIL_SYM_POLICYCAPS;
		break;
	case CIL_PERMISSIONX:
		*sym_index = CIL_SYM_PERMX;
		break;
	default:
		*sym_index = CIL_SYM_UNKNOWN;
		cil_log(CIL_INFO, "Failed to find flavor: %d\n", flavor);
		return SEPOL_ERR;
	}

	return SEPOL_OK;
}

const char * cil_node_to_string(struct cil_tree_node *node)
{
	switch (node->flavor) {
	case CIL_NONE:
		return "<none>";
	case CIL_ROOT:
		return CIL_KEY_ROOT;
	case CIL_NODE:
		return CIL_KEY_NODE;
	case CIL_STRING:
		return "string";
	case CIL_DATUM:
		return "<datum>";
	case CIL_LIST:
		return "<list>";
	case CIL_LIST_ITEM:
		return "<list_item>";
	case CIL_PARAM:
		return "<param>";
	case CIL_ARGS:
		return "<args>";
	case CIL_BLOCK:
		return CIL_KEY_BLOCK;
	case CIL_BLOCKINHERIT:
		return CIL_KEY_BLOCKINHERIT;
	case CIL_BLOCKABSTRACT:
		return CIL_KEY_BLOCKABSTRACT;
	case CIL_IN:
		return CIL_KEY_IN;
	case CIL_MACRO:
		return CIL_KEY_MACRO;
	case CIL_CALL:
		return CIL_KEY_CALL;
	case CIL_OPTIONAL:
		return CIL_KEY_OPTIONAL;
	case CIL_BOOL:
		return CIL_KEY_BOOL;
	case CIL_BOOLEANIF:
		return CIL_KEY_BOOLEANIF;
	case CIL_TUNABLE:
		return CIL_KEY_TUNABLE;
	case CIL_TUNABLEIF:
		return CIL_KEY_TUNABLEIF;
	case CIL_CONDBLOCK:
		switch (((struct cil_condblock*)node->data)->flavor) {
		case CIL_CONDTRUE:
			return CIL_KEY_CONDTRUE;
		case CIL_CONDFALSE:
			return CIL_KEY_CONDFALSE;
		default:
			break;
		}
		break;
	case CIL_CONDTRUE:
		return CIL_KEY_CONDTRUE;
	case CIL_CONDFALSE:
		return CIL_KEY_CONDFALSE;
	case CIL_PERM:
		return CIL_KEY_PERM;
	case CIL_COMMON:
		return CIL_KEY_COMMON;
	case CIL_CLASS:
		return CIL_KEY_CLASS;
	case CIL_CLASSORDER:
		return CIL_KEY_CLASSORDER;
	case CIL_MAP_CLASS:
		return CIL_KEY_MAP_CLASS;
	case CIL_CLASSPERMISSION:
		return CIL_KEY_CLASSPERMISSION;
	case CIL_CLASSCOMMON:
		return CIL_KEY_CLASSCOMMON;
	case CIL_CLASSMAPPING:
		return CIL_KEY_CLASSMAPPING;
	case CIL_CLASSPERMISSIONSET:
		return CIL_KEY_CLASSPERMISSIONSET;
	case CIL_USER:
		return CIL_KEY_USER;
	case CIL_USERATTRIBUTE:
		return CIL_KEY_USERATTRIBUTE;
	case CIL_USERATTRIBUTESET:
		return CIL_KEY_USERATTRIBUTESET;
	case CIL_USERPREFIX:
		return CIL_KEY_USERPREFIX;
	case CIL_USERROLE:
		return CIL_KEY_USERROLE;
	case CIL_USERLEVEL:
		return CIL_KEY_USERLEVEL;
	case CIL_USERRANGE:
		return CIL_KEY_USERRANGE;
	case CIL_USERBOUNDS:
		return CIL_KEY_USERBOUNDS;
	case CIL_SELINUXUSER:
		return CIL_KEY_SELINUXUSER;
	case CIL_SELINUXUSERDEFAULT:
		return CIL_KEY_SELINUXUSERDEFAULT;
	case CIL_ROLE:
		return CIL_KEY_ROLE;
	case CIL_ROLEATTRIBUTE:
		return CIL_KEY_ROLEATTRIBUTE;
	case CIL_ROLEATTRIBUTESET:
		return CIL_KEY_ROLEATTRIBUTESET;
	case CIL_ROLETYPE:
		return CIL_KEY_ROLETYPE;
	case CIL_ROLEBOUNDS:
		return CIL_KEY_ROLEBOUNDS;
	case CIL_TYPE:
		return CIL_KEY_TYPE;
	case CIL_TYPEATTRIBUTE:
		return CIL_KEY_TYPEATTRIBUTE;
	case CIL_TYPEALIAS:
		return CIL_KEY_TYPEALIAS;
	case CIL_TYPEATTRIBUTESET:
		return CIL_KEY_TYPEATTRIBUTESET;
	case CIL_EXPANDTYPEATTRIBUTE:
		return CIL_KEY_EXPANDTYPEATTRIBUTE;
	case CIL_TYPEALIASACTUAL:
		return CIL_KEY_TYPEALIASACTUAL;
	case CIL_TYPEBOUNDS:
		return CIL_KEY_TYPEBOUNDS;
	case CIL_TYPEPERMISSIVE:
		return CIL_KEY_TYPEPERMISSIVE;
	case CIL_SENS:
		return CIL_KEY_SENSITIVITY;
	case CIL_SENSALIAS:
		return CIL_KEY_SENSALIAS;
	case CIL_SENSALIASACTUAL:
		return CIL_KEY_SENSALIASACTUAL;
	case CIL_SENSITIVITYORDER:
		return CIL_KEY_SENSITIVITYORDER;
	case CIL_SENSCAT:
		return CIL_KEY_SENSCAT;
	case CIL_CAT:
		return CIL_KEY_CATEGORY;
	case CIL_CATSET:
		return CIL_KEY_CATSET;
	case CIL_CATALIAS:
		return CIL_KEY_CATALIAS;
	case CIL_CATALIASACTUAL:
		return CIL_KEY_CATALIASACTUAL;
	case CIL_CATORDER:
		return CIL_KEY_CATORDER;
	case CIL_LEVEL:
		return CIL_KEY_LEVEL;
	case CIL_LEVELRANGE:
		return CIL_KEY_LEVELRANGE;
	case CIL_SID:
		return CIL_KEY_SID;
	case CIL_SIDORDER:
		return CIL_KEY_SIDORDER;
	case CIL_NAME:
		return CIL_KEY_NAME;
	case CIL_ROLEALLOW:
		return CIL_KEY_ROLEALLOW;
	case CIL_AVRULE:
		switch (((struct cil_avrule *)node->data)->rule_kind) {
		case CIL_AVRULE_ALLOWED:
			return CIL_KEY_ALLOW;
		case CIL_AVRULE_AUDITALLOW:
			return CIL_KEY_AUDITALLOW;
		case CIL_AVRULE_DONTAUDIT:
			return CIL_KEY_DONTAUDIT;
		case CIL_AVRULE_NEVERALLOW:
			return CIL_KEY_NEVERALLOW;
		default:
			break;
		}
		break;
	case CIL_AVRULEX:
		switch (((struct cil_avrule *)node->data)->rule_kind) {
		case CIL_AVRULE_ALLOWED:
			return CIL_KEY_ALLOWX;
		case CIL_AVRULE_AUDITALLOW:
			return CIL_KEY_AUDITALLOWX;
		case CIL_AVRULE_DONTAUDIT:
			return CIL_KEY_DONTAUDITX;
		case CIL_AVRULE_NEVERALLOW:
			return CIL_KEY_NEVERALLOWX;
		default:
			break;
		}
		break;
	case CIL_PERMISSIONX:
		return CIL_KEY_PERMISSIONX;
	case CIL_ROLETRANSITION:
		return CIL_KEY_ROLETRANSITION;
	case CIL_TYPE_RULE:
		switch (((struct cil_type_rule *)node->data)->rule_kind) {
		case CIL_TYPE_TRANSITION:
			return CIL_KEY_TYPETRANSITION;
		case CIL_TYPE_MEMBER:
			return CIL_KEY_TYPEMEMBER;
		case CIL_TYPE_CHANGE:
			return CIL_KEY_TYPECHANGE;
		default:
			break;
		}
		break;
	case CIL_NAMETYPETRANSITION:
		return CIL_KEY_TYPETRANSITION;
	case CIL_RANGETRANSITION:
		return CIL_KEY_RANGETRANSITION;
	case CIL_CONSTRAIN:
		return CIL_KEY_CONSTRAIN;
	case CIL_MLSCONSTRAIN:
		return CIL_KEY_MLSCONSTRAIN;
	case CIL_VALIDATETRANS:
		return CIL_KEY_VALIDATETRANS;
	case CIL_MLSVALIDATETRANS:
		return CIL_KEY_MLSVALIDATETRANS;
	case CIL_CONTEXT:
		return CIL_KEY_CONTEXT;
	case CIL_IPADDR:
		return CIL_KEY_IPADDR;
	case CIL_SIDCONTEXT:
		return CIL_KEY_SIDCONTEXT;
	case CIL_FSUSE:
		return CIL_KEY_FSUSE;
	case CIL_FILECON:
		return CIL_KEY_FILECON;
	case CIL_IBPKEYCON:
		return CIL_KEY_IBPKEYCON;
	case CIL_IBENDPORTCON:
		return CIL_KEY_IBENDPORTCON;
	case CIL_PORTCON:
		return CIL_KEY_PORTCON;
	case CIL_NODECON:
		return CIL_KEY_NODECON;
	case CIL_GENFSCON:
		return CIL_KEY_GENFSCON;
	case CIL_NETIFCON:
		return CIL_KEY_NETIFCON;
	case CIL_PIRQCON:
		return CIL_KEY_PIRQCON;
	case CIL_IOMEMCON:
		return CIL_KEY_IOMEMCON;
	case CIL_IOPORTCON:
		return CIL_KEY_IOPORTCON;
	case CIL_PCIDEVICECON:
		return CIL_KEY_PCIDEVICECON;
	case CIL_DEVICETREECON:
		return CIL_KEY_DEVICETREECON;
	case CIL_POLICYCAP:
		return CIL_KEY_POLICYCAP;
	case CIL_DEFAULTUSER:
		return CIL_KEY_DEFAULTUSER;
	case CIL_DEFAULTROLE:
		return CIL_KEY_DEFAULTROLE;
	case CIL_DEFAULTTYPE:
		return CIL_KEY_DEFAULTTYPE;
	case CIL_DEFAULTRANGE:
		return CIL_KEY_DEFAULTRANGE;
	case CIL_HANDLEUNKNOWN:
		return CIL_KEY_HANDLEUNKNOWN;
	case CIL_MLS:
		return CIL_KEY_MLS;
	case CIL_SRC_INFO:
		return CIL_KEY_SRC_INFO;
	case CIL_ALL:
		return CIL_KEY_ALL;
	case CIL_RANGE:
		return CIL_KEY_RANGE;
	case CIL_AND:
		return CIL_KEY_AND;
	case CIL_OR:
		return CIL_KEY_OR;
	case CIL_XOR:
		return CIL_KEY_XOR;
	case CIL_NOT:
		return CIL_KEY_NOT;
	case CIL_EQ:
		return CIL_KEY_EQ;
	case CIL_NEQ:
		return CIL_KEY_NEQ;
	case CIL_CONS_DOM:
		return CIL_KEY_CONS_DOM;
	case CIL_CONS_DOMBY:
		return CIL_KEY_CONS_DOMBY;
	case CIL_CONS_INCOMP:
		return CIL_KEY_CONS_INCOMP;
	case CIL_CONS_U1:
		return CIL_KEY_CONS_U1;
	case CIL_CONS_U2:
		return CIL_KEY_CONS_U2;
	case CIL_CONS_U3:
		return CIL_KEY_CONS_U3;
	case CIL_CONS_T1:
		return CIL_KEY_CONS_T1;
	case CIL_CONS_T2:
		return CIL_KEY_CONS_T2;
	case CIL_CONS_T3:
		return CIL_KEY_CONS_T3;
	case CIL_CONS_R1:
		return CIL_KEY_CONS_R1;
	case CIL_CONS_R2:
		return CIL_KEY_CONS_R2;
	case CIL_CONS_R3:
		return CIL_KEY_CONS_R3;
	case CIL_CONS_L1:
		return CIL_KEY_CONS_L1;
	case CIL_CONS_L2:
		return CIL_KEY_CONS_L2;
	case CIL_CONS_H1:
		return CIL_KEY_CONS_H1;
	case CIL_CONS_H2:
		return CIL_KEY_CONS_H2;

	default:
		break;
	}

	return "<unknown>";
}

int cil_userprefixes_to_string(struct cil_db *db, char **out, size_t *size)
{
	int rc = SEPOL_ERR;
	size_t str_len = 0;
	int buf_pos = 0;
	char *str_tmp = NULL;
	struct cil_list_item *curr;
	struct cil_userprefix *userprefix = NULL;
	struct cil_user *user = NULL;

	*out = NULL;

	if (db->userprefixes->head == NULL) {
		rc = SEPOL_OK;
		*size = 0;
		goto exit;
	}

	cil_list_for_each(curr, db->userprefixes) {
		userprefix = curr->data;
		user = userprefix->user;
		str_len += strlen("user ") + strlen(user->datum.fqn) + strlen(" prefix ") + strlen(userprefix->prefix_str) + 2;
	}

	*size = str_len * sizeof(char);
	str_len++;
	str_tmp = cil_malloc(str_len * sizeof(char));
	*out = str_tmp;

	cil_list_for_each(curr, db->userprefixes) {
		userprefix = curr->data;
		user = userprefix->user;

		buf_pos = snprintf(str_tmp, str_len, "user %s prefix %s;\n", user->datum.fqn,
									userprefix->prefix_str);
		if (buf_pos < 0) {
			free(str_tmp);
			*size = 0;
			*out = NULL;
			goto exit;
		}
		str_len -= buf_pos;
		str_tmp += buf_pos;
	}

	rc = SEPOL_OK;
exit:
	return rc;

}

static int cil_cats_to_ebitmap(struct cil_cats *cats, struct ebitmap* cats_ebitmap)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *i;
	struct cil_list_item *j;
	struct cil_cat* cat;
	struct cil_catset *cs;
	struct cil_tree_node *node;

	if (cats == NULL) {
		rc = SEPOL_OK;
		goto exit;
	}

	cil_list_for_each(i, cats->datum_expr) {
		node = NODE(i->data);
		if (node->flavor == CIL_CATSET) {
			cs = (struct cil_catset*)i->data;
			cil_list_for_each(j, cs->cats->datum_expr) {
				cat = (struct cil_cat*)j->data;
				rc = ksu_ebitmap_set_bit(cats_ebitmap, cat->value, 1);
				if (rc != SEPOL_OK) {
					goto exit;
				}
			}
		} else {
			cat = (struct cil_cat*)i->data;
			rc = ksu_ebitmap_set_bit(cats_ebitmap, cat->value, 1);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int cil_level_equals(struct cil_level *low, struct cil_level *high)
{
	int rc;
	struct ebitmap elow;
	struct ebitmap ehigh;

	if (strcmp(low->sens->datum.fqn, high->sens->datum.fqn)) {
		rc = 0;
		goto exit;
	}

	ebitmap_init(&elow);
	ebitmap_init(&ehigh);

	rc = cil_cats_to_ebitmap(low->cats, &elow);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	rc = cil_cats_to_ebitmap(high->cats, &ehigh);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	rc = ksu_ebitmap_cmp(&elow, &ehigh);
	ksu_ebitmap_destroy(&elow);
	ksu_ebitmap_destroy(&ehigh);

exit:
	return rc;
}

static int __cil_level_strlen(struct cil_level *lvl)
{
	struct cil_list_item *item;
	struct cil_cats *cats = lvl->cats;
	int str_len = 0;
	char *str1 = NULL;
	char *str2 = NULL;
	int first = -1;
	int last = -1;

	str_len += strlen(lvl->sens->datum.fqn);

	if (cats && cats->datum_expr != NULL) {
		str_len++; /* initial ":" */
		cil_list_for_each(item, cats->datum_expr) {
			struct cil_cat *cat = item->data;
			if (first == -1) {
				str1 = cat->datum.fqn;
				first = cat->value;
				last = first;
			} else if (cat->value == last + 1) {
				last++;
				str2 = cat->datum.fqn;
			} else {
				if (first == last) {
					str_len += strlen(str1) + strlen(cat->datum.fqn) + 1;
				} else if (last == first + 1) {
					str_len += strlen(str1) + strlen(str2) + strlen(cat->datum.fqn) + 2;
				} else {
					str_len += strlen(str1) + strlen(str2) + strlen(cat->datum.fqn) + 2;
				}
				first = -1;
				last = -1;
				if (item->next != NULL) {
					str_len++; /* space for "," after */
				}
			}
		}
		if (first != -1) {
			if (first == last) {
				str_len += strlen(str1);
			} else if (last == first + 1) {
				str_len += strlen(str1) + strlen(str2) + 1;
			} else {
				str_len += strlen(str1) + strlen(str2) + 1;
			}
		}
	}

	return str_len;
}

static int __cil_level_to_string(struct cil_level *lvl, char *out)
{
	struct cil_list_item *item;
	struct cil_cats *cats = lvl->cats;
	int buf_pos = 0;
	char *str_tmp = out;
	char *str1 = NULL;
	char *str2 = NULL;
	int first = -1;
	int last = -1;

	buf_pos = sprintf(str_tmp, "%s", lvl->sens->datum.fqn);
	str_tmp += buf_pos;

	if (cats && cats->datum_expr != NULL) {
		buf_pos = sprintf(str_tmp, ":");
		str_tmp += buf_pos;

		cil_list_for_each(item, cats->datum_expr) {
			struct cil_cat *cat = item->data;
			if (first == -1) {
				str1 = cat->datum.fqn;
				first = cat->value;
				last = first;
			} else if (cat->value == last + 1) {
				last++;
				str2 = cat->datum.fqn;
			} else {
				if (first == last) {
					buf_pos = sprintf(str_tmp, "%s,%s", str1, cat->datum.fqn);
					str_tmp += buf_pos;
				} else if (last == first + 1) {
					buf_pos = sprintf(str_tmp, "%s,%s,%s", str1, str2, cat->datum.fqn);
					str_tmp += buf_pos;
				} else {
					buf_pos = sprintf(str_tmp, "%s.%s,%s",str1, str2, cat->datum.fqn);
					str_tmp += buf_pos;
				}
				first = -1;
				last = -1;
				if (item->next != NULL) {
					buf_pos = sprintf(str_tmp, ",");
					str_tmp += buf_pos;
				}
			}
		}
		if (first != -1) {
			if (first == last) {
				buf_pos = sprintf(str_tmp, "%s", str1);
				str_tmp += buf_pos;
			} else if (last == first + 1) {
				buf_pos = sprintf(str_tmp, "%s,%s", str1, str2);
				str_tmp += buf_pos;
			} else {
				buf_pos = sprintf(str_tmp, "%s.%s",str1, str2);
				str_tmp += buf_pos;
			}
		}
	}

	return str_tmp - out;
}

int cil_selinuxusers_to_string(struct cil_db *db, char **out, size_t *size)
{
	size_t str_len = 0;
	int buf_pos = 0;
	char *str_tmp = NULL;
	struct cil_list_item *curr;

	if (db->selinuxusers->head == NULL) {
		*size = 0;
		*out = NULL;
		return SEPOL_OK;
	}

	cil_list_for_each(curr, db->selinuxusers) {
		struct cil_selinuxuser *selinuxuser = curr->data;
		struct cil_user *user = selinuxuser->user;

		str_len += strlen(selinuxuser->name_str) + strlen(user->datum.fqn) + 1;

		if (db->mls == CIL_TRUE) {
			struct cil_levelrange *range = selinuxuser->range;
			str_len += __cil_level_strlen(range->low) + __cil_level_strlen(range->high) + 2;
		}

		str_len++;
	}

	*size = str_len * sizeof(char);
	str_tmp = cil_malloc(*size+1);
	*out = str_tmp;

	for(curr = db->selinuxusers->head; curr != NULL; curr = curr->next) {
		struct cil_selinuxuser *selinuxuser = curr->data;
		struct cil_user *user = selinuxuser->user;

		buf_pos = sprintf(str_tmp, "%s:%s", selinuxuser->name_str, user->datum.fqn);
		str_tmp += buf_pos;

		if (db->mls == CIL_TRUE) {
			struct cil_levelrange *range = selinuxuser->range;
			buf_pos = sprintf(str_tmp, ":");
			str_tmp += buf_pos;
			buf_pos = __cil_level_to_string(range->low, str_tmp);
			str_tmp += buf_pos;
			buf_pos = sprintf(str_tmp, "-");
			str_tmp += buf_pos;
			buf_pos = __cil_level_to_string(range->high, str_tmp);
			str_tmp += buf_pos;
		}

		buf_pos = sprintf(str_tmp, "\n");
		str_tmp += buf_pos;
	}

	return SEPOL_OK;
}

int cil_filecons_to_string(struct cil_db *db, char **out, size_t *size)
{
	uint32_t i = 0;
	int buf_pos = 0;
	size_t str_len = 0;
	char *str_tmp = NULL;
	struct cil_sort *filecons = db->filecon;

	for (i = 0; i < filecons->count; i++) {
		struct cil_filecon *filecon = filecons->array[i];
		struct cil_context *ctx = filecon->context;

		str_len += strlen(filecon->path_str);

		if (filecon->type != CIL_FILECON_ANY) {
			/* If a type is specified,
			   +2 for type string, +1 for tab */
			str_len += 3;
		}

		if (ctx != NULL) {
			struct cil_user *user = ctx->user;
			struct cil_role *role = ctx->role;
			struct cil_type *type = ctx->type;

			str_len += (strlen(user->datum.fqn) + strlen(role->datum.fqn) + strlen(type->datum.fqn) + 3);

			if (db->mls == CIL_TRUE) {
				struct cil_levelrange *range = ctx->range;
				if (cil_level_equals(range->low, range->high)) {
					str_len += __cil_level_strlen(range->low) + 1;
				} else {
					str_len += __cil_level_strlen(range->low) + __cil_level_strlen(range->high) + 2;
				}
			}
		} else {
			str_len += strlen("\t<<none>>");
		}

		str_len++;
	}

	*size = str_len * sizeof(char);
	str_tmp = cil_malloc(*size+1);
	*out = str_tmp;

	for (i = 0; i < filecons->count; i++) {
		struct cil_filecon *filecon = filecons->array[i];
		struct cil_context *ctx = filecon->context;
		const char *str_type = NULL;

		buf_pos = sprintf(str_tmp, "%s", filecon->path_str);
		str_tmp += buf_pos;

		switch(filecon->type) {
		case CIL_FILECON_ANY:
			str_type = "";
			break;
		case CIL_FILECON_FILE:
			str_type = "\t--";
			break;
		case CIL_FILECON_DIR:
			str_type = "\t-d";
			break;
		case CIL_FILECON_CHAR:
			str_type = "\t-c";
			break;
		case CIL_FILECON_BLOCK:
			str_type = "\t-b";
			break;
		case CIL_FILECON_SOCKET:
			str_type = "\t-s";
			break;
		case CIL_FILECON_PIPE:
			str_type = "\t-p";
			break;
		case CIL_FILECON_SYMLINK:
			str_type = "\t-l";
			break;
		default:
			str_type = "";
			break;
		}
		buf_pos = sprintf(str_tmp, "%s", str_type);
		str_tmp += buf_pos;

		if (ctx != NULL) {
			struct cil_user *user = ctx->user;
			struct cil_role *role = ctx->role;
			struct cil_type *type = ctx->type;

			buf_pos = sprintf(str_tmp, "\t%s:%s:%s", user->datum.fqn, role->datum.fqn,
							  type->datum.fqn);
			str_tmp += buf_pos;

			if (db->mls == CIL_TRUE) {
				struct cil_levelrange *range = ctx->range;
				buf_pos = sprintf(str_tmp, ":");
				str_tmp += buf_pos;
				buf_pos = __cil_level_to_string(range->low, str_tmp);
				str_tmp += buf_pos;

				if (!cil_level_equals(range->low, range->high)) {
					buf_pos = sprintf(str_tmp, "-");
					str_tmp += buf_pos;
					buf_pos = __cil_level_to_string(range->high, str_tmp);
					str_tmp += buf_pos;
				}
			}
		} else {
			buf_pos = sprintf(str_tmp, "\t<<none>>");
			str_tmp += buf_pos;
		}

		buf_pos = sprintf(str_tmp, "\n");
		str_tmp += buf_pos;
	}

	return SEPOL_OK;
}

void cil_set_disable_dontaudit(struct cil_db *db, int disable_dontaudit)
{
	db->disable_dontaudit = disable_dontaudit;
}

void cil_set_disable_neverallow(struct cil_db *db, int disable_neverallow)
{
	db->disable_neverallow = disable_neverallow;
}

void cil_set_attrs_expand_generated(struct cil_db *db, int attrs_expand_generated)
{
	db->attrs_expand_generated = attrs_expand_generated;
}

void cil_set_attrs_expand_size(struct cil_db *db, unsigned attrs_expand_size)
{
	db->attrs_expand_size = attrs_expand_size;
}

void cil_set_preserve_tunables(struct cil_db *db, int preserve_tunables)
{
	db->preserve_tunables = preserve_tunables;
}

int cil_set_handle_unknown(struct cil_db *db, int handle_unknown)
{
	int rc = 0;

	switch (handle_unknown) {
		case SEPOL_DENY_UNKNOWN:
		case SEPOL_REJECT_UNKNOWN:
		case SEPOL_ALLOW_UNKNOWN:
			db->handle_unknown = handle_unknown;
			break;
		default:
			cil_log(CIL_ERR, "Unknown value for handle-unknown: %i\n", handle_unknown);
			rc = -1;
	}

	return rc;
}

void cil_set_mls(struct cil_db *db, int mls)
{
	db->mls = mls;
}

void cil_set_multiple_decls(struct cil_db *db, int multiple_decls)
{
	db->multiple_decls = multiple_decls;
}

void cil_set_qualified_names(struct cil_db *db, int qualified_names)
{
	db->qualified_names = qualified_names;
}

void cil_set_target_platform(struct cil_db *db, int target_platform)
{
	db->target_platform = target_platform;
}

void cil_set_policy_version(struct cil_db *db, int policy_version)
{
	db->policy_version = policy_version;
}

void cil_symtab_array_init(symtab_t symtab[], const int symtab_sizes[CIL_SYM_NUM])
{
	uint32_t i = 0;
	for (i = 0; i < CIL_SYM_NUM; i++) {
		cil_symtab_init(&symtab[i], symtab_sizes[i]);
	}
}

void cil_symtab_array_destroy(symtab_t symtab[])
{
	int i = 0;
	for (i = 0; i < CIL_SYM_NUM; i++) {
		cil_symtab_destroy(&symtab[i]);
	}
}

void cil_destroy_ast_symtabs(struct cil_tree_node *current)
{
	while (current) {
		switch (current->flavor) {
		case CIL_BLOCK:
			cil_symtab_array_destroy(((struct cil_block*)current->data)->symtab);
			break;
		case CIL_IN:
			cil_symtab_array_destroy(((struct cil_in*)current->data)->symtab);
			break;
		case CIL_CLASS:
		case CIL_COMMON:
		case CIL_MAP_CLASS:
			cil_symtab_destroy(&((struct cil_class*)current->data)->perms);
			break;
		case CIL_MACRO:
			cil_symtab_array_destroy(((struct cil_macro*)current->data)->symtab);
			break;
		case CIL_CONDBLOCK:
			cil_symtab_array_destroy(((struct cil_condblock*)current->data)->symtab);
			break;
		default:
			break;
		}

		if (current->cl_head) {
			cil_destroy_ast_symtabs(current->cl_head);
		}

		current = current->next;
	}
}

int cil_get_symtab(struct cil_tree_node *ast_node, symtab_t **symtab, enum cil_sym_index sym_index)
{
	struct cil_tree_node *node = ast_node;
	*symtab = NULL;
	
	if (sym_index == CIL_SYM_PERMS) {
		/* Class statements are not blocks, so the passed node should be the class */
		if (node->flavor == CIL_CLASS || node->flavor == CIL_MAP_CLASS ||
			node->flavor == CIL_COMMON) {
			*symtab = &((struct cil_class*)node->data)->perms;
			return SEPOL_OK;
		}
		goto exit;
	}

	if (sym_index < CIL_SYM_BLOCKS || sym_index >= CIL_SYM_NUM) {
		cil_log(CIL_ERR, "Invalid symtab type\n");
		goto exit;
	}

	while (node != NULL && *symtab == NULL) {
		switch (node->flavor) {
		case CIL_ROOT:
			*symtab = &((struct cil_root *)node->data)->symtab[sym_index];
			break;
		case CIL_BLOCK:
			*symtab = &((struct cil_block*)node->data)->symtab[sym_index];
			break;
		case CIL_MACRO:
			*symtab = &((struct cil_macro*)node->data)->symtab[sym_index];
			break;
		case CIL_IN:
			/* In blocks only exist before resolving the AST */
			*symtab = &((struct cil_in*)node->data)->symtab[sym_index];
			break;
		case CIL_CONDBLOCK: {
			if (node->parent->flavor == CIL_TUNABLEIF) {
				/* Cond blocks only exist before resolving the AST */
				*symtab = &((struct cil_condblock*)node->data)->symtab[sym_index];
			} else if (node->parent->flavor == CIL_BOOLEANIF) {
				node = node->parent->parent;
			}
			break;
		}
		default:
			node = node->parent;
		}
	}

	if (*symtab == NULL) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_tree_log(ast_node, CIL_ERR, "Failed to get symtab from node");
	return SEPOL_ERR;	
}

int cil_string_to_uint32(const char *string, uint32_t *value, int base)
{
	unsigned long val;
	char *end = NULL;
	int rc = SEPOL_ERR;

	if (string == NULL || value  == NULL) {
		goto exit;
	}

	errno = 0;
	val = strtoul(string, &end, base);
	if (errno != 0 || end == string || *end != '\0') {
		rc = SEPOL_ERR;
		goto exit;
	}

	/* Ensure that the value fits a 32-bit integer without triggering -Wtype-limits */
#if ULONG_MAX > UINT32_MAX
	if (val > UINT32_MAX) {
		rc = SEPOL_ERR;
		goto exit;
	}
#endif

	*value = val;

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR, "Failed to create uint32_t from string\n");
	return rc;
}

int cil_string_to_uint64(const char *string, uint64_t *value, int base)
{
	char *end = NULL;
	int rc = SEPOL_ERR;

	if (string == NULL || value  == NULL) {
		goto exit;
	}

	errno = 0;
	*value = strtoull(string, &end, base);
	if (errno != 0 || end == string || *end != '\0') {
		rc = SEPOL_ERR;
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR, "Failed to create uint64_t from string\n");
	return rc;
}

void cil_sort_init(struct cil_sort **sort)
{
	*sort = cil_malloc(sizeof(**sort));

	(*sort)->flavor = CIL_NONE;
	(*sort)->count = 0;
	(*sort)->index = 0;
	(*sort)->array = NULL;
}

void cil_sort_destroy(struct cil_sort **sort)
{
	(*sort)->flavor = CIL_NONE;
	(*sort)->count = 0;
	(*sort)->index = 0;
	if ((*sort)->array != NULL) {
		free((*sort)->array);
	}
	(*sort)->array = NULL;

	free(*sort);
	*sort = NULL;
}

void cil_netifcon_init(struct cil_netifcon **netifcon)
{
	*netifcon = cil_malloc(sizeof(**netifcon));

	(*netifcon)->interface_str = NULL;
	(*netifcon)->if_context_str = NULL;
	(*netifcon)->if_context = NULL;
	(*netifcon)->packet_context_str = NULL;
	(*netifcon)->packet_context = NULL;
	(*netifcon)->context_str = NULL;
}

void cil_ibendportcon_init(struct cil_ibendportcon **ibendportcon)
{
	*ibendportcon = cil_malloc(sizeof(**ibendportcon));

	(*ibendportcon)->dev_name_str = NULL;
	(*ibendportcon)->port = 0;
	(*ibendportcon)->context_str = NULL;
	(*ibendportcon)->context = NULL;
}

void cil_context_init(struct cil_context **context)
{
	*context = cil_malloc(sizeof(**context));

	cil_symtab_datum_init(&(*context)->datum);
	(*context)->user_str = NULL;
	(*context)->user = NULL;
	(*context)->role_str = NULL;
	(*context)->role = NULL;
	(*context)->type_str = NULL;
	(*context)->type = NULL;
	(*context)->range_str = NULL;
	(*context)->range = NULL;
}

void cil_level_init(struct cil_level **level)
{
	*level = cil_malloc(sizeof(**level));

	cil_symtab_datum_init(&(*level)->datum);
	(*level)->sens_str = NULL;
	(*level)->sens = NULL;
	(*level)->cats = NULL;
}

void cil_levelrange_init(struct cil_levelrange **range)
{
	*range = cil_malloc(sizeof(**range));

	cil_symtab_datum_init(&(*range)->datum);
	(*range)->low_str = NULL;
	(*range)->low = NULL;
	(*range)->high_str = NULL;
	(*range)->high = NULL;
}

void cil_sens_init(struct cil_sens **sens)
{
	*sens = cil_malloc(sizeof(**sens));

	cil_symtab_datum_init(&(*sens)->datum);

	(*sens)->cats_list = NULL;

	(*sens)->ordered = CIL_FALSE;
}

void cil_block_init(struct cil_block **block)
{
	*block = cil_malloc(sizeof(**block));

	cil_symtab_datum_init(&(*block)->datum);

	cil_symtab_array_init((*block)->symtab, cil_sym_sizes[CIL_SYM_ARRAY_BLOCK]);

	(*block)->is_abstract = CIL_FALSE;

	(*block)->bi_nodes = NULL;
}

void cil_blockinherit_init(struct cil_blockinherit **inherit)
{
	*inherit = cil_malloc(sizeof(**inherit));
	(*inherit)->block_str = NULL;
	(*inherit)->block = NULL;
}

void cil_blockabstract_init(struct cil_blockabstract **abstract)
{
	*abstract = cil_malloc(sizeof(**abstract));
	(*abstract)->block_str = NULL;
}

void cil_in_init(struct cil_in **in)
{
	*in = cil_malloc(sizeof(**in));

	cil_symtab_array_init((*in)->symtab, cil_sym_sizes[CIL_SYM_ARRAY_IN]);
	(*in)->is_after = CIL_FALSE;
	(*in)->block_str = NULL;
}

void cil_class_init(struct cil_class **class)
{
	*class = cil_malloc(sizeof(**class));

	cil_symtab_datum_init(&(*class)->datum);

	cil_symtab_init(&(*class)->perms, CIL_CLASS_SYM_SIZE);

	(*class)->num_perms = 0;
	(*class)->common = NULL;
	(*class)->ordered = CIL_FALSE;
}

void cil_classorder_init(struct cil_classorder **classorder)
{
	*classorder = cil_malloc(sizeof(**classorder));

	(*classorder)->class_list_str = NULL;
}

void cil_classcommon_init(struct cil_classcommon **classcommon)
{
	*classcommon = cil_malloc(sizeof(**classcommon));

	(*classcommon)->class_str = NULL;
	(*classcommon)->common_str = NULL;
}

void cil_sid_init(struct cil_sid **sid)
{
	*sid = cil_malloc(sizeof(**sid));

	cil_symtab_datum_init(&(*sid)->datum);

	(*sid)->ordered = CIL_FALSE;
	(*sid)->context = NULL;
}

void cil_sidcontext_init(struct cil_sidcontext **sidcontext)
{
	*sidcontext = cil_malloc(sizeof(**sidcontext));

	(*sidcontext)->sid_str = NULL;
	(*sidcontext)->context_str = NULL;
	(*sidcontext)->context = NULL;
}

void cil_sidorder_init(struct cil_sidorder **sidorder)
{
	*sidorder = cil_malloc(sizeof(**sidorder));

	(*sidorder)->sid_list_str = NULL;
}

void cil_userrole_init(struct cil_userrole **userrole)
{
	*userrole = cil_malloc(sizeof(**userrole));

	(*userrole)->user_str = NULL;
	(*userrole)->user = NULL;
	(*userrole)->role_str = NULL;
	(*userrole)->role = NULL;
}

void cil_userprefix_init(struct cil_userprefix **userprefix)
{
	*userprefix = cil_malloc(sizeof(**userprefix));

	(*userprefix)->user_str = NULL;
	(*userprefix)->user = NULL;
	(*userprefix)->prefix_str = NULL;
}

void cil_selinuxuser_init(struct cil_selinuxuser **selinuxuser)
{
	*selinuxuser = cil_malloc(sizeof(**selinuxuser));

	(*selinuxuser)->name_str = NULL;
	(*selinuxuser)->user_str = NULL;
	(*selinuxuser)->user = NULL;
	(*selinuxuser)->range_str = NULL;
	(*selinuxuser)->range = NULL;
}

void cil_roletype_init(struct cil_roletype **roletype)
{
	*roletype = cil_malloc(sizeof(**roletype));

	(*roletype)->role_str = NULL;
	(*roletype)->role = NULL;
	(*roletype)->type_str = NULL;
	(*roletype)->type = NULL;
}

void cil_roleattribute_init(struct cil_roleattribute **attr)
{
	*attr = cil_malloc(sizeof(**attr));

	cil_symtab_datum_init(&(*attr)->datum);

	(*attr)->expr_list = NULL;
	(*attr)->roles = NULL;
}

void cil_roleattributeset_init(struct cil_roleattributeset **attrset)
{
	*attrset = cil_malloc(sizeof(**attrset));

	(*attrset)->attr_str = NULL;
	(*attrset)->str_expr = NULL;
	(*attrset)->datum_expr = NULL;
}

void cil_typeattribute_init(struct cil_typeattribute **attr)
{
	*attr = cil_malloc(sizeof(**attr));

	cil_symtab_datum_init(&(*attr)->datum);

	(*attr)->expr_list = NULL;
	(*attr)->types = NULL;
	(*attr)->used = CIL_FALSE;
	(*attr)->keep = CIL_FALSE;
}

void cil_typeattributeset_init(struct cil_typeattributeset **attrset)
{
	*attrset = cil_malloc(sizeof(**attrset));

	(*attrset)->attr_str = NULL;
	(*attrset)->str_expr = NULL;
	(*attrset)->datum_expr = NULL;
}

void cil_expandtypeattribute_init(struct cil_expandtypeattribute **expandattr)
{
	*expandattr = cil_malloc(sizeof(**expandattr));

	(*expandattr)->attr_strs = NULL;
	(*expandattr)->attr_datums = NULL;
	(*expandattr)->expand = 0;
}

void cil_alias_init(struct cil_alias **alias)
{
	*alias = cil_malloc(sizeof(**alias));

	(*alias)->actual = NULL;

	cil_symtab_datum_init(&(*alias)->datum);
}

void cil_aliasactual_init(struct cil_aliasactual **aliasactual)
{
	*aliasactual = cil_malloc(sizeof(**aliasactual));

	(*aliasactual)->alias_str = NULL;
	(*aliasactual)->actual_str = NULL;
}

void cil_typepermissive_init(struct cil_typepermissive **typeperm)
{
	*typeperm = cil_malloc(sizeof(**typeperm));

	(*typeperm)->type_str = NULL;
	(*typeperm)->type = NULL;
}

void cil_name_init(struct cil_name **name)
{
	*name = cil_malloc(sizeof(**name));

	cil_symtab_datum_init(&(*name)->datum);
	(*name)->name_str = NULL;
}

void cil_nametypetransition_init(struct cil_nametypetransition **nametypetrans)
{
	*nametypetrans = cil_malloc(sizeof(**nametypetrans));

	(*nametypetrans)->src_str = NULL;
	(*nametypetrans)->src = NULL;
	(*nametypetrans)->tgt_str = NULL;
	(*nametypetrans)->tgt = NULL;
	(*nametypetrans)->obj_str = NULL;
	(*nametypetrans)->obj = NULL;
	(*nametypetrans)->name_str = NULL;
	(*nametypetrans)->name = NULL;
	(*nametypetrans)->result_str = NULL;
	(*nametypetrans)->result = NULL;
}

void cil_rangetransition_init(struct cil_rangetransition **rangetrans)
{
        *rangetrans = cil_malloc(sizeof(**rangetrans));

	(*rangetrans)->src_str = NULL;
	(*rangetrans)->src = NULL;
	(*rangetrans)->exec_str = NULL;
	(*rangetrans)->exec = NULL;
	(*rangetrans)->obj_str = NULL;
	(*rangetrans)->obj = NULL;
	(*rangetrans)->range_str = NULL;
	(*rangetrans)->range = NULL;
}

void cil_bool_init(struct cil_bool **cilbool)
{
	*cilbool = cil_malloc(sizeof(**cilbool));

	cil_symtab_datum_init(&(*cilbool)->datum);
	(*cilbool)->value = 0;
}

void cil_tunable_init(struct cil_tunable **ciltun)
{
	*ciltun = cil_malloc(sizeof(**ciltun));

	cil_symtab_datum_init(&(*ciltun)->datum);
	(*ciltun)->value = 0;
}

void cil_condblock_init(struct cil_condblock **cb)
{
	*cb = cil_malloc(sizeof(**cb));

	(*cb)->flavor = CIL_NONE;
	cil_symtab_array_init((*cb)->symtab, cil_sym_sizes[CIL_SYM_ARRAY_CONDBLOCK]);
}

void cil_boolif_init(struct cil_booleanif **bif)
{
	*bif = cil_malloc(sizeof(**bif));

	(*bif)->str_expr = NULL;
	(*bif)->datum_expr = NULL;
}

void cil_tunif_init(struct cil_tunableif **tif)
{
	*tif = cil_malloc(sizeof(**tif));

	(*tif)->str_expr = NULL;
	(*tif)->datum_expr = NULL;
}

void cil_avrule_init(struct cil_avrule **avrule)
{
	*avrule = cil_malloc(sizeof(**avrule));

	(*avrule)->is_extended = 0;
	(*avrule)->rule_kind = CIL_NONE;
	(*avrule)->src_str = NULL;
	(*avrule)->src = NULL;
	(*avrule)->tgt_str = NULL;
	(*avrule)->tgt = NULL;
	memset(&((*avrule)->perms), 0, sizeof((*avrule)->perms));
}

void cil_permissionx_init(struct cil_permissionx **permx)
{
	*permx = cil_malloc(sizeof(**permx));

	cil_symtab_datum_init(&(*permx)->datum);
	(*permx)->kind = CIL_NONE;
	(*permx)->obj_str = NULL;
	(*permx)->obj = NULL;
	(*permx)->expr_str = NULL;
	(*permx)->perms = NULL;
}

void cil_type_rule_init(struct cil_type_rule **type_rule)
{
	*type_rule = cil_malloc(sizeof(**type_rule));

	(*type_rule)->rule_kind = CIL_NONE;
	(*type_rule)->src_str = NULL;
	(*type_rule)->src = NULL;
	(*type_rule)->tgt_str = NULL;
	(*type_rule)->tgt = NULL;
	(*type_rule)->obj_str = NULL;
	(*type_rule)->obj = NULL;
	(*type_rule)->result_str = NULL;
	(*type_rule)->result = NULL;
}

void cil_roletransition_init(struct cil_roletransition **role_trans)
{
	*role_trans = cil_malloc(sizeof(**role_trans));

	(*role_trans)->src_str = NULL;
	(*role_trans)->src = NULL;
	(*role_trans)->tgt_str = NULL;
	(*role_trans)->tgt = NULL;
	(*role_trans)->obj_str = NULL;
	(*role_trans)->obj = NULL;
	(*role_trans)->result_str = NULL;
	(*role_trans)->result = NULL;
}

void cil_roleallow_init(struct cil_roleallow **roleallow)
{
	*roleallow = cil_malloc(sizeof(**roleallow));

	(*roleallow)->src_str = NULL;
	(*roleallow)->src = NULL;
	(*roleallow)->tgt_str = NULL;
	(*roleallow)->tgt = NULL;
}

void cil_catset_init(struct cil_catset **catset)
{
	*catset = cil_malloc(sizeof(**catset));

	cil_symtab_datum_init(&(*catset)->datum);
	(*catset)->cats = NULL;
}

void cil_senscat_init(struct cil_senscat **senscat)
{
	*senscat = cil_malloc(sizeof(**senscat));

	(*senscat)->sens_str = NULL;
	(*senscat)->cats = NULL;
}

void cil_cats_init(struct cil_cats **cats)
{
	*cats = cil_malloc(sizeof(**cats));

	(*cats)->evaluated = CIL_FALSE;
	(*cats)->str_expr = NULL;
	(*cats)->datum_expr = NULL;
}

void cil_filecon_init(struct cil_filecon **filecon)
{
	*filecon = cil_malloc(sizeof(**filecon));

	(*filecon)->path_str = NULL;
	(*filecon)->type = CIL_FILECON_ANY;
	(*filecon)->context_str = NULL;
	(*filecon)->context = NULL;
}

void cil_ibpkeycon_init(struct cil_ibpkeycon **ibpkeycon)
{
	*ibpkeycon = cil_malloc(sizeof(**ibpkeycon));

	(*ibpkeycon)->subnet_prefix_str = NULL;
	(*ibpkeycon)->pkey_low = 0;
	(*ibpkeycon)->pkey_high = 0;
	(*ibpkeycon)->context_str = NULL;
	(*ibpkeycon)->context = NULL;
}

void cil_portcon_init(struct cil_portcon **portcon)
{
	*portcon = cil_malloc(sizeof(**portcon));
	(*portcon)->proto = 0;
	(*portcon)->port_low = 0;
	(*portcon)->port_high = 0;
	(*portcon)->context_str = NULL;
	(*portcon)->context = NULL;
}

void cil_nodecon_init(struct cil_nodecon **nodecon)
{
	*nodecon = cil_malloc(sizeof(**nodecon));

	(*nodecon)->addr_str = NULL;
	(*nodecon)->addr = NULL;
	(*nodecon)->mask_str = NULL;
	(*nodecon)->mask = NULL;
	(*nodecon)->context_str = NULL;
	(*nodecon)->context = NULL;
}

void cil_genfscon_init(struct cil_genfscon **genfscon)
{
	*genfscon = cil_malloc(sizeof(**genfscon));

	(*genfscon)->fs_str = NULL;
	(*genfscon)->path_str = NULL;
	(*genfscon)->file_type = CIL_FILECON_ANY;
	(*genfscon)->context_str = NULL;
	(*genfscon)->context = NULL;
}

void cil_pirqcon_init(struct cil_pirqcon **pirqcon)
{
	*pirqcon = cil_malloc(sizeof(**pirqcon));
	
	(*pirqcon)->pirq = 0;
	(*pirqcon)->context_str = NULL;
	(*pirqcon)->context = NULL;
}

void cil_iomemcon_init(struct cil_iomemcon **iomemcon)
{
	*iomemcon = cil_malloc(sizeof(**iomemcon));

	(*iomemcon)->iomem_low = 0;
	(*iomemcon)->iomem_high = 0;
	(*iomemcon)->context_str = NULL;
	(*iomemcon)->context = NULL;
}

void cil_ioportcon_init(struct cil_ioportcon **ioportcon)
{
	*ioportcon = cil_malloc(sizeof(**ioportcon));

	(*ioportcon)->context_str = NULL;
	(*ioportcon)->context = NULL;
}

void cil_pcidevicecon_init(struct cil_pcidevicecon **pcidevicecon)
{
	*pcidevicecon = cil_malloc(sizeof(**pcidevicecon));

	(*pcidevicecon)->dev = 0;
	(*pcidevicecon)->context_str = NULL;
	(*pcidevicecon)->context = NULL;
}

void cil_devicetreecon_init(struct cil_devicetreecon **dtcon)
{
	*dtcon = cil_malloc(sizeof(**dtcon));

	(*dtcon)->path = NULL;
	(*dtcon)->context_str = NULL;
	(*dtcon)->context = NULL;
}

void cil_fsuse_init(struct cil_fsuse **fsuse)
{
	*fsuse = cil_malloc(sizeof(**fsuse));

	(*fsuse)->type = 0;
	(*fsuse)->fs_str = NULL;
	(*fsuse)->context_str = NULL;
	(*fsuse)->context = NULL;
}

void cil_constrain_init(struct cil_constrain **constrain)
{
	*constrain = cil_malloc(sizeof(**constrain));

	(*constrain)->classperms = NULL;
	(*constrain)->str_expr = NULL;
	(*constrain)->datum_expr = NULL;
}

void cil_validatetrans_init(struct cil_validatetrans **validtrans)
{
	*validtrans = cil_malloc(sizeof(**validtrans));

	(*validtrans)->class_str = NULL;
	(*validtrans)->class = NULL;
	(*validtrans)->str_expr = NULL;
	(*validtrans)->datum_expr = NULL;
}

void cil_ipaddr_init(struct cil_ipaddr **ipaddr)
{
	*ipaddr = cil_malloc(sizeof(**ipaddr));

	cil_symtab_datum_init(&(*ipaddr)->datum);
	memset(&(*ipaddr)->ip, 0, sizeof((*ipaddr)->ip));
}

void cil_perm_init(struct cil_perm **perm)
{
	*perm = cil_malloc(sizeof(**perm));

	cil_symtab_datum_init(&(*perm)->datum);
	(*perm)->value = 0;
	(*perm)->classperms = NULL;
}

void cil_classpermission_init(struct cil_classpermission **cp)
{
	*cp = cil_malloc(sizeof(**cp));

	cil_symtab_datum_init(&(*cp)->datum);
	(*cp)->classperms = NULL;
}

void cil_classpermissionset_init(struct cil_classpermissionset **cps)
{
	*cps = cil_malloc(sizeof(**cps));

	(*cps)->set_str = NULL;
	(*cps)->classperms = NULL;
}

void cil_classperms_set_init(struct cil_classperms_set **cp_set)
{
	*cp_set = cil_malloc(sizeof(**cp_set));
	(*cp_set)->set_str = NULL;
	(*cp_set)->set = NULL;
}

void cil_classperms_init(struct cil_classperms **cp)
{
	*cp = cil_malloc(sizeof(**cp));
	(*cp)->class_str = NULL;
	(*cp)->class = NULL;
	(*cp)->perm_strs = NULL;
	(*cp)->perms = NULL;
}

void cil_classmapping_init(struct cil_classmapping **mapping)
{
	*mapping = cil_malloc(sizeof(**mapping));

	(*mapping)->map_class_str = NULL;
	(*mapping)->map_perm_str = NULL;
	(*mapping)->classperms = NULL;
}

void cil_user_init(struct cil_user **user)
{
	*user = cil_malloc(sizeof(**user));

	cil_symtab_datum_init(&(*user)->datum);
	(*user)->bounds = NULL;
	(*user)->roles = NULL;
	(*user)->dftlevel = NULL;
	(*user)->range = NULL;
	(*user)->value = 0;
}

void cil_userattribute_init(struct cil_userattribute **attr)
{
	*attr = cil_malloc(sizeof(**attr));

	cil_symtab_datum_init(&(*attr)->datum);

	(*attr)->expr_list = NULL;
	(*attr)->users = NULL;
}

void cil_userattributeset_init(struct cil_userattributeset **attrset)
{
	*attrset = cil_malloc(sizeof(**attrset));

	(*attrset)->attr_str = NULL;
	(*attrset)->str_expr = NULL;
	(*attrset)->datum_expr = NULL;
}

void cil_userlevel_init(struct cil_userlevel **usrlvl)
{
	*usrlvl = cil_malloc(sizeof(**usrlvl));

	(*usrlvl)->user_str = NULL;
	(*usrlvl)->level_str = NULL;
	(*usrlvl)->level = NULL;
}

void cil_userrange_init(struct cil_userrange **userrange)
{
	*userrange = cil_malloc(sizeof(**userrange));

	(*userrange)->user_str = NULL;
	(*userrange)->range_str = NULL;
	(*userrange)->range = NULL;
}

void cil_role_init(struct cil_role **role)
{
	*role = cil_malloc(sizeof(**role));

	cil_symtab_datum_init(&(*role)->datum);
	(*role)->bounds = NULL;
	(*role)->types = NULL;
	(*role)->value = 0;
}

void cil_type_init(struct cil_type **type)
{
	*type = cil_malloc(sizeof(**type));

	cil_symtab_datum_init(&(*type)->datum);
	(*type)->bounds = NULL;
	(*type)->value = 0;
}

void cil_cat_init(struct cil_cat **cat)
{
	*cat = cil_malloc(sizeof(**cat));

	cil_symtab_datum_init(&(*cat)->datum);
	(*cat)->ordered = CIL_FALSE;
	(*cat)->value = 0;
}

void cil_catorder_init(struct cil_catorder **catorder)
{
	*catorder = cil_malloc(sizeof(**catorder));

	(*catorder)->cat_list_str = NULL;
}

void cil_sensorder_init(struct cil_sensorder **sensorder)
{
	*sensorder = cil_malloc(sizeof(**sensorder));

	(*sensorder)->sens_list_str = NULL;
}

void cil_args_init(struct cil_args **args)
{
	*args = cil_malloc(sizeof(**args));
	(*args)->arg_str = NULL;
	(*args)->arg = NULL;
	(*args)->param_str = NULL;
	(*args)->flavor = CIL_NONE;
}

void cil_call_init(struct cil_call **call)
{
	*call = cil_malloc(sizeof(**call));

	(*call)->macro_str = NULL;
	(*call)->macro = NULL;
	(*call)->args_tree = NULL;
	(*call)->args = NULL;
	(*call)->copied = 0;
}

void cil_optional_init(struct cil_optional **optional)
{
	*optional = cil_malloc(sizeof(**optional));
	cil_symtab_datum_init(&(*optional)->datum);
}

void cil_param_init(struct cil_param **param)
{
	*param = cil_malloc(sizeof(**param));

	(*param)->str = NULL;
	(*param)->flavor = CIL_NONE;
}

void cil_macro_init(struct cil_macro **macro)
{
	*macro = cil_malloc(sizeof(**macro));

	cil_symtab_datum_init(&(*macro)->datum);
	cil_symtab_array_init((*macro)->symtab, cil_sym_sizes[CIL_SYM_ARRAY_MACRO]);
	(*macro)->params = NULL;
}

void cil_policycap_init(struct cil_policycap **policycap)
{
	*policycap = cil_malloc(sizeof(**policycap));

	cil_symtab_datum_init(&(*policycap)->datum);
}

void cil_bounds_init(struct cil_bounds **bounds)
{
	*bounds = cil_malloc(sizeof(**bounds));

	(*bounds)->parent_str = NULL;
	(*bounds)->child_str = NULL;
}

void cil_default_init(struct cil_default **def)
{
	*def = cil_malloc(sizeof(**def));

	(*def)->flavor = CIL_NONE;
	(*def)->class_strs = NULL;
	(*def)->class_datums = NULL;
}

void cil_defaultrange_init(struct cil_defaultrange **def)
{
	*def = cil_malloc(sizeof(**def));

	(*def)->class_strs = NULL;
	(*def)->class_datums = NULL;
}

void cil_handleunknown_init(struct cil_handleunknown **unk)
{
	*unk = cil_malloc(sizeof(**unk));
}

void cil_mls_init(struct cil_mls **mls)
{
	*mls = cil_malloc(sizeof(**mls));
	(*mls)->value = 0;
}

void cil_src_info_init(struct cil_src_info **info)
{
	*info = cil_malloc(sizeof(**info));
	(*info)->kind = NULL;
	(*info)->hll_line = 0;
	(*info)->path = NULL;
}
