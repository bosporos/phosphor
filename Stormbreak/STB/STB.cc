//
// project stormbreak
// author Maximilien M. Cura
//

#include <STB/ECS/ECS>
#include <Pewter/Engines/TTY/TTY>
#include <Pewter/Display>
#include <Pewter/Glyph>

typedef STB::ECS::World<64, 256> STBWorld;

STBWorld world;

struct TimeComponent
{
    static constexpr STBWorld::ComponentMask Mask = 2;
    typedef STB::ECS::StandardComponentStorage<TimeComponent> Storage;

    void destroy () {}
};

STBDeclSystem (STBTimeSystem, TimeComponent::Mask, world)
{
    STB::ECS::Entity e = _STBSystem_Entity;
    _STBSystem_World.mask (e);
}

int main (int argc, char ** argv)
{
    STB::ECS::Entity e = world.create_entity (TimeComponent::Mask);
    STB::ECS::ComponentManager<TimeComponent>::try_add (e, new TimeComponent);
    STB::ECS::SystemRegistrar<64, 256>::instance ().run_system (STBTimeSystem::ID, world);

    return 0;
}
