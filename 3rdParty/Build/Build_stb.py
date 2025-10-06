import os
import shutil
import glob

script_dir = os.path.dirname(os.path.abspath(__file__))

source_dir =  os.path.join(script_dir, "../Sources/stb")
install_dir =  os.path.join(script_dir, "../stb/include/stb")

os.makedirs(install_dir, exist_ok=True)

for file_path in glob.glob(os.path.join(source_dir, "*.h")):
    print(f"   Copying: {os.path.basename(file_path)}")
    shutil.copy(file_path, install_dir)

print("\n✅ Finished")