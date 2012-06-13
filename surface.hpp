#ifndef SURFACE_HPP__
#define SURFACE_HPP__

#include "blit.hpp"

#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace Blit
{
   class Surface
   {
      public:
         struct Data
         {
            Data(std::vector<Pixel> pixels, int w, int h);
            Data(Pixel pixel, int w, int h);

            std::vector<Pixel> pixels;
            int w, h;
         };

         Surface();
         Surface(Pixel pix, int width, int height);
         Surface(std::shared_ptr<const Data> data);

         Surface sub(Rect rect) const;

         Rect& rect();
         const Rect& rect() const;

         void ignore_camera(bool ignore);
         bool ignore_camera() const;

         Pixel pixel(Pos pos) const;
         const Pixel* pixel_raw(Pos pos) const;

      private:
         std::shared_ptr<const Data> data;
         Rect m_rect;
         bool m_ignore_camera;
   };

   class RenderTarget;

   class Renderable
   {
      public:
         virtual void render(RenderTarget& target) = 0;
         virtual Pos pos() const { return position; }
         virtual void pos(Pos position) { this->position = position; }

         void move(Pos offset) { pos(pos() + offset); }

      protected:
         Pos position;
   };

   class SurfaceCluster : public Renderable
   {
      public:
         struct Elem
         {
            Surface surf;
            Pos offset;
            unsigned tag;
         };

         void pos(Pos position);
         std::vector<Elem>& vec();
         const std::vector<Elem>& vec() const;

         void set_transform(std::function<Pos (Pos)> func);
         void render(RenderTarget& target);

      private:
         std::vector<Elem> elems;
         std::function<Pos (Pos)> func;
   };

   class SurfaceCache
   {
      public:
         Surface from_image(const std::string& path);

      private:
         std::map<std::string, std::shared_ptr<const Surface::Data>> cache;
         std::shared_ptr<const Surface::Data> load_image(const std::string& path);
   };

   class RenderTarget
   {
      public:
         RenderTarget(int width, int height);

         Surface convert_surface();

         const Pixel* buffer() const;
         Pixel* pixel_raw(Pos pos);
         Pixel* pixel_raw_no_offset(Pos pos);

         int width() const;
         int height() const;

         void clear(Pixel pix);

         void camera_move(Pos pos);
         void camera_set(Pos pos);
         Pos camera_pos() const;

         void blit(const Surface& surf, Rect subrect);
         void blit_offset(const Surface& surf, Rect subrect, Pos offset);

      private:
         std::vector<Pixel> m_buffer;
         Rect rect;
   };
}

#endif

