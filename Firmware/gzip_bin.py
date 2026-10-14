import gzip
import os
import shutil

Import("env")

def compress_file(bin_path):
    if not os.path.isfile(bin_path):
        return
    gz_path = bin_path + ".gz"
    with open(bin_path, "rb") as f_in, gzip.open(gz_path, "wb", compresslevel=9) as f_out:
        shutil.copyfileobj(f_in, f_out)
    orig_size = os.path.getsize(bin_path)
    gz_size = os.path.getsize(gz_path)
    ratio = (gz_size * 100) // orig_size if orig_size else 0
    print(f"[GZIP] {os.path.basename(bin_path)} ({orig_size} B) -> {os.path.basename(gz_path)} ({gz_size} B, {ratio}%)")

def compress_data_dir():
    data_dir = os.path.join(env.get("PROJECT_DIR"), "data")
    if not os.path.isdir(data_dir):
        return
    compressed_any = False
    for root, _, files in os.walk(data_dir):
        for f in files:
            if f.endswith((".css", ".js", ".htm", ".html", ".json")) and not f.endswith(".gz"):
                src_path = os.path.join(root, f)
                gz_path = src_path + ".gz"
                if not os.path.exists(gz_path) or os.path.getmtime(src_path) > os.path.getmtime(gz_path):
                    with open(src_path, "rb") as f_in, gzip.GzipFile(gz_path, "wb", compresslevel=9, mtime=0) as f_out:
                        shutil.copyfileobj(f_in, f_out)
                    src_size = os.path.getsize(src_path)
                    gz_size = os.path.getsize(gz_path)
                    ratio = (gz_size * 100) // src_size if src_size else 0
                    print(f"[GZIP DATA] {os.path.relpath(src_path, data_dir)} ({src_size} B) -> ({gz_size} B, {ratio}%)")
                    compressed_any = True

    # Clean up orphan .gz files
    for root, _, files in os.walk(data_dir):
        for f in files:
            if f.endswith(".gz"):
                orig_file = f[:-3]
                if orig_file.endswith((".css", ".js", ".htm", ".html", ".json")):
                    orig_path = os.path.join(root, orig_file)
                    if not os.path.exists(orig_path):
                        os.remove(os.path.join(root, f))
                        print(f"[GZIP CLEAN] Removed orphan {f}")

# Compress data files automatically whenever PlatformIO runs
compress_data_dir()

def post_bin_action(source, target, env):
    for t in target:
        compress_file(str(t))

env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), post_bin_action)
env.AddPostAction(os.path.join("$BUILD_DIR", "${ESP8266_FS_IMAGE_NAME}.bin"), post_bin_action)

