import gzip
import os
import shutil
from SCons.Script import AlwaysBuild

Import("env")

def compress_data_file(target, source, env):
    src_path = str(source[0])
    gz_path = str(target[0])
    with open(src_path, "rb") as f_in:
        with gzip.GzipFile(gz_path, "wb", compresslevel=9, mtime=0) as f_out:
            shutil.copyfileobj(f_in, f_out)
    src_size = os.path.getsize(src_path)
    gz_size = os.path.getsize(gz_path)
    ratio = (gz_size * 100) // src_size if src_size else 0
    data_dir = env.get("PROJECT_DATA_DIR")
    print(f"[GZIP DATA] {os.path.relpath(src_path, data_dir)} ({src_size} B) -> ({gz_size} B, {ratio}%)")

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

def clean_orphaned_data_gzip_files():
    data_dir = os.path.join(env.get("PROJECT_DIR"), "data")
    if not os.path.isdir(data_dir):
        return
    for root, _, files in os.walk(data_dir):
        for f in files:
            if f.endswith(".gz"):
                orig_file = f[:-3]
                if orig_file.endswith((".css", ".js", ".htm", ".html", ".json")):
                    orig_path = os.path.join(root, orig_file)
                    if not os.path.exists(orig_path):
                        os.remove(os.path.join(root, f))
                        print(f"[GZIP CLEAN] Removed orphan {f}")

# Register generated gzip assets as dependencies of the filesystem image. This
# ensures they are created or refreshed before mklittlefs reads the data folder.
compressed_assets = []
data_dir = env.get("PROJECT_DATA_DIR")
for root, _, files in os.walk(data_dir):
    for filename in files:
        if filename.endswith((".css", ".js", ".htm", ".html", ".json")) and not filename.endswith(".gz"):
            src_path = os.path.join(root, filename)
            compressed_assets.extend(
                env.Command(src_path + ".gz", src_path, compress_data_file)
            )

clean_orphaned_data_gzip_files()

def post_bin_action(source, target, env):
    for t in target:
        compress_file(str(t))

env.AddPostAction(os.path.join("$BUILD_DIR", "${PROGNAME}.bin"), post_bin_action)
env.AddPostAction(os.path.join("$BUILD_DIR", "${ESP8266_FS_IMAGE_NAME}.bin"), post_bin_action)

# Include the LittleFS image in the normal PlatformIO Build target.
filesystem_image = env.DataToBin(
    os.path.join("$BUILD_DIR", "${ESP8266_FS_IMAGE_NAME}"),
    "$PROJECT_DATA_DIR")
env.NoCache(filesystem_image)
AlwaysBuild(filesystem_image)
env.Depends(filesystem_image, compressed_assets)
env.AddPostAction(filesystem_image, post_bin_action)
env.Depends("buildprog", filesystem_image)
