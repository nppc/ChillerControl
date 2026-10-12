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

def post_bin_action(source, target, env):
    for t in target:
        compress_file(str(t))

env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), post_bin_action)
env.AddPostAction(os.path.join("$BUILD_DIR", "${ESP8266_FS_IMAGE_NAME}.bin"), post_bin_action)

