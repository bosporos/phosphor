//
// project pewter
// author Maximilien M. Cura
//

#include <Venice/Lang/Move>
#include <Pewter/Display/View>

#include <vector>

using namespace pewter::display;

View::View (math::Rect<DisplayCoordinate> box, event::EventRegistry * er)
    : view_buffers ()
    , view_event_responders ()
    , view_box { box }
{
    view_size_change_event_id = er->try_register_event ("view-size!change");
    view_termination_event_id = er->try_register_event ("view!death");
}

View::~View ()
{
    view_event_responders.dispatch (event::event (view_termination_event_id, nullptr));
}

void View::add_buffer (Buffer * buffer)
{
    view_buffers.push_back (buffer);
}

void View::remove_buffer (Buffer * buffer)
{
    view_buffers.remove (buffer);
}

vnz::math::_usize View::depth ()
{
    return view_buffers.size ();
}

std::vector<Buffer *> View::get_buffers ()
{
    return std::vector (view_buffers.begin (), view_buffers.end ());
}

void View::notify_of_resize (math::Rect<DisplayCoordinate> new_box)
{
    view_box = new_box;
    for (Buffer * buffer : view_buffers) {
        buffer->allocate_buffers (new_box);
    }
    view_event_responders.dispatch (event::event (view_termination_event_id, &new_box));
}
