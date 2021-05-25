static void getvars_volume_slider_option()
{
   struct retro_variable var = { "dino_volume" };

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      unsigned val = atoi(var.value);

      Icy::get_mixer().master_volume((float) val / 100.0f);

      if (log_cb)
         log_cb(RETRO_LOG_INFO, "Dinothawr: Master volume: %d.\n", val);
   }
}
