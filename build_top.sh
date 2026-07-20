#!/bin/bash

export TOP_DIR=`pwd`
export TOP_CPU_DIR="${TOP_DIR}/libcpu/risc-v/spacemit"
export TOP_BSP_DIR="${TOP_DIR}/bsp/spacemit"
export TOP_BOARD_DIR="${TOP_BSP_DIR}/platform"
export TOP_ESOS_BASE_DEFCONF="${TOP_BSP_DIR}/.esos_top.config"
export TOP_ESOS_DEFCONF="${TOP_BSP_DIR}/.config"
if [ -z "${TOP_OUTPUT_DIR}" ]; then
	export TOP_OUTPUT_DIR="${TOP_DIR}/../output/esos"
fi

TARGET_CHIP=
TARGET_BOARD=
TARGET_ENTRY_POINT=
TOP_TARGET_CHIP=
TOP_TARGET_BOARD=
TOP_TARGET_PROJECT=
TOP_TARGET_ENTRY_POINT=
TOP_TARGET_DEFCONFIG=

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

	for chip in $(cd $TOP_CPU_DIR/; find -mindepth 1 -maxdepth 1 -type d |sort); do
		if [ `basename $TOP_CPU_DIR/$TOP_TARGET_CHIP/$chip` != ".git" ] ; then
			chips[$count]=`basename $TOP_CPU_DIR/$chip`
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

		TOP_TARGET_CHIP=${chips[$REPLY]}
		return 0
	else
		mk_error "No valid chip!"
		return 1
	fi
}

function select_entry_point()
{
	if [ "x${TOP_TARGET_CHIP}_${TOP_TARGET_BOARD}" = "xn308_k1-x" ]; then
		TOP_TARGET_ENTRY_POINT=0x30300000
	elif [ "x${TOP_TARGET_CHIP}_${TOP_TARGET_BOARD}" = "xrt24_os0_rcpu" ]; then
 		TOP_TARGET_ENTRY_POINT=0x100200000
	elif [ "x${TOP_TARGET_CHIP}_${TOP_TARGET_BOARD}" = "xrt24_os1_rcpu" ]; then
		TOP_TARGET_ENTRY_POINT=0x100800000
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
	'$CMD_PROMPT'                        build component"
}

# show target configuration
function show_target_config()
{
	echo
	mk_info "target configuration is as follows:"
	mk_info "-------------------------------------------------------------------------"
	cat ${TOP_ESOS_BASE_DEFCONF}
	mk_info "-------------------------------------------------------------------------"
}

# create the version id with git commit id
function create_version_id()
{
	mk_info "create the verion id ..."

	pushd ${TOP_DIR}
	commit_id=$(git log | head -1)
	version_id=${commit_id: -12}
	__version_id=".verid=\"${TOP_TARGET_BOARD}:${version_id}\""
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
	rm -rf ${TOP_ESOS_DEFCONF}
	rm -rf ${TOP_ESOS_BASE_DEFCONF}

	select_chip

	# check if the build.cfg is full configured
	if [ "x${TOP_TARGET_CHIP}" = "x" ]; then
		mk_error "TOP_TARGET_CHIP is not configured!!!"
	fi

	echo "export TOP_TARGET_CHIP=${TOP_TARGET_CHIP}" >> ${TOP_ESOS_BASE_DEFCONF}

	source ${TOP_ESOS_BASE_DEFCONF}

	show_target_config

	mk_info "prepare to toolchain ..."

	if [ "x${TOP_TARGET_CHIP}" = "xn308" ]; then
		if [ ! -d "${TOP_DIR}/tools/toolchain/gcc" ]; then
			cd ${TOP_DIR}/tools/toolchain/
			tar -jxvf ${TOP_DIR}/tools/toolchain/nuclei_riscv_newlibc_prebuilt_linux64_2022.12.tar.bz2
			cd -
		fi
	elif [ "x${TOP_TARGET_CHIP}" = "xrt24" ]; then
		if [ ! -d "${TOP_DIR}/tools/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9" ]; then
			cd ${TOP_DIR}/tools/toolchain/
			tar -xf ${TOP_DIR}/tools/toolchain/spacemit-toolchain-elf-newlib-x86_64-v1.0.9.tar.xz
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

	source ${TOP_ESOS_BASE_DEFCONF}

	# Always output as esos.itb regardless of signing
	local itb_file="esos.itb"
	# KEY_DIR is the sign/no-sign switch: present → use signed ITS + -k -r
	local its_file key_para
	if [ -n "${KEY_DIR}" ]; then
		its_file="esos_${TOP_TARGET_CHIP}_sign.its"
		key_para="-k ${KEY_DIR} -r"
	else
		its_file="esos_${TOP_TARGET_CHIP}.its"
		key_para=""
	fi

	# Use ITS template from top directory
	local its_path="${TOP_DIR}/${its_file}"
	if [ ! -f "${its_path}" ]; then
		mk_info "ITS template not found: ${its_path}"
		return 0
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

	local its_work="${TOP_DIR}/.esos_${TOP_TARGET_CHIP}.its"
	sed "s#../output/esos#${TOP_OUTPUT_DIR}#g" "${its_path}" > "${its_work}"

	mkimage -f "${its_work}" ${key_para} ${TOP_BSP_DIR}/${itb_file}
	rm -f "${its_work}"
	mk_info "ITB created: ${TOP_BSP_DIR}/${itb_file}"
	[ -n "${KEY_DIR}" ] && mkimage -l ${TOP_BSP_DIR}/${itb_file} | grep -i "sign\|Sign"

	# Copy ITB to output directory
	OUTPUT_DIR="${TOP_OUTPUT_DIR}"
	mkdir -p "${OUTPUT_DIR}"
	cp ${TOP_BSP_DIR}/${itb_file} "${OUTPUT_DIR}/"
	mk_info "ITB copied to: ${OUTPUT_DIR}/${itb_file}"

	cd -
}

function build_kernel()
{
	# build esos-lite first
	cd components/esos-lite/rt-thread/
	./build_top.sh
	cd -

	local count=0
	source ${TOP_ESOS_BASE_DEFCONF}

	mkdir -p ${TOP_OUTPUT_DIR}
	cp ./null.spacemit ${TOP_OUTPUT_DIR}

	for board in $(cd $TOP_BOARD_DIR/$TOP_TARGET_CHIP; find -mindepth 1 -maxdepth 1 -type d |grep -v default|sort); do
		if [ `basename $TOP_BOARD_DIR/$TOP_TARGET_CHIP/$board` != ".git" ] ; then
			boards[$count]=`basename $TOP_BOARD_DIR/$TOP_TARGET_CHIP/$board`
			printf "\t$count: ${boards[$count]}\n"

			TOP_TARGET_BOARD=${boards[$count]}
			BOARD_BASE_DIR="${TOP_BOARD_DIR}/${TOP_TARGET_CHIP}/${TOP_TARGET_BOARD}"

			SUB_DIRS=$(find "${BOARD_BASE_DIR}" -mindepth 1 -maxdepth 1 -type d | grep -v .git | sort)

			if [ -n "${SUB_DIRS}" ]; then
				for sub in ${SUB_DIRS}; do
					SUB_PROJECT=$(basename "${sub}")
					#build all dtb
					cd ${TOP_BSP_DIR}/platform/${TOP_TARGET_CHIP}/${TOP_TARGET_BOARD}/${SUB_PROJECT}/dts/
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
				cd ${TOP_BSP_DIR}/platform/${TOP_TARGET_CHIP}/${TOP_TARGET_BOARD}/dts/
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

			# Update config files
			echo "export TOP_TARGET_CHIP=${TOP_TARGET_CHIP}" > ${TOP_ESOS_BASE_DEFCONF}
			echo "export TOP_TARGET_BOARD=${TOP_TARGET_BOARD}" >> ${TOP_ESOS_BASE_DEFCONF}

			source ${TOP_ESOS_BASE_DEFCONF}

			select_entry_point

			TOP_TARGET_DEFCONFIG=${TOP_TARGET_CHIP}_${TOP_TARGET_BOARD}_defconfig

			echo "export TOP_TARGET_DEFCONFIG=${TOP_TARGET_DEFCONFIG}" >> ${TOP_ESOS_BASE_DEFCONF}
			echo "export TOP_TARGET_ENTRY_POINT=${TOP_TARGET_ENTRY_POINT}" >> ${TOP_ESOS_BASE_DEFCONF}

			source ${TOP_ESOS_BASE_DEFCONF}

			# generate version id
			create_version_id

			cp ${TOP_BOARD_DIR}/${TOP_TARGET_CHIP}/${TOP_TARGET_BOARD}/${TOP_TARGET_DEFCONFIG} ${TOP_BSP_DIR}/.config

			TARGET_CHIP=${TOP_TARGET_CHIP}
			TARGET_BOARD=${TOP_TARGET_BOARD}
			TARGET_ENTRY_POINT=${TOP_TARGET_ENTRY_POINT}
			export TARGET_CHIP TARGET_BOARD TARGET_ENTRY_POINT TOP_TARGET_DEFCONFIG
			cd ${TOP_BSP_DIR}
			scons --useconfig=.config
			if [ $? -ne 0 ]; then
				mk_error "Failed to load config for ${TOP_TARGET_BOARD}"
				return -1;
			fi
			# build kernel
			scons
			if [ $? -ne 0 ]; then
				mk_error "Failed to build ${core_name}"
				return -1
			fi

			# copy the elf
			cp ${TOP_TARGET_CHIP}_${TOP_TARGET_BOARD}.elf ${TOP_OUTPUT_DIR}

			# clean kernel
			scons -c
			cd -
		fi
	done

	create_esos_itb

	return 0
}

# execute some command without configuration
if [ "x$1" = "xhelp" ]; then
	build_usage
	exit 0
elif [ "x$1" = "xconfig" ]; then
	config_sdk
	exit 0
elif [ "x$1" = "x" ]; then
	build_kernel
	exit $?
fi
