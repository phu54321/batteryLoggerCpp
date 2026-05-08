#%%

import datetime
import dateutil
import dateutil.parser
import sys
import os

if len(sys.argv) > 1:
    logname = sys.argv[1]
else:
    logname = os.path.expanduser('~/batterylog.csv')

cells = []
with open(logname, 'r') as rf:
    for l in rf.readlines():
        l = l.strip()
        l = l.replace('\0', '')
        if l:
            cells.append(l.split(','))

segments = []

current_segment = None

for cell in cells:
    time, plugged, percent, machine_id = cell[:4]
    if time == 'time':
        continue

    time = dateutil.parser.isoparse(time)
    plugged = (plugged == 'True')
    percent = int(percent)
    chunk = (time, percent, machine_id)

    if current_segment is None:
        if plugged:
            continue
        current_segment = (chunk, chunk)
    else:
        if plugged:
            segments.append(current_segment)
            current_segment = None
            continue

        segment_first_chunk = current_segment[0]
        segment_last_chunk = current_segment[1]

        last_time = segment_last_chunk[0]
        timediff = (time - last_time).total_seconds()
        if timediff > 90 or segment_first_chunk[2] != machine_id:
            # Sleep inbetween: split chunk
            segments.append(current_segment)
            current_segment = (chunk, chunk)
        else:
            current_segment = (segment_first_chunk, chunk)


speedListMap = {}
for i, segment in enumerate(segments):
    first_chunk, last_chunk = segment
    start_time = first_chunk[0]
    last_time = last_chunk[0]
    machine_id = first_chunk[2]

    if start_time == last_time:
        continue

    elapsed_min = (last_time - start_time).total_seconds() / 60
    if elapsed_min <= 30.5:  # Ignore short segment
        continue

    start_percent = first_chunk[1]
    last_percent = last_chunk[1]
    percent_diff = last_percent - start_percent
    print('[%s:%s] Segment %3d: Time %3d min, percent diff %3d%%, speed %.2f %%/hr (%.2f W)' %
          (machine_id, start_time, i, int(elapsed_min), percent_diff, -percent_diff / elapsed_min * 60, -percent_diff / elapsed_min * 60 * 0.72))
    speed = -percent_diff / elapsed_min * 60
    # print(speed)

    if machine_id not in speedListMap:
        speedListMap[machine_id] = []
    speedListMap[machine_id].append(speed)

for machine_id, speedList in speedListMap.items():
    if len(speedList) > 3:
        speedList = speedList[3:]
        print(' - [Machine %s] Average consumption (excluding initial 3 segments): %.2f %%/hr' % (machine_id, sum(speedList) / len(speedList)))

# %%


# import psutil
# import datetime
# import os
# import time
# from tendo import singleton

# def logTask():
#     wpath = os.path.expanduser('~/batteryLog.csv')
#     battery = psutil.sensors_battery()
#     if not os.path.exists(wpath):
#         with open(wpath, 'w') as wf:
#             wf.write('time,plugged,percent\n')

#     with open(wpath, 'a') as wf:
#         wf.write('%s,%s,%s\n' % (
#             datetime.datetime.now().isoformat(),
#             battery.power_plugged,
#             battery.percent
#         ))

# me = singleton.SingleInstance()

# if __name__ == '__main__':
#     # Ignore unused variable warning: me should live until the end of the script.
#     while True:
#         logTask()
#         time.sleep(60)

# %%
