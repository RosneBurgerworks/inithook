/*
 * Paint.cpp
 *
 *  Created on: Dec 31, 2017
 *      Author: nullifiedcat
 */

#include <hacks/hacklist.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "hitrate.hpp"
#include "hack.hpp"

namespace hooked_methods
{

DEFINE_HOOKED_METHOD(Paint, void, IEngineVGui *this_, PaintMode_t mode)
{
    if (!isHackActive())
    {
#if !ENABLE_NULL_GRAPHICS
        return original::Paint(this_, mode);
#else
        return;
#endif
    }

    if (!g_IEngine->IsInGame())
        g_Settings.bInvalid = true;

    if (mode & PaintMode_t::PAINT_UIPANELS)
    {
        hitrate::Update();
        if (!hack::command_stack().empty())
        {
            PROF_SECTION(PT_command_stack);
            std::lock_guard<std::mutex> guard(hack::command_stack_mutex);
            // logging::Info("executing %s",
            //              hack::command_stack().top().c_str());
            g_IEngine->ClientCmd_Unrestricted(hack::command_stack().top().c_str());
            hack::command_stack().pop();
        }
#if ENABLE_TEXTMODE_STDIN == 1
        static auto last_stdin = std::chrono::system_clock::from_time_t(0);
        auto ms                = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - last_stdin).count();
        if (ms > 500)
        {
            UpdateInput();
            last_stdin = std::chrono::system_clock::now();
        }
#endif
        // MOVED BACK because glez and imgui flicker in painttraveerse
#if ENABLE_VISUALS && (ENABLE_IMGUI_DRAWING || !ENABLE_ENGINE_DRAWING)
        render_cheat_visuals();
#endif

        // Call all paint functions
        EC::run(EC::Paint);
    }
#if !ENABLE_NULL_GRAPHICS
    return original::Paint(this_, mode);
#else
    return;
#endif
}
} // namespace hooked_methods
