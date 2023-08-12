#include "common.hpp"
#include "HookTools.hpp"
#include <set>

namespace EC
{

struct EventCallbackData
{
    explicit EventCallbackData(const EventFunction &function, const std::string &name) : function{ function }, event_name{ name }, section{ name }
    {}
    // Lightweight constructor for searching purposes
    EventCallbackData(const std::string &name) : event_name{ name }, section{ name }
    {}
    EventFunction function;
    mutable ProfilerSection section;
    std::string event_name;
    bool operator<(const EventCallbackData &other) const
    {
        return event_name < other.event_name;
    }
};
// Ordered set to always keep priorities correct
static std::set<EventCallbackData> events[ec_types::EcTypesSize];

CatCommand evt_print("debug_print_events", "Print EC events", []() {
    for (int i = 0; i < int(ec_types::EcTypesSize); ++i)
    {
        logging::Info("%d events:", i);
        for (auto it = events[i].begin(); it != events[i].end(); ++it)
            logging::Info("%s", it->event_name.c_str());

        logging::Info("");
    }
});

static void craft_priority_string(char *out, enum ec_priority priority)
{
    std::snprintf(out, 5, "%04X", std::clamp(int(priority), -0x7FFF, 0x8000) + 0x7FFF);
}

void Register(enum ec_types type, const EventFunction &function, const std::string &name, enum ec_priority priority)
{
    char hex_priority[5];

    craft_priority_string(hex_priority, priority);
    events[type].emplace(function, hex_priority + name);
}

void Unregister(enum ec_types type, const std::string &name, enum ec_priority priority)
{
    char hex_priority[5];

    craft_priority_string(hex_priority, priority);
    auto &evt = events[type];
    auto it = evt.find(EventCallbackData(hex_priority + name));
    if (it != evt.end())
        evt.erase(it);
}

void run(ec_types type)
{
    auto &set = events[type];
    for (auto &i : set)
    {
#if ENABLE_PROFILER
        volatile ProfilerNode node(i.section);
#endif
        i.function();
    }
}

} // namespace EC
