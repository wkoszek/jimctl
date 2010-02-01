/*-
 * Copyright (c) 2010 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JIM_EMBEDDED
#include <jim.h>

/*
 * Internal variables that carry values read from configuration
 * files blocks.
 */
#define	JCONF_VAR_IP	"__ip"

/*
 * Sample structure representing the main subject of our configuration
 * files -- interface.
 */
struct iface {
	const char	*ip;
	const char	*name;
	struct iface	*next;
};
#define	INSERT(list, elem) do {		\
	(elem)->next = (list);		\
	(list) = (elem);		\
} while (0)

/*
 * Simple structure and helper functions for better management of state,
 * that has to be passed between Jim function calls to keep information
 * on where we are in a parsing stage.
 */
struct mystate {
	/*
	 * Do we actually parse "interface" block?
	 */
	int		 block_parsing;

	/*
	 * List of persed interfaces.
	 */
	struct iface	*head;
};

/*
 * Create new state.
 */
static struct mystate *
mystate_new(void)
{
	struct mystate *s;

	s = calloc(1, sizeof(*s));
	assert(s != NULL);
	s->block_parsing = 0;
	s->head = NULL;
	return (s);
}

/*
 * Destroy state.
 */
static void
mystate_destroy(struct mystate *s)
{

	assert(s != NULL);
	s->block_parsing = -1;
	s->head = (struct iface *)0xdeadc0de;
}

/*
 * Create interface with name "ip" and name "name".
 */
static struct iface *
iface_alloc(const char *ip, const char *name)
{
	struct iface *i;

	i = calloc(1, sizeof(*i));
	assert(i != NULL && "can't allocate interface");
	i->ip = strdup(ip);
	assert(i->ip != NULL && "can't allocate IP");
	i->name = strdup(name);
	assert(i->name != NULL && "can't allocate name");
	i->next = NULL;
	return (i);
}

/*
 * Free interface.
 */
static void
iface_free(struct iface *i)
{

	if (i == NULL)
		return;
	free((void *)i->name);
	free((void *)i->ip);
	free(i);
}

/*
 * Parse "ip" keyword.
 */
static int
InterfaceIPFunc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
	struct mystate *softc;
	int error;

	/*
	 * Get private date shared across this parser.
	 */
	softc = Jim_CmdPrivData(interp);

	/* Check, if we're run from the right place. */
	if (softc->block_parsing == 0) {
		printf("Trying to use 'ip' outside 'interface' block\n");
		return (JIM_ERR);
	}
	if (argc != 2)
		return (JIM_ERR);

	error = Jim_SetVariableStr(interp, JCONF_VAR_IP, argv[1]);
	assert(error == JIM_OK);
	return (JIM_OK);
}

/*
 * Parse "interface" keyword.
 */
static int
InterfaceFunc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
	struct mystate *softc;
	struct iface *iface;
	Jim_Obj *ipobj;
	const char *ip;
	const char *name;
	int error;

	iface = NULL;
	ipobj = NULL;
	ip = name = NULL;
	error = 0;

	if (argc != 3)
		return (JIM_ERR);

	softc = Jim_CmdPrivData(interp);

	/* Name of the interface. */
	name = Jim_GetString(argv[1], NULL);

	softc->block_parsing = 1;
	/* Body of the block */
	error = Jim_EvalObj(interp, argv[2]);
	if (error) {
		printf("couldn't evaluate\n");
		return (JIM_ERR);
	}
	softc->block_parsing = 0;

	/* Take our hidden variable */
	ipobj = Jim_GetVariableStr(interp, JCONF_VAR_IP, JIM_NONE);
	assert(ipobj != NULL);

	ip = Jim_GetString(ipobj, NULL);
	if (ip == NULL) {
		Jim_fprintf(interp, interp->cookie_stdout, "NULL!\n");
		return (JIM_ERR);
	}
	iface = iface_alloc(ip, name);
	assert(iface != NULL);
	INSERT(softc->head, iface);
	return (JIM_OK);
}

/*
 * Show all interfaces that appeared in a configuration file.
 */
static void
ShowInterfaces(struct iface *head)
{
	struct iface *ci, *old;

	ci = head;
	while (ci != NULL) {
		printf("Interface '%s' has IP: '%s'\n", ci->name, ci->ip);
		old = ci;
		ci = ci->next;
		iface_free(old);
	}
}

/*
 * Now we try to write big enough code to duplication our array in Jim's
 * list implementation. Later, we try to load a sample script in Tcl that
 * could print our list.
 */
int
main(int argc, char **argv)
{
	struct mystate *jconf;
	Jim_Interp *interp;
	Jim_Obj *errstr;
	Jim_Obj *n;
	Jim_Obj *v;
	int error;

	errstr = n = v = NULL;
	error = 0;

	/* This is the first function embedders should call. */
	Jim_InitEmbedded();

	/* Create an interpreter */
	interp = Jim_CreateInterp();
	assert(interp != NULL && "couldn't create interpreter");

	/*
	 * Then, we register base commands, so that we actually implement Tcl.
	 */
	Jim_RegisterCoreCommands(interp);

	/*
	 * Create unique state for our sample parser and register all
	 * Jim commands.
	 */
	jconf = mystate_new();
	Jim_CreateCommand(interp, "interface", InterfaceFunc, jconf, NULL);
	Jim_CreateCommand(interp, "ip", InterfaceIPFunc, jconf, NULL);

	/*
	 * Parse a script.
	 */
	error = Jim_EvalFile(interp, "./net.tcl");
	if (error == JIM_ERR) {
		fprintf(stderr, "Couldn't execute Jim's script. "
		    "Error occured\n");
		Jim_PrintErrorMessage(interp);
		Jim_FreeInterp(interp);
		exit(EXIT_FAILURE);
	}

	ShowInterfaces(jconf->head);
	
	mystate_destroy(jconf);

	if (n != NULL)
		Jim_FreeObj(interp, n);
	Jim_FreeInterp(interp);
	return (EXIT_SUCCESS);
}
