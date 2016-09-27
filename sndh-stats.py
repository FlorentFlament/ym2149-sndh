import sys

def main():
    arr = {}
    old_ts = None
    for l in sys.stdin.readlines():
        sample = []
        _, ts, _ = l.split()
        ts = int(ts, 16)
        if old_ts: # Deal with first pass
            delta = ts - old_ts
            if delta not in arr:
                arr[delta] = 0
            arr[delta]+= 1
        old_ts = ts
    if arr:
        print min(arr.keys())
    #for k in sorted(arr.keys()):
    #    print k, 2000000./k, arr[k]

main()
