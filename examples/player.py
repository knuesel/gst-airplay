#!/usr/bin/python3

# From http://lifestyletransfer.com/how-to-launch-gstreamer-pipeline-in-python/

import sys
import traceback
import argparse

import gi
gi.require_version('Gst', '1.0')
from gi.repository import GLib, Gst, GObject  # noqa:F401,F402


# Initializes Gstreamer, it's variables, paths
Gst.init(sys.argv)

DEFAULT_PIPELINE = "airplaysrc name=source ! queue ! h264parse ! avdec_h264 max-threads=1 ! autovideosink"

ap = argparse.ArgumentParser()
ap.add_argument("-p", "--pipeline", required=False,
        default=DEFAULT_PIPELINE, help="Gstreamer pipeline without gst-launch")

args = vars(ap.parse_args())


def on_message(bus: Gst.Bus, message: Gst.Message, loop: GLib.MainLoop):
    mtype = message.type
    """
        Gstreamer Message Types and how to parse
        https://lazka.github.io/pgi-docs/Gst-1.0/flags.html#Gst.MessageType
    """
    if mtype == Gst.MessageType.EOS:
        print("End of stream")
        loop.quit()

    elif mtype == Gst.MessageType.ERROR:
        err, debug = message.parse_error()
        print(err, debug)
        loop.quit()

    elif mtype == Gst.MessageType.WARNING:
        err, debug = message.parse_warning()
        print(err, debug)

    return True

def on_connected(instance, param):
    print("Connected: {}".format(instance.get_property(param.name)))

command = args["pipeline"]

# Gst.Pipeline https://lazka.github.io/pgi-docs/Gst-1.0/classes/Pipeline.html
# https://lazka.github.io/pgi-docs/Gst-1.0/functions.html#Gst.parse_launch
pipeline = Gst.parse_launch(command)

# https://lazka.github.io/pgi-docs/Gst-1.0/classes/Bus.html
bus = pipeline.get_bus()

# allow bus to emit messages to main thread
bus.add_signal_watch()

# Start pipeline
pipeline.set_state(Gst.State.PLAYING)

# Init GLib loop to handle Gstreamer Bus Events
loop = GLib.MainLoop()

# Add handler to specific signal
# https://lazka.github.io/pgi-docs/GObject-2.0/classes/Object.html#GObject.Object.connect
bus.connect("message", on_message, loop)

src = pipeline.get_by_name("source")
src.connect("notify::connected", on_connected)

try:
    loop.run()
except Exception:
    traceback.print_exc()
    loop.quit()

# Stop Pipeline
pipeline.set_state(Gst.State.NULL)
