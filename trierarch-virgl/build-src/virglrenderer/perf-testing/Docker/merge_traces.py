#!/usr/bin/python3
#
# Copyright (C) 2020 Collabora Ltd
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

from google import protobuf
import protos.perfetto.trace.perfetto_trace_pb2
from protos.perfetto.trace.perfetto_trace_pb2 import BUILTIN_CLOCK_BOOTTIME
from protos.perfetto.trace.perfetto_trace_pb2 import BUILTIN_CLOCK_REALTIME
import math
import sys
import operator
import time

def add_ftrace_event(out_message, in_packet, in_event, max_host_sequence_id = 0):
    out_packet = out_message.packet.add()
    out_packet.ftrace_events.cpu = in_packet.ftrace_events.cpu
    out_packet.trusted_uid = in_packet.trusted_uid
    out_packet.trusted_packet_sequence_id += max_host_sequence_id
    out_packet.ftrace_events.event.add().CopyFrom(in_event)

virtio_gpu_pids = set()

print('%d Loading host trace' % time.time())

in_message = protos.perfetto.trace.perfetto_trace_pb2.Trace()
in_message.ParseFromString(open(sys.argv[1], 'rb').read())

print('%d Copying host trace' % time.time())

out_message = protos.perfetto.trace.perfetto_trace_pb2.Trace()
max_host_sequence_id = 0
first_host_virtio_gpu_cmd = math.inf
host_boot_ts = -1
for in_packet in in_message.packet:
    max_host_sequence_id = max(max_host_sequence_id,
                               in_packet.trusted_packet_sequence_id)

    if in_packet.HasField('ftrace_events'):
        for event in in_packet.ftrace_events.event:
            if event.HasField('sched_switch'):
                if 'virtio_gpu' == event.sched_switch.prev_comm:
                    virtio_gpu_pids.add(event.sched_switch.prev_pid)
                if 'virtio_gpu' == event.sched_switch.next_comm:
                    virtio_gpu_pids.add(event.sched_switch.next_pid)

                if event.sched_switch.prev_pid in virtio_gpu_pids or \
                   event.sched_switch.next_pid in virtio_gpu_pids:
                    add_ftrace_event(out_message, in_packet, event)
            elif event.HasField('sched_wakeup'):
                if 'virtio_gpu' == event.sched_wakeup.comm:
                    virtio_gpu_pids.add(event.sched_wakeup.pid)

                if event.sched_wakeup.pid in virtio_gpu_pids:
                    add_ftrace_event(out_message, in_packet, event)
            elif event.HasField('print'):
                 event_type, guest_pid, label, cookie = event.print.buf.split('|')

                 # Replace host PID with the guest PID
                 event.pid = int(guest_pid)
                 add_ftrace_event(out_message, in_packet, event)
    else:
        if in_packet.HasField('track_descriptor'):
            if in_packet.track_descriptor.HasField('name'):
               in_packet.track_descriptor.name += ' (Host)'
        elif in_packet.HasField('track_event'):
            if in_packet.track_event.type == in_packet.track_event.TYPE_SLICE_BEGIN and \
               in_packet.track_event.name == 'GetCapset':
                first_host_virtio_gpu_cmd = min(first_host_virtio_gpu_cmd, in_packet.timestamp)
        elif host_boot_ts == -1 and in_packet.HasField('clock_snapshot'):
            for clock in in_packet.clock_snapshot.clocks:
                if clock.clock_id == BUILTIN_CLOCK_BOOTTIME:
                    host_boottime = clock.timestamp
                elif clock.clock_id == BUILTIN_CLOCK_REALTIME:
                    host_realtime = clock.timestamp
            host_boot_ts = host_realtime - host_boottime
        out_packet = out_message.packet.add()
        out_packet.CopyFrom(in_packet)

print('%d Loading guest trace' % time.time())
in_message.ParseFromString(open(sys.argv[2], 'rb').read())

#print('%d Writing guest trace txt' % time.time())
#open('../traces-db/perfetto-guest.txt', 'w').write(str(in_message))

first_guest_virtio_gpu_cmd = math.inf
guest_boot_ts = -1
for in_packet in in_message.packet:
    if guest_boot_ts == -1 and in_packet.HasField('clock_snapshot'):
        for clock in in_packet.clock_snapshot.clocks:
            if clock.clock_id == BUILTIN_CLOCK_BOOTTIME:
                guest_boottime = clock.timestamp
            elif clock.clock_id == BUILTIN_CLOCK_REALTIME:
                guest_realtime = clock.timestamp
        guest_boot_ts = guest_realtime - guest_boottime
    elif in_packet.HasField('track_event'):
     if in_packet.track_event.type == in_packet.track_event.TYPE_SLICE_BEGIN and \
        in_packet.track_event.name == 'DRM_IOCTL_VIRTGPU_GET_CAPS':
         first_guest_virtio_gpu_cmd = min(first_guest_virtio_gpu_cmd, in_packet.timestamp)

delta = guest_boot_ts - host_boot_ts
cmd_delta = first_host_virtio_gpu_cmd - first_guest_virtio_gpu_cmd - delta
print("boottime delta %ds." % (delta / 1000 / 1000 / 1000))
print("cmd delta %dus." % (cmd_delta / 1000))

for in_packet in in_message.packet:
    if in_packet.HasField('process_tree') or \
       in_packet.HasField('service_event') or \
       in_packet.HasField('track_event') or \
       in_packet.HasField('trace_packet_defaults') or \
       in_packet.HasField('track_descriptor'):
        out_packet = out_message.packet.add()
        out_packet.CopyFrom(in_packet)
        out_packet.trusted_packet_sequence_id += max_host_sequence_id
        out_packet.timestamp += delta
        if out_packet.HasField('track_descriptor'):
            if out_packet.track_descriptor.HasField('name'):
                out_packet.track_descriptor.name += ' (Guest)'
    elif in_packet.HasField('ftrace_events'):
         for event in in_packet.ftrace_events.event:
             event.timestamp += delta
             add_ftrace_event(out_message, in_packet, event, max_host_sequence_id)

def get_timestamp(packet):
    if packet.HasField('timestamp'):
        return packet.timestamp
    elif packet.HasField('ftrace_events') and \
         packet.ftrace_events.event:
        return packet.ftrace_events.event[0].timestamp
    return 0

out_message.packet.sort(key=get_timestamp)
print('%d Writing merged trace' % time.time())
open(sys.argv[3], 'wb').write(out_message.SerializeToString())

#print('%d Writing merged trace txt' % time.time())
#open('../traces-db/perfetto.txt', 'w').write(str(out_message))
