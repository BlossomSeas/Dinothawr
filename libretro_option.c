static void hide_option(char *key)
{
   struct retro_core_option_display option_display = { key, false };

   environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
}
