//
// project pewter
// author Maximilien M. Cura
//

#include <Pewter/Event/Event>
#include <strings.h>

using namespace pewter::event;

EventResponderStack::EventResponderStack ()
    : responders ()
{}

void EventResponderStack::add_responder (EventResponderFn fn)
{
    responders.push_back (fn);
}

void EventResponderStack::dispatch (Event e)
{
    for (EventResponderFn fn : responders) {
        fn (e);
    }
}

EventID EventRegistry::try_register_event (const char * _event_name)
{
    vnz::math::_usize _sl = strlen (_event_name), _idn = event_names.size ();
    for (EventID _id = 0; _id < _idn; _id++) {
        if (event_names[_id] == _event_name) {
            return _id;
        }
    }
    event_names.push_back (std::string (_event_name));
    return event_names.size () - 1;
}
