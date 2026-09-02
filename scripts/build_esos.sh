#!/bin/bash -e

PACKAGE_SRC_NAME="esos"
CLEAN_CMD='rm -rf output/esos; rm -f bsp/spacemit/.esos_top.config bsp/spacemit/.esos.config bsp/spacemit/*.itb bsp/spacemit/*.elf bsp/spacemit/*.bin bsp/spacemit/platform/rt24/*.dtb; (cd bsp/spacemit && scons -c >/dev/null 2>&1 || true)'
BUILD_CMD='git config --global --add safe.directory "*"; TOP_OUTPUT_DIR=$(pwd)/output/esos ./build_top.sh'
BUILD_DEB_CMD='git config --global --add safe.directory "*"; mkdir -p /usr/lib/gcc/riscv64-unknown-elf/14.2.0 /usr/local/bin; for t in objdump readelf strip nm objcopy size strings addr2line ranlib ar elfedit; do [ -e /usr/bin/$t ] && ln -sf /usr/bin/$t /usr/local/bin/riscv64-linux-gnu-$t; done; GIT_VERSION=$(git rev-parse --short HEAD 2>/dev/null); VERSION=$(if [ -n "$GIT_VERSION" ]; then echo "0~g$GIT_VERSION"; else echo "0~$(date +%Y%m%d%H%M%S)"; fi); rm -rf debian/changelog; dch --create --package '"$PACKAGE_SRC_NAME"' -v ${VERSION} --distribution resolute-porting --force-distribution "Bianbu Test"; DEB_BUILD_OPTIONS="nocheck nostrip" dpkg-buildpackage -us -uc -b -ariscv64 -d -j${JOBS:-$(nproc)}'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SOURCE_PARENT_DIR="$(cd "$SOURCE_DIR/.." && pwd)"
WORKDIR_IN_CONTAINER="/workspace/$(basename "$SOURCE_DIR")"
DOCKER_REPO="${DOCKER_REPO:-harbor.spacemit.com/bianbu}"
IMAGE_NAME="${IMAGE_NAME:-k3-bsp-builder:latest}"
CROSS_COMPILE="${CROSS_COMPILE:-riscv64-unknown-elf-}"
TOOLCHAIN_PATH="${TOOLCHAIN_PATH:-$WORKDIR_IN_CONTAINER/tools/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9/bin}"
DIRECT_BUILD="${DIRECT_BUILD:-0}"
JOBS=""

CLEAN=false
BUILD_DEB=false
ACTION="build"
DEBUG=false

# Function to handle symlinks within git directory
handle_git_internal_symlinks() {
    local git_dir="$1"
    local container_git_dir="$2"
    local -n git_mounts_ref="$3"  # Reference to the git mounts array

    [ "$DEBUG" = true ] && echo "Scanning for symlinks in git directory: $git_dir"

    # Find all symlinks in the git directory
    while IFS= read -r -d '' symlink; do
        if [ -L "$symlink" ]; then
            local real_target
            real_target="$(readlink -f "$symlink")"
            local relative_path="${symlink#$git_dir/}"
            local container_target="$container_git_dir/$relative_path"

            if [ -e "$real_target" ]; then
                [ "$DEBUG" = true ] && echo "Found symlink: $relative_path -> $real_target"
                # Store mounts in the array - add both -v and the mount path
                git_mounts_ref+=("-v" "$real_target:$container_target")
            else
                [ "$DEBUG" = true ] && echo "Warning: Broken symlink found: $symlink -> $real_target"
            fi
        fi
    done < <(find "$git_dir" -type l -print0 2>/dev/null)
}

# Function to setup all container volume mounts (including git handling)
setup_volume_mounts() {
    local project_dir="$1"
    local package_dir="$2"
    local git_path="$package_dir/.git"
    local container_base
    container_base="/workspace/$(basename "$package_dir")"

    # Local git_mounts array for this function
    local git_mounts=()

    [ "$DEBUG" = true ] && echo "Setting up container volume mounts..."

    # Initialize with basic mounts
    VOLUME_MOUNTS=("-v" "$project_dir:/workspace" "-w" "$container_base")

    # Handle git directory if exists
    if [ -e "$git_path" ]; then
        local actual_git_dir
        local container_git_path="$container_base/.git"

        if [ -L "$git_path" ]; then
            [ "$DEBUG" = true ] && echo "Detected .git as symbolic link, resolving real git directory..."
            actual_git_dir="$(readlink -f "$git_path")"

            if [ ! -d "$actual_git_dir" ]; then
            echo "Error: Cannot find real git directory: $actual_git_dir"
            echo "Please ensure the git repository is accessible."
                exit 1
            fi

            # Set up main git directory mount
            git_mounts+=("-v" "$actual_git_dir:$container_git_path")
            [ "$DEBUG" = true ] && echo "Will mount real git directory: $actual_git_dir -> $container_git_path"

        elif [ -d "$git_path" ]; then
            [ "$DEBUG" = true ] && echo "Found normal .git directory"
            actual_git_dir="$git_path"
            # For normal directories, no additional mount needed as it's part of the main project mount

        else
            [ "$DEBUG" = true ] && echo "Warning: .git exists but is neither a directory nor a symlink"
            actual_git_dir=""
        fi

        # Handle symlinks within the git directory
        if [ -n "$actual_git_dir" ]; then
            handle_git_internal_symlinks "$actual_git_dir" "$container_git_path" git_mounts
        fi
    else
        [ "$DEBUG" = true ] && echo "Warning: No .git directory found, some git operations may fail"
    fi

    # Add all git mounts (both main directory and internal symlinks)
    if [ ${#git_mounts[@]} -gt 0 ]; then
        VOLUME_MOUNTS+=("${git_mounts[@]}")
        [ "$DEBUG" = true ] && echo "Added $(( ${#git_mounts[@]} / 2 )) git-related mounts"
    fi

    [ "$DEBUG" = true ] && echo "Total volume mounts configured: $(( (${#VOLUME_MOUNTS[@]} + 1) / 2 ))"
    # fix this function will exit if bash set -e
    return 0
}

# Setup container volume mounts (includes git handling)
setup_volume_mounts "$SOURCE_PARENT_DIR" "$SOURCE_DIR"

usage() {
    echo "Container-based build wrapper (esos version)"
    echo "Usage: $(basename "$0") [OPTIONS] [COMMAND]"
    echo ""
    echo "Options:"
    echo "  -c, --clean          Clean build (CLEAN_CMD before BUILD[_DEB]_CMD)"
    echo "  -d, --deb            Build DEB packages (BUILD_DEB_CMD)"
    echo "  -h, --help           Show this help message"
    echo "  -j, --jobs NUM       Number of parallel jobs (default: nproc)"
    echo "  -x, --debug          Enable debug output (show docker command)"
    echo ""
    echo "Commands (run inside container):"
    echo "  build               Build (BUILD_CMD, default)"
    echo "  shell               Interactive shell"
    echo ""
    echo "Examples:"
    echo "  $(basename "$0")                    # Build in container"
    echo "  $(basename "$0") -c -d              # Clean build with DEB packages"
    echo "  $(basename "$0") shell              # Interactive container shell"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -d|--deb)
            BUILD_DEB=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -x|--debug)
            DEBUG=true
            shift
            ;;
        build|shell)
            ACTION="$1"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ -n "$JOBS" && ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid jobs value: $JOBS (must be a positive integer)"
    exit 1
fi

# Check if Docker is available
if [ -z "$DIRECT_BUILD" ] || [ "$DIRECT_BUILD" = "0" ]; then
    if ! command -v docker &> /dev/null; then
        echo "Error: Docker not found. Please install Docker."
        exit 1
    fi
fi

# Set DEBEMAIL and DEBFULLNAME environment variable
if [ -z "$DEBEMAIL" ]; then
    if [ -n "$MAIL" ]; then
        DEBEMAIL="$MAIL"
    else
        DEBEMAIL="$(id -nu)@$(hostname -f 2>/dev/null || hostname)"
    fi
fi

if [ -z "$DEBFULLNAME" ]; then
    if [ -n "$NAME" ]; then
        DEBFULLNAME="$NAME"
    elif echo "$DEBEMAIL" | grep -qE '^([^<]+)\s*<[^>]+>$'; then
        # If DEBEMAIL matches "name <email>", extract name as DEBFULLNAME
        DEBFULLNAME="$(echo "$DEBEMAIL" | sed -n 's/^\([^<][^<]*\)<.*$/\1/p' | sed 's/[[:space:]]*$//')"
    else
        DEBFULLNAME="$(id -nu)"
    fi
fi

CONTAINER_ENV=("-e" "ARCH=riscv" "-e" "CROSS_COMPILE=$CROSS_COMPILE" "-e" "PATH=$TOOLCHAIN_PATH:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" "-e" "DEBEMAIL=$DEBEMAIL" "-e" "DEBFULLNAME=$DEBFULLNAME")
if [[ -n "$JOBS" ]]; then
    CONTAINER_ENV+=("-e" "JOBS=$JOBS")
fi

# Function to run command in container
#
# NOTE: Unlike the kernel/opensbi/uboot wrappers, this runs as root (no `-u`)
# because esos's debian/rules needs to write into /usr/lib (the `cp debian/lib/*`
# step) and to have the elf toolchain discoverable. The compiler itself is found
# via PATH (TOOLCHAIN_PATH above) even though debian/rules sets RTT_EXEC_PATH=/usr/bin.
run_in_container() {
    local cmd="$1"

    if [ "$DEBUG" = true ]; then
        echo "--- Docker/Podman full command ---"
        local full_cmd
        full_cmd="docker run --rm -it --init"
        for v in "${VOLUME_MOUNTS[@]}"; do
            full_cmd+=" $v"
        done
        for e in "${CONTAINER_ENV[@]}"; do
            full_cmd+=" $e"
        done
        full_cmd+=" $DOCKER_REPO/$IMAGE_NAME"
        full_cmd+=" bash -c \"$cmd\""
        echo "$full_cmd"
        echo "--- End docker command ---"
    fi

    docker run --rm -it --init \
        "${VOLUME_MOUNTS[@]}" \
        "${CONTAINER_ENV[@]}" \
        "$DOCKER_REPO/$IMAGE_NAME" \
        bash -c "$cmd"
}

# Run command directly or in container based on DIRECT_BUILD
run_command() {
    local cmd="$1"
    if [ -n "$DIRECT_BUILD" ] && [ "$DIRECT_BUILD" != "0" ]; then
        [ "$DEBUG" = true ] && echo "[DIRECT_BUILD] Executing locally: $cmd"
        local local_tc="$SOURCE_DIR/tools/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9/bin"
        if ! command -v riscv64-unknown-elf-gcc > /dev/null 2>&1; then
            if [ -x "$local_tc/riscv64-unknown-elf-gcc" ]; then
                export PATH="$local_tc:$PATH"
                export RTT_EXEC_PATH="$local_tc"
            else
                echo "Install cross compile (riscv64-unknown-elf-gcc) and add to PATH"
                exit 1
            fi
        fi
        export ARCH=riscv
        export CROSS_COMPILE=riscv64-unknown-elf-
        if [[ -n "$JOBS" ]]; then
            export JOBS
        fi
        bash -c "$cmd"
    else
        [ "$DEBUG" = true ] && echo "Building in container..."
        run_in_container "$cmd"
    fi
}

# Execute container actions
case "$ACTION" in
    build)
        if [[ "$CLEAN" == true ]]; then
            run_command "$CLEAN_CMD"
        fi
        if [[ "$BUILD_DEB" == true ]]; then
            run_command "$BUILD_DEB_CMD"
        else
            run_command "$BUILD_CMD"
        fi
        echo "Build completed successfully!"
        ;;
    shell)
        run_in_container "bash"
        echo "Container shell session ended."
        ;;
    *)
        echo "Unknown action: $ACTION"
        usage
        exit 1
        ;;
esac
