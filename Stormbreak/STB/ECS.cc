//
// project stormbreak
// author Maximilien M. Cura
//

#include <STB/ECS>

using namespace STB;

const EntityFamilyID STB::DummyFamilyID = 0;

EntityID EntityManager::create_entity (EntityFamilyID _f_id)
{
    EntityID _e_id;
    if (free_entity_stack.size ()) {
        _e_id = free_entity_stack.top ();
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

void EntityManager::family (EntityID _e_id, EntityFamilyID _ef_id)
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

void SystemStub::system_applies_to (EntityFamilyID _ef_id, bool _applies)
{
    if (_applies)
        system_applicable_families.insert (_ef_id);
    else
        system_applicable_families.erase (_ef_id);
}

bool SystemStub::system_applies_to (EntityFamilyID _ef_id) const
{
    return system_applicable_families.contains (_ef_id);
}

void SystemStub::system_run (EntityID _e_id, World & _world)
{
    (*this->system_impl) (_e_id, _world);
}

std::unordered_map<SystemRegistrationID, SystemStub *> SystemManager::system_mapping = {};
SystemRegistrationID SystemManager::next_registration_id                             = 0;

SystemRegistrationID SystemManager::register_system (SystemStub * _stub)
{
    SystemManager::system_mapping[SystemManager::next_registration_id] = _stub;
    return SystemManager::next_registration_id++;
}

SystemStub * SystemManager::system (SystemRegistrationID _sr_id)
{
    if (LIKELY (SystemManager::system_mapping.contains (_sr_id)))
        return SystemManager::system_mapping[_sr_id];
    return nullptr;
}

SystemExecutor::SystemExecutor (World & _world)
    : world { _world }
{}

SystemExecutor::~SystemExecutor ()
{}

void SystemExecutor::apply_system (SystemStub * _stub)
{
    for (std::pair<EntityID, EntityFamilyID> _e_pair : world.entity_manager.entity_mapping) {
        if (_stub->system_applies_to (_e_pair.second)) {
            _stub->system_run (_e_pair.first, world);
        }
    }
}
