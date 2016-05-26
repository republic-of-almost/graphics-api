#ifndef RENDERER_MESH_DATA_INCLUDED_07A2C7C0_DD0B_4188_8F36_F68046D09FF0
#define RENDERER_MESH_DATA_INCLUDED_07A2C7C0_DD0B_4188_8F36_F68046D09FF0


#include <utilities/generic_id.hpp>
#include <utilities/memory_pool.hpp>
#include <graphics_api/ogl/ogl_texture.hpp>
#include <graphics_api/mesh.hpp>
#include <stdint.h>
#include <stddef.h>


namespace World_data {


struct Mesh_renderer_draw_call
{
  uint32_t model;
  uint32_t texture; // Move this to the material.
  float    world_matrix[16];
};


struct Mesh_renderer_data
{
  util::generic_id          *entity_id       = nullptr;
  Mesh_renderer_draw_call   *mesh_draw_calls = nullptr;
  
  uint32_t                  size             = 0;
  const uint32_t            capacity         = 0;
  const util::memory_chunk  memory           = util::memory_chunk();
};


void
lock(Mesh_renderer_data *data);


void
unlock(Mesh_renderer_data *data);


/*!
  Initialize the mesh renderer data.
*/
void
mesh_renderer_init(Mesh_renderer_data *data,
                   const uint32_t entity_hint);


/*!
  Add a new mesh renderering node to the data.
*/
void
mesh_renderer_add(Mesh_renderer_data *data,
                  const util::generic_id id,
                  const uint32_t model_id,
                  const uint32_t texture_id);


void
mesh_renderer_update_model(Mesh_renderer_data *data,
                           const util::generic_id id,
                           const uint32_t model_id);


void
mesh_renderer_update_texture(Mesh_renderer_data *data,
                             const util::generic_id id,
                             const uint32_t texture_id);


/*!
  Remove a mesh rendenring node.
*/
void
mesh_renderer_remove(Mesh_renderer_data *data,
                     const util::generic_id id);
  

/*!
  Checks to see if an id exists
*/
bool
mesh_renderer_exists(const Mesh_renderer_data *data,
                     const util::generic_id id,
                     size_t *index = nullptr);


} // ns


#endif // inc guard