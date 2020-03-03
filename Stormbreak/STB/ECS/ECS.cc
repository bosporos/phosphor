//
// project stormbreak
// author Maximilien M. Cura
//

#include <STB/ECS/ECS>

using namespace STB::ECS;

const EntityFamilyID STB::ECS::DummyFamilyID = 0;

EntityID EntityManager::create_entity (EntityFamilyID _f_id)
{
    EntityID _e_id;
    if (free_entity_stack.size ()) {
        _e_id = free_entity_stack.front ();
        free_entity_stack.pop ();
    } else {
        _e_id = next_entity++;
    }
    entity_mapping[_e_id] = _f_id;
    return _e_id;
}

bool EntityManager::entity_exists (EntityID _e_id)
{
    return entity_mapping.contains (_e_id);
}

void EntityManager::destroy_entity (EntityID _e_id)
{
    if (entity_exists (_e_id)) {
        entity_mapping.erase (_e_id);
        free_entity_stack.push (_e_id);
    }
}

void EntityManager::family (EntityID _e_id, EntityFamilyId _ef_id)
{
    if (entity_exists (_e_id))
        entity_mapping[_e_id] = _ef_id;
}

EntityFamilyID EntityManager::family (EntityID _e_id)
{
    if (entity_exists (_e_id))
        return entity_mapping[_e_id];
    return DummyFamilyID;
}

SystemStub::SystemStub (SystemImplementationFn _si_fn)
    : system_impl { _si_fn }
{}
