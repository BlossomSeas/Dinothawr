#include "game.hpp"
#include "utils.hpp"
#include <iostream>
#include <stdexcept>

using namespace Blit;

namespace Icy
{
   Game::Game(const std::string& level)
      : map(level), target(map.pix_width(), map.pix_height())
   {
      player = cache.from_sprite(Utils::join(level, ".sprite"));
      facing = Input::Right;
      player.active_alt(input_to_string(facing));

      player.rect().pos = {16, 16};
   }

   void Game::iterate()
   {
      update_player();

      target.clear(Pixel::ARGB(0xff, 0xa0, 0x80, 0x80));
      map.render(target);
      target.blit(player, {});

      target.finalize();

      if (m_video_cb)
         m_video_cb(target.buffer(), target.width(), target.height(), target.width() * sizeof(Pixel));
   }

   std::vector<SurfaceCluster::Elem> Game::get_tiles_with_attr(const std::string& name,
         const std::string& attr, const std::string& val) const
   {
      std::vector<SurfaceCluster::Elem> surfs;
      auto layer = map.find_layer(name);
      if (!layer)
         return surfs;

      std::copy_if(std::begin(layer->cluster.vec()),
            std::end(layer->cluster.vec()),
            std::back_inserter(surfs), [&attr, &val](const SurfaceCluster::Elem& surf) -> bool {
               auto itr = surf.surf.attr().find(attr);
               return itr != std::end(surf.surf.attr()) && itr->second == val; 
            });

      return surfs;
   }

   // Checks if all goals on floor and blocks are aligned with each other.
   bool Game::won() const
   {
      auto goal_floor  = get_tiles_with_attr("floor", "goal", "true");
      auto goal_blocks = get_tiles_with_attr("blocks", "goal", "true");

      if (goal_floor.size() != goal_blocks.size())
         throw std::logic_error("Number of goal floors and goal blocks do not match.");

      if (goal_floor.empty() || goal_blocks.empty())
         throw std::logic_error("Goal floor or blocks are empty.");

      auto func = [](const SurfaceCluster::Elem& a, const SurfaceCluster::Elem& b) {
         return a.surf.rect().pos < b.surf.rect().pos;
      };

      std::sort(std::begin(goal_floor), std::end(goal_floor), func);
      std::sort(std::begin(goal_blocks), std::end(goal_blocks), func);

      return std::equal(std::begin(goal_floor), std::end(goal_floor),
            std::begin(goal_blocks), [](const SurfaceCluster::Elem& a, const SurfaceCluster::Elem& b) {
               return a.surf.rect().pos == b.surf.rect().pos;
            });
   }

   void Game::update_player()
   {
      if (!m_input_cb)
         return;

      run_stepper();

      if (!stepper)
         update_input();
   }

   void Game::update_input()
   {
      if (m_input_cb(Input::Push))
         push_block();
      else if (m_input_cb(Input::Up))
         move_if_no_collision(Input::Up);
      else if (m_input_cb(Input::Down))
         move_if_no_collision(Input::Down);
      else if (m_input_cb(Input::Left))
         move_if_no_collision(Input::Left);
      else if (m_input_cb(Input::Right))
         move_if_no_collision(Input::Right);
   }

   std::string Game::input_to_string(Input input)
   {
      switch (input)
      {
         case Input::Up:    return "up";
         case Input::Left:  return "left";
         case Input::Right: return "right";
         case Input::Down:  return "down";
         default:           return "";
      }
   }

   Blit::Pos Game::input_to_offset(Input input)
   {
      switch (input)
      {
         case Input::Up:    return {0, -1};
         case Input::Left:  return {-1, 0};
         case Input::Right: return {1, 0};
         case Input::Down:  return {0, 1};
         default:           return {};
      }
   }

   bool Game::is_offset_collision(Surface& surf, Pos offset)
   {
      auto new_rect = surf.rect() + offset;

      bool outside_grid = surf.rect().pos.x % map.tile_width() || surf.rect().pos.y % map.tile_height();
      if (outside_grid)
         throw std::logic_error("Offset collision check was performed outside tile grid.");

      int current_x = surf.rect().pos.x / map.tile_width();
      int current_y = surf.rect().pos.y / map.tile_height();

      int min_tile_x = new_rect.pos.x / map.tile_width();
      int max_tile_x = (new_rect.pos.x + new_rect.w - 1) / map.tile_width();

      int min_tile_y = new_rect.pos.y / map.tile_height();
      int max_tile_y = (new_rect.pos.y + new_rect.h - 1) / map.tile_height();

      for (int y = min_tile_y; y <= max_tile_y; y++)
         for (int x = min_tile_x; x <= max_tile_x; x++)
            if (Pos{x, y} != Pos{current_x, current_y} && map.collision({x, y})) // Can't collide against ourselves.
               return true;

      return false;
   }

   void Game::push_block()
   {
      auto offset = input_to_offset(facing);
      auto dir    = offset * Pos{map.tile_width(), map.tile_height()};
      auto tile   = map.find_tile("blocks", player.rect().pos + dir);

      if (!tile)
         return;

      int tile_x = player.rect().pos.x / map.tile_width();
      int tile_y = player.rect().pos.y / map.tile_height();
      Pos tile_pos{tile_x, tile_y};

      if (!map.collision(tile_pos + (2 * offset)))
         stepper = std::bind(&Game::tile_stepper, this, std::ref(*tile), offset);
   }

   void Game::move_if_no_collision(Input input)
   {
      facing = input;
      player.active_alt(input_to_string(facing));

      auto offset = input_to_offset(input);
      if (!is_offset_collision(player, offset))
         stepper = std::bind(&Game::tile_stepper, this, std::ref(player), offset);
   }

   bool Game::tile_stepper(Surface& surf, Pos step_dir)
   {
      surf.rect() += step_dir;

      if (surf.rect().pos.x % map.tile_width() || surf.rect().pos.y % map.tile_height())
         return true;

      if (is_offset_collision(surf, step_dir))
         return false;

      //std::cerr << "Player: " << player.rect().pos << " Surf: " << surf->rect().pos << std::endl; 
      auto surface = map.find_tile("floor", surf.rect().pos);
      return surface && surface->attr().find(&surf == &player ? "slippery_player" : "slippery_block") != std::end(surface->attr());
   }

   void Game::run_stepper()
   {
      if (stepper && !stepper())
         stepper = {};
   }
}

