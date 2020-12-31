#ifndef __GST_AIRPLAY_SRC_H__
#define __GST_AIRPLAY_SRC_H__

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_AIRPLAY_SRC              (gst_airplay_src_get_type ())
#define GST_IS_AIRPLAY_SRC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_AIRPLAY_SRC))
#define GST_IS_AIRPLAY_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_AIRPLAY_SRC))
#define GST_AIRPLAY_SRC_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AIRPLAY_SRC, GstAirPlaySrcClass))
#define GST_AIRPLAY_SRC(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_AIRPLAY_SRC, GstAirPlaySrc))
#define GST_AIRPLAY_SRC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_AIRPLAY_SRC, GstAirPlaySrcClass))
#define GST_AIRPLAY_SRC_CAST(obj)         ((GstAirPlaySrc*)(obj))
#define GST_AIRPLAY_SRC_CLASS_CAST(klass) ((GstAirPlaySrcClass*)(klass))

typedef struct _GstAirPlayData GstAirPlayData;
typedef struct _GstAirPlaySrc GstAirPlaySrc;
typedef struct _GstAirPlaySrcClass GstAirPlaySrcClass;

struct _GstAirPlaySrc {
  GstPushSrc parent;

  GstCaps      *caps;

  GstAirPlayData *data;
  GCancellable *cancellable;
};

struct _GstAirPlaySrcClass {
  GstPushSrcClass parent_class;
};

GType   gst_airplay_src_get_type (void);

G_END_DECLS

#endif // __GST_AIRPLAY_SRC_H__
