#include <core/world/world.hpp>
#include <core/world/world_setup.hpp>

#include <core/memory/memory.hpp>

#include <graphics_api/initialize.hpp>
#include <graphics_api/clear.hpp>

#include <core/physics/collision_pair.hpp>
#include <core/entity/entity.hpp>
#include <core/entity/entity_ref.hpp>
#include <core/world/detail/world_detail.hpp>

#include <renderer/renderer.hpp>
#include <renderer/simple_renderer/simple_renderer.hpp>
#include <renderer/debug_line_renderer/debug_line_renderer.hpp>
#include <renderer/gui_renderer/gui_renderer.hpp>

#include <data/world_data/physics_data.hpp>

#include <utilities/logging.hpp>
#include <utilities/generic_id.hpp>
#include <math/math.hpp> // remove
#include <vector> // remove

#include <systems/physics_engine/broadphase/sweep.hpp>
#include <systems/physics_engine/broadphase/prune.hpp>
#include <systems/physics_engine/collision/aabb_overlap.hpp>
#include <systems/physics_engine/collision/collision_pairs.hpp>
#include <systems/physics_engine/physics_engine.hpp>
#include <systems/physics_engine/collision/axis_collidable.hpp>

#include <data/global_data/resource_data.hpp>

#include <data/world_data/transform_data.hpp>
#include <data/world_data/entity_data.hpp>

#include <3rdparty/imgui/imgui.h>
#include <3rdparty/imgui/imgui_impl_sdl_gl3.h>
#include <string>

#include <debug_gui/debug_menu.hpp>

// Header dump start
#include <core/renderer/material_renderer.hpp>
#include <core/entity/entity.hpp>
#include <core/entity/entity_ref.hpp>
#include <core/color/color.hpp>
#include <core/color/color_utils.hpp>
#include <core/camera/camera_properties.hpp>
#include <core/transform/transform.hpp>

#include <graphics_api/clear.hpp>
#include <graphics_api/initialize.hpp>
#include <graphics_api/mesh.hpp>

#include <renderer/simple_renderer/simple_renderer_node.hpp>
#include <renderer/simple_renderer/simple_renderer.hpp>
#include <renderer/debug_line_renderer/debug_line_renderer_node.hpp>
#include <renderer/debug_line_renderer/debug_line_renderer.hpp>
#include <renderer/renderer.hpp>
#include <renderer/simple_renderer/simple_renderer.hpp>

#include <data/global_data/resource_data.hpp>

#include <systems/transform/transformations.hpp>
#include <data/world_data/world_data.hpp>
#include <math/math.hpp>
#include <vector>

#include <utilities/conversion.hpp>
// Header dump end

#include <systems/renderer_material/material_renderer.hpp>


namespace Core {


struct World::Impl
{
  std::shared_ptr<World_detail::Data> world_data = std::make_shared<World_detail::Data>();
};


World::World(const World_setup setup)
: m_impl(new World::Impl)
{
  LOG_TODO("Make world require context to setup.");

  Core::Memory::initialize(util::convert_mb_to_bytes(128));

  LOG_TODO("Remove static data stores");
  
  constexpr uint32_t entity_hint = 2048;
  
  static World_data::Pending_scene_graph_change_data graph_changes;
  World_data::pending_scene_graph_change_init(&graph_changes, entity_hint);
  
  static World_data::Camera_pool camera_pool;
  World_data::camera_pool_init(&camera_pool);
    
  static World_data::Physics_data physics_data;
  World_data::physics_init(&physics_data, entity_hint);
  
  static World_data::Mesh_renderer_data mesh_data;
  World_data::mesh_renderer_init(&mesh_data, entity_hint);
  
  static World_data::Transform_data transform_data;
  World_data::transform_data_init(&transform_data, entity_hint);
  
  static World_data::Entity_data entity_data;
  World_data::entity_data_init(&entity_data, entity_hint);
  
  m_impl->world_data->data.entity_graph_changes = &graph_changes;
  m_impl->world_data->data.camera_pool          = &camera_pool;
  m_impl->world_data->data.physics_data         = &physics_data;
  m_impl->world_data->data.mesh_data            = &mesh_data;
  
  m_impl->world_data->data.transform            = &transform_data;
  m_impl->world_data->data.entity               = &entity_data;
  
  LOG_TODO("We can store the data directly and get rid of ::World_data::World")
  World_data::set_world_data(&m_impl->world_data->data);
  
  Simple_renderer::initialize();
  Debug_line_renderer::initialize();
  
  ::Material_renderer::initialize();
  
}


World::~World()
{
}


void
World::think(const float dt)
{
  Debug_menu::display_global_data_menu();
  Debug_menu::dispaly_world_data_menu(&m_impl->world_data->data);

  // Update world
  

  auto data = &m_impl->world_data->data;
  auto graph_changes = m_impl->world_data->data.entity_graph_changes;

  // Push in new phy entities.
  World_data::world_update_scene_graph_changes(data, graph_changes);
  
  // Reset the entity pool for new changes.
  World_data::pending_scene_graph_change_reset(graph_changes);
  
  // Render world
  {
    const uint32_t peer = 0;
    World_data::World *world = World_data::get_world();

    static std::vector<Simple_renderer::Node> nodes;
    nodes.resize(world->entity->size);
    const uint32_t size_of_node_pool = nodes.size();

    // Get active camera and generate a projection matrix.
    // TODO: Need to pass in the environment height and width into this function.
    const auto cam = World_data::camera_pool_get_properties_for_priority(world->camera_pool, peer, 0);
    
    math::mat4 proj;
    
    if(cam.type == Core::Camera_type::orthographic)
    {
      proj = math::mat4_orthographic(cam.viewport_width,
                                     cam.viewport_height,
                                     cam.near_plane,
                                     cam.far_plane);
    }
    else
    {
      proj = math::mat4_projection(cam.viewport_width,
                                   cam.viewport_height,
                                   cam.near_plane,
                                   cam.far_plane,
                                   cam.fov);
  }
  
  const Core::Color clear_color(cam.clear_color);
  const float red = Core::Color_utils::get_red_f(clear_color);
  const float green = Core::Color_utils::get_green_f(clear_color);
  const float blue = Core::Color_utils::get_blue_f(clear_color);
  
  Graphics_api::clear_color_set(red, green, blue);
  
  const uint32_t flags = Graphics_api::Clear_flag::color | Graphics_api::Clear_flag::depth;
  Graphics_api::clear(flags);
  
  // Get entity's transform so we can generate a view.
  math::mat4 view = math::mat4_zero();
  {
    const auto id = World_data::camera_pool_get_entity_id_for_priority(world->camera_pool,
                                                                       peer,
                                                                       0);
    
    // If we cant find the camera we'll just make a dummy orbit one for the time.
    // This is good for debugging.
    if (id != util::generic_id_invalid())
    //if(false) // debug cam route
    {
      size_t ent_index;
//      World_data::entity_pool_find_index(world->entity_pool, id, &ent_index);
      if(World_data::transform_data_exists(world->transform, id, &ent_index))
      {
        const math::transform trans = world->transform->transform[ent_index];
        const Core::Transform cam_transform(trans.position, trans.scale, trans.rotation);
        
        view = math::mat4_lookat(cam_transform.get_position(),
                                 math::vec3_add(cam_transform.get_position(), cam_transform.get_forward()),
                                 cam_transform.get_up());
      }
      else
      {
        assert(false);
        LOG_FATAL("");
      }
    }
    else
    {
      static float time = 4;
      time += 0.005f;

      const float x = math::sin(time) * 9;
      const float z = math::cos(time) * 9;

      const math::vec3 eye_pos = math::vec3_init(x, 5.f, z);
      const math::vec3 look_at = math::vec3_zero();
      const math::vec3 up      = math::vec3_init(0.f, 1.f, 0.f);
      
      view = math::mat4_lookat(eye_pos, look_at, up);
    }
  }

  const math::mat4 view_proj = math::mat4_multiply(view, proj);

  ::Transform::transforms_to_wvp_mats(world->transform->transform,
                                      world->transform->size,
                                      view_proj,
                                      nodes[0].wvp,
                                      size_of_node_pool,
                                      sizeof(Simple_renderer::Node));

//  ::Transform::transforms_to_world_mats(world->transform->transform,
//                                        world->transform->size,
//                                        nodes[0].world_mat,
//                                        size_of_node_pool,
//                                        sizeof(Simple_renderer::Node));
  
  // Texture/vbo info
  for (uint32_t i = 0; i < size_of_node_pool; ++i)
  {
    const uint32_t mesh_id    = world->mesh_data->mesh_draw_calls[i].model;
    const uint32_t texture_id = world->mesh_data->mesh_draw_calls[i].texture;
    
    if(mesh_id && texture_id)
    {
      Resource_data::Resources *resources = Resource_data::get_resources();
      
      Graphics_api::Mesh get_mesh = Resource_data::mesh_pool_find(resources->mesh_pool, mesh_id);
      Ogl::Texture get_texture    = Resource_data::texture_pool_find(resources->texture_pool, texture_id);
      
      nodes[i].vbo     = get_mesh.vbo;
      nodes[i].diffuse = get_texture;
      memcpy(nodes[i].world_mat, world->mesh_data->mesh_draw_calls[i].world_matrix, sizeof(float) * 16);
    }
  }
  
  Simple_renderer::render_nodes_fullbright(nodes.data(), size_of_node_pool);
  //Simple_renderer::render_nodes_directional_light(nodes, size_of_node_pool);

  math::mat4 wvp = math::mat4_multiply(math::mat4_id(), view, proj);
  Debug_line_renderer::render(math::mat4_get_data(wvp));

  }
}


void
World::get_overlapping_aabbs(const std::function<void(const Core::Collision_pair pairs[],
                                                      const uint32_t number_of_pairs)> &callback)
{
  // Check we have a callback.
  if(!callback) { return; }

  const World_data::Entity_data  *entity_data = m_impl->world_data->data.entity;
  const World_data::Physics_data *data = m_impl->world_data->data.physics_data;
  
  static std::vector<math::aabb> boundings;
  boundings.clear();
  
  for(uint32_t i = 0; i < data->size; ++i)
  {
    math::aabb box_copy(data->aabb_collider[i]);
    math::aabb_scale(box_copy, data->transform[i].scale);
    math::aabb_set_origin(box_copy, data->transform[i].position);
    
    boundings.push_back(box_copy);
  }
  
  Physics::Broadphase::Sweep sweep;
  Physics::Broadphase::sweep_init(&sweep, data->size);
  Physics::Broadphase::sweep_calculate(&sweep, boundings.data(), boundings.size());
  
  assert(sweep.size == boundings.size());
  
  Physics::Broadphase::Prune prune;
  Physics::Broadphase::prune_init(&prune, &sweep);
  Physics::Broadphase::prune_calculate(&prune, &sweep);

  // Prune out
  static std::vector<util::generic_id> id;
  static std::vector<Physics::Collision::Axis_collidable> boxes;
  
  id.clear();
  boxes.clear();
  
  uint32_t prune_stack = 0;
    
  for(uint32_t i = 0; i < data->size; ++i)
  {
    if(prune_stack < prune.size && i == prune.ids[prune_stack])
    {
      ++prune_stack;
            
      continue;
    }
    
    id.push_back(data->entity_id[i]);
    
    math::aabb box_copy(data->aabb_collider[i]);
    uint64_t collision_mask(data->collision_id[i]);
    math::aabb_scale(box_copy, data->transform[i].scale);
    math::aabb_set_origin(box_copy, data->transform[i].position);
    
    boxes.push_back(Physics::Collision::Axis_collidable{collision_mask, box_copy});
  }
  
  assert(prune_stack == prune.size);
  
  // Calculate collisions
  Physics::Collision::Pairs out_pairs;
  Physics::Collision::pairs_init(&out_pairs, 2048 * 10);
  
  Physics::Collision::aabb_calculate_overlaps_pairs(&out_pairs, boxes.data(), boxes.size());
  
  static Core::Collision_pair *pairs = nullptr;
  if(!pairs)
  {
    pairs = new Core::Collision_pair[2048 * 10];
  }
  
  uint32_t number_of_pairs = 0;
  const uint32_t max_pairs = 2048 * 10;
  
  if(out_pairs.size)
  {
    // Build collision pairs array.
    for(int32_t i = 0; i < std::min(out_pairs.size, max_pairs); ++i)
    {
      const uint32_t index_a = out_pairs.pair_arr[i].a;
      const uint32_t index_b = out_pairs.pair_arr[i].b;

      pairs[number_of_pairs++] = Core::Collision_pair{find_entity_by_id(id[index_a]), find_entity_by_id(id[index_b])};
    }
  
    callback(pairs, number_of_pairs);
  }
  
  
  Physics::Broadphase::sweep_free(&sweep);
  Physics::Broadphase::prune_free(&prune);
  Physics::Collision::pairs_free(&out_pairs);
}


Entity_ref
World::find_entity_by_id(const util::generic_id id) const
{
  return Entity_ref(id, *const_cast<World*>(this));
}


void
World::find_entities_by_tag(const uint32_t tag_id,
                            Entity_ref **out_array,
                            size_t *out_array_size)
{
  static Entity_ref found_ents[1024];
  static size_t count = 0;
  
  count = 0;
  
  // Loop through entity data and find entities.
  
  auto data = m_impl->world_data->data.entity;
  
  
  lock(data);
  
  for(uint32_t i = 0; i < data->size; ++i)
  {
    if(data->tags[i] & tag_id)
    {
      found_ents[count] = Entity_ref(data->entity_id[i], *const_cast<World*>(this));
      count++;
    }
  }
  
  unlock(data);
  
  *out_array = found_ents;
  *out_array_size = count;
}


std::shared_ptr<const World_detail::Data>
World::get_world_data() const
{
  assert(m_impl);
  return m_impl->world_data;
}


std::shared_ptr<World_detail::Data>
World::get_world_data()
{
  assert(m_impl);
  return m_impl->world_data;
}


} // ns