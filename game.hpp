#ifndef GAME_HPP__
#define GAME_HPP__

#include "surface.hpp"
#include "tilemap.hpp"
#include "font.hpp"
#include "audio/mixer.hpp"

#include <string>
#include <functional>
#include <cstddef>
#include <functional>

namespace Icy
{
   Audio::Mixer& get_mixer();
   const std::string& get_basedir();

   enum class Input : unsigned
   {
      Up = 0,
      Down,
      Left,
      Right,
      Push,
      Cancel,
      Start,
      Select,
      Menu,
      None
   };

   class SFXManager
   {
      public:
         void add_stream(const std::string &ident, const std::string &path);
         void play_sfx(const std::string &ident, float volume = 1.0f) const;

      private:
         std::map<std::string, std::shared_ptr<std::vector<float>>> effects;
   };

   SFXManager& get_sfx();

   class CameraManager
   {
      public:
         CameraManager(Blit::RenderTarget& target, const Blit::Rect& rect, Blit::Pos map_size);
         void update();

      private:
         Blit::RenderTarget* target;
         const Blit::Rect* rect;
         Blit::Pos map_size;
   };

   class EdgeDetector
   {
      public:
         EdgeDetector(bool init);
         bool set(bool state);
      private:
         bool pos;
   };

   class Game
   {
      public:
         Game(const std::string& level);

         void input_cb(std::function<bool (Input)> cb) { m_input_cb = cb; }
         void video_cb(std::function<void (const void*, unsigned, unsigned, std::size_t)> cb) { m_video_cb = cb; }

         int width() const { return map.pix_width(); }
         int height() const { return map.pix_height(); }

         void iterate();
         bool won() const;

         static const unsigned fb_width = 320;
         static const unsigned fb_height = 200;

      private:
         Blit::Tilemap map;
         Blit::RenderTarget target;
         Blit::Surface player;
         Blit::Pos player_off;
         Blit::SurfaceCache cache;
         Input facing;

         CameraManager camera;

         unsigned won_frame_cnt;
         enum { won_frame_cnt_limit = 60 * 5 };
         bool won_condition();

         std::function<bool (Input)> m_input_cb;
         std::function<void (const void*, unsigned, unsigned, std::size_t)> m_video_cb;

         std::function<bool ()> stepper;
         void run_stepper();

         unsigned frame_cnt;
         bool player_walking;
         bool is_sliding;

         void set_initial_pos(const std::string& level);
         void update_player();
         void update_animation();
         void prepare_won_animation();
         void update_input();
         void update_triggers();
         void move_if_no_collision(Input input);
         void push_block();
         bool is_offset_collision(Blit::Surface& surf, Blit::Pos offset);

         bool tile_stepper(Blit::Surface& surf, Blit::Pos step_dir);
         bool win_animation_stepper();

         static Blit::Pos input_to_offset(Input input);
         std::string input_to_string(Input input);
         Input string_to_input(const std::string& dir);

         std::vector<std::reference_wrapper<Blit::SurfaceCluster::Elem>> get_tiles_with_attr(const std::string& layer,
               const std::string& attr, const std::string& val = "");

         EdgeDetector push;
   };

   class GameManager
   {
      public:

         enum class State
         {
            Title,
            Menu,
            MenuSlide,
            Game
         };

         GameManager(const std::string& path_game,
               std::function<bool (Input)> input_cb,
               std::function<void (const void*, unsigned, unsigned, std::size_t)> video_cb);

         GameManager();
         GameManager(GameManager&&) = default;
         GameManager& operator=(GameManager&&) = default;

         void input_cb(std::function<bool (Input)> cb) { m_input_cb = cb; }
         void video_cb(std::function<void (const void*, unsigned, unsigned, std::size_t)> cb) { m_video_cb = cb; }

         void iterate();

         bool done() const;

         void reset_level();
         void change_level(unsigned chapter, unsigned level);
         unsigned current_level() const { return m_current_level; }
         State game_state() const { return m_game_state; }

         std::size_t save_size() const { return save.size(); }
         void* save_data() { return save.data(); }

      private:

         class Level : public Blit::Renderable
         {
            public:
               Level() : completion(false) {}
               Level& operator=(const Level&) = default;
               Level(const Level&) = default;
               Level& operator=(Level&&) = default;
               Level(Level&&) = default;

               Level(const std::string& path);
               const std::string& path() const { return m_path; }

               void set_name(const std::string& name) { m_name = name; }
               const std::string& name() const { return m_name; }

               void render(Blit::RenderTarget& target) const;

               void set_completion(bool state) { completion = state; }
               bool get_completion() const { return completion; }

            private:
               std::string m_path;
               std::string m_name;
               Blit::Surface preview;
               bool completion;
         };

         class Chapter
         {
            public:
               Chapter() : minimum_clear(0) {}
               Chapter& operator=(const Chapter&) = default;
               Chapter(const Chapter&) = default;
               Chapter& operator=(Chapter&&) = default;
               Chapter(Chapter&&) = default;

               Chapter(std::vector<Level> levels, const std::string& name) :
                  m_levels(std::move(levels)), m_name(name), minimum_clear(0) {}

               void set_minimum_clear(unsigned minimum) { minimum_clear = minimum; }
               bool cleared() const
               {
                  return cleared_count() >= minimum_clear;
               }

               unsigned cleared_count() const
               {
                  unsigned clear_cnt = 0;
                  for (auto& level : m_levels)
                     clear_cnt += level.get_completion();

                  return clear_cnt;
               }

               void set_completion(unsigned level, bool state) { m_levels.at(level).set_completion(state); }
               bool get_completion(unsigned level) const { return m_levels.at(level).get_completion(); }
               const std::string& name() const { return m_name; }
               const Level& level(unsigned i) const { return m_levels.at(i); }
               const std::vector<Level>& levels() const { return m_levels; }
               std::vector<Level>& levels() { return m_levels; }
               unsigned num_levels() const { return m_levels.size(); }

            private:
               std::vector<Level> m_levels;
               std::string m_name;
               unsigned minimum_clear;
         };

         class SaveManager
         {
            public:
               SaveManager(std::vector<Chapter> &chaps);

               void *data();
               void serialize();
               void unserialize();
               std::size_t size() const;

            private:
               std::vector<Chapter> &chaps;
               std::vector<char> save_data;
               static const std::size_t save_game_size = 512;
         };

         SaveManager save;

         std::vector<Chapter> chapters;
         std::unique_ptr<Game> game;
         std::string dir;

         unsigned m_current_chap;
         unsigned m_current_level;
         State m_game_state;

         Blit::SurfaceCache cache;
         Blit::RenderTarget target;

         Blit::RenderTarget font_bg;
         Blit::FontCluster font;

         Blit::Surface arrow_top;
         Blit::Surface arrow_bottom;
         Blit::Surface level_complete;

         std::function<bool (Input)> m_input_cb;
         std::function<void (const void*, unsigned, unsigned, std::size_t)> m_video_cb;

         void init_menu(const std::string& title);
         void init_level(unsigned chapter, unsigned level);
         void init_arrow(pugi::xml_node doc);
         void init_sfx(pugi::xml_node doc);

         Chapter load_chapter(pugi::xml_node chap_node, int chapter);
         const Level& get_selected_level() const;

         void step_title();
         void step_game();

         // Menu stuff.
         void enter_menu();
         void step_menu();
         void step_menu_slide();
         void start_slide(Blit::Pos dir, unsigned cnt);

         int chap_select;
         int level_select;
         unsigned total_levels() const;
         unsigned total_cleared_levels() const;

         bool old_pressed_menu_left;
         bool old_pressed_menu_right;
         bool old_pressed_menu_up;
         bool old_pressed_menu_down;

         Blit::Pos menu_slide_dir;

         unsigned slide_cnt;
         unsigned slide_end;

         enum {
            preview_base_x      = 80,
            preview_base_y      = 50,
            font_preview_base_x = 40,
            font_preview_base_y = 40,
            preview_delta_x     = 8 * 24,
            preview_delta_y     = 8 * 24,
            preview_slide_cnt   = 24
         };
   };
}

#endif

