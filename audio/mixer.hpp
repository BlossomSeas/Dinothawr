#ifndef MIXER_HPP__
#define MIXER_HPP__

#include <cstdint>
#include <vector>
#include <memory>
#include <utility>
#include <cmath>

namespace Audio
{
   class Stream
   {
      public:
         Stream() : m_volume(1.0) {}

         virtual std::size_t render(float* buffer, std::size_t frames) = 0;
         virtual bool valid() const = 0;
         virtual ~Stream() = default;

         float volume() const { return m_volume; }
         void volume(float vol) { m_volume = vol; }

      private:
         float m_volume;
   };

   class SineStream : public Stream
   {
      public:
         SineStream(float freq, float sample_rate) : omega(2.0 * M_PI * freq / sample_rate), index(0.0) {}

         std::size_t render(float* buffer, std::size_t frames)
         {
            for (std::size_t i = 0; i < frames; i++, index += omega)
            {
               float val = std::sin(index);
               buffer[2 * i + 0] = buffer[2 * i + 1] = val;
            }

            return frames;
         }

         bool valid() const { return true; }

      private:
         double omega;
         double index;
   };

   class Mixer
   {
      public:
         static const unsigned channels = 2;

         Mixer();
         Mixer& operator=(Mixer&&) = default;
         Mixer(Mixer&&) = default;

         void add_stream(std::unique_ptr<Stream> str);

         void render(float *buffer, std::size_t frames);
         void render(std::int16_t *buffer, std::size_t frames);
         void master_volume(float vol) { master_vol = vol; }
         float master_volume() const { return master_vol; }

      private:
         std::vector<float> buffer;
         std::vector<float> conv_buffer;
         std::vector<std::unique_ptr<Stream>> streams;

         float master_vol;
         void purge_dead_streams();
   };
}

#endif

