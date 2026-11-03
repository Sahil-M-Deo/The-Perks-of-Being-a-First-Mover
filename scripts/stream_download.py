"""
stream_download.py: streams one Lichess file into a named pipe, and survives a dropped connection.

    python3 stream_download.py URL PIPE STATUS_JSON [SHA256]

The program (white_advantage) reads the other end of PIPE, so the file is never saved to disk.
If the connection drops or stalls, the download asks Lichess for the rest of the file from the
exact byte where it stopped (an HTTP "Range" request), so the program just sees one unbroken
stream. A SHA256 checksum of every byte is computed on the way; if SHA256 is given (Lichess
publishes them), the file is checked against it at the end.

STATUS_JSON is rewritten every few seconds with the progress, and at the end with
"state": "done" (every byte arrived, checksum matches), "failed", or "reader_closed"
(the program stopped reading, for example after --max-games).

Exit code: 0 done, 1 failed, 3 reader closed.
"""
import hashlib
import json
import os
import sys
import time

import requests

CHUNK = 1 << 20                 # 1 MB
CONNECT_TIMEOUT = 30            # seconds to connect
READ_TIMEOUT = int(os.environ.get("STREAM_READ_TIMEOUT", 90))   # seconds with no data = stalled
MAX_FAILS_IN_A_ROW = 30         # give up after this many reconnects with no progress at all
ZSTD_MAGIC = b"\x28\xb5\x2f\xfd"


def log(msg):
    print(time.strftime("[%H:%M:%S] ") + msg, file=sys.stderr, flush=True)


def save_status(path, status):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(status, f, indent=1)
    os.replace(tmp, path)


def main():
    url, pipe_path, status_path = sys.argv[1], sys.argv[2], sys.argv[3]
    expected_sha = sys.argv[4].lower() if len(sys.argv) > 4 and sys.argv[4] else None

    status = {"url": url, "state": "starting", "bytes": 0, "total": None, "reconnects": 0,
              "mb_per_s": 0.0, "sha256": None, "sha256_expected": expected_sha,
              "error": None, "started": time.time(), "updated": time.time()}
    save_status(status_path, status)

    # open the pipe first, so the program on the other end never waits for nothing
    out = open(pipe_path, "wb")
    sha = hashlib.sha256()
    offset = 0
    total = None
    fails = 0
    last_save = 0.0
    window_start, window_bytes = time.time(), 0

    def finish(state, error=None, code=1):
        status.update(state=state, error=error, bytes=offset, updated=time.time())
        save_status(status_path, status)
        try:
            out.close()
        except BrokenPipeError:
            pass
        log(f"{state}: {offset:,} of {total or 0:,} bytes" + (f" ({error})" if error else ""))
        sys.exit(code)

    while True:
        headers = {"Range": f"bytes={offset}-"} if offset > 0 else {}
        progressed = False
        try:
            with requests.get(url, headers=headers, stream=True,
                              timeout=(CONNECT_TIMEOUT, READ_TIMEOUT)) as r:
                if r.status_code == 404:
                    finish("failed", f"HTTP 404: {url} does not exist (check the month)")
                if 400 <= r.status_code < 500 and r.status_code not in (408, 429):
                    finish("failed", f"HTTP {r.status_code} from Lichess")
                if offset > 0:
                    # resuming: the server must send exactly the rest of the file
                    cr = r.headers.get("Content-Range", "")
                    if r.status_code != 206 or not cr.startswith(f"bytes {offset}-"):
                        finish("failed", f"server cannot resume (HTTP {r.status_code}, "
                                         f"Content-Range '{cr}'); the month has to start again")
                else:
                    r.raise_for_status()
                    total = int(r.headers["Content-Length"]) if "Content-Length" in r.headers else None
                    status["total"] = total
                    log(f"started, {total / 1e9:.2f} GB" if total else "started, size unknown")

                for chunk in r.iter_content(CHUNK):
                    if not chunk:
                        continue
                    if offset == 0 and not chunk.startswith(ZSTD_MAGIC):
                        finish("failed", "the download is not a .zst file (an error page?)")
                    try:
                        out.write(chunk)
                    except BrokenPipeError:
                        finish("reader_closed", None, 3)
                    sha.update(chunk)
                    offset += len(chunk)
                    window_bytes += len(chunk)
                    progressed = True
                    fails = 0
                    now = time.time()
                    if now - last_save > 5:
                        status.update(state="downloading", bytes=offset, updated=now,
                                      mb_per_s=round(window_bytes / 1e6 / max(now - window_start, 1e-9), 2))
                        save_status(status_path, status)
                        last_save = now
                        if now - window_start > 60:
                            window_start, window_bytes = now, 0

            if total is not None and offset >= total:
                break
            if total is None and progressed:
                break       # no size given: a clean end of the response is the end of the file
            raise IOError(f"connection ended early at byte {offset:,}")

        except SystemExit:
            raise
        except BrokenPipeError:
            finish("reader_closed", None, 3)
        except Exception as e:      # dropped, stalled, reset, DNS, 5xx ...
            fails = 0 if progressed else fails + 1
            status["reconnects"] += 1
            status.update(state="reconnecting", bytes=offset, error=str(e)[:300], updated=time.time())
            save_status(status_path, status)
            if fails >= MAX_FAILS_IN_A_ROW:
                finish("failed", f"gave up after {fails} failed reconnects: {e}")
            wait = min(60, 2 ** min(fails, 6))
            log(f"connection problem at {offset:,} bytes ({e}); retrying in {wait}s")
            time.sleep(wait)

    try:
        out.flush()
    except BrokenPipeError:
        finish("reader_closed", None, 3)
    digest = sha.hexdigest()
    status["sha256"] = digest
    if total is not None and offset != total:
        finish("failed", f"got {offset:,} bytes, expected {total:,}")
    if expected_sha and digest != expected_sha:
        finish("failed", "SHA256 does not match Lichess's checksum: the data is corrupt")
    finish("done", None, 0)


if __name__ == "__main__":
    main()
