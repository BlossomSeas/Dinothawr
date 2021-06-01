unsigned bgm_option;


static void getvars_music_shuffle_option()
{
   struct retro_variable var = { "dino_bgm_option" };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "Shuffle"))
         bgm_option = 2;
      else if (!strcmp(var.value, "Original"))
         bgm_option = 1;
      else if (!strcmp(var.value, "Normal"))
         bgm_option = 0;  // Random

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Dinothawr: BGM option: %s.\n", bgm_option == 0 ? "Normal (Random)" : bgm_option == 1 ? "Shuffle" : "Original (Rotate)" );
   }
}
