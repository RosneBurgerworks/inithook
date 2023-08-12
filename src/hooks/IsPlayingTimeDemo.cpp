/*
 * IsPlayingTimeDemo.cpp
 *
 *  Created on: May 13, 2018
 *      Author: bencat07
 */
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"

namespace hooked_methods
{
DEFINE_HOOKED_METHOD(IsPlayingTimeDemo, bool, void *_this)
{
    if (nolerp)
    {
        uintptr_t ret_addr      = (uintptr_t) __builtin_return_address(1);
        static auto wanted_addr = gSignatures.GetClientSignature("84 C0 0F 85 ? ? ? ? E9 ? ? ? ? 8D 76 00 C6 05");
        if (ret_addr == wanted_addr && CE_GOOD(LOCAL_E) && LOCAL_E->m_bAlivePlayer())
            return true;
    }
    return original::IsPlayingTimeDemo(_this);
}
} // namespace hooked_methods
