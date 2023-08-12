/*
 * logging.h
 *
 *  Created on: Oct 3, 2016
 *      Author: nullifiedcat
 */

#pragma once

#include <stdio.h>

#ifdef __cplusplus
namespace logging
{
#endif

extern FILE *handle;

void Initialize();
void Shutdown();
void InfoV(bool console, const char *fmt, va_list list);
void InfoConsole(const char *fmt, ...);
void Info(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
