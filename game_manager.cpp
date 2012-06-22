#include "game.hpp"
#include "pugixml/pugixml.hpp"
#include "utils.hpp"

#include <iostream>
#include <assert.h>

using namespace Blit;
using namespace pugi;

namespace Icy
{
   GameManager::GameManager(const std::string& path_game,
         std::function<bool (Input)> input_cb,
         std::function<void (const void*, unsigned, unsigned, std::size_t)> video_cb)
      : dir(Utils::basedir(path_game)), m_current_level(0), m_game_state(State::Title),
         m_input_cb(input_cb), m_video_cb(video_cb)
   {
      xml_document doc;
      if (!doc.load_file(path_game.c_str()))
         throw std::runtime_error(Utils::join("Failed to load game: ", path_game, "."));

      Utils::xml_node_walker walk{doc.child("game"), "map", "source"};
      for (auto& val : walk)
         levels.push_back(Utils::join(dir, "/", val));
      
      for (auto& str : levels)
         std::cerr << "Found level: " << str << std::endl;

      font_sel = font_unsel = Font(Utils::join(dir, "/", doc.child("game").child("font").attribute("source").value()));
      font_sel.set_color(Pixel::ARGB(0xff, 0xff, 0xff, 0x00));
      font_unsel.set_color(Pixel::ARGB(0xff, 0x00, 0xff, 0xff));

      font_bg = RenderTarget(Game::fb_width, Game::fb_height);

      init_menu(doc.child("game").child("title").attribute("source").value());
   }

   GameManager::GameManager() : m_current_level(0), m_game_state(State::Game) {}

   void GameManager::init_menu(const std::string& level)
   {
      Surface surf = cache.from_image(Utils::join(dir, "/", level));

      target = RenderTarget(Game::fb_width, Game::fb_height);
      target.blit(surf, {});
      font_sel.render_msg(target, "Press Start", 100, 150);
   }

   void GameManager::reset_level()
   {
      change_level(m_current_level);
   }

   void GameManager::change_level(unsigned level) 
   {
      game = std::unique_ptr<Game>(new Game(levels.at(level)));
      game->input_cb(m_input_cb);
      game->video_cb(m_video_cb);

      m_current_level = level;
   }

   void GameManager::init_level(unsigned level)
   {
      change_level(level);
      m_game_state = State::Game;
   }

   void GameManager::step_title()
   {
      if (m_input_cb(Input::Start))
         enter_menu();

      m_video_cb(target.buffer(), target.width(), target.height(), target.width() * sizeof(Pixel));
   }

   void GameManager::enter_menu()
   {
      m_game_state = State::Menu;
      level_select = m_current_level;
   }

   void GameManager::step_menu()
   {
      font_bg.clear(Pixel::ARGB(0xff, 0x18, 0x18, 0x18));

      bool pressed_menu_up     = m_input_cb(Input::Up);
      bool pressed_menu_down   = m_input_cb(Input::Down);
      bool pressed_menu_ok     = m_input_cb(Input::Push);
      bool pressed_menu_cancel = m_input_cb(Input::Cancel);

      int start_level = std::max(level_select - 5, 0);
      int end_level   = std::min(start_level + 10, static_cast<int>(levels.size()));

      int x = 20;
      int y = 20;
      for (int i = start_level; i < end_level; i++,
            y += font_sel.glyph_size().y + 2)
      {
         const Font& font =
            i == level_select ? font_sel : font_unsel;
         font.render_msg(font_bg, levels[i], x, y);
      }

      if (pressed_menu_up && !old_pressed_menu_up)
         level_select = std::max(level_select - 1, 0);
      else if (pressed_menu_down && !old_pressed_menu_down)
         level_select = std::min(level_select + 1, static_cast<int>(levels.size() - 1));

      if (pressed_menu_ok)
         init_level(level_select);
      else if (pressed_menu_cancel && game)
         m_game_state = State::Game;

      old_pressed_menu_up   = pressed_menu_up;
      old_pressed_menu_down = pressed_menu_down;

      m_video_cb(font_bg.buffer(), font_bg.width(), font_bg.height(), font_bg.width() * sizeof(Pixel));
   }

   void GameManager::step_game()
   {
      if (!game)
         return;

      game->iterate();

      if (m_input_cb(Input::Select) && m_input_cb(Input::Start))
         reset_level();

      if (m_input_cb(Input::Menu))
         enter_menu();

      if (game->won())
      {
         m_current_level++;
         if (m_current_level >= levels.size())
            game.reset();
         else
            change_level(m_current_level);
      }
   }

   void GameManager::iterate()
   {
      switch (m_game_state)
      {
         case State::Title: return step_title();
         case State::Menu: return step_menu();
         case State::Game: return step_game();
         default: throw std::logic_error("Game state is invalid.");
      }
   }

   bool GameManager::done() const
   {
      return !game && m_game_state == State::Game;
   }
}

