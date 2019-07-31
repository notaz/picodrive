#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_definition option_defs_us[] = {
   {
      "picodrive_input1",
      "Input device 1",
      "Choose which kind of controller is plugged in slot 1.",
      {
         { "3 button pad", NULL },
         { "6 button pad", NULL },
         { "None", NULL },
         { NULL, NULL },
      },
      "3 button pad"
   },
   {
      "picodrive_input2",
      "Input device 2",
      "Choose which kind of controller is plugged in slot 2.",
      {
         { "3 button pad", NULL },
         { "6 button pad", NULL },
         { "None", NULL },
         { NULL, NULL },
      },
      "3 button pad"
   },
   {
      "picodrive_sprlim",
      "No sprite limit",
      "Enable this to remove the sprite limit.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_ramcart",
      "MegaCD RAM cart",
      "Emulate a MegaCD RAM cart, used for save game data.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_region",
      "Region",
      "Force a specific region.",
      {
         { "Auto",  NULL },
         { "Japan NTSC", NULL },
         { "Japan PAL", NULL },
         { "US", NULL },
         { "Europe", NULL },
         { NULL, NULL },
      },
      "Auto"
   },
   {
      "picodrive_aspect",
      "Core-provided aspect ratio",
      "Choose the core-provided aspect ratio. RetroArch's aspect ratio must be set to Core provided in the Video settings.",
      {
         { "PAR", NULL },
         { "4/3", NULL },
         { "CRT", NULL },
         { NULL, NULL },
      },
      "PAR"
   },
   {
      "picodrive_overscan",
      "Show Overscan",
      "Crop out the potentially random glitchy video output that would have been hidden by the bezel around the edge of a standard-definition television screen.",
      {
         { "disabled",          NULL },
         { "enabled",           NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_overclk68k",
      "68K Overclock",
      "Overclock the emulated 68K chip.",
      {
         { "disabled", NULL },
         { "+25%", NULL },
         { "+50%", NULL },
         { "+75%", NULL },
         { "+100%", NULL },
         { "+200%", NULL },
         { "+400%", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
#ifdef DRC_SH2
   {
      "picodrive_drc",
      "Dynamic recompilers",
      "Enable dynamic recompilers which help to improve performance. Less accurate than interpreter CPU cores, but much faster.",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
#endif
   {
      "picodrive_audio_filter",
      "Audio filter",
      "Enable a low pass audio filter to better simulate the characteristic sound of a Model 1 Genesis. This option is ignored when running Master System and PICO titles. Only the Genesis and its add-on hardware (Sega CD, 32X) employed a physical low pass filter.",
      {
         { "disabled", NULL },
         { "low-pass",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_lowpass_range",
      "Low-pass filter %",
      "Specify the cut-off frequency of the audio low pass filter. A higher value increases the perceived 'strength' of the filter, since a wider range of the high frequency spectrum is attenuated.",
      {
         { "5",  NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "35", NULL },
         { "40", NULL },
         { "45", NULL },
         { "50", NULL },
         { "55", NULL },
         { "60", NULL },
         { "65", NULL },
         { "70", NULL },
         { "75", NULL },
         { "80", NULL },
         { "85", NULL },
         { "90", NULL },
         { "95", NULL },
         { NULL, NULL },
      },
      "60"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

struct retro_core_option_definition option_defs_tr[] = {
   {
      "picodrive_input1",
      "Giriş cihazı 1",
      "Hangi tür kontrolör'ün yuva 1'e takılı olduğunu seçin.",
      {
         { "3 button pad", NULL },
         { "6 button pad", NULL },
         { "None", "hiçbiri" },
         { NULL, NULL },
      },
      "3 button pad"
   },
   {
      "picodrive_input2",
      "Giriş cihazı 2",
      "Hangi tür kontrolör'ün yuva 2'e takılı olduğunu seçin",
      {
         { "3 button pad", NULL },
         { "6 button pad", NULL },
         { "None", "hiçbiri" },
         { NULL, NULL },
      },
      "3 button pad"
   },
   {
      "picodrive_sprlim",
      "Sprite sınırı yok",
      "Sprite sınırını kaldırmak için bunu etkinleştirin.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_ramcart",
      "Sega CD RAM Kartuşu",
      "Oyun verilerini kaydetmek için kullanılan bir MegaCD RAM kartuşunu taklit edin.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_region",
      "Bölge",
      "Belirli bir bölgeye zorlayın.",
      {
         { "Auto",  "Otomatik" },
         { "Japan NTSC", "Japonya NTSC" },
         { "Japan PAL", "Japonya PAL" },
         { "US", NULL },
         { "Europe", "Avrupa" },
         { NULL, NULL },
      },
      "Auto"
   },
   {
      "picodrive_aspect",
      "Core tarafından belirlenen en boy oranı",
      "Core tarafından sağlanan en boy oranını seçin. RetroArch'ın en boy oranı, Video ayarlarında sağlanan Core olarak ayarlanmalıdır.",
      {
         { "PAR", NULL },
         { "4/3", NULL },
         { "CRT", NULL },
         { NULL, NULL },
      },
      "PAR"
   },
   {
      "picodrive_overscan",
      "Tarama Taşmasını Göster",
      "Standart tanımlı bir televizyon ekranının kenar çevresinde, çerçeve tarafından gizlenmiş olabilecek rastgele video çıkışını kesin.",
      {
         { "disabled",          NULL },
         { "enabled",           NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_overclk68k",
      "68K Hızaşırtma",
      "Öykünülmüş 68K yongasına Hızaşırtma uygulayın.",
      {
         { "disabled", NULL },
         { "+25%", NULL },
         { "+50%", NULL },
         { "+75%", NULL },
         { "+100%", NULL },
         { "+200%", NULL },
         { "+400%", NULL },
         { NULL, NULL },
      },
      "disabled"
   },
#ifdef DRC_SH2
   {
      "picodrive_drc",
      "Dinamik Yeniden Derleyici",
      "Performansı artırmaya yardımcı olan dinamik yeniden derleyicileri etkinleştirin. Tercüman CPU çekirdeğinden daha az hassas, ancak çok daha hızlıdır.",
      {
         { "enabled", NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled"
   },
#endif
   {
      "picodrive_audio_filter",
      "Ses Filtresi",
      "Model 1 Genesis'in karakteristik sesini daha iyi simüle etmek için düşük geçişli bir ses filtresini etkinleştirin. Master System ve PICO başlıkları kullanılırken bu seçenek yoksayılır. Sadece Genesis ve eklenti donanımı (Sega CD, 32X) fiziksel düşük geçiş filtresi kullanır.",
      {
         { "disabled", "devre dışı" },
         { "low-pass",  "alçak geçiş" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "picodrive_lowpass_range",
      "Alçak geçiş filtresi %",
      "Ses düşük geçiş filtresinin kesme frekansını belirtin. Daha yüksek bir değer, yüksek frekans spektrumunun daha geniş bir aralığı azaltıldığı için filtrenin algılanan gücünü arttırır.",
      {
         { "5",  NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "35", NULL },
         { "40", NULL },
         { "45", NULL },
         { "50", NULL },
         { "55", NULL },
         { "60", NULL },
         { "65", NULL },
         { "70", NULL },
         { "75", NULL },
         { "80", NULL },
         { "85", NULL },
         { "90", NULL },
         { "95", NULL },
         { NULL, NULL },
      },
      "60"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   option_defs_tr, /* RETRO_LANGUAGE_TURKISH */
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should only be called inside retro_set_environment().
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version == 1))
   {
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
   }
   else
   {
      size_t i;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string */
            if (num_values > 1)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[i].key   = key;
         variables[i].value = values_buf[i];
      }
      
      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
