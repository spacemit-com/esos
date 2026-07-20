#!/bin/bash

TARGET_CHIP=
TARGET_BOARD=
TARGET_ENTRY_POINT=
TARGET_DEFCONFIG=

export TOP_DIR=`pwd`
export CPU_DIR="${TOP_DIR}/libcpu/risc-v/spacemit"
export BSP_DIR="${TOP_DIR}/bsp/spacemit"
export BOARD_DIR="${BSP_DIR}/platform"
export ESOS_BASE_DEFCONF="${BSP_DIR}/.esos.config"
export ESOS_DEFCONF="${BSP_DIR}/.config"
export TOP_OUTPUT_DIR="${TOP_DIR}/../output/esos"

function mk_error()
{
	echo -e "\033[40;31mERROR: $*\033[0m"
}

function mk_warn()
{
	echo -e "\033[40;33;1mmWARN: $*\033[0m"
}

function mk_info()
{
	echo -e "\033[40;37mINFO: $*\033[0m"
}

function select_chip()
{
	count=0

	printf "All valid soc chips:\n"

	for chip in $(cd $CPU_DIR/; find -mindepth 1 -maxdepth 1 -type d |sort); do
		if [ `basename $CPU_DIR/$TARGET_CHIP/$chip` != ".git" ] ; then
			chips[$count]=`basename $CPU_DIR/$chip`
			printf "	$count: ${chips[$count]}\n"
			let count=$count+1
		fi
	done

	if [ "$count" -gt 0 ] ; then
		while true; do
			read -p "Please select a chip:"
			RES=`expr match $REPLY "[0-9][0-9]*$"`
			if [ "$RES" -le 0 ]; then
				printf "please use index number\n"
				continue
			fi
			if [ "$REPLY" -ge $count ] || [ "$REPLY" -lt "0" ]; then
				mk_error "input is invalid!"
				continue
			fi
			break
		done

		TARGET_CHIP=${chips[$REPLY]}
		return 0
	else
		mk_error "No valid chip!"
		return 1
	fi
}

function select_board()
{
	count=0

	printf "All valid boards:\n"

	for board in $(cd $BOARD_DIR/$TARGET_CHIP; find -mindepth 1 -maxdepth 1 -type d |grep -v default|sort); do
		if [ `basename $BOARD_DIR/$TARGET_CHIP/$board` != ".git" ] ; then
			boards[$count]=`basename $BOARD_DIR/$TARGET_CHIP/$board`
			printf "\t$count: ${boards[$count]}\n"
			let count=$count+1
		fi
	done

	if [ "$count" -gt 0 ] ; then
		while true; do
			read -p "Please select a board:"
			RES=`expr match $REPLY "[0-9][0-9]*$"`
			if [ "$RES" -le 0 ]; then
				printf "please use index number\n"
				continue
			fi
			if [ "$REPLY" -ge $count ] || [ "$REPLY" -lt "0" ]; then
				printf "input is invalid!\n"
				continue
			fi
			break
		done

		TARGET_BOARD=${boards[$REPLY]}
		return 0
	else
		mk_error "No valid board!"
		return 1
	fi
}

function select_entry_point()
{
	if [ "x${TARGET_CHIP}_${TARGET_BOARD}" = "xn308_k1-x" ]; then
		TARGET_ENTRY_POINT=0x30300000
	elif [ "x${TARGET_CHIP}_${TARGET_BOARD}" = "xrt24_os0_rcpu" ]; then
 		TARGET_ENTRY_POINT=0x100200000
	elif [ "x${TARGET_CHIP}_${TARGET_BOARD}" = "xrt24_os1_rcpu" ]; then
		TARGET_ENTRY_POINT=0x100800000
	else
		mk_error "No valid entry point!"
		return 1
	fi
}

# build script usage helper
function build_usage()
{
	CMD_PROMPT="./build.sh"
	mk_info "usage of build script is as follows:
	'$CMD_PROMPT config'                 set the SDK configuration
	'$CMD_PROMPT'                        build component
        '$CMD_PROMPT'menuconfig              config the SDK
        '$CMD_PROMPT'itb                     Generate ITB file
	'$CMD_PROMPT clean'                  clean the kernel\n"
}

# show target configuration
function show_target_config()
{
	echo
	mk_info "target configuration is as follows:"
	mk_info "-------------------------------------------------------------------------"
	cat ${ESOS_BASE_DEFCONF}
	mk_info "-------------------------------------------------------------------------"
}

# create the version id with git commit id
function create_version_id()
{
	mk_info "create the verion id ..."

	pushd ${TOP_DIR}
	commit_id=$(git log | head -1)
	version_id=${commit_id: -12}
	__version_id=".verid=\"${TARGET_BOARD}:${version_id}\""
	echo ${__version_id}
	version_id_cfg_file=${TOP_DIR}/bsp/spacemit/platform/version_id_gen.cc
	rm -f ${version_id_cfg_file}
	touch ${version_id_cfg_file}
	echo "struct version_id __versionid spacemit_verid = {" >> ${version_id_cfg_file}
	echo "    ${__version_id}," >> ${version_id_cfg_file}
	echo "};" >> ${version_id_cfg_file}
	popd
}

function config_sdk()
{
	mk_info "prepare to config esos sdk ..."

	# delete the old configuration script
	rm -rf ${ESOS_DEFCONF}
	rm -rf ${ESOS_BASE_DEFCONF}

	select_chip
	select_board
	select_entry_point

	TARGET_DEFCONFIG=${TARGET_CHIP}_${TARGET_BOARD}_defconfig

	# check if the build.cfg is full configured
	if [ "x${TARGET_CHIP}" = "x" ]; then
		mk_error "TARGET_CHIP is not configured!!!"
	fi

	if [ "x${TARGET_BOARD}" = "x" ]; then
		mk_error "TARGET_BOARD is not configured!!!"
	fi

	cp ${BOARD_DIR}/${TARGET_CHIP}/${TARGET_BOARD}/${TARGET_DEFCONFIG} ${BSP_DIR}/.config

	echo "export TARGET_CHIP=${TARGET_CHIP}" >> ${ESOS_BASE_DEFCONF}
	echo "export TARGET_BOARD=${TARGET_BOARD}" >> ${ESOS_BASE_DEFCONF}
	echo "export TARGET_DEFCONFIG=${TARGET_DEFCONFIG}" >> ${ESOS_BASE_DEFCONF}
	echo "export TARGET_ENTRY_POINT=${TARGET_ENTRY_POINT}" >> ${ESOS_BASE_DEFCONF}

	source ${ESOS_BASE_DEFCONF}

	show_target_config

	mk_info "prepare to toolchain ..."

	if [ "x${TARGET_CHIP}" = "xn308" ]; then
		if [ ! -d "${TOP_DIR}/tools/toolchain/gcc" ]; then
			mkdir -p ${TOP_DIR}/tools/toolchain/
			cd ${TOP_DIR}/tools/toolchain/
			if [ ! -f "nuclei_riscv_newlibc_prebuilt_linux64_2022.12.tar.bz2" ]; then
				mk_info "Downloading nuclei toolchain..."
				wget http://archive.spacemit.com/toolchain/nuclei_riscv_newlibc_prebuilt_linux64_2022.12.tar.bz2
				if [ $? -ne 0 ]; then
					mk_error "Failed to download nuclei toolchain"
					cd -
					return 1
				fi
			fi
			tar -jxvf nuclei_riscv_newlibc_prebuilt_linux64_2022.12.tar.bz2
			cd -
		fi
	elif [ "x${TARGET_CHIP}" = "xrt24" ]; then
		if [ ! -d "${TOP_DIR}/tools/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9" ]; then
			mkdir -p ${TOP_DIR}/tools/toolchain/
			cd ${TOP_DIR}/tools/toolchain/
			if [ ! -f "spacemit-toolchain-elf-newlib-x86_64-v1.0.9.tar.xz" ]; then
				mk_info "Downloading spacemit toolchain..."
				wget http://archive.spacemit.com/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9.tar.xz
				if [ $? -ne 0 ]; then
					mk_error "Failed to download spacemit toolchain"
					cd -
					return 1
				fi
			fi
			tar -xf spacemit-toolchain-elf-newlib-x86_64-v1.0.9.tar.xz
			cd -
		fi
	fi

	# create the rtconfig.h, it will be updated
	touch ${TOP_DIR}/bsp/spacemit/rtconfig.h
}

function compress_esos_fit_inputs()
{
	local output_dir="$1"
	local payload=

	if ! command -v lzop >/dev/null 2>&1; then
		mk_error "lzop not found. Please install lzop before generating ESOS ITB."
		return 1
	fi

	if [ ! -d "${output_dir}" ]; then
		mk_error "ESOS output directory not found: ${output_dir}"
		return 1
	fi

	while IFS= read -r -d '' payload; do
		mk_info "Compressing FIT payload: ${payload}"
		if ! lzop -9 -f "${payload}"; then
			mk_error "Failed to compress FIT payload: ${payload}"
			return 1
		fi
	done < <(find "${output_dir}" -maxdepth 1 -type f \( -name "*.dtb" -o -name "*.elf" \) -print0)

	# Keep the ITS self-consistent by compressing the AP interaction blob too.
	if [ -f "${output_dir}/null.spacemit" ]; then
		mk_info "Compressing FIT payload: ${output_dir}/null.spacemit"
		if ! lzop -9 -f "${output_dir}/null.spacemit"; then
			mk_error "Failed to compress FIT payload: ${output_dir}/null.spacemit"
			return 1
		fi
	fi
}

function create_esos_itb()
{
	mk_info "Creating ESOS ITB package..."

	source ${ESOS_BASE_DEFCONF}

	# Always output as esos.itb regardless of signing
	local itb_file="esos.itb"
	# KEY_DIR is the sign/no-sign switch: present → use signed ITS + -k -r
	local its_file key_para
	if [ -n "${KEY_DIR}" ]; then
		its_file="esos_${TARGET_CHIP}_sign.its"
		key_para="-k ${KEY_DIR} -r"
	else
		its_file="esos_${TARGET_CHIP}.its"
		key_para=""
	fi
	# Use ITS template from top directory
	local its_path="${TOP_DIR}/${its_file}"
	if [ ! -f "${its_path}" ]; then
		mk_error "ITS template not found: ${its_path}"
		return 1
	fi
	mk_info "Using ITS template: ${its_path}"

	# Check if mkimage is available
	if ! command -v mkimage >/dev/null 2>&1; then
		mk_warn "mkimage not found, ITB not created. Please install u-boot-tools."
		return 1
	fi

	if ! compress_esos_fit_inputs "${TOP_OUTPUT_DIR}"; then
		return 1
	fi

	# Generate ITB using mkimage (run from TOP_DIR for correct relative paths in ITS)
	cd ${TOP_DIR}

	mkimage -f "${its_path}" ${key_para} ${BSP_DIR}/${itb_file}
	mk_info "ITB created: ${BSP_DIR}/${itb_file}"
	[ -n "${KEY_DIR}" ] && mkimage -l ${BSP_DIR}/${itb_file} | grep -i "sign\|Sign"

	# Copy ITB to output directory
	if [ -d "${TOP_DIR}/../humbird" ]; then
		OUTPUT_DIR="${TOP_DIR}/../output/esos"
		mkdir -p "${OUTPUT_DIR}"
		cp ${BSP_DIR}/${itb_file} "${OUTPUT_DIR}/"
		cp ${BSP_DIR}/${itb_file} "${TOP_DIR}/../output/"
		mk_info "ITB copied to: ${OUTPUT_DIR}/${itb_file}"
	fi

	cd -
}

function build_kernel()
{
	if [ -d "${TOP_DIR}/../humbird" ]; then
		mkdir -p ${TOP_OUTPUT_DIR}
	fi

	cp ./null.spacemit ${TOP_OUTPUT_DIR}

	# build dtb
	source ${ESOS_BASE_DEFCONF}

	BOARD_BASE_DIR="${BOARD_DIR}/${TARGET_CHIP}/${TARGET_BOARD}"

	SUB_DIRS=$(find "${BOARD_BASE_DIR}" -mindepth 1 -maxdepth 1 -type d | grep -v .git | sort)
	if [ -n "${SUB_DIRS}" ]; then
		for sub in ${SUB_DIRS}; do
			SUB_PROJECT=$(basename "${sub}")
			#build all dtb
			cd ${BSP_DIR}/platform/${TARGET_CHIP}/${TARGET_BOARD}/${SUB_PROJECT}/dts/
			make
			if [ $? -ne 0 ]; then
				mk_error "Failed to build dtb"
				cd -
				return 1
			fi

			cp ./*.dtb ${TOP_OUTPUT_DIR}/
			make clean
			cd -

		done
	else
		#build dtb
		cd ${BSP_DIR}/platform/${TARGET_CHIP}/${TARGET_BOARD}/dts/
		make
		if [ $? -ne 0 ]; then
			mk_error "Failed to build dtb"
			cd -
			return 1
		fi
		cp ./*.dtb ../../
		make clean
		cd -
	fi

	# generate version id
	create_version_id

	# build src
	source ${ESOS_BASE_DEFCONF}
	# Export variables for Python scripts
	export TARGET_CHIP TARGET_BOARD TARGET_ENTRY_POINT TARGET_DEFCONFIG
	cd ${BSP_DIR}
	scons --useconfig=.config
	if [ $? -ne 0 ]; then
		mk_error "Failed to load config"
		cd -
		return 1
	fi
	scons
	if [ $? -ne 0 ]; then
		mk_error "Failed to build esos"
		cd -
		return 1
	fi

	if [ -d "${TOP_DIR}/../humbird" ]; then
		cp ${TARGET_CHIP}_${TARGET_BOARD}.elf ${TOP_OUTPUT_DIR}
	fi

	cd -
	return 0
}

function kernel_menuconfig()
{
	# build src
	source ${ESOS_BASE_DEFCONF}
	# Export variables for Python scripts
	export TARGET_CHIP TARGET_BOARD TARGET_ENTRY_POINT TARGET_DEFCONFIG
	cd ${BSP_DIR}
	scons --useconfig=.config
	if [ $? -ne 0 ]; then
		mk_error "Failed to load config"
		cd -
		return 1
	fi

	# Calculate MD5 before menuconfig
	if [ -f .config ]; then
		CUR_MD5=`md5sum .config`
	else
		CUR_MD5=""
	fi

	scons --menuconfig
	if [ $? -ne 0 ]; then
		mk_error "Failed to build esos"
		cd -
		return 1
	fi
	
	# Calculate MD5 after menuconfig
	if [ -f .config ]; then
		NEW_MD5=`md5sum .config`
	else
		NEW_MD5=""
	fi

	# If config changed, save back to defconfig
	if [ "${CUR_MD5}" != "${NEW_MD5}" ]; then
		mk_info "Configuration changed, saving to defconfig..."

		# Save full .config format (preserves all options and comments)
		if [ -d "platform/${TARGET_CHIP}/${TARGET_BOARD}/" ]; then
			cp .config platform/${TARGET_CHIP}/${TARGET_BOARD}/${TARGET_DEFCONFIG}
			mk_info "Updated platform/${TARGET_CHIP}/${TARGET_BOARD}/${TARGET_DEFCONFIG}"
		fi
	else
		mk_info "Configuration not changed"
	fi

	cd -

	return 0
}

function clean_kernel()
{
	# clean src
	source ${ESOS_BASE_DEFCONF}
	cd ${BSP_DIR}
	touch rtconfig.h
	scons -c
	cd -
}

# execute some command without configuration
if [ "x$1" = "xhelp" ]; then
	build_usage
	exit 0
elif [ "x$1" = "xconfig" ]; then
	config_sdk
	exit 0
elif [ "x$1" = "xmenuconfig" ]; then
	kernel_menuconfig
	exit 0
elif [ "x$1" = "x" ]; then
	build_kernel
	exit $?
elif [ "x$1" = "xitb" ]; then
	create_esos_itb
	exit $?
elif [ "x$1" = "xclean" ]; then
	clean_kernel
	exit 0
fi

