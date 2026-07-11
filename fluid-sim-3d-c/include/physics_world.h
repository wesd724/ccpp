#ifndef PHYSICS_WORLD_H
#define PHYSICS_WORLD_H

#include "camera.h"
#include "fluid_sim.h"
#include "math3d.h"
#include "platform.h"

#include <stdbool.h>

#define PHYSICS_MAX_BODIES 384
#define PHYSICS_MAX_CLOTHS 4
#define CLOTH_COLUMNS 14
#define CLOTH_ROWS 10
#define CLOTH_NODE_COUNT (CLOTH_COLUMNS * CLOTH_ROWS)

typedef enum {
    PHYSICS_PREFAB_CRATE,
    PHYSICS_PREFAB_SANDCASTLE,
    PHYSICS_PREFAB_CLOTH,
    PHYSICS_PREFAB_GLASS,
    PHYSICS_PREFAB_LEGO,
    PHYSICS_PREFAB_RAFT,
    PHYSICS_PREFAB_COUNT
} PhysicsPrefab;

typedef enum {
    PHYSICS_MATERIAL_WOOD,
    PHYSICS_MATERIAL_SAND,
    PHYSICS_MATERIAL_GLASS,
    PHYSICS_MATERIAL_LEGO
} PhysicsMaterial;

typedef struct {
    bool active;
    int group_id;
    int parent;
    PhysicsMaterial material;
    Vec3 position;
    Vec3 previous_position;
    Vec3 velocity;
    Vec3 rotation;
    Vec3 angular_velocity;
    Vec3 half_extents;
    Vec3 rest_offset;
    Vec3 color;
    float alpha;
    float mass;
    float buoyancy;
    float water_drag;
    float integrity;
    float break_force;
    float floor_height;
} PhysicsBody;

typedef struct {
    Vec3 position;
    Vec3 previous_position;
    Vec3 velocity;
} ClothNode;

typedef struct {
    bool active;
    int group_id;
    Vec3 color;
    ClothNode nodes[CLOTH_NODE_COUNT];
} PhysicsCloth;

typedef struct {
    PhysicsBody bodies[PHYSICS_MAX_BODIES];
    PhysicsCloth cloths[PHYSICS_MAX_CLOTHS];
    PhysicsPrefab selected_prefab;
    int next_group_id;
    int selected_group;
    int grabbed_group;
    bool placing;
    bool placement_valid;
    bool previous_delete;
    Vec3 placement_position;
    Vec3 grab_offset;
} PhysicsWorld;

void physics_world_init(PhysicsWorld *world);
void physics_world_reset(PhysicsWorld *world);
void physics_world_step(PhysicsWorld *world, FluidSim *fluid, float delta_time);
bool physics_world_update_interaction(PhysicsWorld *world, const InputState *input,
                                      const Camera *camera, const FluidSim *fluid);
bool physics_world_spawn(PhysicsWorld *world, PhysicsPrefab prefab, Vec3 water_position);
void physics_world_delete_selected(PhysicsWorld *world);
int physics_world_group_count(const PhysicsWorld *world);
const char *physics_prefab_name(PhysicsPrefab prefab);

#endif
