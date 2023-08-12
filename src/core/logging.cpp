/*
 * logging.cpp
 *
 *  Created on: Oct 3, 2016
 *      Author: nullifiedcat
 */

#include "common.hpp"

static settings::Boolean log_to_console{ "hack.log-console", "false" };
static settings::Boolean log_to_file{ "hack.log-file", "true" };

FILE *logging::handle{ nullptr };

const Color orange(153, 102, 0, 255);
const Color white(255, 255, 255, 255);

void logging::Initialize()
{
    char fmt_path[1024];
    passwd *pwd = getpwuid(getuid());
    snprintf(fmt_path, sizeof(fmt_path), "%s/logs/cathook-%s-%d.log", DATA_PATH, pwd->pw_name, getpid());
    logging::handle = fopen(fmt_path, "a");
}

void logging::InfoV(bool console, const char *fmt, va_list list)
{
    char *buffer, *result, timeString[10];
    time_t current_time;
    struct tm *time_info;
    int res;

    /* Logging turned off */
#if !ENABLE_VISUALS
    if (!*log_to_file)
        return;
#else
    if (!console && !*log_to_file)
        return;
#endif

    if (*log_to_file && logging::handle == nullptr)
        logging::Initialize();
    else if (!*log_to_file && logging::handle)
        logging::Shutdown();

    buffer = result = nullptr;
    res = vasprintf(&buffer, fmt, list);
    if (res < 0)
        return;

    time(&current_time);
    time_info = localtime(&current_time);
    strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);

    res = asprintf(&result, "[%s] %s\n", timeString, buffer);
    if (res < 0)
    {
        free(buffer);
        return;
    }
    if (*log_to_file)
    {
        fputs(result, logging::handle);
        fflush(logging::handle);
    }
#if ENABLE_VISUALS
    if (console)
    {
        g_ICvar->ConsoleColorPrintf(orange, "[");
        g_ICvar->ConsoleColorPrintf(white, "INIT");
        g_ICvar->ConsoleColorPrintf(orange, "]");
        g_ICvar->ConsoleColorPrintf(orange, " %s\n", buffer);
    }
#endif

    free(buffer);
    free(result);
}

void logging::InfoConsole(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    logging::InfoV(true, fmt, va);
    va_end(va);
}

void logging::Info(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    logging::InfoV(*log_to_console, fmt, va);
    va_end(va);
}

void logging::Shutdown()
{
    if (!logging::handle)
        return;

    fclose(logging::handle);
    logging::handle = nullptr;
}
