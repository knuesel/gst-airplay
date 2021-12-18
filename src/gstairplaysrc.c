#include <stdio.h>
#include <string.h>

/**
 * SECTION:element-airplaysrc
 * @title: airplaysrc
 *
 * airplaysrc is a network source that allows receiving video streamed from an
 * Apple device using the AirPlay protocol.
 *
 * ## Example
 * |[
 * gst-launch-1.0 airplaysrc ! queue ! h264parse ! avdec_h264 max-threads=1 ! autovideosink
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <lib/stream.h>
#include <lib/dnssd.h>
#include <lib/raop.h>

#include "gstairplaysrc.h"

const int max_buffers = 10;
const char* name = "gstairplay";
const char hw_address[] = {0x48, 0x5d, 0x60, 0x7c, 0xee, 0x22 };

enum
{
  PROP_0,
  PROP_CONNECTED
};

struct _GstAirPlayData {
  raop_t *raop;
  dnssd_t *dnssd;
  GAsyncQueue *done_buffers;
  GAsyncQueue *filled_buffers;

  GMutex property_lock;
  gboolean connected;
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GParamSpec *prop_connected;

#define GST_CAT_DEFAULT gst_debug_airplay_src
GST_DEBUG_CATEGORY (GST_CAT_DEFAULT);

#define gst_airplay_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAirPlaySrc, gst_airplay_src,
    GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "airplaysrc", 0, "AirPlay Source"));

static void audio_process(void *cls, raop_ntp_t *ntp, aac_decode_struct *data)
{
}

static void video_process(void *cls, raop_ntp_t *ntp, h264_decode_struct *data) {
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (cls);

  int waiting = g_async_queue_length(self->data->filled_buffers);

  GST_LOG_OBJECT (self, "incoming frame, queued %d", waiting);

  // Try getting a buffer from done queue
  GByteArray* array = array = g_async_queue_try_pop(self->data->done_buffers);

  // If that didn't work, allocate new buffer, or give up if too many are already waiting
  if (array == NULL) {
    if (waiting >= max_buffers) {
      GST_WARNING_OBJECT (self, "too many buffers waiting, dropping incoming frame");
      return;
    }
    GST_DEBUG_OBJECT (self, "allocating new buffer");
    array = g_byte_array_new();
  }

  g_byte_array_set_size(array, 0);
  g_byte_array_append(array, data->data, data->data_len);

  g_async_queue_push(self->data->filled_buffers, array);

  // video_renderer_render_buffer(video_renderer, ntp, data->data, data->data_len, data->pts, data->frame_type);
}

static void conn_init(void *cls)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (cls);
  gboolean notify = false;

  g_mutex_lock(&self->data->property_lock);
  if (!self->data->connected)
  {
    self->data->connected = true;
    notify = true;
  }
  g_mutex_unlock(&self->data->property_lock);

  if (notify)
  {
    g_object_notify_by_pspec(G_OBJECT(self), prop_connected);
  }
}

static void conn_destroy(void *cls)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (cls);
  gboolean notify = false;

  g_mutex_lock(&self->data->property_lock);
  if (self->data->connected)
  {
    self->data->connected = false;
    notify = true;
  }
  g_mutex_unlock(&self->data->property_lock);

  if (notify)
  {
    g_object_notify_by_pspec(G_OBJECT(self), prop_connected);
  }
}

static void log_callback(void *cls, int level, const char *msg)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (cls);
  GST_LOG_OBJECT (self, "raop: %s", msg);
}

static gboolean
gst_airplay_src_start (GstBaseSrc * bsrc)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (bsrc);

  GST_DEBUG_OBJECT (self, "start");

  raop_callbacks_t raop_cbs;
  memset(&raop_cbs, 0, sizeof(raop_cbs));
  raop_cbs.cls = self;
  raop_cbs.conn_init = conn_init;
  raop_cbs.conn_destroy = conn_destroy;
  raop_cbs.audio_process = audio_process;
  raop_cbs.video_process = video_process;
  //raop_cbs.audio_flush = audio_flush;
  //raop_cbs.video_flush = video_flush;
  //raop_cbs.audio_set_volume = audio_set_volume;

  self->data->raop = raop_init(10, &raop_cbs);
  if (self->data->raop == NULL) {
    /* ensure error is posted since state change will fail */
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL), ("Failed to init raop"));
    return FALSE;
  }

  raop_set_log_callback(self->data->raop, log_callback, NULL);
  gboolean debug_log = TRUE;
  raop_set_log_level(self->data->raop, debug_log ? RAOP_LOG_DEBUG : LOGGER_INFO);

  unsigned short port = 0;
  raop_start(self->data->raop, &port);
  raop_set_port(self->data->raop, port);

  // TODO: allow user to specify correct mac address
  int err;
  self->data->dnssd = dnssd_init(name, strlen(name), hw_address, sizeof(hw_address), &err);
  if (err) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL), ("Failed to init dnssd"));
    raop_destroy (self->data->raop);
    return FALSE;
  }

  raop_set_dnssd(self->data->raop, self->data->dnssd);

  dnssd_register_raop(self->data->dnssd, port);
  dnssd_register_airplay(self->data->dnssd, port + 1);

  return TRUE;
}

static gboolean
gst_airplay_src_stop (GstBaseSrc * bsrc)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (bsrc);

  GST_DEBUG_OBJECT (self, "stop: destroying raop");
  raop_destroy(self->data->raop);
  GST_DEBUG_OBJECT (self, "stop: unregistering raop");
  dnssd_unregister_raop(self->data->dnssd);
  GST_DEBUG_OBJECT (self, "stop: unregistering airplay");
  dnssd_unregister_airplay(self->data->dnssd);
  
  GST_DEBUG_OBJECT (self, "stop");

  return TRUE;
}

static GstFlowReturn
gst_airplay_src_fill (GstPushSrc * src, GstBuffer * outbuf)
{
  // TODO: find a way to avoid double copy of data
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (src);
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo info;
  GError *err = NULL;
  gssize recv_len;

  GST_TRACE_OBJECT (self, "fill: popping buffer");
  GByteArray* array = NULL;
  while (array == NULL) {
    if (g_cancellable_is_cancelled (self->cancellable)) {
      ret = GST_FLOW_FLUSHING;
      goto out;
    }
    array = g_async_queue_timeout_pop(self->data->filled_buffers, 500000);
  }

  if (!gst_buffer_map (outbuf, &info, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ,
        ("Could not map the buffer for writing "), (NULL));
    ret = GST_FLOW_ERROR;
    goto out;
  }
  
  gsize bufsize = gst_buffer_get_size(outbuf);
  gsize size = bufsize >= array->len ? array->len : bufsize;
  GST_LOG_OBJECT (self, "fill: incoming %u, buffer %lu, mapped %lu, max %lu", array->len, bufsize, info.size, info.maxsize);
  if (bufsize < array->len) {
    GST_WARNING_OBJECT (self, "mapped buffer too small! %lu < %u", bufsize, array->len);
  }
  memcpy (info.data, array->data, size);
  g_async_queue_push(self->data->done_buffers, array);
  recv_len = size;


  gst_buffer_unmap (outbuf, &info);

  if (g_cancellable_is_cancelled (self->cancellable)) {
    ret = GST_FLOW_FLUSHING;
    goto out;
  }

  if (recv_len < 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), ("%s", err->message));
    ret = GST_FLOW_ERROR;
    g_clear_error (&err);
    goto out;
  } else if (recv_len == 0) {
    ret = GST_FLOW_EOS;
    goto out;
  }

  gst_buffer_resize (outbuf, 0, recv_len);

  // GST_LOG_OBJECT (src,
  //     "filled buffer from _get of size %" G_GSIZE_FORMAT ", ts %"
  //     GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
  //     ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT,
  //     gst_buffer_get_size (outbuf),
  //     GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
  //     GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)),
  //     GST_BUFFER_OFFSET (outbuf), GST_BUFFER_OFFSET_END (outbuf));

out:
  GST_LOG_OBJECT (self, "fill exit");
  return ret;
}

static void
gst_airplay_src_init (GstAirPlaySrc * self)
{
  self->cancellable = g_cancellable_new ();

  GST_DEBUG_OBJECT (self, "init");

  self->data = g_new0(GstAirPlayData, 1);
  self->data->done_buffers = g_async_queue_new_full ((GDestroyNotify) g_byte_array_unref);
  self->data->filled_buffers = g_async_queue_new_full ((GDestroyNotify) g_byte_array_unref);
  self->data->raop = NULL;
  self->data->dnssd = NULL;
  self->data->connected = false;
  g_mutex_init(&self->data->property_lock);

  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (self), TRUE); // TODO: consider changing this

  gst_base_src_set_blocksize (GST_BASE_SRC (self), 2<<20); // TODO: consider changing this
}

static void
gst_airplay_src_finalize (GObject * object)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_clear_object (&self->cancellable);

  g_mutex_clear(&self->data->property_lock);
  g_async_queue_unref (self->data->filled_buffers);
  g_async_queue_unref (self->data->done_buffers);
  g_free (self->data);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_airplay_src_unlock (GstBaseSrc * bsrc)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (bsrc);

  GST_DEBUG_OBJECT (self, "unlock");

  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
gst_airplay_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (bsrc);

  GST_DEBUG_OBJECT (self, "unlock stop");

  g_cancellable_reset (self->cancellable);

  return TRUE;
}

static void
gst_airplay_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (object);

  GST_DEBUG_OBJECT (self, "set property");

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_airplay_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstAirPlaySrc *self = GST_AIRPLAY_SRC (object);

  GST_DEBUG_OBJECT (self, "get property");

  switch (prop_id) {
    case PROP_CONNECTED:
      g_mutex_lock(&self->data->property_lock);
      g_value_set_boolean (value, self->data->connected);
      g_mutex_unlock(&self->data->property_lock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_airplay_src_class_init (GstAirPlaySrcClass * klass)
{
  GST_DEBUG ("class init");

  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_airplay_src_set_property;
  gobject_class->get_property = gst_airplay_src_get_property;
  gobject_class->finalize = gst_airplay_src_finalize;

  prop_connected = g_param_spec_boolean ("connected", "Connected",
      "Whether a client is connected to the airplay server",
      FALSE, G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_CONNECTED, prop_connected);

  gst_element_class_add_static_pad_template (gstelement_class, &src_template);
  gst_element_class_set_metadata (gstelement_class,
      "AirPlay source", "Source/Video/Network",
      "Receives AirPlay video",
      "Jeremie Knuesel <knuesel@gmail.com>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_airplay_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_airplay_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_airplay_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_airplay_src_unlock_stop);
  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_airplay_src_fill);
}
