#include "physics_world.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define PALETTE_X 12
#define PALETTE_Y 78
#define PALETTE_WIDTH 76
#define PALETTE_SLOT 66
#define PALETTE_GAP 7

static int add_body(PhysicsWorld *world, int group, PhysicsMaterial material, Vec3 position,
                    Vec3 half_extents, Vec3 color, float mass, float buoyancy,
                    float drag, float break_force, float floor_height) {
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        if (world->bodies[i].active) continue;
        world->bodies[i] = (PhysicsBody){
            .active = true,
            .group_id = group,
            .parent = -1,
            .material = material,
            .position = position,
            .previous_position = position,
            .half_extents = half_extents,
            .color = color,
            .alpha = material == PHYSICS_MATERIAL_GLASS ? 0.38f : 1.0f,
            .mass = mass,
            .buoyancy = buoyancy,
            .water_drag = drag,
            .integrity = 1.0f,
            .break_force = break_force,
            .floor_height = floor_height,
        };
        return i;
    }
    return -1;
}

static void connect_body(PhysicsWorld *world, int child, int parent, float break_force) {
    if (child < 0 || parent < 0) return;
    PhysicsBody *body = &world->bodies[child];
    body->parent = parent;
    body->rest_offset = vec3_sub(body->position, world->bodies[parent].position);
    body->break_force = break_force;
}

static int create_group(PhysicsWorld *world) {
    return world->next_group_id++;
}

static bool spawn_crate(PhysicsWorld *world, Vec3 p) {
    int group = create_group(world);
    p.y += 2.2f;
    return add_body(world, group, PHYSICS_MATERIAL_WOOD, p, vec3(0.85f, 0.72f, 0.85f),
                    vec3(0.42f, 0.20f, 0.075f), 3.2f, 1.52f, 2.8f, 100.0f, -2.35f) >= 0;
}

static bool spawn_sandcastle(PhysicsWorld *world, Vec3 p) {
    int group = create_group(world);
    p.y = 0.32f;
    int base = add_body(world, group, PHYSICS_MATERIAL_SAND, p, vec3(1.75f, 0.32f, 1.35f),
                        vec3(0.64f, 0.43f, 0.20f), 18.0f, 0.25f, 4.2f, 1.3f, -0.03f);
    if (base < 0) return false;
    int center = add_body(world, group, PHYSICS_MATERIAL_SAND, vec3_add(p, vec3(0, 1.02f, 0)),
                          vec3(0.66f, 0.72f, 0.58f), vec3(0.70f, 0.48f, 0.23f),
                          8.0f, 0.18f, 4.8f, 1.0f, -0.03f);
    connect_body(world, center, base, 1.0f);
    const float sx[] = {-1.18f, 1.18f, -1.18f, 1.18f};
    const float sz[] = {-0.84f, -0.84f, 0.84f, 0.84f};
    for (int i = 0; i < 4; ++i) {
        int tower = add_body(world, group, PHYSICS_MATERIAL_SAND,
                             vec3_add(p, vec3(sx[i], 0.88f, sz[i])), vec3(0.42f, 0.58f, 0.42f),
                             vec3(0.72f, 0.50f, 0.25f), 5.0f, 0.16f, 5.0f, 0.82f, -0.03f);
        connect_body(world, tower, base, 0.82f);
        int crown = add_body(world, group, PHYSICS_MATERIAL_SAND,
                             vec3_add(p, vec3(sx[i], 1.58f, sz[i])), vec3(0.28f, 0.18f, 0.28f),
                             vec3(0.76f, 0.54f, 0.28f), 1.5f, 0.12f, 5.2f, 0.60f, -0.03f);
        connect_body(world, crown, tower, 0.60f);
    }
    return true;
}

static bool spawn_cloth(PhysicsWorld *world, Vec3 p) {
    int slot = -1;
    for (int i = 0; i < PHYSICS_MAX_CLOTHS; ++i) {
        if (!world->cloths[i].active) { slot = i; break; }
    }
    if (slot < 0) return false;
    PhysicsCloth *cloth = &world->cloths[slot];
    memset(cloth, 0, sizeof(*cloth));
    cloth->active = true;
    cloth->group_id = create_group(world);
    cloth->color = vec3(0.72f, 0.08f, 0.055f);
    const float spacing = 0.25f;
    for (int z = 0; z < CLOTH_ROWS; ++z) {
        for (int x = 0; x < CLOTH_COLUMNS; ++x) {
            int id = z * CLOTH_COLUMNS + x;
            Vec3 position = vec3(p.x + ((float)x - (CLOTH_COLUMNS - 1) * 0.5f) * spacing,
                                 p.y + 2.4f + 0.10f * sinf((float)x * 0.7f),
                                 p.z + ((float)z - (CLOTH_ROWS - 1) * 0.5f) * spacing);
            cloth->nodes[id].position = position;
            cloth->nodes[id].previous_position = position;
        }
    }
    return true;
}

static bool spawn_glass(PhysicsWorld *world, Vec3 p) {
    int group = create_group(world);
    int previous[4] = {-1, -1, -1, -1};
    bool created = false;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            Vec3 position = vec3(p.x + ((float)x - 1.5f) * 0.62f, p.y + 0.62f + y * 1.05f, p.z);
            int body = add_body(world, group, PHYSICS_MATERIAL_GLASS, position,
                                vec3(0.29f, 0.49f, 0.075f), vec3(0.50f, 0.84f, 0.90f),
                                2.6f, 0.20f, 1.2f, 0.72f, -2.35f);
            if (body < 0) continue;
            created = true;
            if (y > 0) connect_body(world, body, previous[x], 0.72f);
            else if (x > 0) connect_body(world, body, previous[x - 1], 0.72f);
            previous[x] = body;
        }
    }
    return created;
}

static bool spawn_lego(PhysicsWorld *world, Vec3 p) {
    int group = create_group(world);
    int below[3] = {-1, -1, -1};
    bool created = false;
    const Vec3 colors[] = {
        {0.82f, 0.04f, 0.03f}, {0.98f, 0.72f, 0.02f}, {0.03f, 0.28f, 0.78f}, {0.04f, 0.60f, 0.20f}
    };
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 3; ++x) {
            Vec3 position = vec3(p.x + ((float)x - 1.0f) * 1.18f, p.y + 0.34f + y * 0.68f, p.z);
            int body = add_body(world, group, PHYSICS_MATERIAL_LEGO, position,
                                vec3(0.55f, 0.30f, 0.48f), colors[(x + y) % 4],
                                1.4f, 0.92f, 2.0f, 1.65f, -2.35f);
            if (body < 0) continue;
            created = true;
            if (below[x] >= 0) connect_body(world, body, below[x], 1.65f);
            else if (x > 0) connect_body(world, body, below[x - 1], 1.65f);
            below[x] = body;
        }
    }
    return created;
}

static bool spawn_raft(PhysicsWorld *world, Vec3 p) {
    int group = create_group(world);
    int previous = -1;
    bool created = false;
    for (int x = 0; x < 6; ++x) {
        Vec3 position = vec3(p.x + ((float)x - 2.5f) * 0.72f, p.y + 1.25f, p.z);
        int body = add_body(world, group, PHYSICS_MATERIAL_WOOD, position,
                            vec3(0.32f, 0.14f, 2.0f), vec3(0.48f + x * 0.025f, 0.27f, 0.09f),
                            2.2f, 1.82f, 3.2f, 1.1f, -2.35f);
        if (body < 0) continue;
        created = true;
        if (previous >= 0) connect_body(world, body, previous, 1.1f);
        previous = body;
    }
    return created;
}

bool physics_world_spawn(PhysicsWorld *world, PhysicsPrefab prefab, Vec3 water_position) {
    int group = world->next_group_id;
    bool created = false;
    switch (prefab) {
    case PHYSICS_PREFAB_CRATE: created = spawn_crate(world, water_position); break;
    case PHYSICS_PREFAB_SANDCASTLE: created = spawn_sandcastle(world, water_position); break;
    case PHYSICS_PREFAB_CLOTH: created = spawn_cloth(world, water_position); break;
    case PHYSICS_PREFAB_GLASS: created = spawn_glass(world, water_position); break;
    case PHYSICS_PREFAB_LEGO: created = spawn_lego(world, water_position); break;
    case PHYSICS_PREFAB_RAFT: created = spawn_raft(world, water_position); break;
    default: break;
    }
    if (created) world->selected_group = group;
    return created;
}

void physics_world_init(PhysicsWorld *world) {
    memset(world, 0, sizeof(*world));
    world->selected_prefab = PHYSICS_PREFAB_CRATE;
    world->next_group_id = 1;
    world->selected_group = -1;
    world->grabbed_group = -1;
}

void physics_world_reset(PhysicsWorld *world) {
    PhysicsPrefab selected = world->selected_prefab;
    physics_world_init(world);
    world->selected_prefab = selected;
}

static bool ray_box(Vec3 origin, Vec3 direction, Vec3 center, Vec3 half, float *distance) {
    float near_t = 0.0f;
    float far_t = FLT_MAX;
    const float o[] = {origin.x, origin.y, origin.z};
    const float d[] = {direction.x, direction.y, direction.z};
    const float c[] = {center.x, center.y, center.z};
    const float h[] = {half.x, half.y, half.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (fabsf(d[axis]) < 0.00001f) {
            if (o[axis] < c[axis] - h[axis] || o[axis] > c[axis] + h[axis]) return false;
            continue;
        }
        float a = (c[axis] - h[axis] - o[axis]) / d[axis];
        float b = (c[axis] + h[axis] - o[axis]) / d[axis];
        if (a > b) { float temp = a; a = b; b = temp; }
        near_t = fmaxf(near_t, a);
        far_t = fminf(far_t, b);
        if (near_t > far_t) return false;
    }
    *distance = near_t;
    return true;
}

static Vec3 group_center(const PhysicsWorld *world, int group) {
    Vec3 sum = vec3(0, 0, 0);
    int count = 0;
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        if (world->bodies[i].active && world->bodies[i].group_id == group) {
            sum = vec3_add(sum, world->bodies[i].position);
            ++count;
        }
    }
    for (int c = 0; c < PHYSICS_MAX_CLOTHS; ++c) {
        if (!world->cloths[c].active || world->cloths[c].group_id != group) continue;
        for (int i = 0; i < CLOTH_NODE_COUNT; ++i) {
            sum = vec3_add(sum, world->cloths[c].nodes[i].position);
            ++count;
        }
    }
    return count ? vec3_scale(sum, 1.0f / (float)count) : vec3(0, 0, 0);
}

static int pick_group(const PhysicsWorld *world, Vec3 origin, Vec3 direction) {
    float nearest = FLT_MAX;
    int group = -1;
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        const PhysicsBody *body = &world->bodies[i];
        float distance;
        if (body->active && ray_box(origin, direction, body->position,
                                    vec3_scale(body->half_extents, 1.15f), &distance) && distance < nearest) {
            nearest = distance;
            group = body->group_id;
        }
    }
    for (int c = 0; c < PHYSICS_MAX_CLOTHS; ++c) {
        const PhysicsCloth *cloth = &world->cloths[c];
        if (!cloth->active) continue;
        Vec3 low = cloth->nodes[0].position;
        Vec3 high = low;
        for (int i = 1; i < CLOTH_NODE_COUNT; ++i) {
            Vec3 p = cloth->nodes[i].position;
            low = vec3(fminf(low.x, p.x), fminf(low.y, p.y), fminf(low.z, p.z));
            high = vec3(fmaxf(high.x, p.x), fmaxf(high.y, p.y), fmaxf(high.z, p.z));
        }
        float distance;
        Vec3 center = vec3_scale(vec3_add(low, high), 0.5f);
        Vec3 half = vec3_add(vec3_scale(vec3_sub(high, low), 0.5f), vec3(0.12f, 0.12f, 0.12f));
        if (ray_box(origin, direction, center, half, &distance) && distance < nearest) {
            nearest = distance;
            group = cloth->group_id;
        }
    }
    return group;
}

static void move_group(PhysicsWorld *world, int group, Vec3 target) {
    Vec3 center = group_center(world, group);
    Vec3 delta = vec3_sub(target, center);
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        PhysicsBody *body = &world->bodies[i];
        if (!body->active || body->group_id != group) continue;
        body->position = vec3_add(body->position, delta);
        body->previous_position = body->position;
        body->velocity = vec3(0, 0, 0);
        body->angular_velocity = vec3(0, 0, 0);
    }
    for (int c = 0; c < PHYSICS_MAX_CLOTHS; ++c) {
        PhysicsCloth *cloth = &world->cloths[c];
        if (!cloth->active || cloth->group_id != group) continue;
        for (int i = 0; i < CLOTH_NODE_COUNT; ++i) {
            cloth->nodes[i].position = vec3_add(cloth->nodes[i].position, delta);
            cloth->nodes[i].previous_position = cloth->nodes[i].position;
            cloth->nodes[i].velocity = vec3(0, 0, 0);
        }
    }
}

void physics_world_delete_selected(PhysicsWorld *world) {
    int group = world->selected_group;
    if (group < 0) return;
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        if (world->bodies[i].active && world->bodies[i].group_id == group) world->bodies[i].active = false;
    }
    for (int i = 0; i < PHYSICS_MAX_CLOTHS; ++i) {
        if (world->cloths[i].active && world->cloths[i].group_id == group) world->cloths[i].active = false;
    }
    world->selected_group = -1;
    world->grabbed_group = -1;
}

int physics_world_group_count(const PhysicsWorld *world) {
    int count = 0;
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        if (!world->bodies[i].active) continue;
        bool seen = false;
        for (int j = 0; j < i; ++j) {
            if (world->bodies[j].active &&
                world->bodies[j].group_id == world->bodies[i].group_id) { seen = true; break; }
        }
        if (!seen) ++count;
    }
    for (int i = 0; i < PHYSICS_MAX_CLOTHS; ++i) {
        if (!world->cloths[i].active) continue;
        bool seen = false;
        for (int b = 0; b < PHYSICS_MAX_BODIES; ++b) {
            if (world->bodies[b].active &&
                world->bodies[b].group_id == world->cloths[i].group_id) { seen = true; break; }
        }
        for (int j = 0; j < i && !seen; ++j) {
            if (world->cloths[j].active &&
                world->cloths[j].group_id == world->cloths[i].group_id) seen = true;
        }
        if (!seen) ++count;
    }
    return count;
}

bool physics_world_update_interaction(PhysicsWorld *world, const InputState *input,
                                      const Camera *camera, const FluidSim *fluid) {
    for (int key = 0; key < PHYSICS_PREFAB_COUNT; ++key) {
        if (input->keys['1' + key]) world->selected_prefab = (PhysicsPrefab)key;
    }
    bool delete_down = input->keys[VK_DELETE] || input->keys[VK_BACK];
    if (delete_down && !world->previous_delete) physics_world_delete_selected(world);
    world->previous_delete = delete_down;

    bool over_palette = input->mouse_x >= PALETTE_X && input->mouse_x < PALETTE_X + PALETTE_WIDTH;
    int slot = (input->mouse_y - PALETTE_Y) / (PALETTE_SLOT + PALETTE_GAP);
    int slot_y = PALETTE_Y + slot * (PALETTE_SLOT + PALETTE_GAP);
    bool valid_slot = over_palette && slot >= 0 && slot < PHYSICS_PREFAB_COUNT &&
                      input->mouse_y >= slot_y && input->mouse_y < slot_y + PALETTE_SLOT;

    if (input->mouse_pressed && valid_slot) {
        world->selected_prefab = (PhysicsPrefab)slot;
        world->placing = true;
        world->placement_valid = false;
    } else if (input->mouse_pressed && !world->placing) {
        Vec3 origin;
        Vec3 direction;
        if (camera_screen_ray(camera, input, &origin, &direction)) {
            int group = pick_group(world, origin, direction);
            world->selected_group = group;
            world->grabbed_group = group;
        }
    }

    if (input->mouse_down && (world->placing || world->grabbed_group >= 0)) {
        float x;
        float z;
        if (camera_ray_to_water(camera, input, FLUID_WORLD_SIZE, &x, &z)) {
            float water_height = 0.0f;
            fluid_sim_sample(fluid, x, z, &water_height, NULL, NULL);
            world->placement_position = vec3(x, water_height, z);
            world->placement_valid = true;
            if (world->grabbed_group >= 0) {
                move_group(world, world->grabbed_group, vec3(x, water_height + 2.0f, z));
            }
        }
    }

    if (input->mouse_released) {
        if (world->placing && world->placement_valid) {
            physics_world_spawn(world, world->selected_prefab, world->placement_position);
        }
        world->placing = false;
        world->placement_valid = false;
        world->grabbed_group = -1;
    }
    return valid_slot || world->placing || world->grabbed_group >= 0;
}

static void solve_body_constraints(PhysicsWorld *world, float delta_time) {
    for (int iteration = 0; iteration < 4; ++iteration) {
        for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
            PhysicsBody *body = &world->bodies[i];
            if (!body->active || body->parent < 0) continue;
            PhysicsBody *parent = &world->bodies[body->parent];
            if (!parent->active) { body->parent = -1; continue; }
            Vec3 target = vec3_add(parent->position, body->rest_offset);
            Vec3 error = vec3_sub(target, body->position);
            float distance = sqrtf(vec3_dot(error, error));
            float relative_speed = sqrtf(vec3_dot(vec3_sub(body->velocity, parent->velocity),
                                                  vec3_sub(body->velocity, parent->velocity)));
            float stress = distance * 2.8f + relative_speed * 0.18f;
            if (stress > body->break_force * fmaxf(body->integrity, 0.12f)) {
                body->parent = -1;
                body->angular_velocity = vec3_add(body->angular_velocity,
                                                   vec3(error.z, 0.0f, -error.x));
                continue;
            }
            Vec3 correction = vec3_scale(error, 0.36f);
            body->position = vec3_add(body->position, correction);
            parent->position = vec3_sub(parent->position, vec3_scale(correction, 0.22f));
            body->velocity = vec3_add(body->velocity, vec3_scale(error, 5.0f * delta_time));
        }
    }
}

static void resolve_body_collisions(PhysicsWorld *world) {
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        PhysicsBody *a = &world->bodies[i];
        if (!a->active) continue;
        for (int j = i + 1; j < PHYSICS_MAX_BODIES; ++j) {
            PhysicsBody *b = &world->bodies[j];
            if (!b->active) continue;
            float overlap_x = a->half_extents.x + b->half_extents.x - fabsf(a->position.x - b->position.x);
            float overlap_y = a->half_extents.y + b->half_extents.y - fabsf(a->position.y - b->position.y);
            float overlap_z = a->half_extents.z + b->half_extents.z - fabsf(a->position.z - b->position.z);
            if (overlap_x <= 0 || overlap_y <= 0 || overlap_z <= 0) continue;
            if (overlap_y <= overlap_x && overlap_y <= overlap_z) {
                float direction = a->position.y < b->position.y ? -1.0f : 1.0f;
                a->position.y += direction * overlap_y * 0.48f;
                b->position.y -= direction * overlap_y * 0.48f;
                float average = (a->velocity.y + b->velocity.y) * 0.5f;
                a->velocity.y = average * 0.35f;
                b->velocity.y = average * 0.35f;
            } else if (overlap_x < overlap_z) {
                float direction = a->position.x < b->position.x ? -1.0f : 1.0f;
                a->position.x += direction * overlap_x * 0.5f;
                b->position.x -= direction * overlap_x * 0.5f;
            } else {
                float direction = a->position.z < b->position.z ? -1.0f : 1.0f;
                a->position.z += direction * overlap_z * 0.5f;
                b->position.z -= direction * overlap_z * 0.5f;
            }
        }
    }
}

static void satisfy_cloth_link(ClothNode *a, ClothNode *b, float rest_length) {
    Vec3 delta = vec3_sub(b->position, a->position);
    float length = sqrtf(vec3_dot(delta, delta));
    if (length < 0.00001f) return;
    Vec3 correction = vec3_scale(delta, (length - rest_length) / length * 0.48f);
    a->position = vec3_add(a->position, correction);
    b->position = vec3_sub(b->position, correction);
}

static void step_cloth(PhysicsWorld *world, PhysicsCloth *cloth, FluidSim *fluid, float delta_time) {
    if (cloth->group_id == world->grabbed_group) return;
    const float spacing = 0.25f;
    Vec3 center = vec3(0, 0, 0);
    float vertical_motion = 0.0f;
    for (int i = 0; i < CLOTH_NODE_COUNT; ++i) {
        ClothNode *node = &cloth->nodes[i];
        node->previous_position = node->position;
        float water_height = -100.0f;
        float water_x = 0.0f;
        float water_z = 0.0f;
        bool in_water = fluid_sim_sample(fluid, node->position.x, node->position.z,
                                         &water_height, &water_x, &water_z);
        node->velocity.y -= 9.81f * delta_time;
        if (in_water && node->position.y < water_height + 0.08f) {
            float depth = clampf(water_height + 0.08f - node->position.y, 0.0f, 0.7f);
            node->velocity.y += (18.0f * depth + 5.2f) * delta_time;
            float drag = clampf(5.5f * delta_time, 0.0f, 1.0f);
            node->velocity.x += (water_x - node->velocity.x) * drag;
            node->velocity.z += (water_z - node->velocity.z) * drag;
        }
        node->velocity = vec3_scale(node->velocity, expf(-0.45f * delta_time));
        node->position = vec3_add(node->position, vec3_scale(node->velocity, delta_time));
        if (node->position.y < -2.30f) {
            node->position.y = -2.30f;
            node->velocity.y *= -0.12f;
        }
        center = vec3_add(center, node->position);
        vertical_motion += fabsf(node->velocity.y);
    }
    for (int iteration = 0; iteration < 6; ++iteration) {
        for (int z = 0; z < CLOTH_ROWS; ++z) {
            for (int x = 0; x < CLOTH_COLUMNS; ++x) {
                int id = z * CLOTH_COLUMNS + x;
                if (x + 1 < CLOTH_COLUMNS) satisfy_cloth_link(&cloth->nodes[id], &cloth->nodes[id + 1], spacing);
                if (z + 1 < CLOTH_ROWS) satisfy_cloth_link(&cloth->nodes[id], &cloth->nodes[id + CLOTH_COLUMNS], spacing);
                if (x + 1 < CLOTH_COLUMNS && z + 1 < CLOTH_ROWS)
                    satisfy_cloth_link(&cloth->nodes[id], &cloth->nodes[id + CLOTH_COLUMNS + 1], spacing * 1.4142f);
            }
        }
    }
    center = vec3_scale(center, 1.0f / CLOTH_NODE_COUNT);
    if (vertical_motion / CLOTH_NODE_COUNT > 0.35f)
        fluid_sim_disturb(fluid, center.x, center.z, -vertical_motion * delta_time * 0.0008f, 1.6f);
}

void physics_world_step(PhysicsWorld *world, FluidSim *fluid, float delta_time) {
    for (int i = 0; i < PHYSICS_MAX_BODIES; ++i) {
        PhysicsBody *body = &world->bodies[i];
        if (!body->active || body->group_id == world->grabbed_group) continue;
        body->previous_position = body->position;
        float water_height = -100.0f;
        float flow_x = 0.0f;
        float flow_z = 0.0f;
        bool over_water = fluid_sim_sample(fluid, body->position.x, body->position.z,
                                           &water_height, &flow_x, &flow_z);
        float bottom = body->position.y - body->half_extents.y;
        float submerged = over_water ? clampf((water_height - bottom) / (body->half_extents.y * 2.0f), 0.0f, 1.0f) : 0.0f;
        body->velocity.y += (-9.81f + 9.81f * body->buoyancy * submerged) * delta_time;
        if (submerged > 0.0f) {
            float drag = clampf(body->water_drag * submerged * delta_time, 0.0f, 0.92f);
            body->velocity.x += (flow_x - body->velocity.x) * drag;
            body->velocity.z += (flow_z - body->velocity.z) * drag;
            body->velocity.y *= 1.0f - drag * 0.52f;
            float left_height = water_height;
            float right_height = water_height;
            float near_height = water_height;
            float far_height = water_height;
            fluid_sim_sample(fluid, body->position.x - body->half_extents.x, body->position.z, &left_height, NULL, NULL);
            fluid_sim_sample(fluid, body->position.x + body->half_extents.x, body->position.z, &right_height, NULL, NULL);
            fluid_sim_sample(fluid, body->position.x, body->position.z - body->half_extents.z, &near_height, NULL, NULL);
            fluid_sim_sample(fluid, body->position.x, body->position.z + body->half_extents.z, &far_height, NULL, NULL);
            body->angular_velocity.x += (near_height - far_height) * submerged * 2.1f * delta_time;
            body->angular_velocity.z += (right_height - left_height) * submerged * 2.1f * delta_time;

            float flow_speed = hypotf(flow_x, flow_z);
            if (body->material == PHYSICS_MATERIAL_SAND) {
                body->integrity -= submerged * (0.045f + flow_speed * 0.055f) * delta_time;
                float erosion = clampf(body->integrity, 0.25f, 1.0f);
                body->half_extents.x *= 1.0f - (1.0f - erosion) * 0.00045f;
                body->half_extents.z *= 1.0f - (1.0f - erosion) * 0.00045f;
                if (body->integrity < 0.18f) body->parent = -1;
            }
            if (body->material == PHYSICS_MATERIAL_GLASS &&
                flow_speed + fabsf(body->velocity.y) > 3.2f) {
                body->parent = -1;
                body->angular_velocity = vec3_add(body->angular_velocity,
                    vec3(flow_z * 0.6f, flow_x * 0.35f, -flow_x * 0.6f));
            }
            float reaction = -body->velocity.y * submerged * delta_time *
                             (body->half_extents.x + body->half_extents.z) * 0.025f;
            if (fabsf(reaction) > 0.00008f)
                fluid_sim_disturb(fluid, body->position.x, body->position.z, reaction,
                                  fmaxf(body->half_extents.x, body->half_extents.z) + 0.35f);
        }

        body->velocity = vec3_scale(body->velocity, expf(-0.08f * delta_time));
        body->angular_velocity = vec3_scale(body->angular_velocity, expf(-0.70f * delta_time));
        body->position = vec3_add(body->position, vec3_scale(body->velocity, delta_time));
        body->rotation = vec3_add(body->rotation, vec3_scale(body->angular_velocity, delta_time));
        float minimum_y = body->floor_height + body->half_extents.y;
        if (body->position.y < minimum_y) {
            float impact = -body->velocity.y;
            body->position.y = minimum_y;
            body->velocity.y = impact > 0.8f ? impact * 0.12f : 0.0f;
            body->velocity.x *= 0.78f;
            body->velocity.z *= 0.78f;
            if (body->material == PHYSICS_MATERIAL_GLASS && impact > 2.4f) body->parent = -1;
        }
        float limit = FLUID_WORLD_SIZE * 0.5f - 0.4f;
        body->position.x = clampf(body->position.x, -limit, limit);
        body->position.z = clampf(body->position.z, -limit, limit);
    }

    solve_body_constraints(world, delta_time);
    resolve_body_collisions(world);
    for (int c = 0; c < PHYSICS_MAX_CLOTHS; ++c) {
        if (world->cloths[c].active) step_cloth(world, &world->cloths[c], fluid, delta_time);
    }
}

const char *physics_prefab_name(PhysicsPrefab prefab) {
    static const char *names[] = {"Crate", "Sandcastle", "Cloth", "Glass", "Lego tower", "Raft"};
    return prefab >= 0 && prefab < PHYSICS_PREFAB_COUNT ? names[prefab] : "Unknown";
}
