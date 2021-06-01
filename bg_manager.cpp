#include "game.hpp"
#ifndef USE_CXX03
#include <stdlib.h>

using namespace std;

// global namespace
extern unsigned bgm_option;

namespace Icy
{
   void BGManager::init(const vector<Track>& tracks)
   {
      this->tracks = tracks;
      srand(time(NULL));
      first = true;
      last = 0;
   }

   void BGManager::step(Audio::Mixer& mixer)
   {
      lock_guard<Audio::Mixer> guard(mixer);

      if (current && current->valid())
         return;

      if (!tracks.size())
         return;

      if (!loader.size())
      {
         if (first)
         {
            loader.request_vorbis(tracks[0].path);
            last = 0;
         }
         else
         {
            unsigned index;
            static unsigned bgm_tracks[100];
            static unsigned bgm_index = tracks.size();

            switch( bgm_option )
            {
               case 0:
                  //printf( "normal = random\n" );
                  index = rand() % tracks.size();

                  if (index == last)
                     index = (index + 1) % tracks.size();
                  break;

               case 1:
                  //printf( "original = linear\n" );

                  if( bgm_index == last )
                     bgm_index = (bgm_index + 1) % tracks.size();

                  index = bgm_index % tracks.size();
                  bgm_index = (bgm_index + 1) % tracks.size();
                  break;

               case 2:
                  if( bgm_index == tracks.size() )
                  {
                     //printf( "shuffle\n" );

                     for( bgm_index = 0; bgm_index < tracks.size(); bgm_index++ )
                     {
                        unsigned index;
                        while(1)
                        {
                           bool valid = true;
                           index = rand() % tracks.size();

                           if( index == last )
                              valid = false;

                           for( unsigned lcv = 0; lcv < bgm_index; lcv++ )
                           {
                              if( bgm_tracks[ lcv ] == index )
                                 valid = false;
                           }

                           if( valid )
                              break;
                        }

                        //printf( "%d: %d\n", bgm_index, index );
                        bgm_tracks[ bgm_index ] = index;
                        last = index;
                     }

                     bgm_index = 0;
                  }

                  index = bgm_tracks[ bgm_index ];
                  bgm_index++;
                  break;
            }

            loader.request_vorbis(tracks[index].path);
            last = index;
         }

         first = false;
      }

      std::shared_ptr<std::vector<float> > ret = loader.flush();

      if (ret)
      {
         current = make_shared<Audio::PCMStream>(ret);
         current->volume(tracks[last].gain);
      }
      else
         current.reset();

      if (current)
         mixer.add_stream(current);
   }
}
#endif
