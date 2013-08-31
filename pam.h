// This file is part of the loginx project
//
// Copyright (c) 2013 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"

struct account;

void PamOpen (void);
void PamClose (void);
bool PamLogin (const struct account* acct, const char* password);
void PamLogout (void);
