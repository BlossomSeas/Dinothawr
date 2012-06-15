#include "surface.hpp"
#include <stdexcept>
#include <utility>

namespace Blit
{
   Surface::Surface(Pixel pix, int width, int height)
      : data(std::make_shared<const Data>(pix, width, height)),
      m_rect({0, 0}, width, height), m_ignore_camera(false)
   {}

   Surface::Surface(std::shared_ptr<const Data> data)
      : data(data), m_rect({0, 0}, data->w, data->h), m_ignore_camera(false)
   {}

   Surface::Surface(const std::vector<Alt>& alts, const std::string& start_id) : m_ignore_camera(false)
   {
      if (alts.empty())
         throw std::logic_error("Alts is empty.");

      Pos size{alts.front().data->w, alts.front().data->h};
      m_rect = {{0, 0}, size.x, size.y};

      bool same_size = std::all_of(std::begin(alts) + 1, std::end(alts), [&alts, size](const Alt& alt) {
               return size == Pos{alt.data->w, alt.data->h};
            });

      if (!same_size)
         throw std::logic_error("Not all alts are of same size.");

      for (auto& alt : alts)
         this->alts[alt.tag] = alt.data;

      active_alt(start_id);
   }

   void Surface::active_alt(const std::string& id)
   {
      auto ptr = alts[id];
      if (!ptr)
         throw std::logic_error(Utils::join("Alt ID ", id, " does not exist."));

      m_active_alt = id;
      data = ptr;
   }

   Surface::Surface()
      : m_rect({0, 0}, 0, 0), m_ignore_camera(false)
   {}

   Surface Surface::sub(Rect rect) const
   {
      RenderTarget target(rect.w, rect.h);
      Surface surf{*this};
      surf.rect().pos = -rect.pos;
      target.blit(surf, rect);
      return target.convert_surface();
   }

   Pixel Surface::pixel(Pos pos) const
   {
      pos -= m_rect.pos;
      int x = pos.x, y = pos.y;

      if (x >= data->w || y >= data->h)
         return 0;

      return data->pixels[y * data->w + x];
   }

   const Pixel* Surface::pixel_raw(Pos pos) const
   {
      pos -= m_rect.pos;
      int x = pos.x, y = pos.y;

      if (x >= data->w || y >= data->h || x < 0 || y < 0)
         throw std::logic_error(Utils::join(
                  "Pixel was fetched out-of-bounds. ",
                  "Asked for: (", x, ", ", y, ". ",
                  "Real dimension: ", data->w, ", ", data->h, "."
                  ));

      return &data->pixels[y * data->w + x];
   }

   void Surface::ignore_camera(bool ignore)
   {
      m_ignore_camera = ignore;
   }

   bool Surface::ignore_camera() const
   {
      return m_ignore_camera;
   }

   Surface::Data::Data(std::vector<Pixel> pixels, int w, int h)
      : pixels(std::move(pixels)), w(w), h(h)
   {}

   Surface::Data::Data(Pixel pixel, int w, int h)
      : pixels(w * h)
   {
      std::fill(std::begin(pixels), std::end(pixels), pixel);
   }
}

