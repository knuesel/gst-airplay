#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstairplaysrc.h"

GST_DEBUG_CATEGORY (gst_debug_airplay);
#define GST_CAT_DEFAULT gst_debug_airplay

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_debug_airplay, "airplay", 0, "AirPlay");

  if (!gst_element_register (plugin, "airplaysrc", GST_RANK_PRIMARY,
          GST_TYPE_AIRPLAY_SRC))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    airplay,
    "receive video through Apple AirPlay",
    plugin_init, PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
