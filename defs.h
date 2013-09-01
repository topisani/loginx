// This file is part of the loginx project
//
// Copyright (c) 2013 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

enum { MAX_PW_LEN = 64 };

struct account {
    uid_t	uid;
    gid_t	gid;
    unsigned	ltime;
    char*	name;
    char*	dir;
    char*	shell;
};

typedef const struct account* const* acclist_t;

//----------------------------------------------------------------------

// loginx.c
void* xmalloc (size_t n);
void xfree (void* p);
#define xfreenull(pp)	do { xfree(pp); pp = NULL; } while(0)

// pam.c
void PamOpen (void);
void PamClose (void);
bool PamLogin (const struct account* acct, const char* password);
void PamLogout (void);

// acct.c
acclist_t ReadAccounts (void);
void ReadLastlog (void);
unsigned NAccounts (void);

// ui.c
unsigned LoginBox (acclist_t al, char* password);
