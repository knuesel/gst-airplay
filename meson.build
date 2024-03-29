project('gst-airplay', 'c', version : '0.1', license : 'GPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_version = meson.project_version()

api_version = '1.0'

gio_dep = dependency('gio-2.0')

gst_dep = dependency('gstreamer-1.0',
    fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0',
    fallback : ['gstreamer', 'gst_base_dep'])
gstvideo_dep = dependency('gstreamer-video-1.0',
    fallback : ['gst-plugins-base', 'video_dep'])

plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-airplay-plugin')
cdata.set_quoted('GST_LICENSE', 'GPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer airplay Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/knuesel/gst-airplay')
configure_file(output : 'config.h', configuration : cdata)

#gstaudio_dep = dependency('gstreamer-audio-1.0',
#    fallback: ['gst-plugins-base', 'audio_dep'])

# Video source
airplay_sources = [
  'src/gstairplay.c',
  'src/gstairplaysrc.c'
  ]

# RPiPlay airplay library
cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.set_install(false)
libairplay = cmake.subproject('airplay',  options: opt_var)
libdns_sd_dep = dependency('avahi-compat-libdns_sd', required: true)
libairplay_dep = libairplay.dependency('airplay')
llhttp_dep = libairplay.dependency('llhttp')
thread_dep = dependency('threads')
m_dep = cc.find_library('m', required: true)
playfair_dep = libairplay.dependency('playfair')

gstairplay = library('gstairplay',
  airplay_sources,
  c_args: plugin_c_args,
  dependencies : [gstbase_dep, gstvideo_dep, libdns_sd_dep, libairplay_dep, llhttp_dep, thread_dep, m_dep, playfair_dep, gio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
