#include "mixer.hpp"
#include "utils.h"
#include "../utils.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <fstream>

using namespace Blit::Utils;
using namespace std;

typedef lock_guard<recursive_mutex> LockGuard;

namespace Audio
{
   Mixer::Mixer() : master_vol(1.0f)
   {
      m_enabled = make_unique<std::atomic<unsigned>>();
      m_lock = make_unique<recursive_mutex>();
   }

   void Mixer::add_stream(shared_ptr<Stream> str)
   {
      LockGuard guard(*m_lock);
      streams.push_back(move(str));
   }

   void Mixer::purge_dead_streams()
   {
      LockGuard guard(*m_lock);
      streams.erase(remove_if(begin(streams), end(streams),
               [](const shared_ptr<Stream> &str) {
                  return !str->valid();
               }), end(streams));
   }

   void Mixer::render(float* out_buffer, size_t frames)
   {
      LockGuard guard(*m_lock);
      purge_dead_streams();

      fill(out_buffer, out_buffer + frames * channels, 0.0f);

      buffer.reserve(frames * channels);
      for (auto& stream : streams)
      {
         auto rendered = stream->render(buffer.data(), frames);
         audio_mix_volume(out_buffer, buffer.data(), master_vol * stream->volume(), rendered * channels);
      }
   }

   void Mixer::render(int16_t* out_buffer, size_t frames)
   {
      LockGuard guard(*m_lock);
      conv_buffer.reserve(frames * channels);
      render(conv_buffer.data(), frames);

      audio_convert_float_to_s16(out_buffer, conv_buffer.data(), frames * channels);
   }

   void Mixer::clear()
   {
      LockGuard guard(*m_lock);
      streams.clear();
   }

   PCMStream::PCMStream(shared_ptr<vector<float>> data)
      : data(data), ptr(0)
   {}

   size_t PCMStream::render(float* buffer, size_t frames)
   {
      size_t to_write = min(frames * Mixer::channels, data->size() - ptr);

      copy(begin(*data) + ptr,
            begin(*data) + ptr + to_write,
            buffer);

      if (to_write < frames && loop())
      {
         rewind();
         size_t to_write_loop = min(frames * Mixer::channels - to_write, data->size() - (ptr + to_write));
         copy(begin(*data) + ptr + to_write,
               begin(*data) + ptr + to_write + to_write_loop,
               buffer + to_write);

         to_write += to_write_loop;
      }

      ptr += to_write;
      return to_write / Mixer::channels;
   }

   vector<float> WAVFile::load_wave(const string& path)
   {
      using namespace Blit::Utils;
      ifstream file;

      file.exceptions(ifstream::badbit | ifstream::failbit | ifstream::eofbit);
      try
      {
         file.open(path, ifstream::in | ifstream::binary);

         char header[44];
         file.read(header, sizeof(header));

         if (!equal(header + 0, header + 4, "RIFF"))
            throw logic_error("Invalid WAV file.");

         if (!equal(header + 8, header + 12, "WAVE"))
            throw logic_error("Invalid WAV file.");

         if (!equal(header + 12, header + 16, "fmt "))
            throw logic_error("Invalid WAV file.");

         if (read_le16(header + 20) != 1)
            throw logic_error("WAV file not uncompressed.");

         unsigned channels    = read_le16(header + 22);
         unsigned sample_rate = read_le32(header + 24);
         unsigned bits        = read_le16(header + 34);

         if (channels < 1 || channels > 2)
            throw logic_error("Invalid number of channels.");

         if (sample_rate != 44100)
            throw logic_error("Invalid sample rate.");

         if (bits != 16)
            throw logic_error("Invalid bit depth.");

         vector<int16_t> wave;

         unsigned wave_size = read_le32(header + 4);
         wave_size += 8;
         wave_size -= sizeof(header);
         wave.resize(wave_size / sizeof(int16_t));

         file.read(reinterpret_cast<char*>(wave.data()), wave_size);

         vector<float> pcm_data;
         if (channels == 1)
         {
            pcm_data.resize(2 * wave_size / sizeof(int16_t));
            auto ptr = begin(pcm_data);
            for (auto val : wave)
            {
               float fval = static_cast<float>(val) / 0x8000;
               *ptr++ = fval;
               *ptr++ = fval;
            }

         }
         else
         {
            pcm_data.resize(wave_size / sizeof(int16_t));
            auto ptr = begin(pcm_data);
            for (auto val : wave)
            {
               float fval = static_cast<float>(val) / 0x8000;
               *ptr++ = fval;
            }
         }

         return pcm_data;
      }
      catch (const ifstream::failure e)
      {
         cerr << "iostream error: " << e.what() << endl;
         throw runtime_error("Failed to open wave.");
      }
   }

   vector<float> VorbisFile::decode()
   {
      vector<float> data;
      rewind();
      loop(false);

      float buffer[4096 * Mixer::channels];
      size_t rendered = 0;
      while ((rendered = render(buffer, 4096)))
         data.insert(end(data), buffer, buffer + rendered * Mixer::channels);

      return data;
   }

   VorbisFile::VorbisFile(const string& path)
      : path(path), is_eof(false), is_mono(false)
   {
      if (ov_fopen(path.c_str(), &vf) < 0)
         throw runtime_error(join("Failed to open vorbis file: ", path));

      cerr << "Vorbis info:" << endl;
      cerr << "\tStreams: " << ov_streams(&vf) << endl;

      vorbis_info *info = (vorbis_info*)ov_info(&vf, 0);
      if (info)
      {
         switch (info->channels)
         {
            case 1:
               cerr << "Mono!" << endl;
               is_mono = true;
               break;

            case 2:
               cerr << "Stereo!" << endl;
               is_mono = false;
               break;

            default:
               throw logic_error(join("Vorbis file has ", info->channels, " channels."));
         }

         if (info->rate != 44100)
            throw logic_error(join("Sampling rate of file is: ", info->rate));
      }
      else
         throw logic_error("Couldn't find info for vorbis file.");
   }

   VorbisFile::~VorbisFile()
   {
      ov_clear(&vf);
   }

   void VorbisFile::rewind()
   {
      if (ov_time_seek(&vf, 0.0) != 0)
         throw runtime_error("Couldn't rewind vorbis audio!\n");

      is_eof = false;
   }

   size_t VorbisFile::render(float* buffer, size_t frames)
   {
      size_t rendered = 0;

      while (frames)
      {
         float **pcm;
         int bitstream;
         long ret = ov_read_float(&vf, &pcm, frames, &bitstream);

         if (ret < 0)
            throw runtime_error(join("Vorbis decoding failed with: ", ret));

         if (ret == 0) // EOF
         {
            if (loop())
            {
               loop(false); // Avoid infinite recursion incause our audio clip is really short.
               ScopeExit holder([this] { loop(true); });

               if (ov_time_seek(&vf, 0.0) == 0)
               {
                  auto ret = render(buffer, frames);
                  return rendered + ret;
               }
               else
                  is_eof = true;
            }
            else
               is_eof = true;

            return rendered;
         }

         if (!is_mono)
         {
            for (unsigned c = 0; c < Mixer::channels; c++)
               for (long i = 0; i < ret; i++)
                  buffer[2 * i + c] = pcm[c][i];
         }
         else
         {
            for (unsigned c = 0; c < Mixer::channels; c++)
               for (long i = 0; i < ret; i++)
                  buffer[2 * i + c] = pcm[0][i];
         }

         buffer += ret * Mixer::channels;
         frames -= ret;
         rendered += ret;
      }

      return rendered;
   }

   void VorbisLoader::request_vorbis(const string& path)
   {
      inflight.push_back(async(launch::async, [path]() {
                  VorbisFile file{path};
                  return file.decode();
               }));
   }

   void VorbisLoader::cleanup()
   {
      inflight.erase(remove_if(begin(inflight),
               end(inflight), [](const future<vector<float>>& fut) {
               return !fut.valid();
               }), end(inflight));
   }

   shared_ptr<vector<float>> VorbisLoader::flush()
   {
      try
      {
         for (auto& fut : inflight)
            if (fut.wait_for(chrono::seconds(0)) == future_status::ready)
               finished.push(fut.get()); 

         cleanup();

         if (finished.size())
         {
            auto& f = finished.front();
            auto ret = make_shared<vector<float>>(move(f));
            finished.pop();
            return move(ret);
         }
         else
            return {};
      }
      catch (const exception& e)
      {
         cerr << "VorbisLoader::flush() failed ... " << e.what() << endl;
         cleanup();
         return {};
      }
   }
}

