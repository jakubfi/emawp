#!/bin/bash

ACTION=$1

case $ACTION in

	init)
		cmake -E make_directory "$BUILD_DIR"
	;;

	configure)
		cmake "$SRC_DIR" -G "Unix Makefiles" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
	;;

	build)
		cmake --build "$BUILD_DIR"
	;;

	install)
		cmake --install "$BUILD_DIR"
	;;

	*)
		echo "Unknown action: $ACTION"
		exit 1
	;;

esac

# vim: tabstop=4
