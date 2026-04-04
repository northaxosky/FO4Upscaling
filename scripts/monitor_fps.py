"""Monitor F4SE logs in real-time for DLSS-G frame generation activity."""
import time
import sys
import os

LOG_DIR = os.path.expanduser("~/Documents/My Games/Fallout4/F4SE")
FG_LOG = os.path.join(LOG_DIR, "AAAFrameGeneration.log")

def tail_log(filepath, keywords):
    """Follow a log file and print lines matching keywords."""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            # Seek to end
            f.seek(0, 2)
            print(f"Monitoring {filepath}...")
            print("Looking for: " + ", ".join(keywords))
            print("-" * 60)

            while True:
                line = f.readline()
                if line:
                    for kw in keywords:
                        if kw.lower() in line.lower():
                            print(line.rstrip())
                            break
                else:
                    time.sleep(0.5)
    except FileNotFoundError:
        print(f"Log file not found: {filepath}")
    except KeyboardInterrupt:
        print("\nStopped.")

if __name__ == "__main__":
    keywords = [
        "DLSSG",
        "frame gen",
        "interpolat",
        "Present.*hook",
        "swap.*chain",
        "offscreen",
        "FG enabled",
        "status=",
    ]
    if len(sys.argv) > 1:
        keywords = sys.argv[1:]

    tail_log(FG_LOG, keywords)
