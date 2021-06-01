static bool option_use_audio_thread;


static void getvars_audio_thread_option()
{
   static bool once = false;
   struct retro_variable var = { "dino_audio_thread" };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && !once)
   {
      struct retro_audio_callback cb;

      if (!strcmp(var.value, "disabled"))
      {
         cb = { NULL, NULL };
         option_use_audio_thread = false;
      }
      else
      {
         cb = { audio_callback, audio_set_state };
         option_use_audio_thread = true;
      }

      use_audio_cb = environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &cb);
      audio_set_state(true);


      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Dinothawr: Using threaded audio: %s.\n", option_use_audio_thread ? "enabled" : "disabled");
   }
}
