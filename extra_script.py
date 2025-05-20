import shutil
import os

# Import SCons environment variables
Import("env")

# This function handles the actual copying
def before_build_action(source, target, env):
    # Get important paths from the PlatformIO environment
    project_dir = env.subst("$PROJECT_DIR")
    libdeps_dir = env.subst("$PROJECT_LIBDEPS_DIR")
    pioenv = env.subst("$PIOENV") 

    print(f"\n\n*** [extra_script.py] ACTION START: Injecting custom TFT_eSPI setups for environment '{pioenv}'. ***")

    env_libdeps_dir = os.path.join(libdeps_dir, pioenv)
    tft_espi_lib_path = None

    if os.path.exists(env_libdeps_dir):
        for item in os.listdir(env_libdeps_dir):
            # Check if 'TFT_eSPI' is in the name and it's a directory
            if "TFT_eSPI" in item and os.path.isdir(os.path.join(env_libdeps_dir, item)):
                tft_espi_lib_path = os.path.join(env_libdeps_dir, item)
                print(f"TFT_eSPI library found at: {tft_espi_lib_path}")
                break
    
    if not tft_espi_lib_path:
        print(f"ERROR: TFT_eSPI library directory not found in '{env_libdeps_dir}'.")
        print("Please ensure TFT_eSPI is listed as a dependency in platformio.ini and has been installed.")
        print("*** [extra_script.py] ACTION FAILED. ***\n")
        return

    # Path to your custom setup files
    custom_setup_dir = os.path.join(project_dir, "lib", "TFT_eSPI_Setups")

    # Define source and destination paths
    user_setup_select_src = os.path.join(custom_setup_dir, "User_Setup_Select.h")
    user_setup_select_dst = os.path.join(tft_espi_lib_path, "User_Setup_Select.h")

    user_setup_src = os.path.join(custom_setup_dir, "User_Setup.h")
    user_setup_dst = os.path.join(tft_espi_lib_path, "User_Setup.h")

    def copy_file_if_exists(src_path, dst_path):
        if os.path.exists(src_path):
            try:
                shutil.copyfile(src_path, dst_path)
                print(f"Successfully injected: {src_path} -> {dst_path}")
            except Exception as e:
                print(f"ERROR copying {src_path} to {dst_path}: {e}")
        else:
            print(f"Source file not found, cannot inject: {src_path}")

    print("Copying User_Setup_Select.h...")
    copy_file_if_exists(user_setup_select_src, user_setup_select_dst)
        
    print("Copying User_Setup.h...")
    copy_file_if_exists(user_setup_src, user_setup_dst)
        
    print(f"*** [extra_script.py] ACTION COMPLETED for '{pioenv}'. ***\n")

# ----- IMPORTANT CHANGE HERE -----
# Call the function directly after PlatformIO loads this script and Import("env") has been executed.
# 'source' and 'target' arguments are not relevant for this direct call, hence None.
# The 'env' object is already provided by PlatformIO when the script is loaded.

# Check if 'env' is actually available before using it.
# In the global execution of an extra_script, 'env' should be available through Import("env").
if "env" in locals() or "env" in globals():
    print("*** [extra_script.py] Script loaded. Calling 'before_build_action' directly now... ***")
    before_build_action(None, None, env) 
    print("*** [extra_script.py] Direct call of 'before_build_action' completed. ***")
else:
    print("*** [extra_script.py] ERROR: 'env' object not found. Cannot call 'before_build_action' directly. ***")
    print("Ensure 'Import(\"env\")' is at the beginning of the script.")
